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

#include "pdfagentwidgetcommandcenter.h"

#include "pdfcompiler.h"
#include "pdfdocumentbuilder.h"
#include "pdfdrawwidget.h"
#include "pdftextlayout.h"
#include "pdfwidgettool.h"

#include <QAction>
#include <QJsonArray>
#include <QMainWindow>

namespace pdf
{

static QJsonObject createErrorResponse(const QString& error)
{
    QJsonObject result;
    result["ok"] = false;
    result["error"] = error;
    return result;
}

static QJsonObject createSuccessResponse(const QJsonObject& data = QJsonObject())
{
    QJsonObject result;
    result["ok"] = true;
    for (auto it = data.constBegin(); it != data.constEnd(); ++it)
    {
        result[it.key()] = it.value();
    }
    return result;
}

void PDFAgentWidgetCommandCenter::setRuntime(PDFDocument* document, PDFWidget* widget, QMainWindow* mainWindow, QObject* actionHost)
{
    m_documentPointer.clear();
    m_document = document;
    m_widget = widget;
    m_mainWindow = mainWindow;
    m_actionHost = actionHost;
}

bool PDFAgentWidgetCommandCenter::canExtractText() const
{
    return m_document && m_widget && m_widget->getDrawWidgetProxy() &&
           m_widget->getDrawWidgetProxy()->getTextLayoutCompiler();
}

bool PDFAgentWidgetCommandCenter::canSearchText() const
{
    return canExtractText();
}

bool PDFAgentWidgetCommandCenter::canModifyDocument() const
{
    return m_document && m_widget && m_widget->getToolManager();
}

bool PDFAgentWidgetCommandCenter::canNavigate() const
{
    return m_document && m_widget && m_widget->getDrawWidgetProxy();
}

bool PDFAgentWidgetCommandCenter::canTriggerActions() const
{
    return m_actionHost || m_mainWindow;
}

static QJsonObject finalizeAnnotationModification(PDFDocumentModifier& modifier,
                                                  PDFDocumentPointer& documentPointer,
                                                  PDFDocument*& document,
                                                  PDFWidget* widget)
{
    if (!modifier.finalize())
    {
        return createErrorResponse("Failed to finalize document modification.");
    }

    documentPointer = modifier.getDocument();
    document = documentPointer.data();
    Q_EMIT widget->getToolManager()->documentModified(PDFModifiedDocument(documentPointer, nullptr, modifier.getFlags()));
    return QJsonObject();
}

QJsonObject PDFAgentWidgetCommandCenter::extractPageText(int pageIndex) const
{
    if (!canExtractText())
    {
        return createErrorResponse("Text extraction is not available. Widget must be loaded first.");
    }

    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index");
    }

    PDFAsynchronousTextLayoutCompiler* textLayoutCompiler = m_widget->getDrawWidgetProxy()->getTextLayoutCompiler();
    PDFTextLayout textLayout = textLayoutCompiler->getTextLayout(pageIndex);
    PDFTextFlows textFlows = PDFTextFlow::createTextFlows(textLayout, PDFTextFlow::RemoveSoftHyphen, pageIndex);

    QString fullText;
    for (const PDFTextFlow& textFlow : textFlows)
    {
        fullText += textFlow.getText() + "\n";
    }

    return createSuccessResponse(QJsonObject{{"text", fullText.trimmed()}});
}

QJsonObject PDFAgentWidgetCommandCenter::searchText(int pageIndex, const QString& searchText) const
{
    if (!canSearchText())
    {
        return createErrorResponse("Text search is not available. Widget must be loaded first.");
    }

    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index");
    }

    PDFAsynchronousTextLayoutCompiler* textLayoutCompiler = m_widget->getDrawWidgetProxy()->getTextLayoutCompiler();
    PDFTextLayout textLayout = textLayoutCompiler->getTextLayout(pageIndex);
    if (textLayout.getTextBlocks().empty())
    {
        return createSuccessResponse(QJsonObject{{"matches", QJsonArray()}});
    }

    PDFTextFlows textFlows = PDFTextFlow::createTextFlows(textLayout, PDFTextFlow::RemoveSoftHyphen, pageIndex);

    QJsonArray matches;
    for (const PDFTextFlow& textFlow : textFlows)
    {
        PDFFindResults findResults = textFlow.find(searchText, Qt::CaseInsensitive);

        for (const PDFFindResult& fr : findResults)
        {
            QJsonObject match;
            match["page_index"] = pageIndex;
            match["page_number"] = pageIndex + 1;
            match["matched_text"] = fr.matched;
            match["context"] = fr.context;

            QJsonArray quads;
            if (!fr.boundingBoxes.empty())
            {
                QRectF combined;
                for (const QRectF& b : fr.boundingBoxes)
                {
                    if (!b.isNull())
                    {
                        combined = combined.isNull() ? b : combined.united(b);
                    }
                }

                if (!combined.isNull())
                {
                    quads.append(QJsonObject{
                        {"x", combined.x()},
                        {"y", combined.y()},
                        {"width", combined.width()},
                        {"height", combined.height()}
                    });
                }
            }

            if (quads.isEmpty())
            {
                quads.append(QJsonObject{
                    {"x", 0},
                    {"y", 0},
                    {"width", 100},
                    {"height", 20}
                });
            }

            match["quadrilaterals"] = quads;
            matches.append(match);
        }
    }

    return createSuccessResponse(QJsonObject{{"matches", matches}});
}

QJsonObject PDFAgentWidgetCommandCenter::goToPage(int pageIndex) const
{
    if (!canNavigate())
    {
        return createErrorResponse("Navigation is not available.");
    }

    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index.");
    }

    m_widget->getDrawWidgetProxy()->goToPage(pageIndex);
    m_widget->setFocus();

    return createSuccessResponse(QJsonObject{
        {"page_index", pageIndex},
        {"page_number", pageIndex + 1},
        {"message", "Navigated to page successfully."}
    });
}

QJsonObject PDFAgentWidgetCommandCenter::focusRectOnPage(int pageIndex, const QRectF& rect) const
{
    if (!canNavigate())
    {
        return createErrorResponse("Navigation is not available.");
    }

    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index.");
    }

    if (rect.isEmpty())
    {
        return createErrorResponse("Focus rectangle must not be empty.");
    }

    m_widget->getDrawWidgetProxy()->goToPageAndEnsureVisible(pageIndex, rect);
    m_widget->setFocus();

    return createSuccessResponse(QJsonObject{
        {"page_index", pageIndex},
        {"page_number", pageIndex + 1},
        {"x", rect.x()},
        {"y", rect.y()},
        {"width", rect.width()},
        {"height", rect.height()},
        {"message", "Focused rectangle on page successfully."}
    });
}

QJsonObject PDFAgentWidgetCommandCenter::createRectangleAnnotation(int pageIndex,
                                                                   const QRectF& rect,
                                                                   const QColor& strokeColor,
                                                                   const QColor& fillColor,
                                                                   double penWidth) const
{
    if (!canModifyDocument())
    {
        return createErrorResponse("Document modification is not available.");
    }
    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index.");
    }
    if (rect.isEmpty())
    {
        return createErrorResponse("Rectangle must not be empty.");
    }

    PDFDocumentModifier modifier(m_document);
    QString userName = PDFSysUtils::getUserName();
    PDFObjectReference page = m_document->getCatalog()->getPage(pageIndex)->getPageReference();
    PDFObjectReference annotation = modifier.getBuilder()->createAnnotationSquare(page, rect, penWidth, fillColor, strokeColor, userName, QString(), QString());
    if (!annotation.isValid())
    {
        return createErrorResponse("Failed to create rectangle annotation.");
    }
    modifier.markAnnotationsChanged();
    QJsonObject finalizeResult = finalizeAnnotationModification(modifier, m_documentPointer, m_document, m_widget);
    if (finalizeResult.contains("error"))
    {
        return finalizeResult;
    }

    return createSuccessResponse(QJsonObject{
        {"page_index", pageIndex},
        {"annotation_ref", annotation.objectNumber},
        {"message", "Rectangle annotation created successfully."}
    });
}

QJsonObject PDFAgentWidgetCommandCenter::createEllipseAnnotation(int pageIndex,
                                                                 const QRectF& rect,
                                                                 const QColor& strokeColor,
                                                                 const QColor& fillColor,
                                                                 double penWidth) const
{
    if (!canModifyDocument())
    {
        return createErrorResponse("Document modification is not available.");
    }
    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index.");
    }
    if (rect.isEmpty())
    {
        return createErrorResponse("Rectangle must not be empty.");
    }

    PDFDocumentModifier modifier(m_document);
    QString userName = PDFSysUtils::getUserName();
    PDFObjectReference page = m_document->getCatalog()->getPage(pageIndex)->getPageReference();
    PDFObjectReference annotation = modifier.getBuilder()->createAnnotationCircle(page, rect, penWidth, fillColor, strokeColor, userName, QString(), QString());
    if (!annotation.isValid())
    {
        return createErrorResponse("Failed to create ellipse annotation.");
    }
    modifier.markAnnotationsChanged();
    QJsonObject finalizeResult = finalizeAnnotationModification(modifier, m_documentPointer, m_document, m_widget);
    if (finalizeResult.contains("error"))
    {
        return finalizeResult;
    }

    return createSuccessResponse(QJsonObject{
        {"page_index", pageIndex},
        {"annotation_ref", annotation.objectNumber},
        {"message", "Ellipse annotation created successfully."}
    });
}

QJsonObject PDFAgentWidgetCommandCenter::createFreeTextAnnotation(int pageIndex,
                                                                  const QRectF& rect,
                                                                  const QString& text,
                                                                  const QColor& textColor,
                                                                  double fontSize) const
{
    if (!canModifyDocument())
    {
        return createErrorResponse("Document modification is not available.");
    }
    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index.");
    }
    if (rect.isEmpty())
    {
        return createErrorResponse("Rectangle must not be empty.");
    }
    if (text.trimmed().isEmpty())
    {
        return createErrorResponse("Text must not be empty.");
    }

    PDFFreeTextStyle style;
    style.fontSize = fontSize;
    style.textColor = textColor;
    style.fontFamily = QStringLiteral("Helvetica");
    style.textAlignment = TextAlignment(Qt::AlignTop | Qt::AlignLeft);

    PDFDocumentModifier modifier(m_document);
    QString userName = PDFSysUtils::getUserName();
    PDFObjectReference page = m_document->getCatalog()->getPage(pageIndex)->getPageReference();
    PDFObjectReference annotation = modifier.getBuilder()->createAnnotationFreeText(page, rect, userName, QString(), text, style, false);
    if (!annotation.isValid())
    {
        return createErrorResponse("Failed to create free text annotation.");
    }
    modifier.markAnnotationsChanged();
    QJsonObject finalizeResult = finalizeAnnotationModification(modifier, m_documentPointer, m_document, m_widget);
    if (finalizeResult.contains("error"))
    {
        return finalizeResult;
    }

    return createSuccessResponse(QJsonObject{
        {"page_index", pageIndex},
        {"annotation_ref", annotation.objectNumber},
        {"message", "Free text annotation created successfully."}
    });
}

QJsonObject PDFAgentWidgetCommandCenter::createHighlight(int pageIndex,
                                                         const QPolygonF& quadrilaterals,
                                                         const QColor& color,
                                                         const QString& contents) const
{
    Q_UNUSED(contents);

    if (!canModifyDocument())
    {
        return createErrorResponse("Document modification is not available.");
    }

    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index.");
    }

    if (quadrilaterals.isEmpty())
    {
        return createErrorResponse("No quadrilaterals provided.");
    }

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

    PDFDocumentModifier modifier(m_document);
    PDFObjectReference page = m_document->getCatalog()->getPage(pageIndex)->getPageReference();
    PDFObjectReference annotationRef = modifier.getBuilder()->createAnnotationHighlight(page, cleanQuads, color);

    if (!annotationRef.isValid())
    {
        return createErrorResponse("Failed to create highlight annotation.");
    }

    modifier.getBuilder()->setAnnotationOpacity(annotationRef, 0.2);
    modifier.getBuilder()->updateAnnotationAppearanceStreams(annotationRef);
    modifier.markAnnotationsChanged();
    QJsonObject finalizeResult = finalizeAnnotationModification(modifier, m_documentPointer, m_document, m_widget);
    if (finalizeResult.contains("error"))
    {
        return finalizeResult;
    }

    return createSuccessResponse(QJsonObject{
        {"page_index", pageIndex},
        {"highlights_created", 1},
        {"annotation_ref", annotationRef.objectNumber},
        {"message", "Highlight annotation created successfully."}
    });
}

QJsonObject PDFAgentWidgetCommandCenter::createTextAnnotation(int pageIndex,
                                                              const QPointF& position,
                                                              const QString& contents,
                                                              const QString& author) const
{
    if (!canModifyDocument())
    {
        return createErrorResponse("Document modification is not available.");
    }

    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_document->getCatalog()->getPageCount()))
    {
        return createErrorResponse("Invalid page index.");
    }

    if (contents.isEmpty())
    {
        return createErrorResponse("Comment text cannot be empty.");
    }

    PDFDocumentModifier modifier(m_document);
    PDFObjectReference page = m_document->getCatalog()->getPage(pageIndex)->getPageReference();

    QPointF iconPosition = position;
    if (iconPosition.isNull())
    {
        if (const PDFPage* pdfPage = m_document->getCatalog()->getPage(pageIndex))
        {
            iconPosition = pdfPage->getMediaBox().center();
        }
    }

    PDFObjectReference annotationRef = modifier.getBuilder()->createAnnotationText(
        page,
        QRectF(iconPosition, QSizeF(24, 24)),
        TextAnnotationIcon::Comment,
        author,
        QString(),
        contents,
        false);

    if (!annotationRef.isValid())
    {
        return createErrorResponse("Failed to create text annotation.");
    }

    modifier.markAnnotationsChanged();
    QJsonObject finalizeResult = finalizeAnnotationModification(modifier, m_documentPointer, m_document, m_widget);
    if (finalizeResult.contains("error"))
    {
        return finalizeResult;
    }

    return createSuccessResponse(QJsonObject{
        {"page_index", pageIndex},
        {"annotation_ref", annotationRef.objectNumber},
        {"message", "Text comment created successfully."}
    });
}

QJsonObject PDFAgentWidgetCommandCenter::triggerAction(const QString& actionObjectName) const
{
    if (!canTriggerActions())
    {
        return createErrorResponse("GUI actions are not available.");
    }

    QAction* action = nullptr;
    if (m_actionHost)
    {
        action = m_actionHost->findChild<QAction*>(actionObjectName, Qt::FindChildrenRecursively);
    }
    if (!action && m_mainWindow)
    {
        action = m_mainWindow->findChild<QAction*>(actionObjectName, Qt::FindChildrenRecursively);
    }
    if (!action)
    {
        return createErrorResponse(QString("Action not found: %1").arg(actionObjectName));
    }
    if (!action->isEnabled())
    {
        return createErrorResponse(QString("Action is disabled: %1").arg(actionObjectName));
    }

    action->trigger();
    return createSuccessResponse(QJsonObject{
        {"action", actionObjectName},
        {"message", "Action triggered successfully."}
    });
}

}   // namespace pdf
