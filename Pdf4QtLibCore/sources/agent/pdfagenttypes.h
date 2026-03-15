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

#ifndef PDFAGENTTYPES_H
#define PDFAGENTTYPES_H

#include "pdfglobal.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QMetaType>
#include <QVector>

namespace pdf
{

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentChatMessage
{
    QString role;
    QString content;
    QString toolCallId;  // For tool role messages
};

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentLlmConfig
{
    QString endpoint;
    QString model;
    QString apiKey;
    QString systemPrompt;
    int timeoutMs = 30000;
    double temperature = 0.2;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentLlmResponse
{
    bool success = false;
    QString assistantText;
    QString errorMessage;
    int httpStatusCode = -1;
    QByteArray rawResponseBody;
    QString rawResponseText;
    QJsonObject rawJson;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentNormalizedResponse
{
    bool ok = false;
    QString provider;
    QString endpoint;
    QString responseId;
    QString objectType;
    qint64 created = -1;
    QString modelName;
    QString assistantRole;
    QString assistantText;
    QString finishReason;
    int httpStatusCode = -1;
    int promptTokens = -1;
    int completionTokens = -1;
    int totalTokens = -1;
    QJsonObject promptTokenDetails;
    int promptCacheHitTokens = -1;
    int promptCacheMissTokens = -1;
    QString systemFingerprint;
    QString errorMessage;
    QByteArray rawResponseBody;
    QString rawResponseText;
    QJsonObject rawJson;

    QJsonObject toJsonObject() const;
    QString toPrettyJson() const;
};

// Tool call from model
struct PDF4QTLIBCORESHARED_EXPORT PdfAgentToolCall
{
    QString id;
    QString name;
    QJsonObject arguments;
};

// Assistant turn containing either text or tool calls
struct PDF4QTLIBCORESHARED_EXPORT PDFAgentAssistantTurn
{
    bool success = false;
    QString assistantText;
    QVector<PdfAgentToolCall> toolCalls;
    QString errorMessage;
    QJsonObject rawJson;
};

// Conversation message with role support
struct PDF4QTLIBCORESHARED_EXPORT PdfAgentConversationMessage
{
    QString role;          // "system", "user", "assistant", "tool"
    QString content;
    QString toolCallId;     // For tool role
    QString toolName;       // For tool role
    QJsonObject rawAssistantMessage;  // Original assistant message with tool_calls
};

// Conversation manager
class PdfAgentConversation
{
public:
    void clear();
    void appendSystemMessage(const QString& content);
    void appendUserMessage(const QString& content);
    void appendAssistantMessage(const QString& content, const QVector<PdfAgentToolCall>& toolCalls = {}, const QJsonObject& rawMessage = QJsonObject());
    void appendToolResultMessage(const QString& toolCallId, const QString& toolName, const QString& content);

    [[nodiscard]] const QVector<PdfAgentConversationMessage>& getMessages() const { return m_messages; }

private:
    QVector<PdfAgentConversationMessage> m_messages;
    friend class PDFAgentOrchestrator;
};

}   // namespace pdf

Q_DECLARE_METATYPE(pdf::PDFAgentChatMessage)
Q_DECLARE_METATYPE(QVector<pdf::PDFAgentChatMessage>)
Q_DECLARE_METATYPE(pdf::PDFAgentLlmConfig)
Q_DECLARE_METATYPE(pdf::PDFAgentLlmResponse)
Q_DECLARE_METATYPE(pdf::PDFAgentNormalizedResponse)
Q_DECLARE_METATYPE(pdf::PdfAgentToolCall)
Q_DECLARE_METATYPE(pdf::PDFAgentAssistantTurn)
Q_DECLARE_METATYPE(pdf::PdfAgentConversationMessage)

#endif // PDFAGENTTYPES_H
