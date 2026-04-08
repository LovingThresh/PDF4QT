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

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

Q_LOGGING_CATEGORY(pdfAgentHistoryLog, "pdf4qt.agent.history")

namespace pdf
{

namespace
{

QString toIsoString(const QDateTime& value)
{
    const QDateTime normalized = value.isValid() ? value.toUTC() : QDateTime::currentDateTimeUtc();
    return normalized.toString(Qt::ISODateWithMs);
}

QDateTime fromIsoString(const QVariant& value)
{
    const QString text = value.toString().trimmed();
    if (text.isEmpty())
    {
        return QDateTime();
    }

    QDateTime dateTime = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!dateTime.isValid())
    {
        dateTime = QDateTime::fromString(text, Qt::ISODate);
    }
    return dateTime;
}

QByteArray jsonBytes(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QByteArray jsonBytes(const QJsonArray& array)
{
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

QJsonObject parseJsonObject(const QVariant& value)
{
    const QByteArray bytes = value.toByteArray();
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    return document.isObject() ? document.object() : QJsonObject();
}

QJsonArray parseJsonArray(const QVariant& value)
{
    const QByteArray bytes = value.toByteArray();
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    return document.isArray() ? document.array() : QJsonArray();
}

} // namespace

class PdfAgentHistoryManager::ScopedConnection
{
public:
    ScopedConnection(const QString& connectionName, const QString& databasePath) :
        m_connectionName(connectionName)
    {
        if (QSqlDatabase::contains(m_connectionName))
        {
            m_database = QSqlDatabase::database(m_connectionName);
        }
        else
        {
            m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
            m_database.setDatabaseName(databasePath);
        }
    }

    ~ScopedConnection()
    {
        if (m_database.isValid() && m_database.isOpen())
        {
            m_database.close();
        }
    }

    [[nodiscard]] bool open()
    {
        if (m_database.isOpen())
        {
            return true;
        }

        if (m_database.open())
        {
            return true;
        }

        qWarning(pdfAgentHistoryLog) << "Failed to open history database:" << m_database.lastError().text();
        return false;
    }

    [[nodiscard]] QSqlDatabase database() const
    {
        return m_database;
    }

private:
    QString m_connectionName;
    QSqlDatabase m_database;
};

PdfAgentHistoryManager::PdfAgentHistoryManager() :
    m_connectionName(QStringLiteral("PdfAgentHistory_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    initializeDatabase();
    migrateLegacySessionsIfNeeded();
}

PdfAgentHistoryManager::~PdfAgentHistoryManager()
{
    if (QSqlDatabase::contains(m_connectionName))
    {
        QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
        if (database.isValid())
        {
            database.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QString PdfAgentHistoryManager::getStorageDirectory() const
{
    if (m_storageDirectory.isEmpty())
    {
        m_storageDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    return m_storageDirectory;
}

QString PdfAgentHistoryManager::getDatabasePath() const
{
    if (m_databasePath.isEmpty())
    {
        m_databasePath = getStorageDirectory() + QStringLiteral("/agent_history.sqlite");
    }
    return m_databasePath;
}

void PdfAgentHistoryManager::ensureStorageDirectoryExists() const
{
    QDir directory(getStorageDirectory());
    if (!directory.exists())
    {
        directory.mkpath(QStringLiteral("."));
    }
}

bool PdfAgentHistoryManager::initializeDatabase() const
{
    ensureStorageDirectoryExists();

    ScopedConnection connection(m_connectionName, getDatabasePath());
    if (!connection.open())
    {
        return false;
    }

    QSqlQuery query(connection.database());
    query.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sessions ("
            "session_id TEXT PRIMARY KEY,"
            "title TEXT NOT NULL,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "message_count INTEGER NOT NULL DEFAULT 0,"
            "last_model TEXT,"
            "last_document_path TEXT,"
            "todo_json TEXT NOT NULL DEFAULT '[]'"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS messages ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "session_id TEXT NOT NULL,"
            "seq INTEGER NOT NULL,"
            "role TEXT NOT NULL,"
            "content TEXT,"
            "tool_call_id TEXT,"
            "tool_name TEXT,"
            "raw_assistant_message TEXT,"
            "parts_json TEXT,"
            "created_at TEXT NOT NULL,"
            "FOREIGN KEY(session_id) REFERENCES sessions(session_id) ON DELETE CASCADE"
            ")"),
        QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_messages_session_seq ON messages(session_id, seq)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sessions_updated_at ON sessions(updated_at DESC)")
    };

    for (const QString& statement : statements)
    {
        if (!query.exec(statement))
        {
            qWarning(pdfAgentHistoryLog) << "Failed to initialize history schema:" << query.lastError().text();
            return false;
        }
    }

    return true;
}

QString PdfAgentHistoryManager::generateTitle(const PdfAgentConversation& conversation, const QString& sessionId) const
{
    for (const PdfAgentConversationMessage& message : conversation.getMessages())
    {
        if (message.role != QLatin1String("user"))
        {
            continue;
        }

        QString candidate = message.content.trimmed();
        if (candidate.isEmpty())
        {
            for (const PDFAgentMessagePart& part : message.parts)
            {
                if (part.isText() && !part.text.trimmed().isEmpty())
                {
                    candidate = part.text.trimmed();
                    break;
                }
            }
        }

        if (!candidate.isEmpty())
        {
            candidate = candidate.simplified();
            if (candidate.size() > 80)
            {
                candidate = candidate.left(80) + QStringLiteral("...");
            }
            return candidate;
        }
    }

    return QStringLiteral("Session %1").arg(sessionId.left(8));
}

void PdfAgentHistoryManager::saveSession(const QString& sessionId,
                                         const PdfAgentConversation& conversation,
                                         const QJsonArray& todoItems,
                                         const QString& documentPath,
                                         const QString& model)
{
    const QString normalizedSessionId = sessionId.trimmed();
    if (normalizedSessionId.isEmpty())
    {
        return;
    }

    if (!initializeDatabase())
    {
        return;
    }

    ScopedConnection connection(m_connectionName, getDatabasePath());
    if (!connection.open())
    {
        return;
    }

    QSqlDatabase database = connection.database();
    if (!database.transaction())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to start history transaction:" << database.lastError().text();
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QDateTime createdAt = now;
    QSqlQuery selectQuery(database);
    selectQuery.prepare(QStringLiteral("SELECT created_at FROM sessions WHERE session_id = ?"));
    selectQuery.addBindValue(normalizedSessionId);
    if (selectQuery.exec() && selectQuery.next())
    {
        const QDateTime existing = fromIsoString(selectQuery.value(0));
        if (existing.isValid())
        {
            createdAt = existing;
        }
    }

    QSqlQuery upsertQuery(database);
    upsertQuery.prepare(QStringLiteral(
        "INSERT INTO sessions (session_id, title, created_at, updated_at, message_count, last_model, last_document_path, todo_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(session_id) DO UPDATE SET "
        "title = excluded.title, "
        "updated_at = excluded.updated_at, "
        "message_count = excluded.message_count, "
        "last_model = excluded.last_model, "
        "last_document_path = excluded.last_document_path, "
        "todo_json = excluded.todo_json"));
    upsertQuery.addBindValue(normalizedSessionId);
    upsertQuery.addBindValue(generateTitle(conversation, normalizedSessionId));
    upsertQuery.addBindValue(toIsoString(createdAt));
    upsertQuery.addBindValue(toIsoString(now));
    upsertQuery.addBindValue(conversation.getMessages().size());
    upsertQuery.addBindValue(model);
    upsertQuery.addBindValue(documentPath);
    upsertQuery.addBindValue(QString::fromUtf8(jsonBytes(todoItems)));

    if (!upsertQuery.exec())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to upsert session:" << upsertQuery.lastError().text();
        database.rollback();
        return;
    }

    QSqlQuery deleteMessagesQuery(database);
    deleteMessagesQuery.prepare(QStringLiteral("DELETE FROM messages WHERE session_id = ?"));
    deleteMessagesQuery.addBindValue(normalizedSessionId);
    if (!deleteMessagesQuery.exec())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to clear old session messages:" << deleteMessagesQuery.lastError().text();
        database.rollback();
        return;
    }

    QSqlQuery insertMessageQuery(database);
    insertMessageQuery.prepare(QStringLiteral(
        "INSERT INTO messages (session_id, seq, role, content, tool_call_id, tool_name, raw_assistant_message, parts_json, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    const QVector<PdfAgentConversationMessage>& messages = conversation.getMessages();
    for (int index = 0; index < messages.size(); ++index)
    {
        const PdfAgentConversationMessage& message = messages.at(index);
        QJsonArray partsArray;
        for (const PDFAgentMessagePart& part : message.parts)
        {
            if (part.isValid())
            {
                partsArray.append(part.toJson(false));
            }
        }

        insertMessageQuery.addBindValue(normalizedSessionId);
        insertMessageQuery.addBindValue(index);
        insertMessageQuery.addBindValue(message.role);
        insertMessageQuery.addBindValue(message.content);
        insertMessageQuery.addBindValue(message.toolCallId);
        insertMessageQuery.addBindValue(message.toolName);
        insertMessageQuery.addBindValue(QString::fromUtf8(jsonBytes(message.rawAssistantMessage)));
        insertMessageQuery.addBindValue(QString::fromUtf8(jsonBytes(partsArray)));
        insertMessageQuery.addBindValue(toIsoString(now));

        if (!insertMessageQuery.exec())
        {
            qWarning(pdfAgentHistoryLog) << "Failed to insert history message:" << insertMessageQuery.lastError().text();
            database.rollback();
            return;
        }

        insertMessageQuery.finish();
    }

    if (!database.commit())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to commit history session:" << database.lastError().text();
    }
}

QVector<PdfAgentSessionInfo> PdfAgentHistoryManager::getSessionList() const
{
    QVector<PdfAgentSessionInfo> sessions;
    if (!initializeDatabase())
    {
        return sessions;
    }

    ScopedConnection connection(m_connectionName, getDatabasePath());
    if (!connection.open())
    {
        return sessions;
    }

    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral(
        "SELECT session_id, title, created_at, updated_at, message_count, last_model, last_document_path "
        "FROM sessions ORDER BY updated_at DESC")))
    {
        qWarning(pdfAgentHistoryLog) << "Failed to query session list:" << query.lastError().text();
        return sessions;
    }

    while (query.next())
    {
        PdfAgentSessionInfo info;
        info.sessionId = query.value(0).toString();
        info.title = query.value(1).toString();
        info.createdAt = fromIsoString(query.value(2));
        info.lastActivityAt = fromIsoString(query.value(3));
        info.messageCount = query.value(4).toInt();
        info.lastModel = query.value(5).toString();
        info.lastDocumentPath = query.value(6).toString();
        sessions.append(info);
    }

    return sessions;
}

PdfAgentHistoryManager::SessionState PdfAgentHistoryManager::loadSessionState(const QString& sessionId) const
{
    SessionState state;
    const QString normalizedSessionId = sessionId.trimmed();
    if (normalizedSessionId.isEmpty() || !initializeDatabase())
    {
        return state;
    }

    ScopedConnection connection(m_connectionName, getDatabasePath());
    if (!connection.open())
    {
        return state;
    }

    QSqlQuery sessionQuery(connection.database());
    sessionQuery.prepare(QStringLiteral(
        "SELECT session_id, title, created_at, updated_at, message_count, last_model, last_document_path, todo_json "
        "FROM sessions WHERE session_id = ?"));
    sessionQuery.addBindValue(normalizedSessionId);
    if (!sessionQuery.exec() || !sessionQuery.next())
    {
        return state;
    }

    state.info.sessionId = sessionQuery.value(0).toString();
    state.info.title = sessionQuery.value(1).toString();
    state.info.createdAt = fromIsoString(sessionQuery.value(2));
    state.info.lastActivityAt = fromIsoString(sessionQuery.value(3));
    state.info.messageCount = sessionQuery.value(4).toInt();
    state.info.lastModel = sessionQuery.value(5).toString();
    state.info.lastDocumentPath = sessionQuery.value(6).toString();
    state.todoItems = parseJsonArray(sessionQuery.value(7));
    state.conversation.setSessionId(state.info.sessionId);

    QSqlQuery messagesQuery(connection.database());
    messagesQuery.prepare(QStringLiteral(
        "SELECT role, content, tool_call_id, tool_name, raw_assistant_message, parts_json "
        "FROM messages WHERE session_id = ? ORDER BY seq ASC"));
    messagesQuery.addBindValue(normalizedSessionId);
    if (!messagesQuery.exec())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to query session messages:" << messagesQuery.lastError().text();
        return SessionState();
    }

    while (messagesQuery.next())
    {
        PdfAgentConversationMessage message;
        message.role = messagesQuery.value(0).toString();
        message.content = messagesQuery.value(1).toString();
        message.toolCallId = messagesQuery.value(2).toString();
        message.toolName = messagesQuery.value(3).toString();
        message.rawAssistantMessage = parseJsonObject(messagesQuery.value(4));

        const QJsonArray partsArray = parseJsonArray(messagesQuery.value(5));
        for (const QJsonValue& value : partsArray)
        {
            if (!value.isObject())
            {
                continue;
            }

            const PDFAgentMessagePart part = PDFAgentMessagePart::fromJson(value.toObject());
            if (part.isValid())
            {
                message.parts.append(part);
            }
        }

        state.conversation.m_messages.append(message);
    }

    return state;
}

PdfAgentConversation PdfAgentHistoryManager::loadSession(const QString& sessionId) const
{
    return loadSessionState(sessionId).conversation;
}

PdfAgentHistoryManager::SessionState PdfAgentHistoryManager::loadMostRecentSession() const
{
    if (!initializeDatabase())
    {
        return SessionState();
    }

    ScopedConnection connection(m_connectionName, getDatabasePath());
    if (!connection.open())
    {
        return SessionState();
    }

    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT session_id FROM sessions ORDER BY updated_at DESC LIMIT 1")) || !query.next())
    {
        return SessionState();
    }

    return loadSessionState(query.value(0).toString());
}

void PdfAgentHistoryManager::deleteSession(const QString& sessionId)
{
    const QString normalizedSessionId = sessionId.trimmed();
    if (normalizedSessionId.isEmpty() || !initializeDatabase())
    {
        return;
    }

    ScopedConnection connection(m_connectionName, getDatabasePath());
    if (!connection.open())
    {
        return;
    }

    QSqlDatabase database = connection.database();
    if (!database.transaction())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to start delete transaction:" << database.lastError().text();
        return;
    }

    QSqlQuery deleteMessagesQuery(database);
    deleteMessagesQuery.prepare(QStringLiteral("DELETE FROM messages WHERE session_id = ?"));
    deleteMessagesQuery.addBindValue(normalizedSessionId);
    if (!deleteMessagesQuery.exec())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to delete session messages:" << deleteMessagesQuery.lastError().text();
        database.rollback();
        return;
    }

    QSqlQuery deleteSessionQuery(database);
    deleteSessionQuery.prepare(QStringLiteral("DELETE FROM sessions WHERE session_id = ?"));
    deleteSessionQuery.addBindValue(normalizedSessionId);
    if (!deleteSessionQuery.exec())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to delete session:" << deleteSessionQuery.lastError().text();
        database.rollback();
        return;
    }

    if (!database.commit())
    {
        qWarning(pdfAgentHistoryLog) << "Failed to commit session deletion:" << database.lastError().text();
    }
}

void PdfAgentHistoryManager::pruneOldSessions(int maxSessionCount)
{
    if (maxSessionCount <= 0 || !initializeDatabase())
    {
        return;
    }

    const QVector<PdfAgentSessionInfo> sessions = getSessionList();
    for (int index = maxSessionCount; index < sessions.size(); ++index)
    {
        deleteSession(sessions.at(index).sessionId);
    }
}

void PdfAgentHistoryManager::exportSession(const QString& sessionId, const QString& filePath) const
{
    const SessionState state = loadSessionState(sessionId);
    if (!state.isValid() || filePath.trimmed().isEmpty())
    {
        return;
    }

    QJsonObject root;
    root.insert(QStringLiteral("session"), state.info.toJson());
    root.insert(QStringLiteral("conversation"), state.conversation.toJson());
    root.insert(QStringLiteral("todoItems"), state.todoItems);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

PdfAgentConversation PdfAgentHistoryManager::importSession(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return PdfAgentConversation();
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
    {
        return PdfAgentConversation();
    }

    const QJsonObject root = document.object();
    const QJsonObject conversationObject = root.value(QStringLiteral("conversation")).toObject();
    if (conversationObject.isEmpty())
    {
        return PdfAgentConversation();
    }

    PdfAgentConversation conversation = PdfAgentConversation::fromJson(conversationObject);
    const QString newSessionId = generateSessionId();
    conversation.setSessionId(newSessionId);

    const QJsonArray todoItems = root.value(QStringLiteral("todoItems")).toArray();
    saveSession(newSessionId, conversation, todoItems);
    return conversation;
}

bool PdfAgentHistoryManager::migrateLegacySessionsIfNeeded()
{
    if (!initializeDatabase())
    {
        return false;
    }

    if (!getSessionList().isEmpty())
    {
        return true;
    }

    const QString legacyDirectoryPath = getStorageDirectory() + QStringLiteral("/agent_sessions");
    QDir legacyDirectory(legacyDirectoryPath);
    if (!legacyDirectory.exists())
    {
        return true;
    }

    const QFileInfoList files = legacyDirectory.entryInfoList(QStringList() << QStringLiteral("*.json"),
                                                              QDir::Files | QDir::Readable,
                                                              QDir::Time | QDir::Reversed);
    for (const QFileInfo& fileInfo : files)
    {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
        {
            continue;
        }

        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject())
        {
            continue;
        }

        PdfAgentConversation conversation = PdfAgentConversation::fromJson(document.object());
        QString sessionId = conversation.getSessionId().trimmed();
        if (sessionId.isEmpty())
        {
            sessionId = fileInfo.completeBaseName();
            conversation.setSessionId(sessionId);
        }

        saveSession(sessionId, conversation);
    }

    return true;
}

QString PdfAgentHistoryManager::generateSessionId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}   // namespace pdf
