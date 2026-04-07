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

namespace pdf
{

class PDFDocument;
class PDFWidget;
class PDFTextSelection;
class PDFAgentCommandCenter;
class PDFAgentTodoManager;

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
    QString agentTempDirectory;
    QString preferredImageFormat = "png";
    int preferredImageMaxPixelSize = 1536;

    // Selection info
    QString selectedText;
    const PDFTextSelection* textSelection = nullptr;

    // Command entry point for document-aware operations
    const PDFAgentCommandCenter* commandCenter = nullptr;
    PDFAgentTodoManager* todoManager = nullptr;

    // Helpers
    [[nodiscard]] bool hasDocument() const { return document != nullptr; }
    [[nodiscard]] bool hasSelectedText() const { return !selectedText.isEmpty(); }
    [[nodiscard]] bool hasCommandCenter() const { return commandCenter != nullptr; }
    [[nodiscard]] bool hasTodoManager() const { return todoManager != nullptr; }
    [[nodiscard]] bool canModifyDocument() const;
};

}   // namespace pdf

#endif // PDFAGENTEXECUTIONCONTEXT_H
