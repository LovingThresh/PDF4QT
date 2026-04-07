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

#ifndef PDFAGENTWIDGETCOMMANDCENTER_H
#define PDFAGENTWIDGETCOMMANDCENTER_H

#include "pdfdocument.h"
#include "pdfwidgetsglobal.h"
#include "agent/pdfagentcommandcenter.h"

namespace pdf
{

class PDF4QTLIBWIDGETSSHARED_EXPORT PDFAgentWidgetCommandCenter final : public PDFAgentCommandCenter
{
public:
    PDFAgentWidgetCommandCenter() = default;

    virtual void setRuntime(PDFDocument* document, PDFWidget* widget, QMainWindow* mainWindow, QObject* actionHost) override;

    [[nodiscard]] virtual bool canExtractText() const override;
    [[nodiscard]] virtual bool canSearchText() const override;
    [[nodiscard]] virtual bool canModifyDocument() const override;
    [[nodiscard]] virtual bool canNavigate() const override;
    [[nodiscard]] virtual bool canTriggerActions() const override;

    virtual QJsonObject extractPageText(int pageIndex) const override;
    virtual QJsonObject searchText(int pageIndex, const QString& searchText) const override;
    virtual QImage renderPageImage(int pageIndex, int pixelSize) const override;
    virtual QJsonObject goToPage(int pageIndex) const override;
    virtual QJsonObject focusRectOnPage(int pageIndex, const QRectF& rect) const override;
    virtual QJsonObject createRectangleAnnotation(int pageIndex,
                                                  const QRectF& rect,
                                                  const QColor& strokeColor,
                                                  const QColor& fillColor,
                                                  double penWidth) const override;
    virtual QJsonObject createEllipseAnnotation(int pageIndex,
                                                const QRectF& rect,
                                                const QColor& strokeColor,
                                                const QColor& fillColor,
                                                double penWidth) const override;
    virtual QJsonObject createFreeTextAnnotation(int pageIndex,
                                                 const QRectF& rect,
                                                 const QString& text,
                                                 const QColor& textColor,
                                                 double fontSize) const override;
    virtual QJsonObject createHighlight(int pageIndex,
                                        const QPolygonF& quadrilaterals,
                                        const QColor& color,
                                        const QString& contents) const override;
    virtual QJsonObject createTextAnnotation(int pageIndex,
                                             const QPointF& position,
                                             const QString& contents,
                                             const QString& author) const override;
    virtual QJsonObject triggerAction(const QString& actionObjectName) const override;

private:
    mutable PDFDocumentPointer m_documentPointer;
    mutable PDFDocument* m_document = nullptr;
    PDFWidget* m_widget = nullptr;
    QMainWindow* m_mainWindow = nullptr;
    QObject* m_actionHost = nullptr;
};

}   // namespace pdf

#endif // PDFAGENTWIDGETCOMMANDCENTER_H
