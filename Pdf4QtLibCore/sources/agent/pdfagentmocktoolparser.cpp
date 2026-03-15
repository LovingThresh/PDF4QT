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

#include "pdfagentmocktoolparser.h"

#include <QJsonDocument>
#include <QJsonArray>

namespace pdf
{

PdfAgentMockToolParseResult PDFAgentMockToolParser::parse(const QString& jsonText)
{
    PdfAgentMockToolParseResult result;

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.success = false;
        result.errorMessage = QString("Invalid JSON: %1").arg(parseError.errorString());
        return result;
    }

    if (!doc.isObject())
    {
        result.success = false;
        result.errorMessage = "Input must be a JSON object.";
        return result;
    }

    QJsonObject root = doc.object();

    // Check for tool_calls
    if (!root.contains("tool_calls"))
    {
        result.success = false;
        result.errorMessage = "Missing 'tool_calls' field.";
        return result;
    }

    QJsonValue toolCallsValue = root.value("tool_calls");
    if (!toolCallsValue.isArray())
    {
        result.success = false;
        result.errorMessage = "'tool_calls' must be an array.";
        return result;
    }

    QJsonArray toolCallsArray = toolCallsValue.toArray();

    // Validate each tool call
    for (int i = 0; i < toolCallsArray.size(); ++i)
    {
        QJsonValue toolCallValue = toolCallsArray.at(i);

        if (!toolCallValue.isObject())
        {
            result.success = false;
            result.errorMessage = QString("tool_calls[%1] must be an object.").arg(i);
            return result;
        }

        QJsonObject toolCallObj = toolCallValue.toObject();

        // Check for 'name' field
        if (!toolCallObj.contains("name"))
        {
            result.success = false;
            result.errorMessage = QString("tool_calls[%1] missing 'name' field.").arg(i);
            return result;
        }

        QJsonValue nameValue = toolCallObj.value("name");
        if (!nameValue.isString())
        {
            result.success = false;
            result.errorMessage = QString("tool_calls[%1] 'name' must be a string.").arg(i);
            return result;
        }

        QString name = nameValue.toString();
        if (name.isEmpty())
        {
            result.success = false;
            result.errorMessage = QString("tool_calls[%1] 'name' cannot be empty.").arg(i);
            return result;
        }

        // Get arguments (optional, default to empty object)
        QJsonObject arguments;
        if (toolCallObj.contains("arguments"))
        {
            QJsonValue argumentsValue = toolCallObj.value("arguments");
            if (!argumentsValue.isObject())
            {
                result.success = false;
                result.errorMessage = QString("tool_calls[%1] 'arguments' must be an object.").arg(i);
                return result;
            }
            arguments = argumentsValue.toObject();
        }

        // Add to result
        PdfAgentMockToolCall toolCall;
        toolCall.name = name;
        toolCall.arguments = arguments;
        result.toolCalls.append(toolCall);
    }

    if (result.toolCalls.isEmpty())
    {
        result.success = false;
        result.errorMessage = "tool_calls array cannot be empty.";
        return result;
    }

    result.success = true;
    return result;
}

}   // namespace pdf
