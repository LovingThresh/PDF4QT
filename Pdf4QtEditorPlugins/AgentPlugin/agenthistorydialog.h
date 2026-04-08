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

#ifndef AGENTHISTORYDIALOG_H
#define AGENTHISTORYDIALOG_H

#include "agent/pdfagenttypes.h"

#include <QDialog>
#include <QVector>

class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace pdfplugin
{

class AgentHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AgentHistoryDialog(QWidget* parent = nullptr);

    void setSessions(const QVector<pdf::PdfAgentSessionInfo>& sessions);
    QString getSelectedSessionId() const;

signals:
    void deleteSessionRequested(const QString& sessionId);

private slots:
    void onCurrentItemChanged();
    void onOpenClicked();
    void onDeleteClicked();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    QString sessionIdForItem(const QListWidgetItem* item) const;
    void refreshButtons();

    QListWidget* m_sessionList;
    QPushButton* m_openButton;
    QPushButton* m_deleteButton;
    QVector<pdf::PdfAgentSessionInfo> m_sessions;
};

}   // namespace pdfplugin

#endif // AGENTHISTORYDIALOG_H
