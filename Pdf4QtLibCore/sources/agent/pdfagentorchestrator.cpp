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

#include "pdfagentorchestrator.h"

#include <QJsonDocument>

namespace pdf
{

PDFAgentOrchestrator::PDFAgentOrchestrator(QObject* parent) :
    QObject(parent),
    m_llmClient(new PDFAgentLlmClient(this)),
    m_toolRoundCount(0),
    m_isInToolLoop(false)
{
    connect(m_llmClient, &PDFAgentLlmClient::chatFinished, this, &PDFAgentOrchestrator::onChatFinished);
}

void PDFAgentOrchestrator::setConfig(const PDFAgentLlmConfig& config)
{
    m_config = config;
}

void PDFAgentOrchestrator::processSingleTurn(const QString& userText)
{
    const QVector<PDFAgentChatMessage> messages = buildSingleTurnMessages(userText);
    if (messages.isEmpty())
    {
        PDFAgentLlmResponse response;
        response.errorMessage = tr("User message is empty.");
        Q_EMIT responseReady(response);
        return;
    }

    m_llmClient->sendChat(messages, m_config);
}

QVector<PDFAgentChatMessage> PDFAgentOrchestrator::buildSingleTurnMessages(const QString& userText) const
{
    QVector<PDFAgentChatMessage> messages;
    const QString trimmedUserText = userText.trimmed();
    if (trimmedUserText.isEmpty())
    {
        return messages;
    }

    if (!m_config.systemPrompt.trimmed().isEmpty())
    {
        messages.push_back({ QStringLiteral("system"), m_config.systemPrompt.trimmed() });
    }

    messages.push_back({ QStringLiteral("user"), trimmedUserText });
    return messages;
}

PdfAgentToolExecutionResult PDFAgentOrchestrator::processMockToolRequest(const QString& mockJsonText,
                                                                         const PDFAgentExecutionContext& context) const
{
    PdfAgentToolExecutionResult result;

    // Parse mock tool callsdan
    PdfAgentMockToolParseResult parseResult = m_mockParser.parse(mockJsonText);

    if (!parseResult.success)
    {
        result.success = false;
        result.errorMessage = parseResult.errorMessage;
        result.summaryText = QString("Parse error: %1").arg(parseResult.errorMessage);
        return result;
    }

    // Execute each tool call
    int successCount = 0;
    for (const PdfAgentMockToolCall& toolCall : parseResult.toolCalls)
    {
        QJsonObject commandResult = m_functionRegistry.executeCommand(toolCall.name,
                                                                      toolCall.arguments,
                                                                      context);
        result.toolResults.append(commandResult);

        if (commandResult.value("ok").toBool(false))
        {
            ++successCount;
        }
    }

    // Build summary
    int totalCount = parseResult.toolCalls.size();
    if (successCount == totalCount)
    {
        result.success = true;
        result.summaryText = QString("Executed %1 tool call(s) successfully.").arg(totalCount);
    }
    else if (successCount > 0)
    {
        result.success = true;
        result.summaryText = QString("Executed %1/%2 tool call(s) successfully.").arg(successCount).arg(totalCount);
    }
    else
    {
        result.success = false;
        result.summaryText = QString("Failed to execute tool calls.");
    }

    return result;
}

void PDFAgentOrchestrator::setExecutionContext(const PDFAgentExecutionContext& context)
{
    m_executionContext = context;
}

void PDFAgentOrchestrator::clearConversation()
{
    m_conversation.clear();
    m_toolRoundCount = 0;
    m_isInToolLoop = false;
}

void PDFAgentOrchestrator::processWithToolCalls(const QString& userText, const PDFAgentExecutionContext& context)
{
    m_executionContext = context;
    clearConversation();
    m_isInToolLoop = true;

    // Build initial messages
    if (!m_config.systemPrompt.trimmed().isEmpty())
    {
        m_conversation.appendSystemMessage(m_config.systemPrompt.trimmed());
    }

    const QString trimmedUserText = userText.trimmed();
    if (trimmedUserText.isEmpty())
    {
        finishWithError(tr("User message is empty."));
        return;
    }

    m_conversation.appendUserMessage(trimmedUserText);

    // Get tools schema from function registry
    const QJsonArray tools = m_functionRegistry.getToolsSchema();
    qDebug() << "Sending request with tools count:" << tools.size();

    // Send request with tools
    const QVector<PDFAgentChatMessage> messages = buildChatMessagesFromConversation();
    qDebug() << "Message count:" << messages.size() << "Config endpoint:" << m_config.endpoint;

    m_llmClient->sendChatWithTools(messages, tools, m_config);
}

void PDFAgentOrchestrator::onChatFinished(const PDFAgentLlmResponse& response)
{
    // Save last response for debugging
    m_lastResponse = response;

    // If not in tool loop, just emit the response directly (old behavior)
    if (!m_isInToolLoop)
    {
        Q_EMIT responseReady(response);
        return;
    }

    // Parse as assistant turn
    const QByteArray body = response.success ? response.rawResponseBody : QByteArray();

    // Debug: log raw response for troubleshooting
    qDebug() << "LLM Response - success:" << response.success << "httpStatus:" << response.httpStatusCode << "body size:" << body.size();
    if (!body.isEmpty())
    {
        qDebug() << "LLM Raw Response:" << QString::fromUtf8(body).left(1000);
    }
    else
    {
        qDebug() << "LLM Raw Response is EMPTY!";
    }

    const PDFAgentAssistantTurn turn = PDFAgentLlmClient::parseAssistantTurn(body, response.httpStatusCode);

    if (!turn.success)
    {
        // Include more debug info in error
        QString debugInfo = QString::fromUtf8(body).left(200);
        finishWithError(tr("Failed to get response from AI: %1\nRaw: %2").arg(turn.errorMessage).arg(debugInfo));
        return;
    }

    handleAssistantTurn(turn, &response);
}

void PDFAgentOrchestrator::handleAssistantTurn(const PDFAgentAssistantTurn& turn, const PDFAgentLlmResponse* originalResponse)
{
    // Check if there are tool calls
    if (!turn.toolCalls.isEmpty())
    {
        // Check round limit
        if (m_toolRoundCount >= m_maxToolRounds)
        {
            finishWithError(tr("Tool call round limit exceeded (%1 rounds).").arg(m_maxToolRounds));
            return;
        }

        // IMPORTANT: First, append the assistant message with tool_calls to conversation
        // This is required by OpenAI API - you must include the assistant's tool_calls message
        if (originalResponse && !originalResponse->rawJson.isEmpty())
        {
            const QJsonObject messageObj = originalResponse->rawJson.value("choices").toArray().first().toObject().value("message").toObject();
            if (!messageObj.isEmpty())
            {
                m_conversation.appendAssistantMessage(messageObj.value("content").toString(), turn.toolCalls, messageObj);
            }
        }

        // Execute tool calls
        ++m_toolRoundCount;
        executeToolCalls(turn.toolCalls);
    }
    else
    {
        // Final response - no more tool calls
        Q_EMIT finalResponseReady(turn.assistantText);

        // Use original response if available, otherwise create new one
        PDFAgentLlmResponse response;
        if (originalResponse)
        {
            response = *originalResponse;
        }
        else
        {
            response.success = true;
        }
        response.assistantText = turn.assistantText;
        Q_EMIT responseReady(response);
    }
}

void PDFAgentOrchestrator::executeToolCalls(const QVector<PdfAgentToolCall>& toolCalls)
{
    // Execute each tool call
    for (const PdfAgentToolCall& toolCall : toolCalls)
    {
        Q_EMIT toolCallStarted(toolCall.name);

        // Execute the command
        QJsonObject commandResult = m_functionRegistry.executeCommand(toolCall.name,
                                                                       toolCall.arguments,
                                                                       m_executionContext);

        const bool success = commandResult.value("ok").toBool(false);
        Q_EMIT toolCallFinished(toolCall.name, success);

        // Add tool result to conversation
        // Convert result to compact JSON string
        const QString resultStr = QString::fromUtf8(QJsonDocument(commandResult).toJson(QJsonDocument::Compact));
        m_conversation.appendToolResultMessage(toolCall.id, toolCall.name, resultStr);
    }

    // Continue with follow-up request
    sendFollowUpRequest();
}

void PDFAgentOrchestrator::sendFollowUpRequest()
{
    // Debug: log conversation messages
    const auto& convMessages = m_conversation.getMessages();
    qDebug() << "sendFollowUpRequest - conversation has" << convMessages.size() << "messages";
    for (int i = 0; i < convMessages.size(); ++i)
    {
        const auto& msg = convMessages[i];
        qDebug() << "  Message" << i << "role:" << msg.role << "content len:" << msg.content.length()
                 << "toolCallId:" << msg.toolCallId.left(50) << "...";
    }

    // Get updated tools schema
    const QJsonArray tools = m_functionRegistry.getToolsSchema();

    // Send follow-up with updated conversation
    const QVector<PDFAgentChatMessage> messages = buildChatMessagesFromConversation();
    qDebug() << "sendFollowUpRequest - built" << messages.size() << "chat messages";

    m_llmClient->sendChatWithTools(messages, tools, m_config);
}

void PDFAgentOrchestrator::finishWithError(const QString& error)
{
    PDFAgentLlmResponse response;
    response.success = false;
    response.errorMessage = error;
    Q_EMIT responseReady(response);
}

QVector<PDFAgentChatMessage> PDFAgentOrchestrator::buildChatMessagesFromConversation() const
{
    QVector<PDFAgentChatMessage> result;
    const auto& messages = m_conversation.getMessages();
    result.reserve(messages.size());

    for (const PdfAgentConversationMessage& msg : messages)
    {
        // If there's a raw assistant message with tool_calls, serialize it properly
        if (msg.role == "assistant" && !msg.rawAssistantMessage.isEmpty())
        {
            QJsonArray toolCalls = msg.rawAssistantMessage.value("tool_calls").toArray();
            if (!toolCalls.isEmpty())
            {
                // Serialize tool_calls to JSON string and put in toolCallId field
                // This is a workaround since we don't have a dedicated field
                PDFAgentChatMessage chatMsg;
                chatMsg.role = "assistant";
                chatMsg.content = msg.rawAssistantMessage.value("content").toString();
                chatMsg.toolCallId = QString::fromUtf8(QJsonDocument(toolCalls).toJson(QJsonDocument::Compact));
                result.append(chatMsg);
                continue;
            }
        }

        PDFAgentChatMessage chatMsg;
        chatMsg.role = msg.role;
        chatMsg.content = msg.content;

        // Include tool_call_id for tool messages
        if (msg.role == "tool")
        {
            chatMsg.toolCallId = msg.toolCallId;
        }

        result.append(chatMsg);
    }

    return result;
}

}   // namespace pdf
