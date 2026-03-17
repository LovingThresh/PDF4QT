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
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QTabWidget>
#include <QJsonDocument>
#include <QMenu>
#include <QClipboard>
#include <QFontDatabase>

namespace
{

constexpr int MessageRoleData = Qt::UserRole;
constexpr int MessageTextData = Qt::UserRole + 1;
constexpr auto DefaultStatusText = "Status: Ready.";

}

namespace pdfplugin
{

AgentChatDockWidget::AgentChatDockWidget(QWidget* parent) :
    QDockWidget(parent),
    m_activityLabel(nullptr),
    m_splitter(nullptr),
    m_messageList(nullptr),
    m_inputEdit(nullptr),
    m_responseDetailsEdit(nullptr),
    m_sendButton(nullptr),
    m_clearButton(nullptr),
    m_statusLabel(nullptr),
    m_contextLabel(nullptr),
    m_todoLabel(nullptr),
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
    m_contextLabel->setVisible(false);
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

    m_activityLabel = new QLabel(contentWidget);
    m_activityLabel->setWordWrap(false);
    m_activityLabel->setMargin(8);
    m_activityLabel->setMinimumHeight(34);
    mainLayout->addWidget(m_activityLabel);

    m_todoLabel = new QLabel(contentWidget);
    m_todoLabel->setWordWrap(true);
    m_todoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_todoLabel->setMargin(8);
    m_todoLabel->setVisible(false);
    m_todoLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_todoLabel->setStyleSheet("QLabel {"
                               " background-color: rgb(244, 246, 248);"
                               " border: 1px solid rgb(201, 208, 214);"
                               " border-left: 5px solid rgb(120, 133, 145);"
                               " border-radius: 6px;"
                               " color: rgb(48, 57, 65);"
                               " padding: 4px 8px;"
                               "}");
    mainLayout->addWidget(m_todoLabel);

    m_splitter = new QSplitter(Qt::Vertical, contentWidget);
    m_splitter->setChildrenCollapsible(true);
    m_splitter->setHandleWidth(4);

    m_messageList = new QListWidget(m_splitter);
    m_messageList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_messageList->setWordWrap(true);
    m_messageList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_messageList->setContextMenuPolicy(Qt::CustomContextMenu);
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
    m_inputEdit->installEventFilter(this);
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
    connect(m_messageList, &QListWidget::customContextMenuRequested, this, &AgentChatDockWidget::onMessageContextMenuRequested);
    connect(m_messageList, &QListWidget::itemDoubleClicked, this, &AgentChatDockWidget::onMessageItemActivated);

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
    setActivityStatus(busy ? tr("Status: Thinking...") : tr(DefaultStatusText),
                      busy ? ActivityState::Thinking : ActivityState::Ready,
                      busy);
}

void AgentChatDockWidget::setActivityStatus(const QString& text, ActivityState state, bool busy)
{
    m_isBusy = busy;
    m_sendButton->setEnabled(!busy);
    m_inputEdit->setEnabled(!busy);
    m_statusLabel->setText(busy ? tr("Busy") : tr("Ready"));
    m_activityLabel->setText(text.trimmed().isEmpty() ? tr(DefaultStatusText) : text);
    updateActivityAppearance(state);
}

void AgentChatDockWidget::setContextSummary(const QString& summary) const
{
    m_contextLabel->setText(summary);
}

void AgentChatDockWidget::setTodoSummary(const QString& summary) const
{
    const QString trimmed = summary.trimmed();
    if (trimmed.isEmpty())
    {
        m_todoLabel->clear();
        m_todoLabel->setVisible(false);
        return;
    }

    m_todoLabel->setText(tr("Todo\n%1").arg(trimmed));
    m_todoLabel->setVisible(true);
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
    setTodoSummary(QString());
    setResponseDetails(QString());
}

void AgentChatDockWidget::setDraftMessage(const QString& text) const
{
    m_inputEdit->setPlainText(text);
    m_inputEdit->setFocus();

    QTextCursor cursor = m_inputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_inputEdit->setTextCursor(cursor);
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
    item->setData(MessageRoleData, prefix);
    item->setData(MessageTextData, text);
    item->setFlags((item->flags() | Qt::ItemIsSelectable) & ~Qt::ItemIsEditable);

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

    if (m_promptHistory.isEmpty() || m_promptHistory.back() != text)
    {
        m_promptHistory.append(text);
    }
    m_promptHistoryIndex = -1;
    m_unsentDraft.clear();
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

bool AgentChatDockWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_inputEdit && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->modifiers() == Qt::NoModifier)
        {
            const QTextCursor cursor = m_inputEdit->textCursor();
            const bool atFirstLine = cursor.blockNumber() == 0;
            const bool atLastLine = cursor.blockNumber() == m_inputEdit->document()->blockCount() - 1;

            if (keyEvent->key() == Qt::Key_Up && atFirstLine)
            {
                navigatePromptHistory(-1);
                return true;
            }
            if (keyEvent->key() == Qt::Key_Down && atLastLine)
            {
                navigatePromptHistory(1);
                return true;
            }
        }
    }

    return QDockWidget::eventFilter(watched, event);
}

void AgentChatDockWidget::navigatePromptHistory(int direction)
{
    if (m_promptHistory.isEmpty())
    {
        return;
    }

    if (direction < 0)
    {
        if (m_promptHistoryIndex == -1)
        {
            m_unsentDraft = m_inputEdit->toPlainText();
            m_promptHistoryIndex = m_promptHistory.size() - 1;
        }
        else if (m_promptHistoryIndex > 0)
        {
            --m_promptHistoryIndex;
        }
    }
    else
    {
        if (m_promptHistoryIndex == -1)
        {
            return;
        }
        if (m_promptHistoryIndex < m_promptHistory.size() - 1)
        {
            ++m_promptHistoryIndex;
        }
        else
        {
            m_promptHistoryIndex = -1;
            setDraftMessage(m_unsentDraft);
            return;
        }
    }

    setDraftMessage(m_promptHistory.at(m_promptHistoryIndex));
}

void AgentChatDockWidget::updateActivityAppearance(ActivityState state)
{
    QString styleSheet;

    switch (state)
    {
        case ActivityState::Ready:
            styleSheet = "QLabel {"
                         " background-color: rgb(228, 245, 234);"
                         " border: 1px solid rgb(126, 181, 140);"
                         " border-left: 5px solid rgb(67, 135, 84);"
                         " border-radius: 6px;"
                         " color: rgb(31, 79, 43);"
                         " font-weight: 600;"
                         " padding: 2px 8px;"
                         "}";
            break;

        case ActivityState::Thinking:
            styleSheet = "QLabel {"
                         " background-color: rgb(229, 240, 251);"
                         " border: 1px solid rgb(124, 160, 206);"
                         " border-left: 5px solid rgb(53, 104, 171);"
                         " border-radius: 6px;"
                         " color: rgb(26, 63, 117);"
                         " font-weight: 600;"
                         " padding: 2px 8px;"
                         "}";
            break;

        case ActivityState::Tool:
            styleSheet = "QLabel {"
                         " background-color: rgb(252, 239, 223);"
                         " border: 1px solid rgb(223, 170, 110);"
                         " border-left: 5px solid rgb(193, 120, 38);"
                         " border-radius: 6px;"
                         " color: rgb(120, 70, 16);"
                         " font-weight: 600;"
                         " padding: 2px 8px;"
                         "}";
            break;

        case ActivityState::Confirmation:
            styleSheet = "QLabel {"
                         " background-color: rgb(255, 247, 214);"
                         " border: 1px solid rgb(222, 193, 98);"
                         " border-left: 5px solid rgb(189, 148, 28);"
                         " border-radius: 6px;"
                         " color: rgb(121, 93, 15);"
                         " font-weight: 700;"
                         " padding: 2px 8px;"
                         "}";
            break;

        case ActivityState::Error:
            styleSheet = "QLabel {"
                         " background-color: rgb(252, 232, 232);"
                         " border: 1px solid rgb(216, 128, 128);"
                         " border-left: 5px solid rgb(183, 54, 54);"
                         " border-radius: 6px;"
                         " color: rgb(125, 31, 31);"
                         " font-weight: 700;"
                         " padding: 2px 8px;"
                         "}";
            break;
    }

    m_activityLabel->setStyleSheet(styleSheet);
}

void AgentChatDockWidget::copyMessageToClipboard(const QListWidgetItem* item) const
{
    if (!item)
    {
        return;
    }

    if (QClipboard* clipboard = QGuiApplication::clipboard())
    {
        clipboard->setText(item->data(MessageTextData).toString());
    }
}

void AgentChatDockWidget::editMessageInInput(const QListWidgetItem* item) const
{
    if (!item)
    {
        return;
    }

    setDraftMessage(item->data(MessageTextData).toString());
}

void AgentChatDockWidget::onMessageContextMenuRequested(const QPoint& pos)
{
    QListWidgetItem* item = m_messageList->itemAt(pos);
    if (!item)
    {
        return;
    }

    QMenu menu(m_messageList);
    QAction* copyAction = menu.addAction(tr("Copy"));
    QAction* editAction = menu.addAction(tr("Edit In Input"));

    QAction* selectedAction = menu.exec(m_messageList->viewport()->mapToGlobal(pos));
    if (selectedAction == copyAction)
    {
        copyMessageToClipboard(item);
    }
    else if (selectedAction == editAction)
    {
        editMessageInInput(item);
    }
}

void AgentChatDockWidget::onMessageItemActivated(QListWidgetItem* item)
{
    editMessageInInput(item);
}

}   // namespace pdfplugin
