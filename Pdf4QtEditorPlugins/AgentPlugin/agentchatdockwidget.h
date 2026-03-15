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

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSplitter;

namespace pdfplugin
{

class AgentChatDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit AgentChatDockWidget(QWidget* parent = nullptr);

    void appendUserMessage(const QString& text) const;
    void appendAssistantMessage(const QString& text) const;
    void appendSystemMessage(const QString& text) const;
    void appendErrorMessage(const QString& text) const;
    void setBusy(bool busy);
    void setContextSummary(const QString& summary) const;
    void setResponseDetails(const QString& details) const;
    void clearConversation() const;

signals:
    void sendMessageRequested(const QString& text);

private:
    void appendMessage(const QString& prefix, const QString& text) const;
    void onSendClicked();
    void onClearClicked();

    QSplitter* m_splitter;
    QListWidget* m_messageList;
    QPlainTextEdit* m_inputEdit;
    QPlainTextEdit* m_responseDetailsEdit;
    QPushButton* m_sendButton;
    QPushButton* m_clearButton;
    QLabel* m_statusLabel;
    QLabel* m_contextLabel;
    bool m_isBusy;
};

}   // namespace pdfplugin

#endif // AGENTCHATDOCKWIDGET_H
