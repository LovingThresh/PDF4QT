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

#include "agent/pdfagentfunctionregistry.h"
#include "agent/pdfagentexecutioncontext.h"

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
    args = QJsonObject();
    result = registry.executeCommand("extract_page_text", args, context);

    QVERIFY(!result["ok"].toBool());
    QVERIFY(result["error"].toString().contains("page"));
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

    // Set up mock search callback
    context.searchTextCallback = [](int pageIndex, const QString& searchText) -> QJsonObject {
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

    // Set up mock highlight callback
    context.createHighlightCallback = [](int pageIndex, const QPolygonF& quadrilaterals, const QColor& color, const QString& contents) -> QJsonObject {
        QJsonObject result;
        result["ok"] = true;
        result["page_index"] = pageIndex;
        result["highlights_created"] = 1;
        return result;
    };

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

    // Set up mock text annotation callback
    context.createTextAnnotationCallback = [](int pageIndex, const QPointF& position, const QString& contents, const QString& author) -> QJsonObject {
        QJsonObject result;
        result["ok"] = true;
        result["page_index"] = pageIndex;
        result["annotation_ref"] = 1;
        return result;
    };

    QJsonObject args;
    args["text"] = "Test comment";
    args["page"] = 0;

    QJsonObject result = registry.executeCommand("add_text_comment", args, context);

    QVERIFY(result["ok"].toBool());
    QVERIFY(result["annotation_ref"].toInt() == 1);
}

QTEST_MAIN(PDFAgentFunctionRegistryTest)

#include "tst_pdfagentfunctionregistry.moc"
