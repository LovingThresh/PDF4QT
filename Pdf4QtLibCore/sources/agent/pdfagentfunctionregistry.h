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

#ifndef PDFAGENTFUNCTIONREGISTRY_H
#define PDFAGENTFUNCTIONREGISTRY_H

#include "pdfglobal.h"
#include "pdfagentexecutioncontext.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QHash>
#include <functional>

namespace pdf
{

// Forward declarations
class PDFTextLayout;
struct PDFFindResult;
using PDFFindResults = std::vector<PDFFindResult>;

/// Command risk level for modifying operations
enum class PdfAgentCommandRiskLevel
{
    ReadOnly,          // No document modification
    LowRiskWrite,      // Low risk modification (e.g., create bookmark)
    MediumRiskWrite,   // Medium risk modification (e.g., remove links, save)
    HighRiskWrite     // High risk modification (disabled for agent)
};

/// Confirmation policy for commands
enum class PdfAgentConfirmationPolicy
{
    NoConfirmation,         // Execute without user confirmation
    RequireUserApproval,   // Require user approval before execution
    DisabledForAgent      // Command is disabled for agent execution
};

class PDF4QTLIBCORESHARED_EXPORT PdfAgentCommandDescriptor
{
public:
    QString name;
    QString description;
    QJsonObject parameterSchema;
    bool readOnly = true;
    bool requiresDocument = true;
    PdfAgentCommandRiskLevel riskLevel = PdfAgentCommandRiskLevel::ReadOnly;
    PdfAgentConfirmationPolicy confirmationPolicy = PdfAgentConfirmationPolicy::NoConfirmation;
};

using PdfAgentCommandHandler = std::function<QJsonObject(const QJsonObject& args,
                                                          const PDFAgentExecutionContext& context)>;

class PDF4QTLIBCORESHARED_EXPORT PdfFunctionRegistry
{
public:
    explicit PdfFunctionRegistry();

    // Simple registration (for read-only commands)
    void registerCommand(const QString& name,
                         const QString& description,
                         const QJsonObject& parameterSchema,
                         bool readOnly,
                         bool requiresDocument,
                         const PdfAgentCommandHandler& handler);

    // Full registration with risk level and confirmation policy
    void registerCommand(const QString& name,
                         const QString& description,
                         const QJsonObject& parameterSchema,
                         bool readOnly,
                         bool requiresDocument,
                         PdfAgentCommandRiskLevel riskLevel,
                         PdfAgentConfirmationPolicy confirmationPolicy,
                         const PdfAgentCommandHandler& handler);

    bool contains(const QString& name) const;
    QStringList getCommandNames() const;
    QJsonArray getToolsSchema() const;

    // Get command descriptor for confirmation UI
    const PdfAgentCommandDescriptor* getCommandDescriptor(const QString& name) const;

    QJsonObject executeCommand(const QString& name,
                               const QJsonObject& args,
                               const PDFAgentExecutionContext& context) const;

    // Check if command requires user confirmation
    bool requiresConfirmation(const QString& name) const;

    // Check if command is disabled for agent
    bool isCommandDisabled(const QString& name) const;

private:
    struct Command
    {
        PdfAgentCommandDescriptor descriptor;
        PdfAgentCommandHandler handler;
    };

    QHash<QString, Command> m_commands;
};

}   // namespace pdf

#endif // PDFAGENTFUNCTIONREGISTRY_H
