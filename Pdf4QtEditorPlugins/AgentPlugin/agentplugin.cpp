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

#include "agentplugin.h"
#include "agentchatdockwidget.h"
#include "agenthistorydialog.h"
#include "pdfagentsettingsdialog.h"

#include "pdfdrawwidget.h"
#include "pdftextlayout.h"
#include "pdfwidgettool.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QImageWriter>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QBuffer>

#include <algorithm>
#include <functional>
#include <utility>
#include <unordered_map>

#include "pdfcompiler.h"
#include "pdfdrawspacecontroller.h"

namespace pdfplugin
{

namespace
{

constexpr int MaxAttachments = 5;
constexpr int AttachmentPreviewSize = 512;

QString imageToDataUrl(const QImage& image, const QString& mimeType)
{
    if (image.isNull())
    {
        return QString();
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly))
    {
        return QString();
    }

    const QString normalizedMimeType = mimeType.trimmed().isEmpty() ? QStringLiteral("image/png") : mimeType.trimmed();
    QString format = normalizedMimeType;
    const int slashIndex = format.indexOf(QLatin1Char('/'));
    if (slashIndex >= 0)
    {
        format = format.mid(slashIndex + 1);
    }
    format = format.toUpper();
    if (format == QLatin1String("JPG"))
    {
        format = QStringLiteral("JPEG");
    }

    if (!image.save(&buffer, format.toUtf8().constData()))
    {
        return QString();
    }

    return QStringLiteral("data:%1;base64,%2")
        .arg(normalizedMimeType, QString::fromLatin1(bytes.toBase64()));
}

class ScreenCaptureOverlay final : public QWidget
{
public:
    explicit ScreenCaptureOverlay(QWidget* parent = nullptr) :
        QWidget(parent)
    {
        setWindowFlag(Qt::FramelessWindowHint, true);
        setWindowFlag(Qt::Tool, true);
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_DeleteOnClose, true);
        setCursor(Qt::CrossCursor);

        QRect virtualGeometry;
        const QList<QScreen*> screens = QGuiApplication::screens();
        for (QScreen* screen : screens)
        {
            virtualGeometry = virtualGeometry.united(screen->geometry());
        }

        setGeometry(virtualGeometry);
    }

    std::function<void(const QRect&)> onCaptureCommitted;
    std::function<void()> onCaptureCanceled;

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.fillRect(rect(), QColor(0, 0, 0, 70));

        if (m_selectionRect.isValid())
        {
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.fillRect(m_selectionRect, Qt::transparent);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(QColor(0, 170, 255), 2));
            painter.drawRect(m_selectionRect.adjusted(0, 0, -1, -1));
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton)
        {
            return;
        }

        m_dragging = true;
        m_startPoint = event->pos();
        m_selectionRect = QRect(m_startPoint, QSize());
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!m_dragging)
        {
            return;
        }

        m_selectionRect = QRect(m_startPoint, event->pos()).normalized();
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (!m_dragging || event->button() != Qt::LeftButton)
        {
            return;
        }

        m_dragging = false;
        m_selectionRect = QRect(m_startPoint, event->pos()).normalized();
        const QRect selection = m_selectionRect;
        hide();

        if (selection.width() < 4 || selection.height() < 4)
        {
            QTimer::singleShot(0, this, [this]()
            {
                if (onCaptureCanceled)
                {
                    onCaptureCanceled();
                }
                close();
            });
            return;
        }

        const QRect globalRect = QRect(selection.topLeft() + geometry().topLeft(), selection.size());
        QTimer::singleShot(80, this, [this, globalRect]()
        {
            if (onCaptureCommitted)
            {
                onCaptureCommitted(globalRect);
            }
            close();
        });
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape)
        {
            if (onCaptureCanceled)
            {
                onCaptureCanceled();
            }
            close();
            event->accept();
            return;
        }

        QWidget::keyPressEvent(event);
    }

private:
    bool m_dragging = false;
    QPoint m_startPoint;
    QRect m_selectionRect;
};

}

AgentPlugin::AgentPlugin() :
    pdf::PDFPlugin(nullptr),
    m_toggleChatAction(nullptr),
    m_openSettingsAction(nullptr),
    m_chatDockWidget(nullptr),
    m_orchestrator(new pdf::PDFAgentOrchestrator(this))
{
    // Load settings from QSettings
    m_settings = m_settingsManager.load();

    // Apply settings to orchestrator
    m_orchestrator->setConfig(loadConfig());

    // Initialize diagnostics buffer with debug setting
    pdf::getAgentDiagnostics()->setDebugLogToConsole(m_settings.debugLogToConsole);

    connect(m_orchestrator, &pdf::PDFAgentOrchestrator::responseReady, this, &AgentPlugin::onAgentResponseReady);
    connect(m_orchestrator, &pdf::PDFAgentOrchestrator::toolCallStarted, this, &AgentPlugin::onToolCallStarted);
    connect(m_orchestrator, &pdf::PDFAgentOrchestrator::toolCallFinished, this, &AgentPlugin::onToolCallFinished);
    connect(m_orchestrator, &pdf::PDFAgentOrchestrator::finalResponseReady, this, &AgentPlugin::onFinalResponseReady);
    connect(m_orchestrator, &pdf::PDFAgentOrchestrator::confirmationRequested, this, &AgentPlugin::onConfirmationRequested);
    connect(m_orchestrator, &pdf::PDFAgentOrchestrator::todoStateChanged, this, &AgentPlugin::onTodoStateChanged);
    connect(m_orchestrator, &pdf::PDFAgentOrchestrator::conversationChanged, this, &AgentPlugin::saveCurrentSession);
}

void AgentPlugin::setWidget(pdf::PDFWidget* widget)
{
    Q_ASSERT(!m_widget);

    BaseClass::setWidget(widget);

    m_toggleChatAction = new QAction(tr("AI &Agent Chat"), this);
    m_toggleChatAction->setObjectName("actionAgentPlugin_OpenChat");
    connect(m_toggleChatAction, &QAction::triggered, this, &AgentPlugin::onToggleChatDock);

    m_openSettingsAction = new QAction(tr("AI Agent &Settings..."), this);
    m_openSettingsAction->setObjectName("actionAgentPlugin_OpenSettings");
    connect(m_openSettingsAction, &QAction::triggered, this, &AgentPlugin::onOpenSettings);

    updateActions();
}

void AgentPlugin::setDocument(const pdf::PDFModifiedDocument& document)
{
    BaseClass::setDocument(document);
    updateContextState();
    updateActions();
}

std::vector<QAction*> AgentPlugin::getActions() const
{
    return { m_toggleChatAction, m_openSettingsAction };
}

QString AgentPlugin::getPluginMenuName() const
{
    return tr("AI &Agent");
}

void AgentPlugin::onToggleChatDock()
{
    ensureDockWidget();

    m_chatDockWidget->show();
    m_chatDockWidget->raise();
}

void AgentPlugin::onSendMessageRequested(const QString& text)
{
    if (!m_chatDockWidget)
    {
        return;
    }

    if (m_currentSessionId.trimmed().isEmpty())
    {
        startNewSession(false);
    }

    const QString trimmedText = text.trimmed();
    m_chatDockWidget->setResponseDetails(QString());

    if (!m_attachments.isEmpty())
    {
        if (sendMultimodalMessage(trimmedText,
                                  m_attachments,
                                  tr("Attached %1 image(s).").arg(m_attachments.size())))
        {
            for (const Attachment& attachment : std::as_const(m_attachments))
            {
                if (!attachment.filePath.isEmpty())
                {
                    m_inFlightAttachmentFiles.push_back(attachment.filePath);
                }
            }
            clearAttachments(false);
            return;
        }
    }

    const pdf::PDFAgentExecutionContext context = buildExecutionContext();
    const PageImageRequest automaticImageRequest = detectAutomaticPageImageRequest(trimmedText, context);
    if (automaticImageRequest.valid)
    {
        QVector<Attachment> automaticAttachments;
        Attachment attachment;
        attachment.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        attachment.sourceType = QStringLiteral("page_render");
        attachment.title = tr("Page %1").arg(automaticImageRequest.pageIndex + 1);
        attachment.subtitle = automaticImageRequest.reason;
        attachment.pageIndex = automaticImageRequest.pageIndex;
        automaticAttachments.append(attachment);

        if (sendMultimodalMessage(trimmedText, automaticAttachments, automaticImageRequest.reason))
        {
            return;
        }
    }

    m_chatDockWidget->appendUserMessage(text);
    m_chatDockWidget->setActivityStatus(tr("Status: Preparing response..."),
                                        AgentChatDockWidget::ActivityState::Thinking,
                                        true);

    // Check if input is a mock tool call JSON
    if (trimmedText.startsWith("{") && trimmedText.contains("tool_calls"))
    {
        // Process as mock tool call
        pdf::PdfAgentToolExecutionResult result = m_orchestrator->processMockToolRequest(trimmedText, context);

        m_chatDockWidget->setActivityStatus(tr("Status: Ready."),
                                            AgentChatDockWidget::ActivityState::Ready,
                                            false);

        if (result.success)
        {
            m_chatDockWidget->appendAssistantMessage(result.summaryText);
        }
        else
        {
            m_chatDockWidget->appendErrorMessage(result.errorMessage);
        }

        // Format results as JSON for debug info
        QJsonArray jsonArray;
        for (const QJsonValue& v : result.toolResults)
        {
            jsonArray.append(v);
        }
        QJsonDocument doc(jsonArray);
        m_chatDockWidget->setResponseDetails(doc.toJson(QJsonDocument::Indented));
    }
    else
    {
        // Process as normal chat with tool calling support
        m_orchestrator->setConfig(loadConfig());
        m_orchestrator->processWithToolCalls(text, context);
    }
}

void AgentPlugin::onAttachCurrentPageRequested()
{
    const pdf::PDFAgentExecutionContext context = buildExecutionContext();
    if (!addPageRenderAttachment(context.currentPage,
                                 tr("Current Page"),
                                 tr("Page %1 render").arg(context.currentPage + 1)))
    {
        m_chatDockWidget->appendErrorMessage(tr("Failed to attach the current page image."));
    }
}

void AgentPlugin::onAttachSpecificPageRequested(int pageNumber)
{
    if (!addPageRenderAttachment(pageNumber - 1,
                                 tr("Page %1").arg(pageNumber),
                                 tr("Rendered page %1").arg(pageNumber)))
    {
        m_chatDockWidget->appendErrorMessage(tr("Failed to attach page %1 image.").arg(pageNumber));
    }
}

void AgentPlugin::onCapturePageRegionRequested()
{
    beginPageRegionCapture();
}

void AgentPlugin::onCaptureScreenRequested()
{
    beginScreenCapture();
}

void AgentPlugin::onRemoveAttachmentRequested(const QString& id)
{
    if (removeAttachmentById(id))
    {
        syncAttachmentsToDock();
    }
}

void AgentPlugin::onCancelRequested()
{
    m_orchestrator->cancelCurrentOperation();
    if (m_chatDockWidget)
    {
        m_chatDockWidget->setActivityStatus(tr("Status: Canceling current request..."),
                                            AgentChatDockWidget::ActivityState::Error,
                                            true);
    }
}

void AgentPlugin::onClearRequested()
{
    startNewSession(true);
}

void AgentPlugin::onNewChatRequested()
{
    startNewSession(true);
}

void AgentPlugin::onResumeLastRequested()
{
    if (!resumeLastSession() && m_chatDockWidget)
    {
        m_chatDockWidget->appendSystemMessage(tr("No saved chat history was found."));
    }
}

void AgentPlugin::onHistoryRequested()
{
    if (!m_chatDockWidget)
    {
        return;
    }

    AgentHistoryDialog dialog(m_chatDockWidget);
    dialog.setSessions(m_historyManager.getSessionList());
    connect(&dialog, &AgentHistoryDialog::deleteSessionRequested, this, [this, &dialog](const QString& sessionId)
    {
        m_historyManager.deleteSession(sessionId);
        dialog.setSessions(m_historyManager.getSessionList());
        if (sessionId == m_currentSessionId)
        {
            startNewSession(true);
        }
    });

    if (dialog.exec() == QDialog::Accepted)
    {
        loadSession(dialog.getSelectedSessionId());
    }
}

pdf::PDFAgentExecutionContext AgentPlugin::buildExecutionContext() const
{
    pdf::PDFAgentExecutionContext context;
    context.document = m_document;
    context.widget = m_widget;
    context.mainWindow = m_dataExchangeInterface ? m_dataExchangeInterface->getMainWindow() : nullptr;
    context.originalFileName = m_dataExchangeInterface ? m_dataExchangeInterface->getOriginalFileName() : QString();
    context.agentTempDirectory = QDir::tempPath() + "/pdf4qt-agent";

    if (m_document)
    {
        context.pageCount = static_cast<int>(m_document->getCatalog()->getPageCount());
    }

    if (m_widget && m_widget->getDrawWidget())
    {
        const std::vector<pdf::PDFInteger> pages = m_widget->getDrawWidget()->getCurrentPages();
        if (!pages.empty())
        {
            context.currentPage = static_cast<int>(pages.front());
        }
    }

    if (m_dataExchangeInterface)
    {
        // Get selected text from the data exchange interface
        const pdf::PDFTextSelection& textSelection = m_dataExchangeInterface->getSelectedText();
        context.textSelection = &textSelection;
        if (!textSelection.isEmpty() && m_widget && m_widget->getDrawWidgetProxy())
        {
            if (auto* textLayoutCompiler = m_widget->getDrawWidgetProxy()->getTextLayoutCompiler())
            {
                QStringList selectedTexts;
                for (const auto& item : textSelection)
                {
                    pdf::PDFTextLayout textLayout = textLayoutCompiler->getTextLayout(item.start.pageIndex);
                    pdf::PDFTextFlows textFlows = pdf::PDFTextFlow::createTextFlows(textLayout, pdf::PDFTextFlow::RemoveSoftHyphen, item.start.pageIndex);

                    for (const pdf::PDFTextFlow& textFlow : textFlows)
                    {
                        QString part = textFlow.getText(item.start, item.end);
                        if (!part.isEmpty())
                        {
                            selectedTexts << part;
                        }
                    }
                }
                context.selectedText = selectedTexts.join(" ");
            }
        }
    }

    m_commandCenter.setRuntime(m_document, m_widget, context.mainWindow, const_cast<AgentPlugin*>(this));
    context.commandCenter = &m_commandCenter;
    context.todoManager = m_orchestrator->getTodoManager();

    return context;
}

AgentPlugin::PageImageRequest AgentPlugin::detectAutomaticPageImageRequest(const QString& text, const pdf::PDFAgentExecutionContext& context) const
{
    PageImageRequest request;
    const QString normalized = text.trimmed();
    if (normalized.isEmpty())
    {
        return request;
    }

    const bool asksForVisualAnalysis =
        normalized.contains(QRegularExpression(QStringLiteral("(当前页|这页|这一页|第\\s*\\d+\\s*页|current page|this page|page\\s*\\d+)"),
                                               QRegularExpression::CaseInsensitiveOption)) &&
        normalized.contains(QRegularExpression(QStringLiteral("(分析|看看|查看|识别|describe|analy[sz]e|inspect|look at|what is on)"),
                                               QRegularExpression::CaseInsensitiveOption));

    if (!asksForVisualAnalysis)
    {
        return request;
    }

    QRegularExpression cnPageRegex(QStringLiteral("第\\s*(\\d+)\\s*页"));
    QRegularExpression enPageRegex(QStringLiteral("page\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = cnPageRegex.match(normalized);
    if (!match.hasMatch())
    {
        match = enPageRegex.match(normalized);
    }

    if (match.hasMatch())
    {
        const int pageNumber = match.captured(1).toInt();
        request.pageIndex = pageNumber - 1;
        request.valid = request.pageIndex >= 0 && request.pageIndex < context.pageCount;
        request.reason = tr("Automatically attached page %1 image from your request.").arg(pageNumber);
        return request;
    }

    if (context.currentPage >= 0 && context.currentPage < context.pageCount)
    {
        request.valid = true;
        request.pageIndex = context.currentPage;
        request.reason = tr("Automatically attached the current page image from your request.");
    }

    return request;
}

bool AgentPlugin::sendMultimodalMessage(const QString& text,
                                        const QVector<Attachment>& attachments,
                                        const QString& attachmentReason)
{
    if (!m_chatDockWidget)
    {
        return false;
    }

    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty())
    {
        m_chatDockWidget->appendErrorMessage(tr("Please enter your prompt before sending attachments."));
        return false;
    }

    if (attachments.isEmpty())
    {
        m_chatDockWidget->appendErrorMessage(tr("Attach at least one page or screenshot before sending."));
        return false;
    }

    const pdf::PDFAgentChatMessage userMessage = buildMultimodalUserMessage(text, attachments);
    if (!userMessage.hasImageParts())
    {
        return false;
    }

    m_chatDockWidget->appendSystemMessage(attachmentReason);
    m_chatDockWidget->appendUserMessage(trimmedText);
    m_chatDockWidget->setResponseDetails(QString());
    m_chatDockWidget->setActivityStatus(tr("Status: Preparing multimodal request..."),
                                        AgentChatDockWidget::ActivityState::Thinking,
                                        true);

    const pdf::PDFAgentExecutionContext context = buildExecutionContext();
    m_orchestrator->setConfig(loadConfig());
    m_orchestrator->processWithToolCalls(userMessage, context);
    return true;
}

pdf::PDFAgentChatMessage AgentPlugin::buildMultimodalUserMessage(const QString& text,
                                                                 const QVector<Attachment>& attachments) const
{
    pdf::PDFAgentChatMessage message;
    message.role = QStringLiteral("user");

    const QString trimmedText = text.trimmed();
    message.content = trimmedText;
    message.parts.append(pdf::PDFAgentMessagePart::createTextPart(message.content));

    for (const Attachment& attachment : attachments)
    {
        pdf::PDFAgentImagePart imagePart;
        imagePart.sourceType = attachment.sourceType;
        imagePart.pageIndex = attachment.pageIndex;
        imagePart.title = attachment.title;
        imagePart.subtitle = attachment.subtitle;
        imagePart.mimeType = attachment.mimeType;
        imagePart.fileName = QFileInfo(attachment.filePath).fileName();
        imagePart.filePath = attachment.filePath;
        imagePart.dataUrl = imageToDataUrl(attachment.image, imagePart.mimeType);
        imagePart.transportMode = QStringLiteral("inline_data_url");
        imagePart.imagePixelWidth = attachment.image.width();
        imagePart.imagePixelHeight = attachment.image.height();
        imagePart.pageRectX = attachment.pageRectangle.x();
        imagePart.pageRectY = attachment.pageRectangle.y();
        imagePart.pageRectWidth = attachment.pageRectangle.width();
        imagePart.pageRectHeight = attachment.pageRectangle.height();
        message.parts.append(pdf::PDFAgentMessagePart::createImagePart(imagePart));
    }

    return message;
}

void AgentPlugin::onAgentResponseReady(const pdf::PDFAgentLlmResponse& response) const
{
    if (!m_chatDockWidget)
    {
        const_cast<AgentPlugin*>(this)->cleanupInFlightAttachmentFiles();
        return;
    }

    m_chatDockWidget->setActivityStatus(response.success ? tr("Status: Response received.")
                                                         : tr("Status: Response failed."),
                                        response.success ? AgentChatDockWidget::ActivityState::Ready
                                                         : AgentChatDockWidget::ActivityState::Error,
                                        false);

    const pdf::PDFAgentNormalizedResponse normalized =
        pdf::PDFAgentLlmClient::normalizeChatResponse(response, m_orchestrator->getConfig().endpoint);

    if (response.success)
    {
        m_chatDockWidget->appendAssistantMessage(response.assistantText);
    }
    else
    {
        m_chatDockWidget->appendErrorMessage(response.errorMessage);
    }

    m_chatDockWidget->setResponseDetails(pdf::PDFAgentLlmClient::formatNormalizedResponse(normalized));
    updateContextState();
    const_cast<AgentPlugin*>(this)->cleanupInFlightAttachmentFiles();
}

void AgentPlugin::onToolCallStarted(const QString& toolName) const
{
    if (!m_chatDockWidget)
    {
        return;
    }

    m_chatDockWidget->setActivityStatus(tr("Status: Calling tool `%1`...").arg(toolName),
                                        AgentChatDockWidget::ActivityState::Tool,
                                        true);
    m_chatDockWidget->setResponseDetails(tr("Executing tool: %1...").arg(toolName));

    // Add to diagnostics if enabled
    if (m_settings.debugShowToolTrace)
    {
        m_chatDockWidget->appendDiagnosticEvent("tool.requested", tr("Executing: %1").arg(toolName));
    }
}

void AgentPlugin::onToolCallFinished(const QString& toolName, bool success) const
{
    if (!m_chatDockWidget)
    {
        return;
    }

    const QString status = success ? tr("OK") : tr("FAILED");

    // Add to diagnostics if enabled
    if (m_settings.debugShowToolTrace)
    {
        m_chatDockWidget->appendDiagnosticEvent(
            success ? "tool.result" : "tool.error",
            tr("Tool %1: %2").arg(toolName, status));
    }
    m_chatDockWidget->setActivityStatus(success ? tr("Status: Tool `%1` finished successfully.").arg(toolName)
                                                : tr("Status: Tool `%1` failed.").arg(toolName),
                                        success ? AgentChatDockWidget::ActivityState::Ready
                                                : AgentChatDockWidget::ActivityState::Error,
                                        false);
    m_chatDockWidget->setResponseDetails(tr("Tool %1: %2").arg(toolName).arg(status));
}

void AgentPlugin::onFinalResponseReady(const QString& responseText) const
{
    // This is handled by onAgentResponseReady, which is called after final response
    Q_UNUSED(responseText);
}

void AgentPlugin::onConfirmationRequested(const pdf::PDFAgentConfirmationRequest& request)
{
    if (!m_chatDockWidget)
    {
        // No UI available, auto-reject
        submitConfirmationResult({false, "No UI available"});
        return;
    }

    m_chatDockWidget->setActivityStatus(tr("Status: Waiting for confirmation for `%1`.").arg(request.commandName),
                                        AgentChatDockWidget::ActivityState::Confirmation,
                                        true);

    // Show confirmation dialog
    QMessageBox msgBox(m_chatDockWidget);
    msgBox.setWindowTitle(tr("Confirm %1").arg(request.title));
    msgBox.setText(request.summary);

    // Add detailed information
    QString details;
    if (!request.targetFile.isEmpty())
    {
        details = tr("File: %1\n").arg(request.targetFile);
    }
    details += tr("Command: %1\n").arg(request.commandName);

    // Add risk level text
    QString riskText;
    switch (request.riskLevel)
    {
        case 0: riskText = "Read-Only"; break;
        case 1: riskText = "Low Risk"; break;
        case 2: riskText = "Medium Risk"; break;
        case 3: riskText = "High Risk"; break;
        default: riskText = "Unknown"; break;
    }
    details += tr("Risk Level: %1").arg(riskText);

    msgBox.setDetailedText(details);

    // Add buttons based on risk level
    QPushButton* approveButton = msgBox.addButton(tr("Approve"), QMessageBox::AcceptRole);
    QPushButton* rejectButton = msgBox.addButton(tr("Reject"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(approveButton);
    msgBox.setEscapeButton(rejectButton);

    // Show the dialog
    msgBox.exec();

    // Check which button was clicked
    if (msgBox.clickedButton() == approveButton)
    {
        m_chatDockWidget->setActivityStatus(tr("Status: Confirmation approved. Continuing..."),
                                            AgentChatDockWidget::ActivityState::Thinking,
                                            true);
        submitConfirmationResult({true, "Approved by user"});
    }
    else
    {
        m_chatDockWidget->setActivityStatus(tr("Status: Confirmation rejected."),
                                            AgentChatDockWidget::ActivityState::Error,
                                            false);
        submitConfirmationResult({false, "Rejected by user"});
    }
}

void AgentPlugin::submitConfirmationResult(const pdf::PDFAgentConfirmationResult& result)
{
    // Directly call the orchestrator method since we're in the same thread
    m_orchestrator->submitConfirmationResult(result);
}

void AgentPlugin::onTodoStateChanged(const QString& renderedText, bool hasItems) const
{
    if (!m_chatDockWidget)
    {
        return;
    }

    m_chatDockWidget->setTodoSummary(hasItems ? renderedText : QString());
}

void AgentPlugin::updateActions() const
{
    bool enabled = m_widget && m_dataExchangeInterface && m_dataExchangeInterface->getMainWindow();

    if (m_toggleChatAction)
    {
        m_toggleChatAction->setEnabled(enabled);
    }

    if (m_openSettingsAction)
    {
        m_openSettingsAction->setEnabled(enabled);
    }
}

void AgentPlugin::updateContextState() const
{
    if (!m_chatDockWidget || !m_dataExchangeInterface)
    {
        return;
    }

    QString summary;
    if (!m_document)
    {
        summary = tr("No document loaded.");
    }
    else
    {
        const QString originalFileName = m_dataExchangeInterface->getOriginalFileName();
        const QString fileName = originalFileName.isEmpty() ? tr("<unsaved>") : QFileInfo(originalFileName).fileName();
        const int pageCount = static_cast<int>(m_document->getCatalog()->getPageCount());

        int currentPage = -1;
        if (m_widget && m_widget->getDrawWidget())
        {
            const std::vector<pdf::PDFInteger> pages = m_widget->getDrawWidget()->getCurrentPages();
            if (!pages.empty())
            {
                currentPage = static_cast<int>(pages.front()) + 1;
            }
        }

        const bool hasSelectedText = !m_dataExchangeInterface->getSelectedText().isEmpty();
        summary = tr("File: %1 | Pages: %2 | Current page: %3 | Selected text: %4")
                      .arg(fileName)
                      .arg(pageCount)
                      .arg(currentPage > 0 ? QString::number(currentPage) : tr("n/a"))
                      .arg(hasSelectedText ? tr("Yes") : tr("No"));
    }

    m_chatDockWidget->setContextSummary(summary);
}

void AgentPlugin::ensureDockWidget()
{
    if (m_chatDockWidget || !m_dataExchangeInterface)
    {
        return;
    }

    QMainWindow* mainWindow = m_dataExchangeInterface->getMainWindow();
    if (!mainWindow)
    {
        return;
    }

    m_chatDockWidget = new AgentChatDockWidget(mainWindow);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_chatDockWidget, Qt::Vertical);
    connect(m_chatDockWidget, &AgentChatDockWidget::sendMessageRequested, this, &AgentPlugin::onSendMessageRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::attachCurrentPageRequested, this, &AgentPlugin::onAttachCurrentPageRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::attachSpecificPageRequested, this, &AgentPlugin::onAttachSpecificPageRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::capturePageRegionRequested, this, &AgentPlugin::onCapturePageRegionRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::captureScreenRequested, this, &AgentPlugin::onCaptureScreenRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::removeAttachmentRequested, this, &AgentPlugin::onRemoveAttachmentRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::cancelRequested, this, &AgentPlugin::onCancelRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::clearRequested, this, &AgentPlugin::onClearRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::newChatRequested, this, &AgentPlugin::onNewChatRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::resumeLastRequested, this, &AgentPlugin::onResumeLastRequested);
    connect(m_chatDockWidget, &AgentChatDockWidget::historyRequested, this, &AgentPlugin::onHistoryRequested);
    m_chatDockWidget->setTodoSummary(m_orchestrator->getTodoSummaryText());
    syncAttachmentsToDock();
    updateContextState();
    if (!resumeLastSession())
    {
        startNewSession(true);
    }
}

pdf::PDFAgentLlmConfig AgentPlugin::loadConfig() const
{
    static const QString kVisionAnnotationGuidance = QStringLiteral(
        "When attached page images or region captures are used for visual detection, localization, object finding, or region marking, "
        "first reason about the user's goal, use the attached image context, and describe findings conservatively when precise document-space annotation is not reliable.");

    pdf::PDFAgentLlmConfig config;
    config.endpoint = m_settings.endpoint;
    config.model = m_settings.model;
    config.apiKey = m_settings.apiKey;
    config.systemPrompt = m_settings.systemPrompt.trimmed();
    if (!config.systemPrompt.isEmpty())
    {
        config.systemPrompt += QStringLiteral("\n\n");
    }
    config.systemPrompt += kVisionAnnotationGuidance;
    config.timeoutMs = m_settings.timeoutMs;
    config.temperature = m_settings.temperature;
    return config;
}

void AgentPlugin::applySettings(const pdf::PdfAgentSettings& settings)
{
    m_settings = settings;

    // Apply to orchestrator
    m_orchestrator->setConfig(loadConfig());

    // Update diagnostics buffer
    pdf::getAgentDiagnostics()->setDebugLogToConsole(m_settings.debugLogToConsole);
}

void AgentPlugin::saveCurrentSession()
{
    if (m_restoringSession)
    {
        return;
    }

    const pdf::PdfAgentConversation& conversation = m_orchestrator->getConversation();
    if (conversation.getMessages().isEmpty())
    {
        return;
    }

    if (m_currentSessionId.trimmed().isEmpty())
    {
        m_currentSessionId = pdf::PdfAgentHistoryManager::generateSessionId();
    }

    pdf::PdfAgentConversation persistedConversation = conversation;
    persistedConversation.setSessionId(m_currentSessionId);

    const pdf::PDFAgentExecutionContext context = buildExecutionContext();
    m_historyManager.saveSession(m_currentSessionId,
                                 persistedConversation,
                                 m_orchestrator->getTodoManager()->toJsonArray(),
                                 context.originalFileName,
                                 loadConfig().model);
    m_historyManager.pruneOldSessions(m_settings.maxSessionHistoryCount);
}

void AgentPlugin::renderConversationToDock(const pdf::PdfAgentConversation& conversation) const
{
    if (!m_chatDockWidget)
    {
        return;
    }

    m_chatDockWidget->clearConversation();
    for (const pdf::PdfAgentConversationMessage& message : conversation.getMessages())
    {
        if (message.role == QLatin1String("user"))
        {
            QString text = message.content;
            if (text.trimmed().isEmpty() && !message.parts.isEmpty())
            {
                text = tr("[Multimodal message]");
            }
            m_chatDockWidget->appendUserMessage(text);
        }
        else if (message.role == QLatin1String("assistant") && !message.content.trimmed().isEmpty())
        {
            m_chatDockWidget->appendAssistantMessage(message.content);
        }
    }
}

void AgentPlugin::startNewSession(bool clearUi)
{
    clearAttachments(true);
    cleanupInFlightAttachmentFiles();
    m_currentSessionId = pdf::PdfAgentHistoryManager::generateSessionId();
    m_orchestrator->clearConversation();

    if (!m_chatDockWidget || !clearUi)
    {
        return;
    }

    m_chatDockWidget->clearConversation();
    m_chatDockWidget->setResponseDetails(QString());
    m_chatDockWidget->setActivityStatus(tr("Status: Ready."),
                                        AgentChatDockWidget::ActivityState::Ready,
                                        false);
}

bool AgentPlugin::loadSession(const QString& sessionId)
{
    const pdf::PdfAgentHistoryManager::SessionState state = m_historyManager.loadSessionState(sessionId);
    if (!state.isValid())
    {
        return false;
    }

    clearAttachments(true);
    cleanupInFlightAttachmentFiles();

    m_restoringSession = true;
    m_currentSessionId = state.info.sessionId;
    m_orchestrator->restoreConversation(state.conversation, state.todoItems);
    renderConversationToDock(state.conversation);
    if (m_chatDockWidget)
    {
        m_chatDockWidget->setResponseDetails(QString());
        m_chatDockWidget->setActivityStatus(tr("Status: Resumed saved chat."),
                                            AgentChatDockWidget::ActivityState::Ready,
                                            false);
    }
    m_restoringSession = false;
    updateContextState();
    return true;
}

bool AgentPlugin::resumeLastSession()
{
    const pdf::PdfAgentHistoryManager::SessionState state = m_historyManager.loadMostRecentSession();
    if (!state.isValid())
    {
        return false;
    }

    return loadSession(state.info.sessionId);
}

void AgentPlugin::onOpenSettings()
{
    PdfAgentSettingsDialog dialog(m_dataExchangeInterface ? m_dataExchangeInterface->getMainWindow() : nullptr);
    dialog.setSettings(m_settings);

    connect(&dialog, &PdfAgentSettingsDialog::settingsApplied, this, &AgentPlugin::applySettings);

    dialog.exec();
}

void AgentPlugin::syncAttachmentsToDock() const
{
    if (!m_chatDockWidget)
    {
        return;
    }

    QVector<AgentAttachmentPreview> previews;
    previews.reserve(m_attachments.size());

    for (const Attachment& attachment : m_attachments)
    {
        AgentAttachmentPreview preview;
        preview.id = attachment.id;
        preview.title = attachment.title;
        preview.subtitle = attachment.subtitle;
        preview.thumbnail = scaleAttachmentPreview(attachment.image);
        previews.push_back(preview);
    }

    m_chatDockWidget->setAttachments(previews);
}

bool AgentPlugin::addPageRenderAttachment(int pageIndex, const QString& title, const QString& subtitle)
{
    if (m_attachments.size() >= MaxAttachments)
    {
        if (m_chatDockWidget)
        {
            m_chatDockWidget->appendErrorMessage(tr("You can attach up to %1 images at once.").arg(MaxAttachments));
        }
        return false;
    }

    const pdf::PDFAgentExecutionContext context = buildExecutionContext();
    if (!context.commandCenter)
    {
        return false;
    }
    if (pageIndex < 0 || pageIndex >= context.pageCount)
    {
        return false;
    }

    Attachment attachment;
    attachment.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    attachment.sourceType = QStringLiteral("page_render");
    attachment.title = title;
    attachment.subtitle = subtitle;
    attachment.pageIndex = pageIndex;
    if (const pdf::PDFPage* page = context.document ? context.document->getCatalog()->getPage(pageIndex) : nullptr)
    {
        attachment.pageRectangle = page->getMediaBox();
    }
    attachment.image = context.commandCenter->renderPageImage(pageIndex, context.preferredImageMaxPixelSize);
    attachment.mimeType = QStringLiteral("image/%1").arg(context.preferredImageFormat.toLower());
    if (attachment.image.isNull())
    {
        return false;
    }
    if (!writeAttachmentImage(attachment))
    {
        return false;
    }

    addAttachment(attachment);
    return true;
}

void AgentPlugin::beginPageRegionCapture()
{
    if (m_attachments.size() >= MaxAttachments)
    {
        if (m_chatDockWidget)
        {
            m_chatDockWidget->appendErrorMessage(tr("You can attach up to %1 images at once.").arg(MaxAttachments));
        }
        return;
    }

    if (!m_widget || !m_widget->getToolManager())
    {
        if (m_chatDockWidget)
        {
            m_chatDockWidget->appendErrorMessage(tr("PDF region capture is not available."));
        }
        return;
    }

    m_widget->getToolManager()->pickRectangle([this](pdf::PDFInteger pageIndex, QRectF pageRectangle)
    {
        Attachment attachment;
        attachment.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        attachment.sourceType = QStringLiteral("page_region_capture");
        attachment.title = tr("PDF Region");
        attachment.subtitle = tr("Page %1, %2 x %3").arg(pageIndex + 1)
                                                    .arg(qRound(pageRectangle.width()))
                                                    .arg(qRound(pageRectangle.height()));
        attachment.pageIndex = static_cast<int>(pageIndex);
        attachment.pageRectangle = pageRectangle;
        attachment.image = renderPageRegionImage(pageIndex, pageRectangle);
        attachment.mimeType = QStringLiteral("image/png");

        if (attachment.image.isNull() || !writeAttachmentImage(attachment))
        {
            if (m_chatDockWidget)
            {
                m_chatDockWidget->appendErrorMessage(tr("Failed to capture the selected PDF region."));
            }
            return;
        }

        addAttachment(attachment);
    });

    if (m_chatDockWidget)
    {
        m_chatDockWidget->appendSystemMessage(tr("Drag on the PDF page to capture a region."));
    }
}

void AgentPlugin::beginScreenCapture()
{
    if (m_attachments.size() >= MaxAttachments)
    {
        if (m_chatDockWidget)
        {
            m_chatDockWidget->appendErrorMessage(tr("You can attach up to %1 images at once.").arg(MaxAttachments));
        }
        return;
    }

    if (m_screenCaptureOverlay)
    {
        m_screenCaptureOverlay->raise();
        return;
    }

    ScreenCaptureOverlay* overlay = new ScreenCaptureOverlay();
    m_screenCaptureOverlay = overlay;

    overlay->onCaptureCommitted = [this](const QRect& globalRect)
    {
        captureScreenArea(globalRect);
    };
    overlay->onCaptureCanceled = [this]()
    {
        if (m_chatDockWidget)
        {
            m_chatDockWidget->appendSystemMessage(tr("Screen capture canceled."));
        }
    };
    connect(overlay, &QObject::destroyed, this, [this]()
    {
        m_screenCaptureOverlay = nullptr;
    });

    overlay->show();
    overlay->activateWindow();
    overlay->raise();
}

void AgentPlugin::addAttachment(const Attachment& attachment)
{
    if (m_attachments.size() >= MaxAttachments)
    {
        if (m_chatDockWidget)
        {
            m_chatDockWidget->appendErrorMessage(tr("You can attach up to %1 images at once.").arg(MaxAttachments));
        }
        if (!attachment.filePath.isEmpty())
        {
            QFile::remove(attachment.filePath);
        }
        return;
    }

    m_attachments.push_back(attachment);
    syncAttachmentsToDock();
}

bool AgentPlugin::removeAttachmentById(const QString& id)
{
    const auto it = std::find_if(m_attachments.begin(), m_attachments.end(), [&id](const Attachment& attachment)
    {
        return attachment.id == id;
    });
    if (it == m_attachments.end())
    {
        return false;
    }

    if (!it->filePath.isEmpty())
    {
        QFile::remove(it->filePath);
    }
    m_attachments.erase(it);
    return true;
}

void AgentPlugin::clearAttachments(bool deleteFiles)
{
    for (const Attachment& attachment : std::as_const(m_attachments))
    {
        if (deleteFiles && !attachment.filePath.isEmpty())
        {
            QFile::remove(attachment.filePath);
        }
    }
    m_attachments.clear();
    syncAttachmentsToDock();
}

void AgentPlugin::cleanupInFlightAttachmentFiles()
{
    for (const QString& filePath : std::as_const(m_inFlightAttachmentFiles))
    {
        if (!filePath.isEmpty())
        {
            QFile::remove(filePath);
        }
    }
    m_inFlightAttachmentFiles.clear();
}

bool AgentPlugin::writeAttachmentImage(Attachment& attachment) const
{
    if (attachment.image.isNull())
    {
        return false;
    }

    const pdf::PDFAgentExecutionContext context = buildExecutionContext();
    const QString tempDirectoryPath = context.agentTempDirectory.isEmpty()
                                      ? (QDir::tempPath() + "/pdf4qt-agent")
                                      : context.agentTempDirectory;
    if (!QDir().mkpath(tempDirectoryPath))
    {
        return false;
    }

    const QString extension = QStringLiteral("png");
    attachment.filePath = QStringLiteral("%1/%2-%3.%4")
                              .arg(tempDirectoryPath,
                                   attachment.sourceType,
                                   QUuid::createUuid().toString(QUuid::WithoutBraces),
                                   extension);

    QImageWriter writer(attachment.filePath, extension.toUtf8());
    if (!writer.write(attachment.image))
    {
        attachment.filePath.clear();
        return false;
    }

    if (attachment.mimeType.isEmpty())
    {
        attachment.mimeType = QStringLiteral("image/png");
    }

    return true;
}

QImage AgentPlugin::renderPageRegionImage(int pageIndex, const QRectF& pageRectangle) const
{
    if (!m_widget || !m_widget->getDrawWidgetProxy())
    {
        return QImage();
    }

    pdf::PDFDrawWidgetProxy* proxy = m_widget->getDrawWidgetProxy();
    const pdf::PDFWidgetSnapshot snapshot = proxy->getSnapshot();
    const pdf::PDFWidgetSnapshot::SnapshotItem* pageSnapshot = snapshot.getPageSnapshot(pageIndex);
    if (!pageSnapshot)
    {
        return QImage();
    }

    QRect selectedRectangle = pageSnapshot->pageToDeviceMatrix.mapRect(pageRectangle).toAlignedRect();
    selectedRectangle = selectedRectangle.intersected(m_widget->rect());
    if (!selectedRectangle.isValid())
    {
        return QImage();
    }

    QImage image(selectedRectangle.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.translate(-selectedRectangle.topLeft());
    proxy->drawPages(&painter,
                     m_widget->rect(),
                     proxy->getFeatures() | pdf::PDFRenderer::DenyExtraGraphics);
    painter.end();
    return image;
}

void AgentPlugin::captureScreenArea(const QRect& globalRect)
{
    Attachment attachment;
    attachment.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    attachment.sourceType = QStringLiteral("screen_capture");
    attachment.title = tr("Screen Capture");
    attachment.subtitle = tr("%1 x %2 pixels").arg(globalRect.width()).arg(globalRect.height());
    attachment.image = captureGlobalRectImage(globalRect);
    attachment.mimeType = QStringLiteral("image/png");

    if (attachment.image.isNull() || !writeAttachmentImage(attachment))
    {
        if (m_chatDockWidget)
        {
            m_chatDockWidget->appendErrorMessage(tr("Failed to capture the selected screen area."));
        }
        return;
    }

    addAttachment(attachment);
}

QImage AgentPlugin::captureGlobalRectImage(const QRect& globalRect)
{
    if (!globalRect.isValid())
    {
        return QImage();
    }

    QImage image(globalRect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens)
    {
        const QRect screenGeometry = screen->geometry();
        const QRect intersection = globalRect.intersected(screenGeometry);
        if (!intersection.isValid())
        {
            continue;
        }

        const QPixmap pixmap = screen->grabWindow(0,
                                                  intersection.x() - screenGeometry.x(),
                                                  intersection.y() - screenGeometry.y(),
                                                  intersection.width(),
                                                  intersection.height());
        painter.drawPixmap(intersection.topLeft() - globalRect.topLeft(), pixmap);
    }
    painter.end();

    return image;
}

QImage AgentPlugin::scaleAttachmentPreview(const QImage& image)
{
    if (image.isNull())
    {
        return image;
    }

    return image.scaled(AttachmentPreviewSize,
                        AttachmentPreviewSize,
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
}

}   // namespace pdfplugin
