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
#include <QDebug>

namespace pdf
{

// Helper to mask API key in logs
static QString maskApiKey(const QString& key)
{
    if (key.isEmpty()) return QString();
    if (key.length() <= 8) return QString("****");
    return key.left(4) + "****" + key.right(4);
}

PDFAgentOrchestrator::PDFAgentOrchestrator(QObject* parent) :
    QObject(parent),
    m_llmClient(new PDFAgentLlmClient(this)),
    m_toolRoundCount(0),
    m_isInToolLoop(false),
    m_streamingEnabled(false),
    m_waitingForConfirmation(false)
{
    connect(m_llmClient, &PDFAgentLlmClient::chatFinished, this, &PDFAgentOrchestrator::onChatFinished);
    connect(m_llmClient, &PDFAgentLlmClient::streamingChunkReady, this, &PDFAgentOrchestrator::onStreamingChunkReady);
    connect(m_llmClient, &PDFAgentLlmClient::streamingFinished, this, &PDFAgentOrchestrator::onStreamingFinished);
    connect(this, &PDFAgentOrchestrator::confirmationReceived, this, &PDFAgentOrchestrator::onConfirmationReceived);
}

void PDFAgentOrchestrator::setStreamingEnabled(bool enabled)
{
    m_streamingEnabled = enabled;
    m_llmClient->setStreamingEnabled(enabled);
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

void PDFAgentOrchestrator::submitConfirmationResult(const PDFAgentConfirmationResult& result)
{
    Q_EMIT confirmationReceived(result);
}

void PDFAgentOrchestrator::onConfirmationReceived(const PDFAgentConfirmationResult& result)
{
    if (!m_waitingForConfirmation)
    {
        return;
    }

    // Log confirmation result
    const QString toolName = m_pendingToolCall.name;
    const QString category = result.approved ? PDF_AGENT_DIAG_CATEGORY_CONFIRMATION_APPROVED : PDF_AGENT_DIAG_CATEGORY_CONFIRMATION_REJECTED;
    logDiagnosticEvent(category,
                      QString("Confirmation %1: %2").arg(result.approved ? "approved" : "rejected", toolName),
                      QJsonObject({{"reason", result.reason}}));

    if (result.approved)
    {
        // Execute the pending tool call
        executeSingleToolCall(m_pendingToolCall);
    }
    else
    {
        // User rejected - add error result to conversation
        QJsonObject errorResult;
        errorResult["ok"] = false;
        errorResult["error"] = QString("User rejected the operation: %1").arg(result.reason);

        const QString resultStr = QString::fromUtf8(QJsonDocument(errorResult).toJson(QJsonDocument::Compact));
        m_conversation.appendToolResultMessage(m_pendingToolCall.id, m_pendingToolCall.name, resultStr);

        Q_EMIT toolCallFinished(m_pendingToolCall.name, false);
    }

    // Reset confirmation state
    m_waitingForConfirmation = false;
    m_pendingToolCall = PdfAgentToolCall();
    m_pendingCommandResult = QJsonObject();

    // Continue with next round
    sendFollowUpRequest();
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
    // Log diagnostic event
    logDiagnosticEvent(PDF_AGENT_DIAG_CATEGORY_TOOL_REQUESTED,
                       QString("Executing %1 tool call(s)").arg(toolCalls.size()),
                       QJsonObject());

    // Execute each tool call
    for (const PdfAgentToolCall& toolCall : toolCalls)
    {
        Q_EMIT toolCallStarted(toolCall.name);

        // Log tool requested
        logDiagnosticEvent(PDF_AGENT_DIAG_CATEGORY_TOOL_REQUESTED,
                           QString("Tool: %1").arg(toolCall.name),
                           toolCall.arguments);

        // Check if command requires confirmation
        if (m_functionRegistry.requiresConfirmation(toolCall.name))
        {
            // Log confirmation request
            logDiagnosticEvent(PDF_AGENT_DIAG_CATEGORY_CONFIRMATION_REQUESTED,
                              QString("Confirmation needed for: %1").arg(toolCall.name),
                              toolCall.arguments);

            // Execute the command first to get the parameters
            QJsonObject commandResult = m_functionRegistry.executeCommand(toolCall.name,
                                                                          toolCall.arguments,
                                                                          m_executionContext);

            // Request confirmation from user
            requestConfirmation(toolCall, commandResult);
            return; // Wait for confirmation before continuing
        }

        // Execute the command directly
        executeSingleToolCall(toolCall);
    }

    // Continue with follow-up request
    sendFollowUpRequest();
}

void PDFAgentOrchestrator::executeSingleToolCall(const PdfAgentToolCall& toolCall)
{
    // Execute the command
    QJsonObject commandResult = m_functionRegistry.executeCommand(toolCall.name,
                                                                  toolCall.arguments,
                                                                  m_executionContext);

    const bool success = commandResult.value("ok").toBool(false);
    Q_EMIT toolCallFinished(toolCall.name, success);

    // Log tool result
    const QString category = success ? PDF_AGENT_DIAG_CATEGORY_TOOL_RESULT : PDF_AGENT_DIAG_CATEGORY_TOOL_ERROR;
    logToolTrace(toolCall.name, toolCall.arguments, commandResult, success);

    // Add tool result to conversation
    // Convert result to compact JSON string
    const QString resultStr = QString::fromUtf8(QJsonDocument(commandResult).toJson(QJsonDocument::Compact));
    m_conversation.appendToolResultMessage(toolCall.id, toolCall.name, resultStr);
}

void PDFAgentOrchestrator::requestConfirmation(const PdfAgentToolCall& toolCall, const QJsonObject& commandResult)
{
    // Get command descriptor for additional info
    const PdfAgentCommandDescriptor* descriptor = m_functionRegistry.getCommandDescriptor(toolCall.name);

    PDFAgentConfirmationRequest request;
    request.commandName = toolCall.name;
    request.targetFile = m_executionContext.originalFileName;
    request.arguments = toolCall.arguments;
    request.riskLevel = static_cast<int>(descriptor ? descriptor->riskLevel : PdfAgentCommandRiskLevel::LowRiskWrite);

    // Build user-friendly summary
    QString summary;
    if (toolCall.name == "create_highlight")
    {
        int pageIndex = toolCall.arguments.value("page_index").toInt(-1) + 1;
        int quadCount = toolCall.arguments.value("quadrilaterals").toArray().size();
        summary = QString("Create %1 highlight(s) on page %2").arg(quadCount).arg(pageIndex);
    }
    else if (toolCall.name == "add_text_comment")
    {
        int pageIndex = toolCall.arguments.value("page").toInt(-1) + 1;
        QString text = toolCall.arguments.value("text").toString();
        if (text.length() > 50) text = text.left(50) + "...";
        summary = QString("Add comment on page %1: \"%2\"").arg(pageIndex).arg(text);
    }
    else
    {
        summary = QString("Execute %1").arg(toolCall.name);
    }

    request.summary = summary;

    // Set title based on risk level
    switch (static_cast<PdfAgentCommandRiskLevel>(request.riskLevel))
    {
        case PdfAgentCommandRiskLevel::ReadOnly:
            request.title = "Read-Only Operation";
            break;
        case PdfAgentCommandRiskLevel::LowRiskWrite:
            request.title = "Low Risk Operation";
            break;
        case PdfAgentCommandRiskLevel::MediumRiskWrite:
            request.title = "Medium Risk Operation";
            break;
        case PdfAgentCommandRiskLevel::HighRiskWrite:
            request.title = "High Risk Operation";
            break;
    }

    // Store pending tool call for execution after confirmation
    m_pendingToolCall = toolCall;
    m_pendingCommandResult = commandResult;
    m_waitingForConfirmation = true;

    // Emit confirmation request signal
    Q_EMIT confirmationRequested(request);
}

void PDFAgentOrchestrator::processConfirmedToolCall()
{
    if (!m_waitingForConfirmation)
    {
        return;
    }

    // Execute the pending tool call
    executeSingleToolCall(m_pendingToolCall);

    // Reset confirmation state
    m_waitingForConfirmation = false;
    m_pendingToolCall = PdfAgentToolCall();
    m_pendingCommandResult = QJsonObject();

    // Continue with next round
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

void PDFAgentOrchestrator::logDiagnosticEvent(const QString& category, const QString& message, const QJsonObject& payload)
{
    PdfAgentDiagnosticsBuffer* diag = getAgentDiagnostics();
    if (!diag)
    {
        return;
    }

    PdfAgentDiagnosticEvent event;
    event.timestamp = QDateTime::currentDateTime();
    event.category = category;
    event.message = message;
    event.payload = payload;
    event.requestId = diag->getCurrentRequestId();

    diag->append(event);
}

void PDFAgentOrchestrator::logToolTrace(const QString& toolName, const QJsonObject& args, const QJsonObject& result, bool success)
{
    PdfAgentDiagnosticsBuffer* diag = getAgentDiagnostics();
    if (!diag)
    {
        return;
    }

    PdfAgentToolTraceItem item;
    item.toolName = toolName;
    item.arguments = args;
    item.result = result;
    item.success = success;
    item.timestamp = QDateTime::currentDateTime();
    item.requestId = diag->getCurrentRequestId();

    diag->appendToolTrace(item);

    // Also log as diagnostic event
    const QString category = success ? PDF_AGENT_DIAG_CATEGORY_TOOL_RESULT : PDF_AGENT_DIAG_CATEGORY_TOOL_ERROR;
    QString message = QString("%1: %2").arg(toolName, success ? "OK" : "FAILED");

    // Include brief result info
    QJsonObject briefPayload = result;
    // Truncate long messages in payload
    if (briefPayload.contains("message"))
    {
        QString msg = briefPayload.value("message").toString();
        if (msg.length() > 200)
        {
            briefPayload["message"] = msg.left(200) + "...";
        }
    }

    logDiagnosticEvent(category, message, briefPayload);
}

void PDFAgentOrchestrator::onStreamingChunkReady(const QString& chunk)
{
    if (chunk.startsWith("__tool_calls__"))
    {
        // This is a tool call chunk
        QString jsonStr = chunk.mid(14); // Remove prefix
        Q_EMIT streamingToolCallReady(jsonStr);
    }
    else
    {
        // Regular text chunk
        Q_EMIT streamingTextReady(chunk);
    }
}

void PDFAgentOrchestrator::onStreamingFinished(const PDFAgentLlmResponse& response)
{
    m_lastResponse = response;

    if (!m_isInToolLoop)
    {
        Q_EMIT responseReady(response);
        return;
    }

    // Parse as assistant turn
    const QByteArray body = response.success ? response.rawResponseBody : QByteArray();
    const PDFAgentAssistantTurn turn = PDFAgentLlmClient::parseAssistantTurn(body, response.httpStatusCode);

    if (!turn.success)
    {
        finishWithError(tr("Failed to get response from AI: %1").arg(turn.errorMessage));
        return;
    }

    handleAssistantTurn(turn, &response);
}

}   // namespace pdf
