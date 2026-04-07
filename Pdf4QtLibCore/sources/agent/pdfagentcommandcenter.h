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

#ifndef PDFAGENTCOMMANDCENTER_H
#define PDFAGENTCOMMANDCENTER_H

#include "pdfglobal.h"

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QtWidgets/QMainWindow>

namespace pdf
{

class PDFDocument;
class PDFWidget;

class PDF4QTLIBCORESHARED_EXPORT PDFAgentCommandCenter
{
public:
    virtual ~PDFAgentCommandCenter() = default;

    virtual void setRuntime(PDFDocument* document, PDFWidget* widget, QMainWindow* mainWindow, QObject* actionHost) = 0;

    [[nodiscard]] virtual bool canExtractText() const = 0;
    [[nodiscard]] virtual bool canSearchText() const = 0;
    [[nodiscard]] virtual bool canModifyDocument() const = 0;
    [[nodiscard]] virtual bool canNavigate() const = 0;
    [[nodiscard]] virtual bool canTriggerActions() const = 0;

    virtual QJsonObject extractPageText(int pageIndex) const = 0;
    virtual QJsonObject searchText(int pageIndex, const QString& searchText) const = 0;
    virtual QImage renderPageImage(int pageIndex, int pixelSize) const = 0;
    virtual QJsonObject goToPage(int pageIndex) const = 0;
    virtual QJsonObject focusRectOnPage(int pageIndex, const QRectF& rect) const = 0;
    virtual QJsonObject createRectangleAnnotation(int pageIndex,
                                                  const QRectF& rect,
                                                  const QColor& strokeColor,
                                                  const QColor& fillColor,
                                                  double penWidth) const = 0;
    virtual QJsonObject createEllipseAnnotation(int pageIndex,
                                                const QRectF& rect,
                                                const QColor& strokeColor,
                                                const QColor& fillColor,
                                                double penWidth) const = 0;
    virtual QJsonObject createFreeTextAnnotation(int pageIndex,
                                                 const QRectF& rect,
                                                 const QString& text,
                                                 const QColor& textColor,
                                                 double fontSize) const = 0;
    virtual QJsonObject createHighlight(int pageIndex,
                                        const QPolygonF& quadrilaterals,
                                        const QColor& color,
                                        const QString& contents) const = 0;
    virtual QJsonObject createTextAnnotation(int pageIndex,
                                             const QPointF& position,
                                             const QString& contents,
                                             const QString& author) const = 0;
    virtual QJsonObject triggerAction(const QString& actionObjectName) const = 0;
};

}   // namespace pdf

#endif // PDFAGENTCOMMANDCENTER_H
