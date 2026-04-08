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

#include "agenthistorydialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace pdfplugin
{

namespace
{

constexpr int SessionIdRole = Qt::UserRole + 1;

QString buildSubtitle(const pdf::PdfAgentSessionInfo& session)
{
    QStringList details;
    if (session.lastActivityAt.isValid())
    {
        details << QObject::tr("Updated %1").arg(QLocale().toString(session.lastActivityAt.toLocalTime(), QLocale::ShortFormat));
    }
    details << QObject::tr("%1 messages").arg(session.messageCount);
    if (!session.lastModel.trimmed().isEmpty())
    {
        details << session.lastModel.trimmed();
    }
    return details.join(QStringLiteral(" | "));
}

} // namespace

AgentHistoryDialog::AgentHistoryDialog(QWidget* parent) :
    QDialog(parent),
    m_sessionList(new QListWidget(this)),
    m_openButton(new QPushButton(tr("Open"), this)),
    m_deleteButton(new QPushButton(tr("Delete"), this))
{
    setWindowTitle(tr("Agent History"));
    resize(720, 420);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_sessionList);

    QHBoxLayout* actionsLayout = new QHBoxLayout();
    actionsLayout->addStretch(1);
    actionsLayout->addWidget(m_deleteButton);
    actionsLayout->addWidget(m_openButton);
    layout->addLayout(actionsLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    connect(m_sessionList, &QListWidget::itemSelectionChanged, this, &AgentHistoryDialog::onCurrentItemChanged);
    connect(m_sessionList, &QListWidget::itemDoubleClicked, this, &AgentHistoryDialog::onItemDoubleClicked);
    connect(m_openButton, &QPushButton::clicked, this, &AgentHistoryDialog::onOpenClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &AgentHistoryDialog::onDeleteClicked);

    refreshButtons();
}

void AgentHistoryDialog::setSessions(const QVector<pdf::PdfAgentSessionInfo>& sessions)
{
    m_sessions = sessions;
    m_sessionList->clear();

    for (const pdf::PdfAgentSessionInfo& session : m_sessions)
    {
        QListWidgetItem* item = new QListWidgetItem(m_sessionList);
        item->setText(QStringLiteral("%1\n%2").arg(session.title, buildSubtitle(session)));
        item->setData(SessionIdRole, session.sessionId);
        m_sessionList->addItem(item);
    }

    if (m_sessionList->count() > 0)
    {
        m_sessionList->setCurrentRow(0);
    }

    refreshButtons();
}

QString AgentHistoryDialog::getSelectedSessionId() const
{
    return sessionIdForItem(m_sessionList->currentItem());
}

void AgentHistoryDialog::onCurrentItemChanged()
{
    refreshButtons();
}

void AgentHistoryDialog::onOpenClicked()
{
    if (getSelectedSessionId().isEmpty())
    {
        return;
    }

    accept();
}

void AgentHistoryDialog::onDeleteClicked()
{
    const QString sessionId = getSelectedSessionId();
    if (sessionId.isEmpty())
    {
        return;
    }

    Q_EMIT deleteSessionRequested(sessionId);
}

void AgentHistoryDialog::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!sessionIdForItem(item).isEmpty())
    {
        accept();
    }
}

QString AgentHistoryDialog::sessionIdForItem(const QListWidgetItem* item) const
{
    return item ? item->data(SessionIdRole).toString() : QString();
}

void AgentHistoryDialog::refreshButtons()
{
    const bool hasSelection = !getSelectedSessionId().isEmpty();
    m_openButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
}

}   // namespace pdfplugin
