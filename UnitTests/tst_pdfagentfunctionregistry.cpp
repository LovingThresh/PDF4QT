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

#include <QtTest>

#include <QPolygonF>
#include <QColor>

#include "agent/pdfagentcommandcenter.h"
#include "agent/pdfagentfunctionregistry.h"
#include "agent/pdfagentexecutioncontext.h"
#include "agent/pdfagenttypes.h"

class MockAgentCommandCenter final : public pdf::PDFAgentCommandCenter
{
public:
    std::function<QJsonObject(int)> extractHandler;
    std::function<QJsonObject(int, const QString&)> searchHandler;
    std::function<QJsonObject(int)> goToPageHandler;
    std::function<QJsonObject(int, const QRectF&)> focusRectHandler;
    std::function<QJsonObject(int, const QPolygonF&, const QColor&, const QString&)> highlightHandler;
    std::function<QJsonObject(int, const QPointF&, const QString&, const QString&)> textAnnotationHandler;

    virtual void setRuntime(pdf::PDFDocument* document, pdf::PDFWidget* widget, QMainWindow* mainWindow, QObject* actionHost) override
    {
        Q_UNUSED(document);
        Q_UNUSED(widget);
        Q_UNUSED(mainWindow);
        Q_UNUSED(actionHost);
    }

    [[nodiscard]] virtual bool canExtractText() const override
    {
        return static_cast<bool>(extractHandler);
    }

    [[nodiscard]] virtual bool canSearchText() const override
    {
        return static_cast<bool>(searchHandler);
    }

    [[nodiscard]] virtual bool canModifyDocument() const override
    {
        return static_cast<bool>(highlightHandler) || static_cast<bool>(textAnnotationHandler);
    }

    [[nodiscard]] virtual bool canNavigate() const override
    {
        return static_cast<bool>(goToPageHandler) || static_cast<bool>(focusRectHandler);
    }

    [[nodiscard]] virtual bool canTriggerActions() const override
    {
        return false;
    }

    virtual QJsonObject extractPageText(int pageIndex) const override
    {
        return extractHandler ? extractHandler(pageIndex)
                              : QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QJsonObject searchText(int pageIndex, const QString& searchText) const override
    {
        return searchHandler ? searchHandler(pageIndex, searchText)
                             : QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QImage renderPageImage(int pageIndex, int pixelSize) const override
    {
        Q_UNUSED(pageIndex);
        Q_UNUSED(pixelSize);
        return QImage();
    }

    virtual QJsonObject goToPage(int pageIndex) const override
    {
        return goToPageHandler ? goToPageHandler(pageIndex)
                               : QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QJsonObject focusRectOnPage(int pageIndex, const QRectF& rect) const override
    {
        return focusRectHandler ? focusRectHandler(pageIndex, rect)
                                : QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QJsonObject createRectangleAnnotation(int pageIndex,
                                                  const QRectF& rect,
                                                  const QColor& strokeColor,
                                                  const QColor& fillColor,
                                                  double penWidth) const override
    {
        Q_UNUSED(pageIndex);
        Q_UNUSED(rect);
        Q_UNUSED(strokeColor);
        Q_UNUSED(fillColor);
        Q_UNUSED(penWidth);
        return QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QJsonObject createEllipseAnnotation(int pageIndex,
                                                const QRectF& rect,
                                                const QColor& strokeColor,
                                                const QColor& fillColor,
                                                double penWidth) const override
    {
        Q_UNUSED(pageIndex);
        Q_UNUSED(rect);
        Q_UNUSED(strokeColor);
        Q_UNUSED(fillColor);
        Q_UNUSED(penWidth);
        return QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QJsonObject createFreeTextAnnotation(int pageIndex,
                                                 const QRectF& rect,
                                                 const QString& text,
                                                 const QColor& textColor,
                                                 double fontSize) const override
    {
        Q_UNUSED(pageIndex);
        Q_UNUSED(rect);
        Q_UNUSED(text);
        Q_UNUSED(textColor);
        Q_UNUSED(fontSize);
        return QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QJsonObject createHighlight(int pageIndex,
                                        const QPolygonF& quadrilaterals,
                                        const QColor& color,
                                        const QString& contents) const override
    {
        return highlightHandler ? highlightHandler(pageIndex, quadrilaterals, color, contents)
                                : QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QJsonObject createTextAnnotation(int pageIndex,
                                             const QPointF& position,
                                             const QString& contents,
                                             const QString& author) const override
    {
        return textAnnotationHandler ? textAnnotationHandler(pageIndex, position, contents, author)
                                     : QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }

    virtual QJsonObject triggerAction(const QString& actionObjectName) const override
    {
        Q_UNUSED(actionObjectName);
        return QJsonObject{{"ok", false}, {"error", "unsupported"}};
    }
};

class PDFAgentFunctionRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void test_builtinCommandsRegistered();
    void test_getCommandNames();
    void test_contains();
    void test_getToolsSchema();
    void test_executeUnknownCommand();
    void test_executeCommandWithoutDocument();
    void test_executeGetDocumentSummary();
    void test_executeGetCurrentPage();
    void test_executeExtractSelectedTextNoSelection();
    void test_executeExtractPageText();
    void test_executeGetPageCount();
    void test_executeGoToPageWithoutCommandCenter();
    void test_executeGoToPageWithCommandCenter();
    void test_executeFocusRectOnPageWithCommandCenter();
    void test_executeTodoWrite();
    void test_executeSearchTextWithoutCallback();
    void test_executeSearchTextMissingText();
    void test_executeCreateHighlightWithoutCallback();
    void test_executeAddTextCommentWithoutCallback();
    static void test_executeAddTextCommentMissingText();

    // Normal path tests with callbacks
    void test_executeSearchTextWithCallback();
    void test_executeCreateHighlightWithCallback();
    void test_executeAddTextCommentWithCallback();
};

void PDFAgentFunctionRegistryTest::test_builtinCommandsRegistered()
{
    pdf::PdfFunctionRegistry registry;

    // Should have at least 5 built-in commands
    QStringList commands = registry.getCommandNames();
    QVERIFY(commands.size() >= 5);
}

void PDFAgentFunctionRegistryTest::test_getCommandNames()
{
    pdf::PdfFunctionRegistry registry;

    QStringList commands = registry.getCommandNames();

    QVERIFY(commands.contains("get_document_summary"));
    QVERIFY(commands.contains("get_current_page"));
    QVERIFY(commands.contains("extract_selected_text"));
    QVERIFY(commands.contains("extract_page_text"));
    QVERIFY(commands.contains("get_page_count"));
    QVERIFY(commands.contains("go_to_page"));
    QVERIFY(commands.contains("focus_rect_on_page"));
    QVERIFY(commands.contains("todo_write"));
}

void PDFAgentFunctionRegistryTest::test_contains()
{
    pdf::PdfFunctionRegistry registry;

    QVERIFY(registry.contains("get_document_summary"));
    QVERIFY(registry.contains("extract_selected_text"));
    QVERIFY(!registry.contains("nonexistent_command"));
}

void PDFAgentFunctionRegistryTest::test_getToolsSchema()
{
    pdf::PdfFunctionRegistry registry;

    QJsonArray schema = registry.getToolsSchema();

    // Should have same number of tools as commands
    QVERIFY(schema.size() == registry.getCommandNames().size());

    // Check structure of first tool
    QJsonObject firstTool = schema[0].toObject();
    QVERIFY(firstTool.contains("type"));
    QVERIFY(firstTool.contains("function"));

    QJsonObject function = firstTool["function"].toObject();
    QVERIFY(function.contains("name"));
    QVERIFY(function.contains("description"));
    QVERIFY(function.contains("parameters"));
}

void PDFAgentFunctionRegistryTest::test_executeUnknownCommand()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    QJsonObject args;

    QJsonObject result = registry.executeCommand("nonexistent_command", args, context);

    QVERIFY(!result["ok"].toBool());
    QVERIFY(result.contains("error"));
    QVERIFY(result["error"].toString().contains("Unknown command"));
}

void PDFAgentFunctionRegistryTest::test_executeCommandWithoutDocument()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = nullptr; // No document

    QJsonObject args;

    // Commands requiring document should fail without one
    QJsonObject result = registry.executeCommand("get_document_summary", args, context);

    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("active document"));
}

void PDFAgentFunctionRegistryTest::test_executeGetDocumentSummary()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1); // Non-null pointer to simulate document
    context.originalFileName = "test.pdf";
    context.pageCount = 10;
    context.currentPage = 2;

    QJsonObject args;
    QJsonObject result = registry.executeCommand("get_document_summary", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["file_name"].toString() == "test.pdf");
    QVERIFY(result["page_count"].toInt() == 10);
    QVERIFY(result["current_page"].toInt() == 2);
    QVERIFY(result["has_selected_text"].toBool() == false);
}

void PDFAgentFunctionRegistryTest::test_executeGetCurrentPage()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.currentPage = 3;
    context.pageCount = 10;

    QJsonObject args;
    QJsonObject result = registry.executeCommand("get_current_page", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["page_index"].toInt() == 3);
    QVERIFY(result["page_number"].toInt() == 4); // 1-based
}

void PDFAgentFunctionRegistryTest::test_executeExtractSelectedTextNoSelection()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.selectedText = ""; // No selection

    QJsonObject args;
    QJsonObject result = registry.executeCommand("extract_selected_text", args, context);

    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("No text is currently selected"));
}

void PDFAgentFunctionRegistryTest::test_executeExtractPageText()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;
    MockAgentCommandCenter commandCenter;
    commandCenter.extractHandler = [](int pageIndex) -> QJsonObject {
        return QJsonObject{
            {"ok", true},
            {"page_index", pageIndex},
            {"page_number", pageIndex + 1},
            {"text", QString("page %1").arg(pageIndex)}
        };
    };
    context.commandCenter = &commandCenter;

    // Valid page
    QJsonObject args;
    args["page"] = 5;
    QJsonObject result = registry.executeCommand("extract_page_text", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["page_index"].toInt() == 5);
    QVERIFY(result["page_number"].toInt() == 6);

    // Invalid page - out of range
    args["page"] = 15;
    result = registry.executeCommand("extract_page_text", args, context);

    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("page"));

    // Missing page parameter
    context.currentPage = 2;
    args = QJsonObject();
    result = registry.executeCommand("extract_page_text", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["page_index"].toInt() == 2);
}

void PDFAgentFunctionRegistryTest::test_executeGetPageCount()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 25;

    QJsonObject args;
    QJsonObject result = registry.executeCommand("get_page_count", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["page_count"].toInt() == 25);
}

void PDFAgentFunctionRegistryTest::test_executeGoToPageWithoutCommandCenter()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;

    QJsonObject args;
    args["page"] = 3;

    QJsonObject result = registry.executeCommand("go_to_page", args, context);

    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("Navigation"));
}

void PDFAgentFunctionRegistryTest::test_executeGoToPageWithCommandCenter()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;
    MockAgentCommandCenter commandCenter;
    commandCenter.goToPageHandler = [](int pageIndex) -> QJsonObject {
        return QJsonObject{
            {"ok", true},
            {"page_index", pageIndex},
            {"page_number", pageIndex + 1}
        };
    };
    context.commandCenter = &commandCenter;

    QJsonObject args;
    args["page"] = 3;

    QJsonObject result = registry.executeCommand("go_to_page", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["page_index"].toInt() == 3);
    QVERIFY(result["page_number"].toInt() == 4);
}

void PDFAgentFunctionRegistryTest::test_executeFocusRectOnPageWithCommandCenter()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;
    MockAgentCommandCenter commandCenter;
    commandCenter.focusRectHandler = [](int pageIndex, const QRectF& rect) -> QJsonObject {
        return QJsonObject{
            {"ok", true},
            {"page_index", pageIndex},
            {"width", rect.width()}
        };
    };
    context.commandCenter = &commandCenter;

    QJsonObject args;
    args["page_index"] = 1;
    args["x"] = 10;
    args["y"] = 20;
    args["width"] = 100;
    args["height"] = 30;

    QJsonObject result = registry.executeCommand("focus_rect_on_page", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["page_index"].toInt() == 1);
    QVERIFY(result["width"].toDouble() == 100.0);
}

void PDFAgentFunctionRegistryTest::test_executeTodoWrite()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    pdf::PDFAgentTodoManager todoManager;
    context.todoManager = &todoManager;

    QJsonArray items;
    items.append(QJsonObject{{"id", "1"}, {"text", "Inspect current page"}, {"status", "completed"}});
    items.append(QJsonObject{{"id", "2"}, {"text", "Create highlight"}, {"status", "in_progress"}});

    QJsonObject args;
    args["items"] = items;

    QJsonObject result = registry.executeCommand("todo_write", args, context);

    QVERIFY(result["ok"].toBool());
    QCOMPARE(result["total_count"].toInt(), 2);
    QCOMPARE(result["completed_count"].toInt(), 1);
    QVERIFY(result["rendered_text"].toString().contains("[>] #2: Create highlight"));
    QCOMPARE(todoManager.getItems().size(), 2);
}

void PDFAgentFunctionRegistryTest::test_executeSearchTextWithoutCallback()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;
    // Note: searchTextCallback is not set (null)

    QJsonObject args;
    args["text"] = "test";

    QJsonObject result = registry.executeCommand("search_text", args, context);

    // Should fail because search callback is not available
    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("not available"));
}

void PDFAgentFunctionRegistryTest::test_executeSearchTextMissingText()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);

    QJsonObject args; // No "text" parameter

    QJsonObject result = registry.executeCommand("search_text", args, context);

    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("text"));
}

void PDFAgentFunctionRegistryTest::test_executeCreateHighlightWithoutCallback()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;
    // Note: createHighlightCallback is not set (null)

    QJsonObject args;
    args["page_index"] = 0;
    args["quadrilaterals"] = QJsonArray();

    QJsonObject result = registry.executeCommand("create_highlight", args, context);

    // Should fail because highlight callback is not available
    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("not available"));
}

void PDFAgentFunctionRegistryTest::test_executeAddTextCommentWithoutCallback()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;
    // Note: createTextAnnotationCallback is not set (null)

    QJsonObject args;
    args["text"] = "Test comment";

    QJsonObject result = registry.executeCommand("add_text_comment", args, context);

    // Should fail because comment callback is not available
    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("not available"));
}

void PDFAgentFunctionRegistryTest::test_executeAddTextCommentMissingText()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;

    QJsonObject args; // No "text" parameter

    QJsonObject result = registry.executeCommand("add_text_comment", args, context);

    QVERIFY(!result["ok"].toBool());
}

void PDFAgentFunctionRegistryTest::test_executeSearchTextWithCallback()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;

    MockAgentCommandCenter commandCenter;
    commandCenter.searchHandler = [](int pageIndex, const QString& searchText) -> QJsonObject {
        QJsonObject result;
        result["ok"] = true;
        result["page_index"] = pageIndex;

        QJsonArray matches;
        QJsonObject match;
        match["matched_text"] = searchText;
        match["page_number"] = pageIndex + 1;
        matches.append(match);
        result["matches"] = matches;
        return result;
    };
    context.commandCenter = &commandCenter;

    QJsonObject args;
    args["text"] = "test";
    args["page"] = 0;

    QJsonObject result = registry.executeCommand("search_text", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["results"].toArray().size() > 0);
}

void PDFAgentFunctionRegistryTest::test_executeCreateHighlightWithCallback()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;

    MockAgentCommandCenter commandCenter;
    commandCenter.highlightHandler = [](int pageIndex, const QPolygonF& quadrilaterals, const QColor& color, const QString& contents) -> QJsonObject {
        Q_UNUSED(quadrilaterals);
        Q_UNUSED(color);
        Q_UNUSED(contents);
        QJsonObject result;
        result["ok"] = true;
        result["page_index"] = pageIndex;
        result["highlights_created"] = 1;
        return result;
    };
    context.commandCenter = &commandCenter;

    QJsonObject args;
    args["page_index"] = 0;

    QJsonArray quads;
    QJsonObject q;
    q["x"] = 10;
    q["y"] = 10;
    q["width"] = 100;
    q["height"] = 20;
    quads.append(q);
    args["quadrilaterals"] = quads;

    QJsonObject result = registry.executeCommand("create_highlight", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["highlights_created"].toInt() == 1);
}

void PDFAgentFunctionRegistryTest::test_executeAddTextCommentWithCallback()
{
    pdf::PdfFunctionRegistry registry;

    pdf::PDFAgentExecutionContext context;
    context.document = reinterpret_cast<pdf::PDFDocument*>(1);
    context.pageCount = 10;

    MockAgentCommandCenter commandCenter;
    commandCenter.textAnnotationHandler = [](int pageIndex, const QPointF& position, const QString& contents, const QString& author) -> QJsonObject {
        Q_UNUSED(position);
        Q_UNUSED(contents);
        Q_UNUSED(author);
        QJsonObject result;
        result["ok"] = true;
        result["page_index"] = pageIndex;
        result["annotation_ref"] = 1;
        return result;
    };
    context.commandCenter = &commandCenter;

    QJsonObject args;
    args["text"] = "Test comment";
    args["page"] = 0;

    QJsonObject result = registry.executeCommand("add_text_comment", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["annotation_ref"].toInt() == 1);
}

QTEST_MAIN(PDFAgentFunctionRegistryTest)

#include "tst_pdfagentfunctionregistry.moc"
