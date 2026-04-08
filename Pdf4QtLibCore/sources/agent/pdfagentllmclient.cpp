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
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
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

enum class LlmProvider
{
    OpenAICompatible,
    Gemini
};

LlmProvider detectProvider(const QString& endpoint)
{
    const QString normalized = endpoint.trimmed().toLower();
    if (normalized.contains("generativelanguage.googleapis.com") || normalized.contains(":generatecontent"))
    {
        return LlmProvider::Gemini;
    }

    return LlmProvider::OpenAICompatible;
}

bool isDashScopeCompatibleEndpoint(const QString& endpoint)
{
    const QString normalized = endpoint.trimmed().toLower();
    return normalized.contains("dashscope.aliyuncs.com")
           || normalized.contains("dashscope-intl.aliyuncs.com")
           || normalized.contains("dashscope-us.aliyuncs.com");
}

void applyGeminiThinkingConfig(QJsonObject& generationConfig, const QString& model)
{
    const QString normalizedModel = model.trimmed().toLower();
    if (normalizedModel.startsWith("gemini-3"))
    {
        generationConfig["thinkingConfig"] = QJsonObject{{"thinkingLevel", "high"}};
    }
    else if (normalizedModel.startsWith("gemini-2.5"))
    {
        generationConfig["thinkingConfig"] = QJsonObject{{"thinkingBudget", -1}};
    }
}

QUrl resolveEndpointUrl(const pdf::PDFAgentLlmConfig& config, bool streaming)
{
    QUrl url(config.endpoint);
    if (detectProvider(config.endpoint) != LlmProvider::Gemini)
    {
        QString path = url.path();
        const QString normalizedPath = path.trimmed().toLower();
        const bool hasChatCompletions = normalizedPath.endsWith("/chat/completions");
        const bool hasResponses = normalizedPath.endsWith("/responses");

        if (!hasChatCompletions && !hasResponses)
        {
            if (!path.endsWith('/'))
            {
                path.append('/');
            }
            path.append(QStringLiteral("chat/completions"));
            url.setPath(path);
        }
        return url;
    }

    QString path = url.path();
    if (!path.contains(":generateContent") && !path.contains(":streamGenerateContent"))
    {
        if (!path.endsWith('/'))
        {
            path.append('/');
        }
        path.append(QString("models/%1:%2")
                        .arg(config.model, streaming ? "streamGenerateContent" : "generateContent"));
    }
    else if (streaming)
    {
        path.replace(":generateContent", ":streamGenerateContent");
    }

    url.setPath(path);
    if (streaming)
    {
        QUrlQuery query(url);
        query.addQueryItem("alt", "sse");
        url.setQuery(query);
    }
    return url;
}

QJsonArray parseOpenAiToolCalls(const QJsonObject& rawAssistantMessage)
{
    if (rawAssistantMessage.contains("tool_calls") && rawAssistantMessage.value("tool_calls").isArray())
    {
        return rawAssistantMessage.value("tool_calls").toArray();
    }

    return QJsonArray();
}

QJsonObject parseJsonObjectOrWrapText(const QString& text)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject())
    {
        return document.object();
    }

    return QJsonObject{{"result", text}};
}

QJsonObject buildGeminiPartFromToolResult(const pdf::PDFAgentChatMessage& message)
{
    QJsonObject functionResponse;
    functionResponse["name"] = message.toolName.isEmpty() ? QStringLiteral("tool_result") : message.toolName;
    functionResponse["response"] = parseJsonObjectOrWrapText(message.content);

    QJsonObject part;
    part["functionResponse"] = functionResponse;
    return part;
}

QString ensureDataUrlFromImagePart(const pdf::PDFAgentImagePart& image)
{
    if (!image.dataUrl.trimmed().isEmpty())
    {
        return image.dataUrl;
    }

    if (image.filePath.trimmed().isEmpty())
    {
        return QString();
    }

    QFile file(image.filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }

    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty())
    {
        return QString();
    }

    QString mimeType = image.mimeType.trimmed();
    if (mimeType.isEmpty())
    {
        QMimeDatabase mimeDatabase;
        mimeType = mimeDatabase.mimeTypeForFile(QFileInfo(image.filePath)).name();
        if (mimeType.isEmpty())
        {
            mimeType = QStringLiteral("image/png");
        }
    }

    return QStringLiteral("data:%1;base64,%2")
        .arg(mimeType, QString::fromLatin1(bytes.toBase64()));
}

QByteArray dataUrlToInlineBytes(const QString& dataUrl)
{
    const int commaIndex = dataUrl.indexOf(',');
    if (commaIndex < 0)
    {
        return QByteArray();
    }

    return QByteArray::fromBase64(dataUrl.mid(commaIndex + 1).toLatin1());
}

QString dataUrlMimeType(const QString& dataUrl)
{
    if (!dataUrl.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive))
    {
        return QString();
    }

    const int semicolonIndex = dataUrl.indexOf(';');
    if (semicolonIndex <= 5)
    {
        return QString();
    }

    return dataUrl.mid(5, semicolonIndex - 5);
}

QString collectTextFromParts(const QVector<pdf::PDFAgentMessagePart>& parts)
{
    QStringList texts;
    for (const pdf::PDFAgentMessagePart& part : parts)
    {
        if (part.isText() && !part.text.isEmpty())
        {
            texts.append(part.text);
        }
    }
    return texts.join(QString());
}

QString buildImageMetadataText(const pdf::PDFAgentImagePart& image)
{
    if (!image.hasSerializableMetadata())
    {
        return QString();
    }

    QStringList lines;
    lines << QStringLiteral("[Image metadata]");
    if (!image.title.trimmed().isEmpty())
    {
        lines << QStringLiteral("title: %1").arg(image.title);
    }
    if (!image.subtitle.trimmed().isEmpty())
    {
        lines << QStringLiteral("subtitle: %1").arg(image.subtitle);
    }
    if (!image.sourceType.trimmed().isEmpty())
    {
        lines << QStringLiteral("source_type: %1").arg(image.sourceType);
    }
    if (image.pageIndex >= 0)
    {
        lines << QStringLiteral("page_index: %1").arg(image.pageIndex);
    }
    if (image.imagePixelWidth > 0 && image.imagePixelHeight > 0)
    {
        lines << QStringLiteral("image_pixel_size: %1 x %2").arg(image.imagePixelWidth).arg(image.imagePixelHeight);
    }
    if (!qFuzzyIsNull(image.pageRectWidth) && !qFuzzyIsNull(image.pageRectHeight))
    {
        lines << QStringLiteral("mapped_page_rect: x=%1, y=%2, width=%3, height=%4")
            .arg(image.pageRectX, 0, 'f', 3)
            .arg(image.pageRectY, 0, 'f', 3)
            .arg(image.pageRectWidth, 0, 'f', 3)
            .arg(image.pageRectHeight, 0, 'f', 3);
        lines << QStringLiteral("coordinate_mapping: use image metadata carefully if you need to reason about where an object appears on the PDF page.");
    }
    return lines.join(QLatin1Char('\n'));
}

QJsonArray buildOpenAiContentParts(const pdf::PDFAgentChatMessage& message)
{
    QJsonArray partsArray;

    for (const pdf::PDFAgentMessagePart& part : message.parts)
    {
        if (part.isText() && !part.text.isEmpty())
        {
            partsArray.append(QJsonObject{
                {"type", "text"},
                {"text", part.text}
            });
        }
        else if (part.isImage())
        {
            const QString dataUrl = ensureDataUrlFromImagePart(part.image);
            if (!dataUrl.isEmpty())
            {
                partsArray.append(QJsonObject{
                    {"type", "image_url"},
                    {"image_url", QJsonObject{{"url", dataUrl}}}
                });
                const QString metadataText = buildImageMetadataText(part.image);
                if (!metadataText.isEmpty())
                {
                    partsArray.append(QJsonObject{
                        {"type", "text"},
                        {"text", metadataText}
                    });
                }
            }
        }
    }

    if (partsArray.isEmpty() && !message.content.isEmpty())
    {
        partsArray.append(QJsonObject{
            {"type", "text"},
            {"text", message.content}
        });
    }

    return partsArray;
}

QJsonObject buildGeminiContentFromMessage(const pdf::PDFAgentChatMessage& message)
{
    QJsonObject content;
    QJsonArray parts;

    if (message.role == "assistant")
    {
        content["role"] = "model";
        if (!message.rawAssistantMessage.isEmpty() && message.rawAssistantMessage.contains("content"))
        {
            const QJsonObject rawContent = message.rawAssistantMessage.value("content").toObject();
            if (!rawContent.isEmpty())
            {
                content["role"] = rawContent.value("role").toString("model");
                content["parts"] = rawContent.value("parts").toArray();
                return content;
            }
        }

        if (!message.content.isEmpty())
        {
            parts.append(QJsonObject{{"text", message.content}});
        }
    }
    else if (message.role == "tool")
    {
        content["role"] = "user";
        parts.append(buildGeminiPartFromToolResult(message));
    }
    else
    {
        content["role"] = "user";
        for (const pdf::PDFAgentMessagePart& part : message.parts)
        {
            if (part.isText() && !part.text.isEmpty())
            {
                parts.append(QJsonObject{{"text", part.text}});
            }
            else if (part.isImage())
            {
                const QString dataUrl = ensureDataUrlFromImagePart(part.image);
                const QByteArray bytes = dataUrlToInlineBytes(dataUrl);
                if (!bytes.isEmpty())
                {
                    QString mimeType = part.image.mimeType;
                    if (mimeType.isEmpty())
                    {
                        mimeType = dataUrlMimeType(dataUrl);
                    }
                    if (mimeType.isEmpty())
                    {
                        mimeType = QStringLiteral("image/png");
                    }

                    parts.append(QJsonObject{
                        {"inlineData", QJsonObject{
                            {"mimeType", mimeType},
                            {"data", QString::fromLatin1(bytes.toBase64())}
                        }}
                    });
                    const QString metadataText = buildImageMetadataText(part.image);
                    if (!metadataText.isEmpty())
                    {
                        parts.append(QJsonObject{{"text", metadataText}});
                    }
                }
            }
        }

        if (parts.isEmpty() && !message.content.isEmpty())
        {
            parts.append(QJsonObject{{"text", message.content}});
        }
    }

    content["parts"] = parts;
    return content;
}

QJsonObject buildGeminiPayload(const QVector<pdf::PDFAgentChatMessage>& messages,
                               const QJsonArray& tools,
                               const pdf::PDFAgentLlmConfig& config)
{
    QJsonArray contents;
    QStringList systemInstructions;

    for (const pdf::PDFAgentChatMessage& message : messages)
    {
        if (message.role == "system")
        {
            if (!message.content.trimmed().isEmpty())
            {
                systemInstructions.append(message.content.trimmed());
            }
            continue;
        }

        contents.append(buildGeminiContentFromMessage(message));
    }

    QJsonObject payload;
    payload["contents"] = contents;

    if (!systemInstructions.isEmpty())
    {
        payload["system_instruction"] = QJsonObject{
            {"parts", QJsonArray{QJsonObject{{"text", systemInstructions.join("\n\n")}}}}
        };
    }

    QJsonObject generationConfig;
    if (config.temperature >= 0.0)
    {
        generationConfig["temperature"] = config.temperature;
    }

    applyGeminiThinkingConfig(generationConfig, config.model);
    if (!generationConfig.isEmpty())
    {
        payload["generationConfig"] = generationConfig;
    }

    if (!tools.isEmpty())
    {
        QJsonArray declarations;
        for (const QJsonValue& toolValue : tools)
        {
            const QJsonObject toolObject = toolValue.toObject();
            const QJsonObject functionObject = toolObject.value("function").toObject();
            if (!functionObject.isEmpty())
            {
                QJsonObject declaration;
                declaration["name"] = functionObject.value("name").toString();
                declaration["description"] = functionObject.value("description").toString();
                declaration["parameters"] = functionObject.value("parameters").toObject();
                declarations.append(declaration);
            }
        }

        if (!declarations.isEmpty())
        {
            payload["tools"] = QJsonArray{QJsonObject{{"functionDeclarations", declarations}}};
        }
    }

    return payload;
}

QString extractGeminiText(const QJsonArray& parts)
{
    QStringList texts;
    for (const QJsonValue& partValue : parts)
    {
        const QJsonObject partObject = partValue.toObject();
        const QString text = partObject.value("text").toString();
        if (!text.isEmpty())
        {
            texts.append(text);
        }
    }
    return texts.join(QString());
}

QVector<pdf::PdfAgentToolCall> extractGeminiToolCalls(const QJsonArray& parts)
{
    QVector<pdf::PdfAgentToolCall> toolCalls;
    int generatedId = 1;

    for (const QJsonValue& partValue : parts)
    {
        const QJsonObject partObject = partValue.toObject();
        const QJsonObject functionCall = partObject.value("functionCall").toObject();
        if (functionCall.isEmpty())
        {
            continue;
        }

        pdf::PdfAgentToolCall toolCall;
        toolCall.id = functionCall.value("id").toString();
        if (toolCall.id.isEmpty())
        {
            toolCall.id = QString("gemini_call_%1").arg(generatedId++);
        }
        toolCall.name = functionCall.value("name").toString();
        toolCall.arguments = functionCall.value("args").toObject();

        if (!toolCall.name.isEmpty())
        {
            toolCalls.append(toolCall);
        }
    }

    return toolCalls;
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

void PDFAgentLlmClient::cancelActiveRequest()
{
    m_cancelRequested = true;
    m_requestTimer.stop();

    if (m_activeReply)
    {
        m_activeReply->abort();
    }
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
    m_cancelRequested = false;

    const QNetworkRequest request = buildRequest(config);
    const QJsonObject payload = buildPayload(messages, config);
    const QByteArray requestBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_networkAccessManager->post(request, requestBody);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, &PDFAgentLlmClient::onReplyFinished);

    m_requestTimer.start(config.timeoutMs > 0 ? config.timeoutMs : 300000);
}

void PDFAgentLlmClient::sendChatWithTools(const QVector<PDFAgentChatMessage>& messages,
                                          const QJsonArray& tools,
                                          const PDFAgentLlmConfig& config)
{
    const QString validationError = validateChatRequest(messages, config);
    if (!validationError.isEmpty())
    {
        qDebug() << "VALIDATION FAILED:" << validationError;

        // Log diagnostic event
        PdfAgentDiagnosticsBuffer* diag = getAgentDiagnostics();
        if (diag)
        {
            PdfAgentDiagnosticEvent event;
            event.timestamp = QDateTime::currentDateTime();
            event.category = PDF_AGENT_DIAG_CATEGORY_NETWORK_ERROR;
            event.message = QString("Validation failed: %1").arg(validationError);
            event.requestId = diag->getCurrentRequestId();
            diag->append(event);
        }

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
    m_cancelRequested = false;

    const QNetworkRequest request = buildRequest(config);
    const QJsonObject payload = buildPayloadWithTools(messages, tools, config);
    const QByteArray requestBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    qDebug() << "Sending request to:" << config.endpoint;
    qDebug() << "Request body:" << requestBody.constData();

    // Log network request diagnostic event
    PdfAgentDiagnosticsBuffer* diag = getAgentDiagnostics();
    if (diag)
    {
        PdfAgentDiagnosticEvent event;
        event.timestamp = QDateTime::currentDateTime();
        event.category = PDF_AGENT_DIAG_CATEGORY_NETWORK_REQUEST;
        event.message = QString("POST %1").arg(config.endpoint);
        event.requestId = diag->getCurrentRequestId();

        // Include request info (without sensitive data)
        QJsonObject reqPayload = payload;
        event.payload = reqPayload;
        diag->append(event);
    }

    QNetworkReply* reply = m_networkAccessManager->post(request, requestBody);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, &PDFAgentLlmClient::onReplyFinished);

    m_requestTimer.start(config.timeoutMs > 0 ? config.timeoutMs : 300000);
}

QString PDFAgentLlmClient::validateChatRequest(const QVector<PDFAgentChatMessage>& messages,
                                               const PDFAgentLlmConfig& config)
{
    if (config.endpoint.trimmed().isEmpty())
    {
        return tr("LLM endpoint is empty.");
    }

    const bool geminiEndpointHasModel = detectProvider(config.endpoint) == LlmProvider::Gemini
                                        && (config.endpoint.contains(":generateContent", Qt::CaseInsensitive)
                                            || config.endpoint.contains(":streamGenerateContent", Qt::CaseInsensitive));

    if (config.model.trimmed().isEmpty() && !geminiEndpointHasModel)
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
        const bool hasToolCalls = message.role == "assistant" && !message.rawAssistantMessage.isEmpty();
        const bool isToolMessage = message.role == "tool";
        if (message.content.trimmed().isEmpty() && !message.hasParts() && !hasToolCalls && !isToolMessage)
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
    if (!choices.isEmpty())
    {
        const QJsonObject messageObject = choices.at(0).toObject().value("message").toObject();
        if (messageObject.isEmpty())
        {
            response.errorMessage = QObject::tr("LLM response did not contain a message object.");
            return response;
        }

        const QJsonArray toolCalls = messageObject.value("tool_calls").toArray();
        const QString content = messageObject.value("content").toString();
        if (!toolCalls.isEmpty())
        {
            response.success = true;
            response.assistantText = content;
            return response;
        }

        if (content.trimmed().isEmpty())
        {
            response.errorMessage = QObject::tr("Assistant response did not contain text content or tool calls.");
            return response;
        }

        response.success = true;
        response.assistantText = content;
        return response;
    }

    const QJsonArray candidates = response.rawJson.value("candidates").toArray();
    if (candidates.isEmpty())
    {
        response.errorMessage = QObject::tr("LLM response did not contain any choices or candidates.");
        return response;
    }

    const QJsonObject contentObject = candidates.at(0).toObject().value("content").toObject();
    const QJsonArray parts = contentObject.value("parts").toArray();
    const QVector<PdfAgentToolCall> toolCalls = extractGeminiToolCalls(parts);
    const QString content = extractGeminiText(parts);

    if (!toolCalls.isEmpty())
    {
        response.success = true;
        response.assistantText = content;
        return response;
    }

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
    if (!choices.isEmpty())
    {
        const QJsonObject messageObject = choices.at(0).toObject().value("message").toObject();
        if (messageObject.isEmpty())
        {
            turn.errorMessage = QObject::tr("LLM response did not contain a message object.");
            return turn;
        }

        const QJsonArray toolCallsArray = messageObject.value("tool_calls").toArray();
        const QString content = messageObject.value("content").toString();

        if (!toolCallsArray.isEmpty())
        {
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

                const QJsonValue argumentsValue = functionObj.value("arguments");
                if (argumentsValue.isString())
                {
                    const QString argsStr = argumentsValue.toString();
                    QJsonParseError parseErr;
                    const QJsonDocument argsDoc = QJsonDocument::fromJson(argsStr.toUtf8(), &parseErr);
                    if (parseErr.error == QJsonParseError::NoError && argsDoc.isObject())
                    {
                        toolCall.arguments = argsDoc.object();
                    }
                    else
                    {
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
            turn.assistantText = content;
            return turn;
        }

        if (!content.isEmpty())
        {
            turn.success = true;
            turn.assistantText = content;
            return turn;
        }
    }

    const QJsonArray candidates = turn.rawJson.value("candidates").toArray();
    if (candidates.isEmpty())
    {
        turn.errorMessage = QObject::tr("LLM response did not contain any choices or candidates.");
        return turn;
    }

    const QJsonObject contentObject = candidates.at(0).toObject().value("content").toObject();
    const QJsonArray parts = contentObject.value("parts").toArray();
    turn.toolCalls = extractGeminiToolCalls(parts);
    turn.assistantText = extractGeminiText(parts);

    if (!turn.toolCalls.isEmpty() || !turn.assistantText.isEmpty())
    {
        turn.success = true;
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
        if (response.rawJson.contains("choices"))
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
        else if (response.rawJson.contains("candidates"))
        {
            normalized.responseId = response.rawJson.value("responseId").toString();
            normalized.objectType = QStringLiteral("generateContentResponse");
            normalized.modelName = response.rawJson.value("modelVersion").toString();

            const QJsonObject usageObject = response.rawJson.value("usageMetadata").toObject();
            normalized.promptTokens = usageObject.value("promptTokenCount").toInt(-1);
            normalized.completionTokens = usageObject.value("candidatesTokenCount").toInt(-1);
            normalized.totalTokens = usageObject.value("totalTokenCount").toInt(-1);

            const QJsonArray candidates = response.rawJson.value("candidates").toArray();
            if (!candidates.isEmpty())
            {
                const QJsonObject candidateObject = candidates.at(0).toObject();
                const QJsonObject contentObject = candidateObject.value("content").toObject();
                normalized.assistantRole = contentObject.value("role").toString();
                normalized.assistantText = extractGeminiText(contentObject.value("parts").toArray());
                normalized.finishReason = candidateObject.value("finishReason").toString();
            }
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

QNetworkRequest PDFAgentLlmClient::buildRequest(const PDFAgentLlmConfig& config, bool streaming)
{
    QNetworkRequest request{ resolveEndpointUrl(config, streaming) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    if (!config.apiKey.trimmed().isEmpty())
    {
        if (detectProvider(config.endpoint) == LlmProvider::Gemini)
        {
            request.setRawHeader("x-goog-api-key", config.apiKey.toUtf8());
        }
        else
        {
            request.setRawHeader("Authorization", QByteArray("Bearer ").append(config.apiKey.toUtf8()));
        }
    }

    return request;
}

QJsonObject PDFAgentLlmClient::buildPayload(const QVector<PDFAgentChatMessage>& messages,
                                            const PDFAgentLlmConfig& config)
{
    if (detectProvider(config.endpoint) == LlmProvider::Gemini)
    {
        return buildGeminiPayload(messages, QJsonArray(), config);
    }

    QJsonArray jsonMessages;
    for (const PDFAgentChatMessage& message : messages)
    {
        QJsonObject jsonMessage;
        jsonMessage["role"] = message.role;
        const QJsonArray partsArray = buildOpenAiContentParts(message);
        if (!partsArray.isEmpty() && (message.hasImageParts() || message.parts.size() > 1))
        {
            jsonMessage["content"] = partsArray;
        }
        else if (!partsArray.isEmpty())
        {
            const QJsonObject firstPart = partsArray.first().toObject();
            jsonMessage["content"] = firstPart.value("text").toString(message.content);
        }
        else
        {
            jsonMessage["content"] = message.content;
        }
        jsonMessages.append(jsonMessage);
    }

    QJsonObject payload;
    payload["model"] = config.model;
    payload["messages"] = jsonMessages;
    payload["temperature"] = config.temperature;
    if (isDashScopeCompatibleEndpoint(config.endpoint))
    {
        payload["enable_thinking"] = true;
    }
    return payload;
}

QJsonObject PDFAgentLlmClient::buildPayloadWithTools(const QVector<PDFAgentChatMessage>& messages,
                                                      const QJsonArray& tools,
                                                      const PDFAgentLlmConfig& config)
{
    if (detectProvider(config.endpoint) == LlmProvider::Gemini)
    {
        return buildGeminiPayload(messages, tools, config);
    }

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
            const QJsonArray partsArray = buildOpenAiContentParts(message);
            if (!partsArray.isEmpty() && (message.hasImageParts() || message.parts.size() > 1))
            {
                jsonMessage["content"] = partsArray;
            }
            else if (!partsArray.isEmpty())
            {
                const QJsonObject firstPart = partsArray.first().toObject();
                jsonMessage["content"] = firstPart.value("text").toString(message.content);
            }
            else
            {
                jsonMessage["content"] = message.content;
            }

            // Handle assistant messages with tool_calls embedded in content as JSON
            if (message.role == "assistant" && !message.rawAssistantMessage.isEmpty())
            {
                const QJsonArray toolCalls = parseOpenAiToolCalls(message.rawAssistantMessage);
                if (!toolCalls.isEmpty())
                {
                    jsonMessage["tool_calls"] = toolCalls;
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
    if (isDashScopeCompatibleEndpoint(config.endpoint))
    {
        payload["enable_thinking"] = true;
    }

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
    m_cancelRequested = false;
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

    // Log diagnostic event
    PdfAgentDiagnosticsBuffer* diag = getAgentDiagnostics();
    if (diag)
    {
        PdfAgentDiagnosticEvent event;
        event.timestamp = QDateTime::currentDateTime();
        event.requestId = diag->getCurrentRequestId();

        if (m_requestTimedOut)
        {
            event.category = PDF_AGENT_DIAG_CATEGORY_NETWORK_ERROR;
            event.message = "Request timed out";
        }
        else if (m_activeReply->error() != QNetworkReply::NoError)
        {
            event.category = PDF_AGENT_DIAG_CATEGORY_NETWORK_ERROR;
            event.message = QString("Network error: %1").arg(m_activeReply->errorString());
        }
        else
        {
            event.category = PDF_AGENT_DIAG_CATEGORY_NETWORK_RESPONSE;
            event.message = QString("HTTP %1").arg(httpStatusCode);
        }

        // Include response info
        QJsonObject respPayload;
        respPayload["httpStatus"] = httpStatusCode;
        respPayload["bodySize"] = body.size();
        event.payload = respPayload;

        diag->append(event);
    }

    PDFAgentLlmResponse response;
    if (m_cancelRequested)
    {
        response.httpStatusCode = httpStatusCode;
        response.rawResponseBody = body;
        response.rawResponseText = QString::fromUtf8(body);
        response.errorMessage = tr("The AI request was canceled.");
    }
    else if (m_requestTimedOut)
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

// Streaming implementation
void PDFAgentLlmClient::sendChatStreaming(const QVector<PDFAgentChatMessage>& messages,
                                          const PDFAgentLlmConfig& config)
{
    const QString validationError = validateChatRequest(messages, config);
    if (!validationError.isEmpty())
    {
        PDFAgentLlmResponse response;
        response.errorMessage = validationError;
        finishStreamingWithResponse(response);
        return;
    }

    if (m_activeReply)
    {
        PDFAgentLlmResponse response;
        response.errorMessage = tr("Another AI request is already in progress.");
        finishStreamingWithResponse(response);
        return;
    }

    m_requestTimedOut = false;
    m_cancelRequested = false;
    m_streamingBuffer.clear();

    const QNetworkRequest request = buildRequest(config, true);
    const QJsonObject payload = buildPayload(messages, config);
    const QByteArray requestBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    startStreamingRequest(request, requestBody);
}

void PDFAgentLlmClient::sendChatWithToolsStreaming(const QVector<PDFAgentChatMessage>& messages,
                                                   const QJsonArray& tools,
                                                   const PDFAgentLlmConfig& config)
{
    const QString validationError = validateChatRequest(messages, config);
    if (!validationError.isEmpty())
    {
        PDFAgentLlmResponse response;
        response.errorMessage = validationError;
        finishStreamingWithResponse(response);
        return;
    }

    if (m_activeReply)
    {
        PDFAgentLlmResponse response;
        response.errorMessage = tr("Another AI request is already in progress.");
        finishStreamingWithResponse(response);
        return;
    }

    m_requestTimedOut = false;
    m_cancelRequested = false;
    m_streamingBuffer.clear();

    const QNetworkRequest request = buildRequest(config, true);
    const QJsonObject payload = buildPayloadWithTools(messages, tools, config);
    const QByteArray requestBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    startStreamingRequest(request, requestBody);
}

void PDFAgentLlmClient::startStreamingRequest(const QNetworkRequest& request, const QByteArray& body)
{
    QNetworkReply* reply = m_networkAccessManager->post(request, body);
    m_activeReply = reply;

    // Enable specific signals for streaming
    connect(reply, &QNetworkReply::finished, this, &PDFAgentLlmClient::onReplyFinished);
    connect(reply, &QNetworkReply::readyRead, this, &PDFAgentLlmClient::onReadyRead);

    m_requestTimer.start(300000);
}

void PDFAgentLlmClient::onReadyRead()
{
    if (!m_activeReply)
    {
        return;
    }

    // Read available data
    QByteArray data = m_activeReply->readAll();
    if (data.isEmpty())
    {
        return;
    }

    // Append to buffer for SSE parsing
    m_streamingBuffer.append(QString::fromUtf8(data));

    // Parse SSE (Server-Sent Events) format
    // Format: "data: {...}\n\n" or "data: {...}\r\n\r\n"
    QStringList lines = m_streamingBuffer.split("\n", Qt::SkipEmptyParts);
    m_streamingBuffer.clear();

    for (const QString& line : lines)
    {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.startsWith("data: "))
        {
            QString jsonStr = trimmedLine.mid(6).trimmed();

            // Check for [DONE] signal
            if (jsonStr == "[DONE]")
            {
                continue;
            }

            // Try to parse the JSON
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);

            if (parseError.error == QJsonParseError::NoError && doc.isObject())
            {
                QJsonObject obj = doc.object();

                if (obj.contains("choices"))
                {
                    QJsonObject delta = obj.value("choices").toArray().first().toObject().value("delta").toObject();
                    QString content = delta.value("content").toString();

                    if (!content.isEmpty())
                    {
                        Q_EMIT streamingChunkReady(content);
                    }

                    QJsonArray toolCalls = delta.value("tool_calls").toArray();
                    if (!toolCalls.isEmpty())
                    {
                        Q_EMIT streamingChunkReady("__tool_calls__" + jsonStr);
                    }
                }
                else if (obj.contains("candidates"))
                {
                    const QJsonObject contentObject = obj.value("candidates").toArray().first().toObject().value("content").toObject();
                    const QJsonArray parts = contentObject.value("parts").toArray();
                    const QString content = extractGeminiText(parts);
                    if (!content.isEmpty())
                    {
                        Q_EMIT streamingChunkReady(content);
                    }

                    if (!extractGeminiToolCalls(parts).isEmpty())
                    {
                        Q_EMIT streamingChunkReady("__tool_calls__" + jsonStr);
                    }
                }
            }
        }
    }
}

void PDFAgentLlmClient::finishStreamingWithResponse(const PDFAgentLlmResponse& response)
{
    if (m_activeReply)
    {
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    m_requestTimer.stop();
    m_requestTimedOut = false;
    m_cancelRequested = false;

    // Build a synthetic response from streaming chunks
    PDFAgentLlmResponse fullResponse = response;
    Q_EMIT streamingFinished(fullResponse);
    Q_EMIT chatFinished(fullResponse);
}

}   // namespace pdf
