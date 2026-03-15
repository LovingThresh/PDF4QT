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

#include "agent/pdfagentdiagnostics.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QTabWidget>
#include <QJsonDocument>

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
    m_isBusy(false),
    m_debugTabWidget(nullptr),
    m_diagnosticsEdit(nullptr),
    m_toolTraceEdit(nullptr),
    m_rawJsonEdit(nullptr),
    m_clearDiagnosticsButton(nullptr),
    m_showDiagnosticsCheckBox(nullptr)
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

    // Show diagnostics checkbox
    m_showDiagnosticsCheckBox = new QCheckBox(tr("Show Debug"), headerWidget);
    m_showDiagnosticsCheckBox->setChecked(false);
    headerLayout->addWidget(m_showDiagnosticsCheckBox);

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

    // Debug tab widget
    m_debugTabWidget = new QTabWidget(m_splitter);
    m_debugTabWidget->setVisible(false);

    // Diagnostics tab
    QWidget* diagnosticsTab = new QWidget();
    QVBoxLayout* diagLayout = new QVBoxLayout(diagnosticsTab);
    diagLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* diagHeaderLayout = new QHBoxLayout();
    QLabel* diagLabel = new QLabel(tr("Diagnostics Log"), diagnosticsTab);
    diagHeaderLayout->addWidget(diagLabel);
    diagHeaderLayout->addStretch();

    m_clearDiagnosticsButton = new QPushButton(tr("Clear"), diagnosticsTab);
    m_clearDiagnosticsButton->setMaximumWidth(80);
    diagHeaderLayout->addWidget(m_clearDiagnosticsButton);
    diagLayout->addLayout(diagHeaderLayout);

    m_diagnosticsEdit = new QPlainTextEdit(diagnosticsTab);
    m_diagnosticsEdit->setReadOnly(true);
    m_diagnosticsEdit->setFont(QFont("Courier New", 9));
    diagLayout->addWidget(m_diagnosticsEdit);

    m_debugTabWidget->addTab(diagnosticsTab, tr("Diagnostics"));

    // Tool Trace tab
    m_toolTraceEdit = new QPlainTextEdit(m_debugTabWidget);
    m_toolTraceEdit->setReadOnly(true);
    m_toolTraceEdit->setFont(QFont("Courier New", 9));
    m_debugTabWidget->addTab(m_toolTraceEdit, tr("Tool Trace"));

    // Raw JSON tab
    m_rawJsonEdit = new QPlainTextEdit(m_debugTabWidget);
    m_rawJsonEdit->setReadOnly(true);
    m_rawJsonEdit->setFont(QFont("Courier New", 9));
    m_debugTabWidget->addTab(m_rawJsonEdit, tr("Raw JSON"));

    m_splitter->addWidget(m_debugTabWidget);

    // Original response details (kept for backward compatibility)
    m_responseDetailsEdit = new QPlainTextEdit(m_splitter);
    m_responseDetailsEdit->setReadOnly(true);
    m_responseDetailsEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_responseDetailsEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_responseDetailsEdit->setMinimumHeight(40);
    m_responseDetailsEdit->setVisible(false); // Hide, use debug tab instead

    m_splitter->setStretchFactor(0, 5);  // 50% for chat
    m_splitter->setStretchFactor(1, 2);  // 20% for input
    m_splitter->setStretchFactor(2, 3);  // 30% for debug

    // 设置初始大小
    m_splitter->setSizes({ 300, 120, 180 });

    mainLayout->addWidget(m_splitter, 1);

    setWidget(contentWidget);

    connect(m_sendButton, &QPushButton::clicked, this, &AgentChatDockWidget::onSendClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &AgentChatDockWidget::onClearClicked);
    connect(m_showDiagnosticsCheckBox, &QCheckBox::toggled, this, &AgentChatDockWidget::onToggleDiagnostics);
    connect(m_clearDiagnosticsButton, &QPushButton::clicked, this, &AgentChatDockWidget::clearDiagnostics);

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
    // Also update raw JSON tab if visible
    if (m_debugTabWidget->isVisible())
    {
        m_rawJsonEdit->appendPlainText(details);
    }
    m_responseDetailsEdit->setPlainText(details);
}

void AgentChatDockWidget::clearConversation() const
{
    m_messageList->clear();
    setResponseDetails(QString());
}

void AgentChatDockWidget::appendDiagnosticEvent(const QString& category, const QString& message) const
{
    if (!m_debugTabWidget->isVisible())
    {
        return;
    }

    QString logLine = QString("[%1] %2: %3")
        .arg(QTime::currentTime().toString("HH:mm:ss.zzz"))
        .arg(category)
        .arg(message);

    m_diagnosticsEdit->appendPlainText(logLine);

    // Also add to tool trace if it's a tool-related event
    if (category.contains("tool"))
    {
        m_toolTraceEdit->appendPlainText(logLine);
    }
}

void AgentChatDockWidget::setShowDiagnostics(bool show) const
{
    m_showDiagnosticsCheckBox->setChecked(show);
}

void AgentChatDockWidget::clearDiagnostics() const
{
    m_diagnosticsEdit->clear();
    m_toolTraceEdit->clear();
    m_rawJsonEdit->clear();
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

void AgentChatDockWidget::onToggleDiagnostics()
{
    m_debugTabWidget->setVisible(m_showDiagnosticsCheckBox->isChecked());
}

}   // namespace pdfplugin
