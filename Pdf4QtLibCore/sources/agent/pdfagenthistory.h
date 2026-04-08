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

#ifndef PDFAGENTHISTORY_H
#define PDFAGENTHISTORY_H

#include "pdfglobal.h"

#include "agent/pdfagenttypes.h"

#include <QJsonArray>
#include <QString>
#include <QVector>

namespace pdf
{

class PDF4QTLIBCORESHARED_EXPORT PdfAgentHistoryManager
{
public:
    struct SessionState
    {
        PdfAgentSessionInfo info;
        PdfAgentConversation conversation;
        QJsonArray todoItems;

        [[nodiscard]] bool isValid() const { return !info.sessionId.trimmed().isEmpty(); }
    };

    PdfAgentHistoryManager();
    ~PdfAgentHistoryManager();

    void saveSession(const QString& sessionId,
                     const PdfAgentConversation& conversation,
                     const QJsonArray& todoItems = QJsonArray(),
                     const QString& documentPath = QString(),
                     const QString& model = QString());

    QVector<PdfAgentSessionInfo> getSessionList() const;
    SessionState loadSessionState(const QString& sessionId) const;
    PdfAgentConversation loadSession(const QString& sessionId) const;
    SessionState loadMostRecentSession() const;
    void deleteSession(const QString& sessionId);
    void pruneOldSessions(int maxSessionCount);

    void exportSession(const QString& sessionId, const QString& filePath) const;
    PdfAgentConversation importSession(const QString& filePath);

    static QString generateSessionId();

    QString getStorageDirectory() const;
    QString getDatabasePath() const;

private:
    class ScopedConnection;

    QString generateTitle(const PdfAgentConversation& conversation, const QString& sessionId) const;
    void ensureStorageDirectoryExists() const;
    bool initializeDatabase() const;
    bool migrateLegacySessionsIfNeeded();

    mutable QString m_storageDirectory;
    mutable QString m_databasePath;
    QString m_connectionName;
};

}   // namespace pdf

#endif // PDFAGENTHISTORY_H
