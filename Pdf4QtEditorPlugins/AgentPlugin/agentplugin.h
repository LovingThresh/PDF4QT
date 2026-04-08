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

#ifndef AGENTPLUGIN_H
#define AGENTPLUGIN_H

#include "pdfplugin.h"
#include "agent/pdfagentorchestrator.h"
#include "agent/pdfagentexecutioncontext.h"
#include "agent/pdfagenttypes.h"
#include "agent/pdfagentsettings.h"
#include "agent/pdfagentdiagnostics.h"
#include "agent/pdfagenthistory.h"
#include "pdfagentwidgetcommandcenter.h"

#include <QObject>
#include <QJsonDocument>
#include <QImage>
#include <QPointer>
#include <QRect>
#include <QRectF>
#include <QVector>

class QAction;
class QWidget;

namespace pdfplugin
{

class AgentChatDockWidget;

class AgentPlugin : public pdf::PDFPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "PDF4QT.AgentPlugin" FILE "AgentPlugin.json")

private:
    using BaseClass = pdf::PDFPlugin;

public:
    AgentPlugin();

    virtual void setWidget(pdf::PDFWidget* widget) override;
    virtual void setDocument(const pdf::PDFModifiedDocument& document) override;
    virtual std::vector<QAction*> getActions() const override;
    virtual QString getPluginMenuName() const override;

private:
    struct Attachment
    {
        QString id;
        QString sourceType;
        QString title;
        QString subtitle;
        QString filePath;
        QString mimeType;
        QImage image;
        int pageIndex = -1;
        QRectF pageRectangle;
    };

    struct PageImageRequest
    {
        bool valid = false;
        int pageIndex = -1;
        QString reason;
    };

    void onToggleChatDock();
    void onSendMessageRequested(const QString& text);
    void onAttachCurrentPageRequested();
    void onAttachSpecificPageRequested(int pageNumber);
    void onCapturePageRegionRequested();
    void onCaptureScreenRequested();
    void onRemoveAttachmentRequested(const QString& id);
    void onCancelRequested();
    void onClearRequested();
    void onNewChatRequested();
    void onResumeLastRequested();
    void onHistoryRequested();
    void onAgentResponseReady(const pdf::PDFAgentLlmResponse& response) const;
    void onToolCallStarted(const QString& toolName) const;
    void onToolCallFinished(const QString& toolName, bool success) const;
    void onFinalResponseReady(const QString& responseText) const;
    void onConfirmationRequested(const pdf::PDFAgentConfirmationRequest& request);
    void onTodoStateChanged(const QString& renderedText, bool hasItems) const;
    void submitConfirmationResult(const pdf::PDFAgentConfirmationResult& result);
    void onOpenSettings();

    void updateActions() const;
    void updateContextState() const;
    void ensureDockWidget();
    void syncAttachmentsToDock() const;
    pdf::PDFAgentLlmConfig loadConfig() const;
    void applySettings(const pdf::PdfAgentSettings& settings);
    void saveCurrentSession();
    void renderConversationToDock(const pdf::PdfAgentConversation& conversation) const;
    void startNewSession(bool clearUi = true);
    bool loadSession(const QString& sessionId);
    bool resumeLastSession();
    pdf::PDFAgentExecutionContext buildExecutionContext() const;
    PageImageRequest detectAutomaticPageImageRequest(const QString& text, const pdf::PDFAgentExecutionContext& context) const;
    bool sendMultimodalMessage(const QString& text,
                               const QVector<Attachment>& attachments,
                               const QString& attachmentReason);
    pdf::PDFAgentChatMessage buildMultimodalUserMessage(const QString& text,
                                                        const QVector<Attachment>& attachments) const;
    bool addPageRenderAttachment(int pageIndex, const QString& title, const QString& subtitle);
    void beginPageRegionCapture();
    void beginScreenCapture();
    void addAttachment(const Attachment& attachment);
    bool removeAttachmentById(const QString& id);
    void clearAttachments(bool deleteFiles = true);
    void cleanupInFlightAttachmentFiles();
    bool writeAttachmentImage(Attachment& attachment) const;
    QImage renderPageRegionImage(int pageIndex, const QRectF& pageRectangle) const;
    void captureScreenArea(const QRect& globalRect);
    static QImage captureGlobalRectImage(const QRect& globalRect);
    static QImage scaleAttachmentPreview(const QImage& image);

    // Settings
    mutable pdf::PdfAgentSettings m_settings;
    mutable pdf::PdfAgentSettingsManager m_settingsManager;
    pdf::PdfAgentHistoryManager m_historyManager;
    QString m_currentSessionId;

    QAction* m_toggleChatAction = nullptr;
    QAction* m_openSettingsAction = nullptr;
    AgentChatDockWidget* m_chatDockWidget = nullptr;
    pdf::PDFAgentOrchestrator* m_orchestrator = nullptr;
    mutable pdf::PDFAgentWidgetCommandCenter m_commandCenter;
    QVector<Attachment> m_attachments;
    QVector<QString> m_inFlightAttachmentFiles;
    QPointer<QWidget> m_screenCaptureOverlay;
    bool m_restoringSession = false;
};

}   // namespace pdfplugin

#endif // AGENTPLUGIN_H
