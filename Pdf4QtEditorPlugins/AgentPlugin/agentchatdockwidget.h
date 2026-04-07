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

#ifndef AGENTCHATDOCKWIDGET_H
#define AGENTCHATDOCKWIDGET_H

#include <QDockWidget>
#include <QImage>
#include <QTabWidget>
#include <QStringList>
#include <QVector>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QCheckBox;
class QPoint;

namespace pdfplugin
{

struct AgentAttachmentPreview
{
    QString id;
    QString title;
    QString subtitle;
    QImage thumbnail;
};

class AgentChatDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    enum class ActivityState
    {
        Ready,
        Thinking,
        Tool,
        Confirmation,
        Error
    };

    explicit AgentChatDockWidget(QWidget* parent = nullptr);

    void appendUserMessage(const QString& text) const;
    void appendAssistantMessage(const QString& text) const;
    void appendSystemMessage(const QString& text) const;
    void appendErrorMessage(const QString& text) const;
    void setBusy(bool busy);
    void setActivityStatus(const QString& text, ActivityState state, bool busy);
    void setContextSummary(const QString& summary) const;
    void setTodoSummary(const QString& summary) const;
    void setResponseDetails(const QString& details) const;
    void clearConversation() const;
    void setDraftMessage(const QString& text) const;
    void setAttachments(const QVector<AgentAttachmentPreview>& attachments) const;

    // Debug panel methods
    void appendDiagnosticEvent(const QString& category, const QString& message) const;
    void setShowDiagnostics(bool show) const;
    void clearDiagnostics() const;

signals:
    void sendMessageRequested(const QString& text);
    void attachCurrentPageRequested();
    void attachSpecificPageRequested(int pageNumber);
    void capturePageRegionRequested();
    void captureScreenRequested();
    void removeAttachmentRequested(const QString& id);

private:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    void appendMessage(const QString& prefix, const QString& text) const;
    void navigatePromptHistory(int direction);
    void updateActivityAppearance(ActivityState state);
    void copyMessageToClipboard(const QListWidgetItem* item) const;
    void editMessageInInput(const QListWidgetItem* item) const;
    void refreshAttachmentList() const;
    void onSendClicked();
    void onAttachCurrentPageClicked();
    void onAttachSpecificPageClicked();
    void onCapturePageRegionClicked();
    void onCaptureScreenClicked();
    void onClearClicked();
    void onToggleDiagnostics();
    void onMessageContextMenuRequested(const QPoint& pos);
    void onMessageItemActivated(QListWidgetItem* item);

    QLabel* m_activityLabel;
    QSplitter* m_splitter;
    QListWidget* m_messageList;
    QListWidget* m_attachmentList;
    QPlainTextEdit* m_inputEdit;
    QPlainTextEdit* m_responseDetailsEdit;
    QPushButton* m_sendButton;
    QPushButton* m_attachCurrentPageButton;
    QPushButton* m_attachSpecificPageButton;
    QPushButton* m_captureRegionButton;
    QPushButton* m_captureScreenButton;
    QPushButton* m_clearButton;
    QLabel* m_statusLabel;
    QLabel* m_contextLabel;
    QLabel* m_todoLabel;
    bool m_isBusy;

    // Debug/Diagnostics panel
    QTabWidget* m_debugTabWidget;
    QPlainTextEdit* m_diagnosticsEdit;
    QPlainTextEdit* m_toolTraceEdit;
    QPlainTextEdit* m_rawJsonEdit;
    QPushButton* m_clearDiagnosticsButton;
    QCheckBox* m_showDiagnosticsCheckBox;
    mutable QStringList m_promptHistory;
    mutable int m_promptHistoryIndex = -1;
    mutable QString m_unsentDraft;
    mutable QVector<AgentAttachmentPreview> m_attachments;
};

}   // namespace pdfplugin

#endif // AGENTCHATDOCKWIDGET_H
