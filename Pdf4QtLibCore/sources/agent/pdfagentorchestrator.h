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

#ifndef PDFAGENTORCHESTRATOR_H
#define PDFAGENTORCHESTRATOR_H

#include "agent/pdfagentllmclient.h"
#include "agent/pdfagentfunctionregistry.h"
#include "agent/pdfagentmocktoolparser.h"
#include "agent/pdfagentexecutioncontext.h"
#include "agent/pdfagenttypes.h"
#include "agent/pdfagentdiagnostics.h"

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

namespace pdf
{

struct PDF4QTLIBCORESHARED_EXPORT PdfAgentToolExecutionResult
{
    bool success = false;
    QString summaryText;
    QJsonArray toolResults;
    QString errorMessage;
};

class PDF4QTLIBCORESHARED_EXPORT PDFAgentOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit PDFAgentOrchestrator(QObject* parent = nullptr);

    void setConfig(const PDFAgentLlmConfig& config);
    const PDFAgentLlmConfig& getConfig() const { return m_config; }

    void processSingleTurn(const QString& userText);
    QVector<PDFAgentChatMessage> buildSingleTurnMessages(const QString& userText) const;
    QVector<PDFAgentChatMessage> buildSingleTurnMessages(const PDFAgentChatMessage& userMessage) const;

    // Multi-turn tool calling orchestration
    void processWithToolCalls(const QString& userText, const PDFAgentExecutionContext& context);
    void processWithToolCalls(const PDFAgentChatMessage& userMessage, const PDFAgentExecutionContext& context);
    void setExecutionContext(const PDFAgentExecutionContext& context);
    void clearConversation();
    [[nodiscard]] const PdfAgentConversation& getConversation() const { return m_conversation; }
    void restoreConversation(const PdfAgentConversation& conversation, const QJsonArray& todoItems = QJsonArray());
    [[nodiscard]] QString getTodoSummaryText() const { return m_todoManager.render(); }
    PDFAgentTodoManager* getTodoManager() { return &m_todoManager; }
    const PDFAgentTodoManager* getTodoManager() const { return &m_todoManager; }

    // Mock tool call support
    PdfFunctionRegistry* getFunctionRegistry() { return &m_functionRegistry; }
    const PdfFunctionRegistry* getFunctionRegistry() const { return &m_functionRegistry; }

    PdfAgentToolExecutionResult processMockToolRequest(const QString& mockJsonText,
                                                       const PDFAgentExecutionContext& context);

    // Submit confirmation result from UI
    Q_INVOKABLE void submitConfirmationResult(const PDFAgentConfirmationResult& result);
    Q_INVOKABLE void cancelCurrentOperation();

    // Streaming support
    void setStreamingEnabled(bool enabled);
    bool isStreamingEnabled() const { return m_streamingEnabled; }

signals:
    void responseReady(const PDFAgentLlmResponse& response);
    void toolCallStarted(const QString& toolName);
    void toolCallFinished(const QString& toolName, bool success);
    void finalResponseReady(const QString& responseText);

    // Streaming signals
    void streamingTextReady(const QString& text);
    void streamingToolCallReady(const QString& toolCallJson);

    // Confirmation signals for mutating commands
    void confirmationRequested(const PDFAgentConfirmationRequest& request);
    void confirmationReceived(const PDFAgentConfirmationResult& result);
    void todoStateChanged(const QString& renderedText, bool hasItems);
    void conversationChanged();

private slots:
    void onChatFinished(const PDFAgentLlmResponse& response);
    void onStreamingChunkReady(const QString& chunk);
    void onStreamingFinished(const PDFAgentLlmResponse& response);
    void onConfirmationReceived(const PDFAgentConfirmationResult& result);

private:
    void handleAssistantTurn(const PDFAgentAssistantTurn& turn, const PDFAgentLlmResponse* originalResponse = nullptr);
    void executeToolCalls(const QVector<PdfAgentToolCall>& toolCalls);
    void executeSingleToolCall(const PdfAgentToolCall& toolCall);
    void sendFollowUpRequest();

    // Confirmation handling
    void requestConfirmation(const PdfAgentToolCall& toolCall);
    void processConfirmedToolCall();
    void finishWithError(const QString& error);
    QVector<PDFAgentChatMessage> buildChatMessagesFromConversation() const;

    // Diagnostics
    void logDiagnosticEvent(const QString& category, const QString& message, const QJsonObject& payload = QJsonObject());
    void logToolTrace(const QString& toolName, const QJsonObject& args, const QJsonObject& result, bool success);

    PDFAgentLlmClient* m_llmClient;
    PDFAgentLlmConfig m_config;
    PdfFunctionRegistry m_functionRegistry;
    PDFAgentMockToolParser m_mockParser;

    // Orchestration state
    PDFAgentExecutionContext m_executionContext;
    PdfAgentConversation m_conversation;
    int m_toolRoundCount = 0;
    int m_maxToolRounds = 30;
    bool m_isInToolLoop = false;
    bool m_streamingEnabled = false;
    bool m_cancellationRequested = false;
    PDFAgentLlmResponse m_lastResponse;
    PDFAgentTodoManager m_todoManager;
    int m_roundsSinceTodoUpdate = 0;

    // Confirmation state
    bool m_waitingForConfirmation = false;
    PdfAgentToolCall m_pendingToolCall;
};

}   // namespace pdf

#endif // PDFAGENTORCHESTRATOR_H
