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
#include "pdfagentsettingsdialog.h"

#include "pdfdrawwidget.h"
#include "pdftextlayout.h"
#include "pdfdocumentbuilder.h"

#include <QAction>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>

#include <unordered_map>

#include "pdfcompiler.h"
#include "pdfdocumentwriter.h"
#include "pdfdrawspacecontroller.h"
#include "pdfwidgettool.h"

namespace pdfplugin
{

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

    m_chatDockWidget->appendUserMessage(text);
    m_chatDockWidget->setResponseDetails(QString());
    m_chatDockWidget->setBusy(true);

    // Check if input is a mock tool call JSON
    const QString trimmedText = text.trimmed();
    if (trimmedText.startsWith("{") && trimmedText.contains("tool_calls"))
    {
        // Process as mock tool call
        pdf::PDFAgentExecutionContext context = buildExecutionContext();
        pdf::PdfAgentToolExecutionResult result = m_orchestrator->processMockToolRequest(trimmedText, context);

        m_chatDockWidget->setBusy(false);

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
        pdf::PDFAgentExecutionContext context = buildExecutionContext();
        m_orchestrator->processWithToolCalls(text, context);
    }
}

pdf::PDFAgentExecutionContext AgentPlugin::buildExecutionContext() const
{
    pdf::PDFAgentExecutionContext context;
    context.document = m_document;
    context.widget = m_widget;
    context.mainWindow = m_dataExchangeInterface ? m_dataExchangeInterface->getMainWindow() : nullptr;
    context.originalFileName = m_dataExchangeInterface ? m_dataExchangeInterface->getOriginalFileName() : QString();

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

        // Implementation of text extraction for selected text
        if (!textSelection.isEmpty() && m_widget && m_widget->getDrawWidgetProxy())
        {
            if (auto* textLayoutCompiler = m_widget->getDrawWidgetProxy()->getTextLayoutCompiler())
            {
                QStringList selectedTexts;
                for (const auto& item : textSelection)
                {
                    // For each selection range (which may span across pages), get text from flow
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

    // Set up callback for text search (get text layout for a page)
    if (m_widget && m_widget->getDrawWidgetProxy())
    {
        if (auto* textLayoutCompiler = m_widget->getDrawWidgetProxy()->getTextLayoutCompiler())
        {
            context.searchTextCallback = [textLayoutCompiler, this](int pageIndex, const QString& searchText) -> QJsonObject
            {
                QJsonObject result;
                if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
                {
                    result["ok"] = false;
                    result["error"] = "Invalid page index";
                    return result;
                }

                // Get text layout synchronously
                pdf::PDFTextLayout textLayout = textLayoutCompiler->getTextLayout(pageIndex);
                if (textLayout.getTextBlocks().empty())
                {
                    result["ok"] = true;
                    result["matches"] = QJsonArray();
                    return result;
                }

                // Search for text - use PDFTextFlow
                pdf::PDFTextFlows textFlows = pdf::PDFTextFlow::createTextFlows(textLayout, pdf::PDFTextFlow::RemoveSoftHyphen, pageIndex);

                QJsonArray matches;
                for (const pdf::PDFTextFlow& textFlow : textFlows)
                {
                    pdf::PDFFindResults findResults = textFlow.find(searchText, Qt::CaseInsensitive);

                    for (const pdf::PDFFindResult& fr : findResults)
                    {
                        QJsonObject match;
                        match["page_index"] = pageIndex;
                        match["page_number"] = pageIndex + 1;
                        match["matched_text"] = fr.matched;
                        match["context"] = fr.context;

                        // Get real quadrilaterals from pre-calculated bounding boxes in find result
                        QJsonArray quads;
                        if (!fr.boundingBoxes.empty())
                        {
                            QRectF combined;
                            for (const QRectF& b : fr.boundingBoxes)
                            {
                                if (!b.isNull())
                                {
                                    if (combined.isNull()) combined = b;
                                    else combined = combined.united(b);
                                }
                            }

                            if (!combined.isNull())
                            {
                                QJsonObject q;
                                q["x"] = combined.x();
                                q["y"] = combined.y();
                                q["width"] = combined.width();
                                q["height"] = combined.height();
                                quads.append(q);
                            }
                        }

                        // If no valid quads found, add a placeholder
                        if (quads.isEmpty())
                        {
                            QJsonObject q;
                            q["x"] = 0;
                            q["y"] = 0;
                            q["width"] = 100;
                            q["height"] = 20;
                            quads.append(q);
                        }

                        match["quadrilaterals"] = quads;
                        matches.append(match);
                    }

                    result["ok"] = true;
                    result["matches"] = matches;
                    return result;
                }
                return result;
            };

            context.extractTextCallback = [textLayoutCompiler, this](int pageIndex) -> QJsonObject
            {
                QJsonObject result;
                if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
                {
                    result["ok"] = false;
                    result["error"] = "Invalid page index";
                    return result;
                }

                pdf::PDFTextLayout textLayout = textLayoutCompiler->getTextLayout(pageIndex);
                pdf::PDFTextFlows textFlows = pdf::PDFTextFlow::createTextFlows(textLayout, pdf::PDFTextFlow::RemoveSoftHyphen, pageIndex);

                QString fullText;
                for (const pdf::PDFTextFlow& textFlow : textFlows)
                {
                    fullText += textFlow.getText() + "\n";
                }

                result["ok"] = true;
                result["text"] = fullText.trimmed();
                return result;
            };
        }
    }

    // Set up callback for creating highlight annotations
    context.createHighlightCallback = [this](int pageIndex, const QPolygonF& quadrilaterals, const QColor& color, const QString& contents) -> QJsonObject {
        return createHighlightAnnotation(pageIndex, quadrilaterals, color, contents);
    };

    // Set up callback for creating text comments
    context.createTextAnnotationCallback = [this](int pageIndex, const QPointF& position, const QString& contents, const QString& author) -> QJsonObject {
        return createTextAnnotation(pageIndex, position, contents, author);
    };

    return context;
}

QJsonObject AgentPlugin::createHighlightAnnotation(int pageIndex, const QPolygonF& quadrilaterals, const QColor& color, const QString& contents) const
{
    if (!m_document)
    {
        QJsonObject result;
        result["ok"] = false;
        result["error"] = "Document not available.";
        return result;
    }

    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        QJsonObject result;
        result["ok"] = false;
        result["error"] = "Invalid page index.";
        return result;
    }

    if (quadrilaterals.isEmpty())
    {
        QJsonObject result;
        result["ok"] = false;
        result["error"] = "No quadrilaterals provided.";
        return result;
    }

    // Remove duplicate last point if first and last points are the same (QPolygonF is closed)
    // Use fuzzy compare for floating point comparison
    QPolygonF cleanQuads = quadrilaterals;
    if (cleanQuads.size() > 1)
    {
        const QPointF& first = cleanQuads.first();
        const QPointF& last = cleanQuads.last();
        if (qFuzzyCompare(first.x(), last.x()) && qFuzzyCompare(first.y(), last.y()))
        {
            cleanQuads.removeLast();
        }
    }

    // Create highlight annotation using PDFDocumentModifier
    pdf::PDFDocumentModifier modifier(m_document);
    pdf::PDFObjectReference page = m_document->getCatalog()->getPage(pageIndex)->getPageReference();

    // Create the highlight annotation
    pdf::PDFObjectReference annotationRef = modifier.getBuilder()->createAnnotationHighlight(
        page, cleanQuads, color);

    if (!annotationRef.isValid())
    {
        QJsonObject result;
        result["ok"] = false;
        result["error"] = "Failed to create highlight annotation.";
        return result;
    }

    // Set opacity
    modifier.getBuilder()->setAnnotationOpacity(annotationRef, 0.2);

    // Update appearance streams
    modifier.getBuilder()->updateAnnotationAppearanceStreams(annotationRef);

    // Mark annotations as changed
    modifier.markAnnotationsChanged();

    // Finalize the modification
    if (modifier.finalize())
    {
        Q_EMIT m_widget->getToolManager()->documentModified(pdf::PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));

        pdf::PDFDocument document = modifier.getBuilder()->build();
        pdf::PDFDocumentWriter writer(nullptr);
        writer.write("AX_HighlightQuadPoints.pdf", &document, false);

        QJsonObject result;
        result["ok"] = true;
        result["page_index"] = pageIndex;
        result["highlights_created"] = 1;
        result["annotation_ref"] = annotationRef.objectNumber;
        result["message"] = "Highlight annotation created successfully.";
        return result;
    }

    QJsonObject result;
    result["ok"] = false;
    result["error"] = "Failed to finalize document modification.";
    return result;
}

QJsonObject AgentPlugin::createTextAnnotation(int pageIndex, const QPointF& position, const QString& contents, const QString& author) const
{
    if (!m_document)
    {
        QJsonObject result;
        result["ok"] = false;
        result["error"] = "Document not available.";
        return result;
    }

    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        QJsonObject result;
        result["ok"] = false;
        result["error"] = "Invalid page index.";
        return result;
    }

    if (contents.isEmpty())
    {
        QJsonObject result;
        result["ok"] = false;
        result["error"] = "Comment text cannot be empty.";
        return result;
    }

    // Create text annotation (sticky note) using PDFDocumentModifier
    pdf::PDFDocumentModifier modifier(m_document);
    pdf::PDFObjectReference page = m_document->getCatalog()->getPage(pageIndex)->getPageReference();

    // Use default icon position (center of page if no position specified)
    QPointF iconPosition = position;
    if (iconPosition.isNull())
    {
        // Default to center of page
        if (const pdf::PDFPage* pdfPage = m_document->getCatalog()->getPage(pageIndex))
        {
            QRectF mediaBox = pdfPage->getMediaBox();
            iconPosition = mediaBox.center();
        }
    }

    // Create the text annotation (comment)
    // Use Comment icon by default
    pdf::PDFObjectReference annotationRef = modifier.getBuilder()->createAnnotationText(
        page, QRectF(iconPosition, QSizeF(24, 24)),
        pdf::TextAnnotationIcon::Comment,
        author,
        QString(),  // subject
        contents,   // contents
        false);    // not open

    if (!annotationRef.isValid())
    {
        QJsonObject result;
        result["ok"] = false;
        result["error"] = "Failed to create text annotation.";
        return result;
    }

    // Mark annotations as changed
    modifier.markAnnotationsChanged();

    // Finalize the modification
    if (modifier.finalize())
    {
        Q_EMIT m_widget->getToolManager()->documentModified(pdf::PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));

        QJsonObject result;
        result["ok"] = true;
        result["page_index"] = pageIndex;
        result["annotation_ref"] = annotationRef.objectNumber;
        result["message"] = "Text comment created successfully.";
        return result;
    }

    QJsonObject result;
    result["ok"] = false;
    result["error"] = "Failed to finalize document modification.";
    return result;
}

void AgentPlugin::onAgentResponseReady(const pdf::PDFAgentLlmResponse& response) const
{
    if (!m_chatDockWidget)
    {
        return;
    }

    m_chatDockWidget->setBusy(false);

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
}

void AgentPlugin::onToolCallStarted(const QString& toolName) const
{
    if (!m_chatDockWidget)
    {
        return;
    }

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
        submitConfirmationResult({true, "Approved by user"});
    }
    else
    {
        submitConfirmationResult({false, "Rejected by user"});
    }
}

void AgentPlugin::submitConfirmationResult(const pdf::PDFAgentConfirmationResult& result)
{
    // Directly call the orchestrator method since we're in the same thread
    m_orchestrator->submitConfirmationResult(result);
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
    updateContextState();
}

pdf::PDFAgentLlmConfig AgentPlugin::loadConfig() const
{
    pdf::PDFAgentLlmConfig config;
    config.endpoint = m_settings.endpoint;
    config.model = m_settings.model;
    config.apiKey = m_settings.apiKey;
    config.systemPrompt = m_settings.systemPrompt;
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

void AgentPlugin::onOpenSettings()
{
    PdfAgentSettingsDialog dialog(m_dataExchangeInterface ? m_dataExchangeInterface->getMainWindow() : nullptr);
    dialog.setSettings(m_settings);

    connect(&dialog, &PdfAgentSettingsDialog::settingsApplied, this, &AgentPlugin::applySettings);

    dialog.exec();
}

}   // namespace pdfplugin
