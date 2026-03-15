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

#include "agent/pdfagenthistory.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

namespace pdf
{

QString PdfAgentHistoryManager::getStorageDirectory() const
{
    if (m_storageDirectory.isEmpty())
    {
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_storageDirectory = appDataPath + "/agent_sessions";
    }
    return m_storageDirectory;
}

void PdfAgentHistoryManager::ensureStorageDirectoryExists() const
{
    QDir dir(getStorageDirectory());
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
}

QString PdfAgentHistoryManager::getSessionFilePath(const QString& sessionId) const
{
    return getStorageDirectory() + "/" + sessionId + ".json";
}

void PdfAgentHistoryManager::saveSession(const QString& sessionId, const PdfAgentConversation& conversation)
{
    ensureStorageDirectoryExists();

    QJsonObject obj = conversation.toJson();
    QJsonDocument doc(obj);

    QFile file(getSessionFilePath(sessionId));
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        updateSessionMetadata(sessionId, conversation);
    }
}

void PdfAgentHistoryManager::updateSessionMetadata(const QString& sessionId, const PdfAgentConversation& conversation)
{
    // Store metadata in QSettings
    QSettings settings;
    QString metaKey = "AIAgent/Sessions/" + sessionId;

    PdfAgentSessionInfo info;
    info.sessionId = sessionId;

    // Generate title from first user message
    const auto& messages = conversation.getMessages();
    for (const auto& msg : messages)
    {
        if (msg.role == "user" && !msg.content.isEmpty())
        {
            // Take first 50 chars as title
            info.title = msg.content.left(50);
            if (msg.content.length() > 50)
            {
                info.title += "...";
            }
            break;
        }
    }

    if (info.title.isEmpty())
    {
        info.title = "Session " + sessionId.left(8);
    }

    info.createdAt = QDateTime::currentDateTime();
    info.lastActivityAt = QDateTime::currentDateTime();
    info.messageCount = messages.size();

    settings.setValue(metaKey, info.toJson());
}

QVector<PdfAgentSessionInfo> PdfAgentHistoryManager::getSessionList() const
{
    QVector<PdfAgentSessionInfo> sessions;

    QSettings settings;
    settings.beginGroup("AIAgent/Sessions");

    const QStringList keys = settings.childKeys();
    for (const QString& key : keys)
    {
        QJsonObject obj = settings.value(key).toJsonObject();
        if (!obj.isEmpty())
        {
            sessions.append(PdfAgentSessionInfo::fromJson(obj));
        }
    }

    settings.endGroup();

    // Sort by last activity (most recent first)
    std::sort(sessions.begin(), sessions.end(),
               [](const PdfAgentSessionInfo& a, const PdfAgentSessionInfo& b) {
                   return b.lastActivityAt < a.lastActivityAt;
               });

    return sessions;
}

PdfAgentConversation PdfAgentHistoryManager::loadSession(const QString& sessionId) const
{
    QFile file(getSessionFilePath(sessionId));
    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject())
        {
            return PdfAgentConversation::fromJson(doc.object());
        }
    }

    return PdfAgentConversation();
}

void PdfAgentHistoryManager::deleteSession(const QString& sessionId)
{
    // Delete the session file
    QFile file(getSessionFilePath(sessionId));
    if (file.exists())
    {
        file.remove();
    }

    // Remove metadata from QSettings
    QSettings settings;
    settings.remove("AIAgent/Sessions/" + sessionId);
}

void PdfAgentHistoryManager::exportSession(const QString& sessionId, const QString& filePath) const
{
    QFile file(getSessionFilePath(sessionId));
    if (file.exists())
    {
        file.copy(filePath);
    }
}

PdfAgentConversation PdfAgentHistoryManager::importSession(const QString& filePath)
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject())
        {
            PdfAgentConversation conv = PdfAgentConversation::fromJson(doc.object());

            // Generate new session ID for imported session
            QString newSessionId = generateSessionId();
            conv.setSessionId(newSessionId);

            // Save with new ID
            saveSession(newSessionId, conv);

            return conv;
        }
    }

    return PdfAgentConversation();
}

QString PdfAgentHistoryManager::generateSessionId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}   // namespace pdf
