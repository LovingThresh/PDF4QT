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

#include "pdfagentdiagnostics.h"

#include <QDebug>
#include <QUuid>

namespace pdf
{

QJsonObject PdfAgentDiagnosticEvent::toJson() const
{
    QJsonObject json;
    json["timestamp"] = timestamp.toString(Qt::ISODate);
    json["category"] = category;
    json["message"] = message;
    json["payload"] = payload;
    json["requestId"] = requestId;
    return json;
}

QJsonObject PdfAgentToolTraceItem::toJson() const
{
    QJsonObject json;
    json["toolName"] = toolName;
    json["arguments"] = arguments;
    json["result"] = result;
    json["success"] = success;
    json["timestamp"] = timestamp.toString(Qt::ISODate);
    json["requestId"] = requestId;
    return json;
}

PdfAgentDiagnosticsBuffer::PdfAgentDiagnosticsBuffer(int maxSize)
    : m_maxSize(maxSize), m_debugLogToConsole(false)
{
    m_currentRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void PdfAgentDiagnosticsBuffer::append(const PdfAgentDiagnosticEvent& event)
{
    QMutexLocker locker(&m_mutex);

    m_events.append(event);

    // Trim old events if buffer is full
    while (m_events.size() > m_maxSize)
    {
        m_events.removeFirst();
    }

    // Log to console if enabled
    if (m_debugLogToConsole)
    {
        QString logMessage = QString("[%1] [%2] %3")
            .arg(event.timestamp.toString("HH:mm:ss.zzz"))
            .arg(event.category)
            .arg(event.message);

        // Determine log level based on category
        if (event.category.contains("error"))
        {
            qWarning() << logMessage;
        }
        else
        {
            qDebug() << logMessage;
        }
    }
}

QVector<PdfAgentDiagnosticEvent> PdfAgentDiagnosticsBuffer::getEvents() const
{
    QMutexLocker locker(&m_mutex);
    return m_events;
}

QVector<PdfAgentDiagnosticEvent> PdfAgentDiagnosticsBuffer::getEventsByCategory(const QString& category) const
{
    QMutexLocker locker(&m_mutex);
    QVector<PdfAgentDiagnosticEvent> result;
    for (const auto& event : m_events)
    {
        if (event.category == category)
        {
            result.append(event);
        }
    }
    return result;
}

QVector<PdfAgentDiagnosticEvent> PdfAgentDiagnosticsBuffer::getEventsByRequestId(const QString& requestId) const
{
    QMutexLocker locker(&m_mutex);
    QVector<PdfAgentDiagnosticEvent> result;
    for (const auto& event : m_events)
    {
        if (event.requestId == requestId)
        {
            result.append(event);
        }
    }
    return result;
}

void PdfAgentDiagnosticsBuffer::clear()
{
    QMutexLocker locker(&m_mutex);
    m_events.clear();
}

QString PdfAgentDiagnosticsBuffer::getCurrentRequestId() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentRequestId;
}

QString PdfAgentDiagnosticsBuffer::generateRequestId()
{
    QMutexLocker locker(&m_mutex);
    m_currentRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return m_currentRequestId;
}

void PdfAgentDiagnosticsBuffer::appendToolTrace(const PdfAgentToolTraceItem& item)
{
    QMutexLocker locker(&m_mutex);
    m_toolTrace.append(item);

    while (m_toolTrace.size() > m_maxSize)
    {
        m_toolTrace.removeFirst();
    }
}

QVector<PdfAgentToolTraceItem> PdfAgentDiagnosticsBuffer::getToolTrace() const
{
    QMutexLocker locker(&m_mutex);
    return m_toolTrace;
}

void PdfAgentDiagnosticsBuffer::clearToolTrace()
{
    QMutexLocker locker(&m_mutex);
    m_toolTrace.clear();
}

void PdfAgentDiagnosticsBuffer::setDebugLogToConsole(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_debugLogToConsole = enabled;
}

bool PdfAgentDiagnosticsBuffer::isDebugLogToConsole() const
{
    QMutexLocker locker(&m_mutex);
    return m_debugLogToConsole;
}

// Global diagnostics instance
static PdfAgentDiagnosticsBuffer* g_diagnostics = nullptr;

PDF4QTLIBCORESHARED_EXPORT PdfAgentDiagnosticsBuffer* getAgentDiagnostics()
{
    if (!g_diagnostics)
    {
        g_diagnostics = new PdfAgentDiagnosticsBuffer(100);
    }
    return g_diagnostics;
}

}   // namespace pdf
