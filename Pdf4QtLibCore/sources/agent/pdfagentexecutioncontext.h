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

#ifndef PDFAGENTEXECUTIONCONTEXT_H
#define PDFAGENTEXECUTIONCONTEXT_H

#include "pdfglobal.h"

#include <QString>
#include <QtWidgets/QMainWindow>
#include <QJsonObject>
#include <QColor>
#include <QPolygonF>
#include <QPointF>
#include <functional>

namespace pdf
{

class PDFDocument;
class PDFWidget;
class PDFTextSelection;
class PDFTextLayout;

// Callback types for document modifications
using SearchTextCallback = std::function<QJsonObject(int pageIndex, const QString& text)>;
using ExtractTextCallback = std::function<QJsonObject(int pageIndex)>;
using CreateHighlightCallback = std::function<QJsonObject(int pageIndex, const QPolygonF& quadrilaterals, const QColor& color, const QString& contents)>;
using CreateTextAnnotationCallback = std::function<QJsonObject(int pageIndex, const QPointF& position, const QString& contents, const QString& author)>;

class PDF4QTLIBCORESHARED_EXPORT PDFAgentExecutionContext
{
public:
    explicit PDFAgentExecutionContext();

    // Document access
    PDFDocument* document = nullptr;
    PDFWidget* widget = nullptr;
    QMainWindow* mainWindow = nullptr;

    // Document info
    QString originalFileName;
    int currentPage = -1;
    int pageCount = 0;

    // Selection info
    QString selectedText;
    const PDFTextSelection* textSelection = nullptr;

    // Callbacks for document modifications
    SearchTextCallback searchTextCallback;
    ExtractTextCallback extractTextCallback;
    CreateHighlightCallback createHighlightCallback;
    CreateTextAnnotationCallback createTextAnnotationCallback;

    // Helpers
    [[nodiscard]] bool hasDocument() const { return document != nullptr; }
    [[nodiscard]] bool hasSelectedText() const { return !selectedText.isEmpty(); }
    [[nodiscard]] bool canModifyDocument() const { return searchTextCallback && createHighlightCallback; }
};

}   // namespace pdf

#endif // PDFAGENTEXECUTIONCONTEXT_H
