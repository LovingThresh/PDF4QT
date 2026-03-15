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

#include "pdfagentfunctionregistry.h"

#include "pdfdocument.h"

namespace pdf
{

// Helper function to create error response
static QJsonObject createErrorResponse(const QString& error)
{
    QJsonObject result;
    result["ok"] = false;
    result["error"] = error;
    return result;
}

// Helper function to create success response
static QJsonObject createSuccessResponse()
{
    QJsonObject result;
    result["ok"] = true;
    return result;
}

// Helper function to create success response with data
static QJsonObject createSuccessResponse(const QJsonObject& data)
{
    QJsonObject result;
    result["ok"] = true;
    for (auto it = data.constBegin(); it != data.constEnd(); ++it)
    {
        result[it.key()] = it.value();
    }
    return result;
}

// === Built-in Commands ===

// Command: get_document_summary
static QJsonObject cmdGetDocumentSummary(const QJsonObject& /*args*/, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    QJsonObject data;
    data["file_name"] = context.originalFileName;
    data["page_count"] = context.pageCount;
    data["current_page"] = context.currentPage;
    data["has_selected_text"] = context.hasSelectedText();
    return createSuccessResponse(data);
}

// Command: get_current_page
static QJsonObject cmdGetCurrentPage(const QJsonObject& /*args*/, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    QJsonObject data;
    data["page_index"] = context.currentPage;
    data["page_number"] = context.currentPage + 1; // User-facing page number is 1-based
    return createSuccessResponse(data);
}

// Command: extract_selected_text
static QJsonObject cmdExtractSelectedText(const QJsonObject& /*args*/, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    if (!context.hasSelectedText())
    {
        return createErrorResponse("No text is currently selected.");
    }

    QJsonObject data;
    data["text"] = context.selectedText;
    return createSuccessResponse(data);
}

// Command: extract_page_text
static QJsonObject cmdExtractPageText(const QJsonObject& args, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    if (!context.extractTextCallback)
    {
        return createErrorResponse("Text extraction is not available. Widget must be loaded first.");
    }

    // Default to current page if not specified
    int pageIndex = args.contains("page") ? args["page"].toInt() : context.currentPage;

    if (pageIndex < 0 || pageIndex >= context.pageCount)
    {
        return createErrorResponse(QString("Invalid page index: %1. Document has %2 pages.").arg(pageIndex).arg(context.pageCount));
    }

    return context.extractTextCallback(pageIndex);
}

// Command: get_page_count
static QJsonObject cmdGetPageCount(const QJsonObject& /*args*/, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    QJsonObject data;
    data["page_count"] = context.pageCount;
    return createSuccessResponse(data);
}

// Command: search_text - 只搜索，返回文本位置
static QJsonObject cmdSearchText(const QJsonObject& args, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    // Get search text parameter
    if (!args.contains("text"))
    {
        return createErrorResponse("Missing required parameter: text");
    }

    QString searchText = args["text"].toString();
    if (searchText.isEmpty())
    {
        return createErrorResponse("Parameter 'text' cannot be empty.");
    }

    // If no search callback available, return error
    if (!context.searchTextCallback)
    {
        return createErrorResponse("Text search is not available. Widget must be loaded first.");
    }

    // Optional: specific page (-1 means all pages)
    int pageFilter = args["page"].toInt(-1);

    QJsonArray results;
    int totalMatches = 0;

    int startPage = (pageFilter >= 0) ? pageFilter : 0;
    int endPage = (pageFilter >= 0) ? pageFilter + 1 : context.pageCount;

    for (int i = startPage; i < endPage && i < context.pageCount; ++i)
    {
        // Call the search callback which handles text layout internally
        QJsonObject pageResult = context.searchTextCallback(i, searchText);
        if (pageResult["ok"].toBool(false))
        {
            QJsonArray pageMatches = pageResult["matches"].toArray();
            for (const QJsonValue& m : pageMatches)
            {
                results.append(m);
                ++totalMatches;
            }
        }
    }

    QJsonObject data;
    data["search_text"] = searchText;
    data["matches_count"] = totalMatches;
    data["results"] = results;
    return createSuccessResponse(data);
}

// Command: extract_text - 提取整页文本
static QJsonObject cmdExtractText(const QJsonObject& args, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    if (!context.extractTextCallback)
    {
        return createErrorResponse("Text extraction is not available. Widget must be loaded first.");
    }

    // Default to current page if not specified
    int pageIndex = args.contains("page") ? args["page"].toInt() : context.currentPage;

    if (pageIndex < 0 || pageIndex >= context.pageCount)
    {
        return createErrorResponse(QString("Invalid page index: %1. Document has %2 pages.").arg(pageIndex).arg(context.pageCount));
    }

    return context.extractTextCallback(pageIndex);
}

// Command: create_highlight - 根据位置创建高亮
static QJsonObject cmdCreateHighlight(const QJsonObject& args, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    if (!context.createHighlightCallback)
    {
        return createErrorResponse("Document modification is not available.");
    }

    // Get required parameters
    if (!args.contains("page_index"))
    {
        return createErrorResponse("Missing required parameter: page_index");
    }

    int pageIndex = args["page_index"].toInt(-1);
    if (pageIndex < 0 || pageIndex >= context.pageCount)
    {
        return createErrorResponse(QString("Parameter 'page_index' must be between 0 and %1.").arg(context.pageCount - 1));
    }

    // Parse quadrilaterals
    if (!args.contains("quadrilaterals"))
    {
        return createErrorResponse("Missing required parameter: quadrilaterals");
    }

    QPolygonF quadrilaterals;
    QJsonArray quads = args["quadrilaterals"].toArray();
    for (const QJsonValue& v : quads)
    {
        QJsonObject q = v.toObject();
        qreal x = q["x"].toDouble();
        qreal y = q["y"].toDouble();
        qreal w = q["width"].toDouble();
        qreal h = q["height"].toDouble();

        // PDF QuadPoints order: bottom-left, bottom-right, top-left, top-right
        QPolygonF rectPoly;
        rectPoly.append(QPointF(x, y + h));         // bottom-left
        rectPoly.append(QPointF(x + w, y + h));     // bottom-right
        rectPoly.append(QPointF(x, y));             // top-left
        rectPoly.append(QPointF(x + w, y));         // top-right
        quadrilaterals.append(rectPoly);
    }

    if (quadrilaterals.isEmpty())
    {
        return createErrorResponse("No valid quadrilaterals provided.");
    }

    // Optional parameters
    QString colorHex = args.value("color").toString("#FFFF00");
    QColor color(colorHex);
    QString contents = args.value("contents").toString();

    return context.createHighlightCallback(pageIndex, quadrilaterals, color, contents);
}

// Command: add_text_comment
static QJsonObject cmdAddTextComment(const QJsonObject& args, const PDFAgentExecutionContext& context)
{
    if (!context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    if (!context.createTextAnnotationCallback)
    {
        return createErrorResponse("Document modification is not available.");
    }

    // Get required parameters
    if (!args.contains("text"))
    {
        return createErrorResponse("Missing required parameter: text");
    }

    QString commentText = args["text"].toString();
    if (commentText.isEmpty())
    {
        return createErrorResponse("Parameter 'text' cannot be empty.");
    }

    // Page index (default: current page)
    int pageIndex = args["page"].toInt(context.currentPage);
    if (pageIndex < 0 || pageIndex >= context.pageCount)
    {
        return createErrorResponse(QString("Parameter 'page' must be between 0 and %1.").arg(context.pageCount - 1));
    }

    // Position (optional, defaults to center of page)
    QPointF position;
    if (args.contains("x") && args.contains("y"))
    {
        position = QPointF(args["x"].toDouble(), args["y"].toDouble());
    }

    // Author (optional)
    QString author = args.value("author").toString("AI Agent");

    // Create the comment
    QJsonObject result = context.createTextAnnotationCallback(pageIndex, position, commentText, author);

    return result;
}

PdfFunctionRegistry::PdfFunctionRegistry()
{
    // Register built-in read-only commands

    // get_document_summary
    {
        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = QJsonObject();
        schema["required"] = QJsonArray();

        registerCommand("get_document_summary",
                        "Returns basic summary information about the current document.",
                        schema,
                        true,
                        true,
                        cmdGetDocumentSummary);
    }

    // get_current_page
    {
        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = QJsonObject();
        schema["required"] = QJsonArray();

        registerCommand("get_current_page",
                        "Returns the current page index and page number.",
                        schema,
                        true,
                        true,
                        cmdGetCurrentPage);
    }

    // extract_selected_text
    {
        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = QJsonObject();
        schema["required"] = QJsonArray();

        registerCommand("extract_selected_text",
                        "Returns the currently selected text in the document.",
                        schema,
                        true,
                        true,
                        cmdExtractSelectedText);
    }

    // extract_page_text
    {
        QJsonObject schema;
        QJsonObject properties;
        properties["page"] = QJsonObject{
            {"type", "integer"},
            {"description", "Zero-based page index."}
        };

        schema["type"] = "object";
        schema["properties"] = properties;
        schema["required"] = QJsonArray{"page"};

        registerCommand("extract_page_text",
                        "Extracts all text from a specified page.",
                        schema,
                        true,
                        true,
                        cmdExtractPageText);
    }

    // get_page_count
    {
        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = QJsonObject();
        schema["required"] = QJsonArray();

        registerCommand("get_page_count",
                        "Returns the total number of pages in the document.",
                        schema,
                        true,
                        true,
                        cmdGetPageCount);
    }

    // search_text
    {
        QJsonObject properties;
        properties["text"] = QJsonObject{
            {"type", "string"},
            {"description", "Text to search for."}
        };
        properties["page"] = QJsonObject{
            {"type", "integer"},
            {"description", "Optional: Zero-based page index. If not specified, searches all pages."}
        };

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = properties;
        schema["required"] = QJsonArray{"text"};

        registerCommand("search_text",
                        "Searches for text in the document and returns the positions of all occurrences. "
                        "Use this to find where specific text is located.",
                        schema,
                        true,   // Read-only - doesn't modify document
                        true,   // Requires document
                        cmdSearchText);
    }

    // create_highlight
    {
        QJsonObject properties;
        properties["page_index"] = QJsonObject{
            {"type", "integer"},
            {"description", "Zero-based page index where the highlight should be created."}
        };
        properties["quadrilaterals"] = QJsonObject{
            {"type", "array"},
            {"description", "Array of quadrilateral objects with x, y, width, height. "
                           "Each should match the position returned by search_text."}
        };
        properties["color"] = QJsonObject{
            {"type", "string"},
            {"description", "Optional: Highlight color in hex format (default: #FFFF00 for yellow)."}
        };
        properties["contents"] = QJsonObject{
            {"type", "string"},
            {"description", "Optional: Contents/label for the highlight."}
        };

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = properties;
        schema["required"] = QJsonArray{"page_index", "quadrilaterals"};

        registerCommand("create_highlight",
                        "Creates a highlight annotation at the specified position. "
                        "Use search_text first to find the positions, then use this to highlight them. "
                        "This command modifies the document.",
                        schema,
                        false,  // Not read-only - modifies document
                        true,    // Requires document
                        PdfAgentCommandRiskLevel::MediumRiskWrite,
                        PdfAgentConfirmationPolicy::RequireUserApproval,
                        cmdCreateHighlight);
    }

    // add_text_comment
    {
        QJsonObject properties;
        properties["text"] = QJsonObject{
            {"type", "string"},
            {"description", "Comment text content."}
        };
        properties["page"] = QJsonObject{
            {"type", "integer"},
            {"description", "Optional: Zero-based page index (default: current page)."}
        };
        properties["x"] = QJsonObject{
            {"type", "number"},
            {"description", "Optional: X coordinate for comment position."}
        };
        properties["y"] = QJsonObject{
            {"type", "number"},
            {"description", "Optional: Y coordinate for comment position."}
        };
        properties["author"] = QJsonObject{
            {"type", "string"},
            {"description", "Optional: Author name (default: AI Agent)."}
        };

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = properties;
        schema["required"] = QJsonArray{"text"};

        registerCommand("add_text_comment",
                        "Adds a text comment (sticky note) annotation to the specified page. "
                        "This command modifies the document by adding a comment.",
                        schema,
                        false,  // Not read-only - modifies document
                        true,   // Requires document
                        PdfAgentCommandRiskLevel::LowRiskWrite,
                        PdfAgentConfirmationPolicy::RequireUserApproval,
                        cmdAddTextComment);
    }
}

void PdfFunctionRegistry::registerCommand(const QString& name,
                                           const QString& description,
                                           const QJsonObject& parameterSchema,
                                           bool readOnly,
                                           bool requiresDocument,
                                           const PdfAgentCommandHandler& handler)
{
    // Default risk level for read-only commands is ReadOnly
    // Default confirmation policy is NoConfirmation
    PdfAgentCommandRiskLevel riskLevel = readOnly ? PdfAgentCommandRiskLevel::ReadOnly
                                                    : PdfAgentCommandRiskLevel::LowRiskWrite;

    registerCommand(name, description, parameterSchema, readOnly, requiresDocument,
                   riskLevel, PdfAgentConfirmationPolicy::NoConfirmation, handler);
}

void PdfFunctionRegistry::registerCommand(const QString& name,
                                           const QString& description,
                                           const QJsonObject& parameterSchema,
                                           bool readOnly,
                                           bool requiresDocument,
                                           PdfAgentCommandRiskLevel riskLevel,
                                           PdfAgentConfirmationPolicy confirmationPolicy,
                                           const PdfAgentCommandHandler& handler)
{
    Command cmd;
    cmd.descriptor.name = name;
    cmd.descriptor.description = description;
    cmd.descriptor.parameterSchema = parameterSchema;
    cmd.descriptor.readOnly = readOnly;
    cmd.descriptor.requiresDocument = requiresDocument;
    cmd.descriptor.riskLevel = riskLevel;
    cmd.descriptor.confirmationPolicy = confirmationPolicy;
    cmd.handler = handler;

    m_commands[name] = cmd;
}

bool PdfFunctionRegistry::contains(const QString& name) const
{
    return m_commands.contains(name);
}

QStringList PdfFunctionRegistry::getCommandNames() const
{
    return m_commands.keys();
}

QJsonArray PdfFunctionRegistry::getToolsSchema() const
{
    QJsonArray tools;

    for (auto it = m_commands.constBegin(); it != m_commands.constEnd(); ++it)
    {
        const Command& cmd = it.value();

        QJsonObject tool;
        tool["type"] = "function";

        QJsonObject functionObj;
        functionObj["name"] = cmd.descriptor.name;
        functionObj["description"] = cmd.descriptor.description;
        functionObj["parameters"] = cmd.descriptor.parameterSchema;

        tool["function"] = functionObj;
        tools.append(tool);
    }

    return tools;
}

QJsonObject PdfFunctionRegistry::executeCommand(const QString& name,
                                                  const QJsonObject& args,
                                                  const PDFAgentExecutionContext& context) const
{
    auto it = m_commands.find(name);
    if (it == m_commands.end())
    {
        return createErrorResponse(QString("Unknown command: %1").arg(name));
    }

    const Command& cmd = it.value();

    // Check if command requires document
    if (cmd.descriptor.requiresDocument && !context.hasDocument())
    {
        return createErrorResponse("Command requires an active document.");
    }

    // Execute the command
    try
    {
        return cmd.handler(args, context);
    }
    catch (const std::exception& e)
    {
        return createErrorResponse(QString("Command execution failed: %1").arg(e.what()));
    }
    catch (...)
    {
        return createErrorResponse("Command execution failed with unknown error.");
    }
}

const PdfAgentCommandDescriptor* PdfFunctionRegistry::getCommandDescriptor(const QString& name) const
{
    auto it = m_commands.find(name);
    if (it == m_commands.end())
    {
        return nullptr;
    }
    return &it.value().descriptor;
}

bool PdfFunctionRegistry::requiresConfirmation(const QString& name) const
{
    auto it = m_commands.find(name);
    if (it == m_commands.end())
    {
        return false;
    }
    const PdfAgentCommandDescriptor& desc = it.value().descriptor;
    return desc.confirmationPolicy == PdfAgentConfirmationPolicy::RequireUserApproval;
}

bool PdfFunctionRegistry::isCommandDisabled(const QString& name) const
{
    auto it = m_commands.find(name);
    if (it == m_commands.end())
    {
        return true;
    }
    const PdfAgentCommandDescriptor& desc = it.value().descriptor;
    return desc.confirmationPolicy == PdfAgentConfirmationPolicy::DisabledForAgent;
}

}   // namespace pdf
