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
#include <QDateTime>

namespace pdf
{

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentImagePart
{
    QString sourceType;
    int pageIndex = -1;
    QString mimeType;
    QString fileName;
    QString filePath;
    QString dataUrl;
    QString transportMode;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool hasSerializableMetadata() const;
    QJsonObject toJson(bool includeTransientData = false) const;
    static PDFAgentImagePart fromJson(const QJsonObject& json);
};

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentMessagePart
{
    QString type;
    QString text;
    PDFAgentImagePart image;

    static PDFAgentMessagePart createTextPart(const QString& text);
    static PDFAgentMessagePart createImagePart(const PDFAgentImagePart& image);

    [[nodiscard]] bool isText() const { return type == "text"; }
    [[nodiscard]] bool isImage() const { return type == "image"; }
    [[nodiscard]] bool isValid() const;
    QJsonObject toJson(bool includeTransientData = false) const;
    static PDFAgentMessagePart fromJson(const QJsonObject& json);
};

// Session metadata for history management
struct PDF4QTLIBCORESHARED_EXPORT PdfAgentSessionInfo
{
    QString sessionId;
    QString title;
    QDateTime createdAt;
    QDateTime lastActivityAt;
    int messageCount = 0;

    QJsonObject toJson() const;
    static PdfAgentSessionInfo fromJson(const QJsonObject& json);
};

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentChatMessage
{
    QString role;
    QString content;
    QVector<PDFAgentMessagePart> parts;
    QString toolCallId;  // For tool role messages
    QString toolName;
    QJsonObject rawAssistantMessage;

    [[nodiscard]] bool hasParts() const { return !parts.isEmpty(); }
    [[nodiscard]] bool hasImageParts() const;
    [[nodiscard]] bool hasTextContent() const;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentLlmConfig
{
    QString endpoint;
    QString model;
    QString apiKey;
    QString systemPrompt;
    int timeoutMs = 30000;
    double temperature = 0.2;
    bool enableStreaming = false;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFAgentLlmResponse
{
    bool success = false;
    QString assistantText;
    QString accumulatedText;  // For streaming - accumulates chunks
    QString errorMessage;
    int httpStatusCode = -1;
    QByteArray rawResponseBody;
    QString rawResponseText;
    QJsonObject rawJson;
    bool isStreaming = false;
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

struct PDF4QTLIBCORESHARED_EXPORT PdfAgentTodoItem
{
    QString id;
    QString text;
    QString status;

    QJsonObject toJson() const;
    static PdfAgentTodoItem fromJson(const QJsonObject& json);
};

class PDF4QTLIBCORESHARED_EXPORT PDFAgentTodoManager
{
public:
    void clear();
    QString update(const QJsonArray& items);
    [[nodiscard]] bool isEmpty() const { return m_items.isEmpty(); }
    [[nodiscard]] const QVector<PdfAgentTodoItem>& getItems() const { return m_items; }
    [[nodiscard]] QJsonArray toJsonArray() const;
    [[nodiscard]] QString render() const;
    [[nodiscard]] int completedCount() const;

private:
    QVector<PdfAgentTodoItem> m_items;
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
    QVector<PDFAgentMessagePart> parts;
    QString toolCallId;     // For tool role
    QString toolName;       // For tool role
    QJsonObject rawAssistantMessage;  // Original assistant message with tool_calls
};

// Conversation manager
class PDF4QTLIBCORESHARED_EXPORT PdfAgentConversation
{
public:
    PdfAgentConversation() = default;
    explicit PdfAgentConversation(const QString& sessionId) : m_sessionId(sessionId) {}

    void clear();
    void setSessionId(const QString& sessionId) { m_sessionId = sessionId; }
    [[nodiscard]] QString getSessionId() const { return m_sessionId; }

    void appendSystemMessage(const QString& content);
    void appendUserMessage(const QString& content);
    void appendUserMessage(const QString& content, const QVector<PDFAgentMessagePart>& parts);
    void appendAssistantMessage(const QString& content, const QVector<PdfAgentToolCall>& toolCalls = {}, const QJsonObject& rawMessage = QJsonObject());
    void appendToolResultMessage(const QString& toolCallId, const QString& toolName, const QString& content);

    [[nodiscard]] const QVector<PdfAgentConversationMessage>& getMessages() const { return m_messages; }

    // Serialization
    QJsonObject toJson() const;
    static PdfAgentConversation fromJson(const QJsonObject& json);

private:
    QString m_sessionId;
    QVector<PdfAgentConversationMessage> m_messages;
    friend class PDFAgentOrchestrator;
    friend class PdfAgentHistoryManager;
};

// Confirmation request from orchestrator to plugin/UI
struct PDF4QTLIBCORESHARED_EXPORT PDFAgentConfirmationRequest
{
    QString commandName;
    QString title;
    QString summary;
    QString targetFile;
    QJsonObject arguments;
    int riskLevel;  // 0=ReadOnly, 1=LowRisk, 2=MediumRisk, 3=HighRisk
};

// Confirmation result from plugin/UI to orchestrator
struct PDF4QTLIBCORESHARED_EXPORT PDFAgentConfirmationResult
{
    bool approved;     // true if user approved, false if rejected
    QString reason;    // Reason for rejection or approval
};

}   // namespace pdf

Q_DECLARE_METATYPE(pdf::PDFAgentChatMessage)
Q_DECLARE_METATYPE(QVector<pdf::PDFAgentChatMessage>)
Q_DECLARE_METATYPE(pdf::PDFAgentImagePart)
Q_DECLARE_METATYPE(pdf::PDFAgentMessagePart)
Q_DECLARE_METATYPE(QVector<pdf::PDFAgentMessagePart>)
Q_DECLARE_METATYPE(pdf::PDFAgentLlmConfig)
Q_DECLARE_METATYPE(pdf::PDFAgentLlmResponse)
Q_DECLARE_METATYPE(pdf::PDFAgentNormalizedResponse)
Q_DECLARE_METATYPE(pdf::PdfAgentTodoItem)
Q_DECLARE_METATYPE(pdf::PdfAgentToolCall)
Q_DECLARE_METATYPE(pdf::PDFAgentAssistantTurn)
Q_DECLARE_METATYPE(pdf::PdfAgentConversationMessage)
Q_DECLARE_METATYPE(pdf::PdfAgentConversation)
Q_DECLARE_METATYPE(pdf::PdfAgentSessionInfo)
Q_DECLARE_METATYPE(pdf::PDFAgentConfirmationRequest)
Q_DECLARE_METATYPE(pdf::PDFAgentConfirmationResult)

#endif // PDFAGENTTYPES_H
