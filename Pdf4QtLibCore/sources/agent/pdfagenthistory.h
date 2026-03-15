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

#include <QString>
#include <QVector>
#include <QDateTime>

namespace pdf
{

class PDF4QTLIBCORESHARED_EXPORT PdfAgentHistoryManager
{
public:
    explicit PdfAgentHistoryManager() = default;

    // Save a session
    void saveSession(const QString& sessionId, const PdfAgentConversation& conversation);

    // Load all sessions (without full message content)
    QVector<PdfAgentSessionInfo> getSessionList() const;

    // Load a specific session
    PdfAgentConversation loadSession(const QString& sessionId) const;

    // Delete a session
    void deleteSession(const QString& sessionId);

    // Export session to file
    void exportSession(const QString& sessionId, const QString& filePath) const;

    // Import session from file
    PdfAgentConversation importSession(const QString& filePath);

    // Generate a new session ID
    static QString generateSessionId();

    // Get the storage directory
    QString getStorageDirectory() const;

private:
    QString getSessionFilePath(const QString& sessionId) const;
    void ensureStorageDirectoryExists() const;
    void updateSessionMetadata(const QString& sessionId, const PdfAgentConversation& conversation);

    mutable QString m_storageDirectory;
};

}   // namespace pdf

#endif // PDFAGENTHISTORY_H
