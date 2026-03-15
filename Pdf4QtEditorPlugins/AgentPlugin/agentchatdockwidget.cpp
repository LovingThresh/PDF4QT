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

#include "agentchatdockwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace pdfplugin
{

AgentChatDockWidget::AgentChatDockWidget(QWidget* parent) :
    QDockWidget(parent),
    m_splitter(nullptr),
    m_messageList(nullptr),
    m_inputEdit(nullptr),
    m_responseDetailsEdit(nullptr),
    m_sendButton(nullptr),
    m_clearButton(nullptr),
    m_statusLabel(nullptr),
    m_contextLabel(nullptr),
    m_isBusy(false)
{
    setObjectName("AIAgentChatDockWidget");
    setWindowTitle(tr("AI Agent Chat"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

    QWidget* contentWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 标题栏：包含上下文摘要和状态
    QWidget* headerWidget = new QWidget(contentWidget);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    m_contextLabel = new QLabel(headerWidget);
    m_contextLabel->setWordWrap(true);
    m_contextLabel->setStyleSheet("font-weight: bold; color: palette(highlight);");
    m_contextLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headerLayout->addWidget(m_contextLabel);

    m_statusLabel = new QLabel(headerWidget);
    m_statusLabel->setStyleSheet("color: palette(mid);");
    m_statusLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    headerLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(headerWidget);

    m_splitter = new QSplitter(Qt::Vertical, contentWidget);
    m_splitter->setChildrenCollapsible(true);
    m_splitter->setHandleWidth(4);

    m_messageList = new QListWidget(m_splitter);
    m_messageList->setSelectionMode(QAbstractItemView::NoSelection);
    m_messageList->setWordWrap(true);
    m_messageList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_splitter->addWidget(m_messageList);

    QWidget* inputArea = new QWidget(m_splitter);
    QVBoxLayout* inputLayout = new QVBoxLayout(inputArea);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(4);

    QLabel* inputLabel = new QLabel(tr("Message"), inputArea);
    inputLayout->addWidget(inputLabel);

    m_inputEdit = new QPlainTextEdit(inputArea);
    m_inputEdit->setPlaceholderText(tr("Ask the AI assistant something about the current PDF..."));
    m_inputEdit->setMinimumHeight(60);
    inputLayout->addWidget(m_inputEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_sendButton = new QPushButton(tr("Send"), inputArea);
    m_clearButton = new QPushButton(tr("Clear"), inputArea);
    buttonLayout->addWidget(m_sendButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addStretch(1);
    inputLayout->addLayout(buttonLayout);

    m_splitter->addWidget(inputArea);

    QWidget* responseArea = new QWidget(m_splitter);
    QVBoxLayout* responseLayout = new QVBoxLayout(responseArea);
    responseLayout->setContentsMargins(0, 0, 0, 0);
    responseLayout->setSpacing(4);

    QLabel* responseLabel = new QLabel(tr("Debug Info"), responseArea);
    responseLayout->addWidget(responseLabel);

    m_responseDetailsEdit = new QPlainTextEdit(responseArea);
    m_responseDetailsEdit->setReadOnly(true);
    m_responseDetailsEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_responseDetailsEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_responseDetailsEdit->setMinimumHeight(40);
    responseLayout->addWidget(m_responseDetailsEdit);

    m_splitter->addWidget(responseArea);

    m_splitter->setStretchFactor(0, 6);  // 60% for chat
    m_splitter->setStretchFactor(1, 2);  // 20% for input
    m_splitter->setStretchFactor(2, 2);  // 20% for debug info

    // 设置初始大小：60:20:20 比例，总和为600
    m_splitter->setSizes({ 360, 120, 120 });

    mainLayout->addWidget(m_splitter, 1);

    setWidget(contentWidget);

    connect(m_sendButton, &QPushButton::clicked, this, &AgentChatDockWidget::onSendClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &AgentChatDockWidget::onClearClicked);

    setContextSummary(tr("No document loaded."));
    setResponseDetails(QString());
    setBusy(false);
}

void AgentChatDockWidget::appendUserMessage(const QString& text) const
{
    appendMessage(tr("User"), text);
}

void AgentChatDockWidget::appendAssistantMessage(const QString& text) const
{
    appendMessage(tr("Assistant"), text);
}

void AgentChatDockWidget::appendSystemMessage(const QString& text) const
{
    appendMessage(tr("System"), text);
}

void AgentChatDockWidget::appendErrorMessage(const QString& text) const
{
    appendMessage(tr("Error"), text);
}

void AgentChatDockWidget::setBusy(bool busy)
{
    m_isBusy = busy;
    m_sendButton->setEnabled(!busy);
    m_inputEdit->setEnabled(!busy);
    m_statusLabel->setText(busy ? tr("Thinking...") : tr("Ready."));
}

void AgentChatDockWidget::setContextSummary(const QString& summary) const
{
    m_contextLabel->setText(summary);
}

void AgentChatDockWidget::setResponseDetails(const QString& details) const
{
    m_responseDetailsEdit->setPlainText(details);
}

void AgentChatDockWidget::clearConversation() const
{
    m_messageList->clear();
    setResponseDetails(QString());
}

void AgentChatDockWidget::appendMessage(const QString& prefix, const QString& text) const
{
    QListWidgetItem* item = new QListWidgetItem(QString("%1: %2").arg(prefix, text));
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEditable);

    if (prefix == tr("User"))
    {
        item->setForeground(QColor(0, 100, 180));
    }
    else if (prefix == tr("Assistant"))
    {
        item->setForeground(QColor(0, 128, 0));
    }
    else if (prefix == tr("Error"))
    {
        item->setForeground(QColor(180, 0, 0));
    }
    else
    {
        item->setForeground(palette().color(QPalette::Mid));
    }

    m_messageList->addItem(item);
    m_messageList->scrollToBottom();
}

void AgentChatDockWidget::onSendClicked()
{
    if (m_isBusy)
    {
        return;
    }

    const QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty())
    {
        return;
    }

    m_inputEdit->clear();
    Q_EMIT sendMessageRequested(text);
}

void AgentChatDockWidget::onClearClicked()
{
    clearConversation();
}

}   // namespace pdfplugin
