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

#include "agent/pdfagenttypes.h"

#include <QJsonDocument>

namespace pdf
{

// PdfAgentSessionInfo implementation
QJsonObject PdfAgentSessionInfo::toJson() const
{
    QJsonObject obj;
    obj["sessionId"] = sessionId;
    obj["title"] = title;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    obj["lastActivityAt"] = lastActivityAt.toString(Qt::ISODate);
    obj["messageCount"] = messageCount;
    return obj;
}

PdfAgentSessionInfo PdfAgentSessionInfo::fromJson(const QJsonObject& json)
{
    PdfAgentSessionInfo info;
    info.sessionId = json["sessionId"].toString();
    info.title = json["title"].toString();
    info.createdAt = QDateTime::fromString(json["createdAt"].toString(), Qt::ISODate);
    info.lastActivityAt = QDateTime::fromString(json["lastActivityAt"].toString(), Qt::ISODate);
    info.messageCount = json["messageCount"].toInt();
    return info;
}

// PdfAgentConversation implementation
void PdfAgentConversation::clear()
{
    m_messages.clear();
}

void PdfAgentConversation::appendSystemMessage(const QString& content)
{
    PdfAgentConversationMessage msg;
    msg.role = "system";
    msg.content = content;
    m_messages.append(msg);
}

void PdfAgentConversation::appendUserMessage(const QString& content)
{
    PdfAgentConversationMessage msg;
    msg.role = "user";
    msg.content = content;
    m_messages.append(msg);
}

void PdfAgentConversation::appendAssistantMessage(const QString& content, const QVector<PdfAgentToolCall>& toolCalls, const QJsonObject& rawMessage)
{
    PdfAgentConversationMessage msg;
    msg.role = "assistant";
    msg.content = content;
    msg.rawAssistantMessage = rawMessage;
    m_messages.append(msg);
}

void PdfAgentConversation::appendToolResultMessage(const QString& toolCallId, const QString& toolName, const QString& content)
{
    PdfAgentConversationMessage msg;
    msg.role = "tool";
    msg.toolCallId = toolCallId;
    msg.toolName = toolName;
    msg.content = content;
    m_messages.append(msg);
}

QJsonObject PdfAgentConversation::toJson() const
{
    QJsonObject obj;
    obj["sessionId"] = m_sessionId;

    QJsonArray messagesArray;
    for (const auto& msg : m_messages)
    {
        QJsonObject msgObj;
        msgObj["role"] = msg.role;
        msgObj["content"] = msg.content;
        if (!msg.toolCallId.isEmpty())
        {
            msgObj["toolCallId"] = msg.toolCallId;
        }
        if (!msg.toolName.isEmpty())
        {
            msgObj["toolName"] = msg.toolName;
        }
        if (!msg.rawAssistantMessage.isEmpty())
        {
            msgObj["rawAssistantMessage"] = msg.rawAssistantMessage;
        }
        messagesArray.append(msgObj);
    }
    obj["messages"] = messagesArray;
    return obj;
}

PdfAgentConversation PdfAgentConversation::fromJson(const QJsonObject& json)
{
    PdfAgentConversation conv;
    conv.m_sessionId = json["sessionId"].toString();

    QJsonArray messagesArray = json["messages"].toArray();
    for (const auto& msgVal : messagesArray)
    {
        QJsonObject msgObj = msgVal.toObject();
        PdfAgentConversationMessage msg;
        msg.role = msgObj["role"].toString();
        msg.content = msgObj["content"].toString();
        msg.toolCallId = msgObj["toolCallId"].toString();
        msg.toolName = msgObj["toolName"].toString();
        msg.rawAssistantMessage = msgObj["rawAssistantMessage"].toObject();
        conv.m_messages.append(msg);
    }
    return conv;
}

}   // namespace pdf
