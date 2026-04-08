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

#ifndef PDFAGENTSETTINGS_H
#define PDFAGENTSETTINGS_H

#include "pdfglobal.h"

#include <QString>
#include <QJsonObject>

namespace pdf
{

struct PDF4QTLIBCORESHARED_EXPORT PdfAgentSettings
{
    // Basic configuration
    QString endpoint;
    QString model;
    QString apiKey;
    QString systemPrompt;
    int timeoutMs = 300000;
    double temperature = 0.2;

    // Feature flags
    bool enableTools = true;
    bool enableMockMode = false;
    bool enableStreaming = false;

    // History settings
    bool enableSessionHistory = true;
    int maxSessionHistoryCount = 50;

    // Debug flags
    bool debugShowRawJson = false;
    bool debugShowToolTrace = false;
    bool debugLogToConsole = false;

    // Convert to LLM config for orchestrator
    QJsonObject toJson() const;

    // Load from JSON
    static PdfAgentSettings fromJson(const QJsonObject& json);

    // Default settings
    static PdfAgentSettings defaultSettings();
};

class PDF4QTLIBCORESHARED_EXPORT PdfAgentSettingsManager
{
public:
    PdfAgentSettingsManager();

    // Load settings from QSettings
    PdfAgentSettings load() const;

    // Save settings to QSettings
    void save(const PdfAgentSettings& settings);

    // Restore default settings
    PdfAgentSettings restoreDefaults();

private:
    static const QString kSettingsPrefix;
};

}   // namespace pdf

#endif // PDFAGENTSETTINGS_H
