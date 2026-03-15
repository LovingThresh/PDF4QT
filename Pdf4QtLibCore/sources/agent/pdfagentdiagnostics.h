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

#ifndef PDFAGENTDIAGNOSTICS_H
#define PDFAGENTDIAGNOSTICS_H

#include "pdfglobal.h"

#include <QDateTime>
#include <QString>
#include <QJsonObject>
#include <QVector>
#include <QMutex>

namespace pdf
{

// Diagnostic event categories
#define PDF_AGENT_DIAG_CATEGORY_NETWORK_REQUEST "network.request"
#define PDF_AGENT_DIAG_CATEGORY_NETWORK_RESPONSE "network.response"
#define PDF_AGENT_DIAG_CATEGORY_NETWORK_ERROR "network.error"
#define PDF_AGENT_DIAG_CATEGORY_ORCHESTRATOR_STATE "orchestrator.state"
#define PDF_AGENT_DIAG_CATEGORY_TOOL_REQUESTED "tool.requested"
#define PDF_AGENT_DIAG_CATEGORY_TOOL_RESULT "tool.result"
#define PDF_AGENT_DIAG_CATEGORY_TOOL_ERROR "tool.error"
#define PDF_AGENT_DIAG_CATEGORY_CONFIRMATION_REQUESTED "confirmation.requested"
#define PDF_AGENT_DIAG_CATEGORY_CONFIRMATION_REJECTED "confirmation.rejected"
#define PDF_AGENT_DIAG_CATEGORY_CONFIRMATION_APPROVED "confirmation.approved"

struct PDF4QTLIBCORESHARED_EXPORT PdfAgentDiagnosticEvent
{
    QDateTime timestamp;
    QString category;
    QString message;
    QJsonObject payload;
    QString requestId;

    // Create JSON for display
    QJsonObject toJson() const;
};

struct PDF4QTLIBCORESHARED_EXPORT PdfAgentToolTraceItem
{
    QString toolName;
    QJsonObject arguments;
    QJsonObject result;
    bool success = false;
    QDateTime timestamp;
    QString requestId;

    QJsonObject toJson() const;
};

class PDF4QTLIBCORESHARED_EXPORT PdfAgentDiagnosticsBuffer
{
public:
    explicit PdfAgentDiagnosticsBuffer(int maxSize = 100);

    // Append event to buffer
    void append(const PdfAgentDiagnosticEvent& event);

    // Get all events
    QVector<PdfAgentDiagnosticEvent> getEvents() const;

    // Get events by category
    QVector<PdfAgentDiagnosticEvent> getEventsByCategory(const QString& category) const;

    // Get events for a specific request
    QVector<PdfAgentDiagnosticEvent> getEventsByRequestId(const QString& requestId) const;

    // Clear all events
    void clear();

    // Get current request ID
    QString getCurrentRequestId() const;

    // Generate new request ID
    QString generateRequestId();

    // Tool trace methods
    void appendToolTrace(const PdfAgentToolTraceItem& item);
    QVector<PdfAgentToolTraceItem> getToolTrace() const;
    void clearToolTrace();

    // Debug mode
    void setDebugLogToConsole(bool enabled);
    bool isDebugLogToConsole() const;

private:
    QVector<PdfAgentDiagnosticEvent> m_events;
    QVector<PdfAgentToolTraceItem> m_toolTrace;
    int m_maxSize;
    QString m_currentRequestId;
    mutable QMutex m_mutex;
    bool m_debugLogToConsole;
};

// Global diagnostics instance
PDF4QTLIBCORESHARED_EXPORT PdfAgentDiagnosticsBuffer* getAgentDiagnostics();

}   // namespace pdf

#endif // PDFAGENTDIAGNOSTICS_H
