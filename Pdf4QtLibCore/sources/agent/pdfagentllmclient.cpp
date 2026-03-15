// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pdfagentllmclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariant>
#include <QLoggingCategory>

namespace
{

QString extractErrorMessage(const QJsonObject& object)
{
    if (object.contains("error"))
    {
        const QJsonValue errorValue = object.value("error");
        if (errorValue.isObject())
        {
            const QString message = errorValue.toObject().value("message").toString();
            if (!message.isEmpty())
            {
                return message;
            }
        }
        else if (errorValue.isString())
        {
            return errorValue.toString();
        }
    }

    return QString();
}

}   // namespace

namespace pdf
{

QJsonObject PDFAgentNormalizedResponse::toJsonObject() const
{
    QJsonObject object;
    object["ok"] = ok;
    object["provider"] = provider;
    object["endpoint"] = endpoint;
    object["response_id"] = responseId;
    object["object"] = objectType;
    object["created"] = created;
    object["model"] = modelName;
    object["http_status_code"] = httpStatusCode;
    object["assistant"] = QJsonObject{
        { "role", assistantRole },
        { "content", assistantText }
    };
    object["finish_reason"] = finishReason;
    object["usage"] = QJsonObject{
        { "prompt_tokens", promptTokens },
        { "completion_tokens", completionTokens },
        { "total_tokens", totalTokens },
        { "prompt_tokens_details", promptTokenDetails },
        { "prompt_cache_hit_tokens", promptCacheHitTokens },
        { "prompt_cache_miss_tokens", promptCacheMissTokens }
    };
    object["system_fingerprint"] = systemFingerprint;
    object["error"] = errorMessage;
    object["raw_response_text"] = rawResponseText;
    return object;
}

QString PDFAgentNormalizedResponse::toPrettyJson() const
{
    return QString::fromUtf8(QJsonDocument(toJsonObject()).toJson(QJsonDocument::Indented));
}

PDFAgentLlmClient::PDFAgentLlmClient(QObject* parent) :
    QObject(parent),
    m_networkAccessManager(new QNetworkAccessManager(this)),
    m_activeReply(nullptr),
    m_requestTimedOut(false)
{
    m_requestTimer.setSingleShot(true);
    connect(&m_requestTimer, &QTimer::timeout, this, &PDFAgentLlmClient::onRequestTimedOut);
}

void PDFAgentLlmClient::sendChat(const QVector<PDFAgentChatMessage>& messages,
                                 const PDFAgentLlmConfig& config)
{
    const QString validationError = validateChatRequest(messages, config);
    if (!validationError.isEmpty())
    {
        PDFAgentLlmResponse response;
        response.errorMessage = validationError;
        finishWithResponse(response);
        return;
    }

    if (m_activeReply)
    {
        PDFAgentLlmResponse response;
        response.errorMessage = tr("Another AI request is already in progress.");
        finishWithResponse(response);
        return;
    }

    m_requestTimedOut = false;

    const QNetworkRequest request = buildRequest(config);
    const QJsonObject payload = buildPayload(messages, config);
    const QByteArray requestBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_networkAccessManager->post(request, requestBody);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, &PDFAgentLlmClient::onReplyFinished);

    m_requestTimer.start(config.timeoutMs > 0 ? config.timeoutMs : 30000);
}

void PDFAgentLlmClient::sendChatWithTools(const QVector<PDFAgentChatMessage>& messages,
                                          const QJsonArray& tools,
                                          const PDFAgentLlmConfig& config)
{
    const QString validationError = validateChatRequest(messages, config);
    if (!validationError.isEmpty())
    {
        qDebug() << "VALIDATION FAILED:" << validationError;
        // Debug: show message details
        for (const auto& msg : messages)
        {
            qDebug() << "  Msg role:" << msg.role << "content len:" << msg.content.length() << "toolCallId len:" << msg.toolCallId.length();
        }
        PDFAgentLlmResponse response;
        response.errorMessage = validationError;
        finishWithResponse(response);
        return;
    }

    if (m_activeReply)
    {
        PDFAgentLlmResponse response;
        response.errorMessage = tr("Another AI request is already in progress.");
        finishWithResponse(response);
        return;
    }

    m_requestTimedOut = false;

    const QNetworkRequest request = buildRequest(config);
    const QJsonObject payload = buildPayloadWithTools(messages, tools, config);
    const QByteArray requestBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    qDebug() << "Sending request to:" << config.endpoint;
    qDebug() << "Request body:" << requestBody.constData();

    QNetworkReply* reply = m_networkAccessManager->post(request, requestBody);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, &PDFAgentLlmClient::onReplyFinished);

    m_requestTimer.start(config.timeoutMs > 0 ? config.timeoutMs : 30000);
}

QString PDFAgentLlmClient::validateChatRequest(const QVector<PDFAgentChatMessage>& messages,
                                               const PDFAgentLlmConfig& config)
{
    if (config.endpoint.trimmed().isEmpty())
    {
        return tr("LLM endpoint is empty.");
    }

    if (config.model.trimmed().isEmpty())
    {
        return tr("LLM model is empty.");
    }

    if (messages.isEmpty())
    {
        return tr("At least one chat message is required.");
    }

    for (const PDFAgentChatMessage& message : messages)
    {
        if (message.role.trimmed().isEmpty())
        {
            return tr("Chat message role is empty.");
        }

        // Allow empty content for assistant messages with tool_calls
        // Also allow empty content for tool messages
        const bool hasToolCalls = !message.toolCallId.isEmpty() && message.role == "assistant";
        const bool isToolMessage = message.role == "tool";
        if (message.content.trimmed().isEmpty() && !hasToolCalls && !isToolMessage)
        {
            return tr("Chat message content is empty.");
        }
    }

    return QString();
}

PDFAgentLlmResponse PDFAgentLlmClient::parseChatResponse(const QByteArray& body,
                                                         int httpStatusCode)
{
    PDFAgentLlmResponse response;
    response.httpStatusCode = httpStatusCode;
    response.rawResponseBody = body;
    response.rawResponseText = QString::fromUtf8(body);

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        response.errorMessage = QObject::tr("Failed to parse LLM response JSON: %1").arg(parseError.errorString());
        return response;
    }

    response.rawJson = document.object();

    if (httpStatusCode >= 300)
    {
        QString errorMessage = extractErrorMessage(response.rawJson);
        if (errorMessage.isEmpty())
        {
            errorMessage = QObject::tr("LLM request failed with HTTP status %1.").arg(httpStatusCode);
        }
        response.errorMessage = errorMessage;
        return response;
    }

    const QJsonArray choices = response.rawJson.value("choices").toArray();
    if (choices.isEmpty())
    {
        response.errorMessage = QObject::tr("LLM response did not contain any choices.");
        return response;
    }

    const QJsonObject messageObject = choices.at(0).toObject().value("message").toObject();
    if (messageObject.isEmpty())
    {
        response.errorMessage = QObject::tr("LLM response did not contain a message object.");
        return response;
    }

    const QJsonArray toolCalls = messageObject.value("tool_calls").toArray();
    const QString content = messageObject.value("content").toString();

    // Handle tool calls - allow them in Phase 4
    // Content can be empty when LLM only responds with tool_calls (no text explanation)
    if (!toolCalls.isEmpty())
    {
        // Store tool calls in raw response for parsing by assistant turn
        // For now, return as success but with special handling
        response.success = true;
        response.assistantText = content;
        return response;
    }

    // Only require non-empty content when there are no tool calls
    // If content is empty AND no tool_calls, that's an error
    if (content.trimmed().isEmpty())
    {
        response.errorMessage = QObject::tr("Assistant response did not contain text content or tool calls.");
        return response;
    }

    response.success = true;
    response.assistantText = content;
    return response;
}

PDFAgentAssistantTurn PDFAgentLlmClient::parseAssistantTurn(const QByteArray& body, int httpStatusCode)
{
    PDFAgentAssistantTurn turn;
    turn.success = false;

    // Try to parse as-is first
    QString bodyStr = QString::fromUtf8(body);

    // If body is empty but HTTP status is success, it might be an edge case
    if (bodyStr.trimmed().isEmpty())
    {
        if (httpStatusCode == 200)
        {
            // Empty body with 200 status - might be streaming or special case
            turn.success = true;
            turn.assistantText = QString();
            return turn;
        }
        turn.errorMessage = QObject::tr("Empty response from LLM. HTTP status: %1").arg(httpStatusCode);
        return turn;
    }

    // Try to clean up markdown-wrapped JSON (e.g., ```json ... ```)
    QString jsonStr = bodyStr.trimmed();
    if (jsonStr.startsWith("```json"))
    {
        jsonStr = jsonStr.mid(7);
    }
    else if (jsonStr.startsWith("```"))
    {
        jsonStr = jsonStr.mid(3);
    }

    if (jsonStr.endsWith("```"))
    {
        jsonStr = jsonStr.chopped(3);
    }

    jsonStr = jsonStr.trimmed();
    QByteArray cleanedBody = jsonStr.toUtf8();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(cleanedBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        turn.errorMessage = QObject::tr("Failed to parse LLM response JSON: %1").arg(parseError.errorString());
        return turn;
    }

    turn.rawJson = document.object();

    if (httpStatusCode >= 300)
    {
        turn.errorMessage = QObject::tr("LLM request failed with HTTP status %1.").arg(httpStatusCode);
        return turn;
    }

    const QJsonArray choices = turn.rawJson.value("choices").toArray();
    if (choices.isEmpty())
    {
        turn.errorMessage = QObject::tr("LLM response did not contain any choices.");
        return turn;
    }

    const QJsonObject messageObject = choices.at(0).toObject().value("message").toObject();
    if (messageObject.isEmpty())
    {
        turn.errorMessage = QObject::tr("LLM response did not contain a message object.");
        return turn;
    }

    // Extract tool calls
    const QJsonArray toolCallsArray = messageObject.value("tool_calls").toArray();
    const QString content = messageObject.value("content").toString();

    if (!toolCallsArray.isEmpty())
    {
        // Parse tool calls
        for (const QJsonValue& tc : toolCallsArray)
        {
            const QJsonObject tcObj = tc.toObject();
            const QJsonObject functionObj = tcObj.value("function").toObject();

            if (functionObj.isEmpty())
            {
                continue;
            }

            PdfAgentToolCall toolCall;
            toolCall.id = tcObj.value("id").toString();
            toolCall.name = functionObj.value("name").toString();

            // Arguments can be a string or object
            const QJsonValue argumentsValue = functionObj.value("arguments");
            if (argumentsValue.isString())
            {
                // Parse JSON string to object
                const QString argsStr = argumentsValue.toString();
                QJsonParseError parseErr;
                const QJsonDocument argsDoc = QJsonDocument::fromJson(argsStr.toUtf8(), &parseErr);
                if (parseErr.error == QJsonParseError::NoError && argsDoc.isObject())
                {
                    toolCall.arguments = argsDoc.object();
                }
                else
                {
                    // Invalid JSON arguments
                    continue;
                }
            }
            else if (argumentsValue.isObject())
            {
                toolCall.arguments = argumentsValue.toObject();
            }

            if (!toolCall.id.isEmpty() && !toolCall.name.isEmpty())
            {
                turn.toolCalls.append(toolCall);
            }
        }

        turn.success = true;
        return turn;
    }

    // Regular text response
    if (!content.isEmpty())
    {
        turn.success = true;
        turn.assistantText = content;
        return turn;
    }

    turn.errorMessage = QObject::tr("Assistant response did not contain text content or tool calls.");
    return turn;
}

PDFAgentNormalizedResponse PDFAgentLlmClient::normalizeChatResponse(const PDFAgentLlmResponse& response,
                                                                    const QString& endpoint)
{
    PDFAgentNormalizedResponse normalized;
    normalized.ok = response.success;
    normalized.endpoint = endpoint;
    normalized.httpStatusCode = response.httpStatusCode;
    normalized.errorMessage = response.errorMessage;
    normalized.rawResponseBody = response.rawResponseBody;
    normalized.rawResponseText = response.rawResponseText;
    normalized.rawJson = response.rawJson;

    if (!endpoint.trimmed().isEmpty())
    {
        const QUrl url(endpoint);
        normalized.provider = url.host();
    }

    if (!response.rawJson.isEmpty())
    {
        normalized.responseId = response.rawJson.value("id").toString();
        normalized.objectType = response.rawJson.value("object").toString();
        normalized.created = response.rawJson.value("created").toInteger(-1);
        normalized.modelName = response.rawJson.value("model").toString();
        normalized.systemFingerprint = response.rawJson.value("system_fingerprint").toString();

        const QJsonObject usageObject = response.rawJson.value("usage").toObject();
        normalized.promptTokens = usageObject.value("prompt_tokens").toInt(-1);
        normalized.completionTokens = usageObject.value("completion_tokens").toInt(-1);
        normalized.totalTokens = usageObject.value("total_tokens").toInt(-1);
        normalized.promptTokenDetails = usageObject.value("prompt_tokens_details").toObject();
        normalized.promptCacheHitTokens = usageObject.value("prompt_cache_hit_tokens").toInt(-1);
        normalized.promptCacheMissTokens = usageObject.value("prompt_cache_miss_tokens").toInt(-1);

        const QJsonArray choices = response.rawJson.value("choices").toArray();
        if (!choices.isEmpty())
        {
            const QJsonObject choiceObject = choices.at(0).toObject();
            const QJsonObject messageObject = choiceObject.value("message").toObject();
            normalized.assistantRole = messageObject.value("role").toString();
            normalized.assistantText = messageObject.value("content").toString();
            normalized.finishReason = choiceObject.value("finish_reason").toString();
        }
    }

    if (normalized.assistantText.isEmpty())
    {
        normalized.assistantText = response.assistantText;
    }

    return normalized;
}

QString PDFAgentLlmClient::formatNormalizedResponse(const PDFAgentNormalizedResponse& response)
{
    return response.toPrettyJson();
}

QNetworkRequest PDFAgentLlmClient::buildRequest(const PDFAgentLlmConfig& config)
{
    QNetworkRequest request{ QUrl(config.endpoint) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    if (!config.apiKey.trimmed().isEmpty())
    {
        request.setRawHeader("Authorization", QByteArray("Bearer ").append(config.apiKey.toUtf8()));
    }

    return request;
}

QJsonObject PDFAgentLlmClient::buildPayload(const QVector<PDFAgentChatMessage>& messages,
                                            const PDFAgentLlmConfig& config)
{
    QJsonArray jsonMessages;
    for (const PDFAgentChatMessage& message : messages)
    {
        QJsonObject jsonMessage;
        jsonMessage["role"] = message.role;
        jsonMessage["content"] = message.content;
        jsonMessages.append(jsonMessage);
    }

    QJsonObject payload;
    payload["model"] = config.model;
    payload["messages"] = jsonMessages;
    payload["temperature"] = config.temperature;
    return payload;
}

QJsonObject PDFAgentLlmClient::buildPayloadWithTools(const QVector<PDFAgentChatMessage>& messages,
                                                      const QJsonArray& tools,
                                                      const PDFAgentLlmConfig& config)
{
    QJsonArray jsonMessages;
    for (const PDFAgentChatMessage& message : messages)
    {
        QJsonObject jsonMessage;

        // Handle tool messages
        if (message.role == "tool")
        {
            jsonMessage["role"] = "tool";
            jsonMessage["content"] = message.content;
            if (!message.toolCallId.isEmpty())
            {
                jsonMessage["tool_call_id"] = message.toolCallId;
            }
        }
        else
        {
            // Regular message (user, assistant, system)
            jsonMessage["role"] = message.role;
            jsonMessage["content"] = message.content;

            // Handle assistant messages with tool_calls embedded in content as JSON
            if (message.role == "assistant" && !message.toolCallId.isEmpty())
            {
                // This is a hack: we're using toolCallId to pass tool_calls JSON
                // Better approach would be to have a separate field
                QJsonParseError err;
                QJsonDocument doc = QJsonDocument::fromJson(message.toolCallId.toUtf8(), &err);
                if (err.error == QJsonParseError::NoError && doc.isArray())
                {
                    jsonMessage["tool_calls"] = doc.array();
                    jsonMessage["content"] = QJsonValue(); // null content
                }
            }
        }

        jsonMessages.append(jsonMessage);
    }

    QJsonObject payload;
    payload["model"] = config.model;
    payload["messages"] = jsonMessages;
    payload["temperature"] = config.temperature;

    // Add tools if provided
    if (!tools.isEmpty())
    {
        payload["tools"] = tools;
    }

    // Debug output
    qDebug() << "Payload with tools:" << QJsonDocument(payload).toJson(QJsonDocument::Indented).constData();

    return payload;
}

void PDFAgentLlmClient::finishWithResponse(const PDFAgentLlmResponse& response)
{
    if (m_activeReply)
    {
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    m_requestTimer.stop();
    m_requestTimedOut = false;
    Q_EMIT chatFinished(response);
}

void PDFAgentLlmClient::onReplyFinished()
{
    if (!m_activeReply)
    {
        return;
    }

    const QByteArray body = m_activeReply->readAll();
    const int httpStatusCode = m_activeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qDebug() << "Reply finished - HTTP status:" << httpStatusCode << "body size:" << body.size();
    qDebug() << "Reply body:" << QString::fromUtf8(body).left(500);

    PDFAgentLlmResponse response;
    if (m_requestTimedOut)
    {
        response.httpStatusCode = httpStatusCode;
        response.rawResponseBody = body;
        response.rawResponseText = QString::fromUtf8(body);
        response.errorMessage = tr("The AI request timed out.");
    }
    else if (m_activeReply->error() != QNetworkReply::NoError)
    {
        response.httpStatusCode = httpStatusCode;
        response.rawResponseBody = body;
        response.rawResponseText = QString::fromUtf8(body);
        response.errorMessage = tr("Network error while contacting the AI service: %1").arg(m_activeReply->errorString());
    }
    else
    {
        response = parseChatResponse(body, httpStatusCode);
    }

    finishWithResponse(response);
}

void PDFAgentLlmClient::onRequestTimedOut()
{
    m_requestTimedOut = true;

    if (m_activeReply)
    {
        m_activeReply->abort();
    }
}

}   // namespace pdf
