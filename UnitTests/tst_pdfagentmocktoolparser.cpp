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

#include "agent/pdfagentmocktoolparser.h"

class PDFAgentMockToolParserTest : public QObject
{
    Q_OBJECT

private slots:
    void test_parseValidSingleToolCall();
    void test_parseValidMultipleToolCalls();
    void test_parseInvalidJson();
    void test_parseMissingToolCallsField();
    void test_parseToolCallsNotArray();
    void test_parseEmptyToolCallsArray();
    void test_parseToolCallMissingName();
    void test_parseToolCallEmptyName();
    void test_parseToolCallInvalidArguments();
    void test_parseToolCallWithEmptyArguments();
};

void PDFAgentMockToolParserTest::test_parseValidSingleToolCall()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = R"({
        "tool_calls": [
            {
                "name": "get_document_summary",
                "arguments": {}
            }
        ]
    })";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(result.success);
    QVERIFY(result.errorMessage.isEmpty());
    QVERIFY(result.toolCalls.size() == 1);
    QVERIFY(result.toolCalls[0].name == "get_document_summary");
    QVERIFY(result.toolCalls[0].arguments.isEmpty());
}

void PDFAgentMockToolParserTest::test_parseValidMultipleToolCalls()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = R"({
        "tool_calls": [
            {
                "name": "get_document_summary",
                "arguments": {}
            },
            {
                "name": "extract_selected_text",
                "arguments": {"page": 0}
            }
        ]
    })";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(result.success);
    QVERIFY(result.toolCalls.size() == 2);
    QVERIFY(result.toolCalls[0].name == "get_document_summary");
    QVERIFY(result.toolCalls[1].name == "extract_selected_text");
    QVERIFY(result.toolCalls[1].arguments.value("page").toInt() == 0);
}

void PDFAgentMockToolParserTest::test_parseInvalidJson()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = "{ invalid json }";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(result.toolCalls.isEmpty());
}

void PDFAgentMockToolParserTest::test_parseMissingToolCallsField()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = R"({"some_other_field": "value"})";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains("tool_calls"));
}

void PDFAgentMockToolParserTest::test_parseToolCallsNotArray()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = R"({"tool_calls": "not an array"})";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains("array"));
}

void PDFAgentMockToolParserTest::test_parseEmptyToolCallsArray()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = R"({"tool_calls": []})";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains("empty"));
}

void PDFAgentMockToolParserTest::test_parseToolCallMissingName()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = R"({
        "tool_calls": [
            {
                "arguments": {}
            }
        ]
    })";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains("name"));
}

void PDFAgentMockToolParserTest::test_parseToolCallEmptyName()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = R"({
        "tool_calls": [
            {
                "name": "",
                "arguments": {}
            }
        ]
    })";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains("empty"));
}

void PDFAgentMockToolParserTest::test_parseToolCallInvalidArguments()
{
    pdf::PDFAgentMockToolParser parser;

    const QString json = R"({
        "tool_calls": [
            {
                "name": "test_command",
                "arguments": "not an object"
            }
        ]
    })";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains("object"));
}

void PDFAgentMockToolParserTest::test_parseToolCallWithEmptyArguments()
{
    pdf::PDFAgentMockToolParser parser;

    // Test case where arguments field is missing entirely - should default to empty object
    const QString json = R"({
        "tool_calls": [
            {
                "name": "get_page_count"
            }
        ]
    })";

    pdf::PdfAgentMockToolParseResult result = parser.parse(json);

    QVERIFY(result.success);
    QVERIFY(result.toolCalls.size() == 1);
    QVERIFY(result.toolCalls[0].name == "get_page_count");
    QVERIFY(result.toolCalls[0].arguments.isEmpty());
}

QTEST_MAIN(PDFAgentMockToolParserTest)

#include "tst_pdfagentmocktoolparser.moc"
