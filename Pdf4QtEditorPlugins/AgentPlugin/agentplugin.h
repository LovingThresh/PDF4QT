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

#ifndef AGENTPLUGIN_H
#define AGENTPLUGIN_H

#include "pdfplugin.h"
#include "agent/pdfagentorchestrator.h"
#include "agent/pdfagentexecutioncontext.h"

#include <QObject>
#include <QJsonDocument>
#include <QColor>
#include <QPointF>
#include <QPolygonF>

class QAction;

namespace pdfplugin
{

class AgentChatDockWidget;

class AgentPlugin : public pdf::PDFPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "PDF4QT.AgentPlugin" FILE "AgentPlugin.json")

private:
    using BaseClass = pdf::PDFPlugin;

public:
    AgentPlugin();

    virtual void setWidget(pdf::PDFWidget* widget) override;
    virtual void setDocument(const pdf::PDFModifiedDocument& document) override;
    virtual std::vector<QAction*> getActions() const override;
    virtual QString getPluginMenuName() const override;

private:
    void onToggleChatDock();
    void onSendMessageRequested(const QString& text);
    void onAgentResponseReady(const pdf::PDFAgentLlmResponse& response) const;
    void onToolCallStarted(const QString& toolName) const;
    void onToolCallFinished(const QString& toolName, bool success) const;
    void onFinalResponseReady(const QString& responseText) const;

    void updateActions() const;
    void updateContextState() const;
    void ensureDockWidget();
    pdf::PDFAgentLlmConfig loadConfig() const;
    pdf::PDFAgentExecutionContext buildExecutionContext() const;
    QJsonObject createHighlightAnnotation(int pageIndex, const QPolygonF& quadrilaterals, const QColor& color, const QString& contents) const;
    QJsonObject createTextAnnotation(int pageIndex, const QPointF& position, const QString& contents, const QString& author) const;

    QAction* m_toggleChatAction = nullptr;
    AgentChatDockWidget* m_chatDockWidget = nullptr;
    pdf::PDFAgentOrchestrator* m_orchestrator = nullptr;
};

}   // namespace pdfplugin

#endif // AGENTPLUGIN_H
