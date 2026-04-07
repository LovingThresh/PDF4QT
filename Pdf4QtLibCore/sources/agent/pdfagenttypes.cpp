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
#include <algorithm>
#include <stdexcept>

namespace pdf
{

bool PDFAgentImagePart::isValid() const
{
    return pageIndex >= 0 || !filePath.trimmed().isEmpty() || !dataUrl.trimmed().isEmpty();
}

bool PDFAgentImagePart::hasSerializableMetadata() const
{
    return !sourceType.trimmed().isEmpty() || pageIndex >= 0 || !mimeType.trimmed().isEmpty() ||
           !fileName.trimmed().isEmpty() || !transportMode.trimmed().isEmpty();
}

QJsonObject PDFAgentImagePart::toJson(bool includeTransientData) const
{
    QJsonObject obj;
    obj["sourceType"] = sourceType;
    obj["pageIndex"] = pageIndex;
    obj["mimeType"] = mimeType;
    obj["fileName"] = fileName;
    obj["transportMode"] = transportMode;

    if (includeTransientData)
    {
        obj["filePath"] = filePath;
        obj["dataUrl"] = dataUrl;
    }

    return obj;
}

PDFAgentImagePart PDFAgentImagePart::fromJson(const QJsonObject& json)
{
    PDFAgentImagePart image;
    image.sourceType = json["sourceType"].toString();
    image.pageIndex = json["pageIndex"].toInt(-1);
    image.mimeType = json["mimeType"].toString();
    image.fileName = json["fileName"].toString();
    image.filePath = json["filePath"].toString();
    image.dataUrl = json["dataUrl"].toString();
    image.transportMode = json["transportMode"].toString();
    return image;
}

PDFAgentMessagePart PDFAgentMessagePart::createTextPart(const QString& text)
{
    PDFAgentMessagePart part;
    part.type = "text";
    part.text = text;
    return part;
}

PDFAgentMessagePart PDFAgentMessagePart::createImagePart(const PDFAgentImagePart& image)
{
    PDFAgentMessagePart part;
    part.type = "image";
    part.image = image;
    return part;
}

bool PDFAgentMessagePart::isValid() const
{
    if (isText())
    {
        return !text.isEmpty();
    }

    if (isImage())
    {
        return image.isValid();
    }

    return false;
}

QJsonObject PDFAgentMessagePart::toJson(bool includeTransientData) const
{
    QJsonObject obj;
    obj["type"] = type;

    if (isText())
    {
        obj["text"] = text;
    }
    else if (isImage())
    {
        obj["image"] = image.toJson(includeTransientData);
    }

    return obj;
}

PDFAgentMessagePart PDFAgentMessagePart::fromJson(const QJsonObject& json)
{
    PDFAgentMessagePart part;
    part.type = json["type"].toString();
    part.text = json["text"].toString();
    if (json.contains("image") && json["image"].isObject())
    {
        part.image = PDFAgentImagePart::fromJson(json["image"].toObject());
    }
    return part;
}

bool PDFAgentChatMessage::hasImageParts() const
{
    return std::any_of(parts.cbegin(), parts.cend(), [](const PDFAgentMessagePart& part) { return part.isImage(); });
}

bool PDFAgentChatMessage::hasTextContent() const
{
    if (!content.trimmed().isEmpty())
    {
        return true;
    }

    return std::any_of(parts.cbegin(), parts.cend(), [](const PDFAgentMessagePart& part)
    {
        return part.isText() && !part.text.trimmed().isEmpty();
    });
}

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

QJsonObject PdfAgentTodoItem::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["text"] = text;
    obj["status"] = status;
    return obj;
}

PdfAgentTodoItem PdfAgentTodoItem::fromJson(const QJsonObject& json)
{
    PdfAgentTodoItem item;
    item.id = json["id"].toString();
    item.text = json["text"].toString();
    item.status = json["status"].toString();
    return item;
}

void PDFAgentTodoManager::clear()
{
    m_items.clear();
}

QString PDFAgentTodoManager::update(const QJsonArray& items)
{
    if (items.size() > 20)
    {
        throw std::runtime_error("Max 20 todos allowed.");
    }

    QVector<PdfAgentTodoItem> validated;
    validated.reserve(items.size());

    int inProgressCount = 0;
    for (int i = 0; i < items.size(); ++i)
    {
        const QJsonObject obj = items.at(i).toObject();

        PdfAgentTodoItem item;
        item.id = obj.value("id").toString(QString::number(i + 1)).trimmed();
        item.text = obj.value("text").toString().trimmed();
        item.status = obj.value("status").toString("pending").trimmed().toLower();

        if (item.text.isEmpty())
        {
            throw std::runtime_error(QString("Item %1: text required.").arg(item.id).toStdString());
        }

        if (item.status != "pending" && item.status != "in_progress" && item.status != "completed")
        {
            throw std::runtime_error(QString("Item %1: invalid status '%2'.").arg(item.id, item.status).toStdString());
        }

        if (item.status == "in_progress")
        {
            ++inProgressCount;
        }

        validated.push_back(item);
    }

    if (inProgressCount > 1)
    {
        throw std::runtime_error("Only one task can be in_progress at a time.");
    }

    m_items = validated;
    return render();
}

QJsonArray PDFAgentTodoManager::toJsonArray() const
{
    QJsonArray items;
    for (const PdfAgentTodoItem& item : m_items)
    {
        items.append(item.toJson());
    }
    return items;
}

QString PDFAgentTodoManager::render() const
{
    if (m_items.isEmpty())
    {
        return QString();
    }

    QStringList lines;
    lines.reserve(m_items.size() + 1);

    for (const PdfAgentTodoItem& item : m_items)
    {
        QString marker = "[ ]";
        if (item.status == "in_progress")
        {
            marker = "[>]";
        }
        else if (item.status == "completed")
        {
            marker = "[x]";
        }

        lines.append(QString("%1 #%2: %3").arg(marker, item.id, item.text));
    }

    lines.append(QString());
    lines.append(QString("(%1/%2 completed)").arg(completedCount()).arg(m_items.size()));
    return lines.join('\n');
}

int PDFAgentTodoManager::completedCount() const
{
    int count = 0;
    for (const PdfAgentTodoItem& item : m_items)
    {
        if (item.status == "completed")
        {
            ++count;
        }
    }
    return count;
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
    appendUserMessage(content, QVector<PDFAgentMessagePart>());
}

void PdfAgentConversation::appendUserMessage(const QString& content, const QVector<PDFAgentMessagePart>& parts)
{
    PdfAgentConversationMessage msg;
    msg.role = "user";
    msg.content = content;
    msg.parts = parts;
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
        if (!msg.parts.isEmpty())
        {
            QJsonArray partsArray;
            for (const PDFAgentMessagePart& part : msg.parts)
            {
                if (part.isValid())
                {
                    partsArray.append(part.toJson(false));
                }
            }
            if (!partsArray.isEmpty())
            {
                msgObj["parts"] = partsArray;
            }
        }
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
        const QJsonArray partsArray = msgObj["parts"].toArray();
        for (const QJsonValue& partValue : partsArray)
        {
            if (partValue.isObject())
            {
                PDFAgentMessagePart part = PDFAgentMessagePart::fromJson(partValue.toObject());
                if (part.isValid())
                {
                    msg.parts.append(part);
                }
            }
        }
        msg.toolCallId = msgObj["toolCallId"].toString();
        msg.toolName = msgObj["toolName"].toString();
        msg.rawAssistantMessage = msgObj["rawAssistantMessage"].toObject();
        conv.m_messages.append(msg);
    }
    return conv;
}

}   // namespace pdf
