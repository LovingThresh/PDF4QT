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

#include "pdfagentsettings.h"

#include <QSettings>

namespace pdf
{

const QString PdfAgentSettingsManager::kSettingsPrefix = QStringLiteral("AIAgent/");

PdfAgentSettings PdfAgentSettings::defaultSettings()
{
    PdfAgentSettings settings;
    settings.endpoint = QStringLiteral("https://api.deepseek.com/chat/completions");
    settings.model = QStringLiteral("deepseek-chat");
    settings.apiKey.clear();
    settings.systemPrompt = QStringLiteral(
        "You are a helpful PDF assistant. "
        "For multi-step tasks, maintain a todo list with the todo_write tool. "
        "Mark exactly one item as in_progress while working and mark items completed when finished.");
    settings.timeoutMs = 60000;
    settings.temperature = 0.2;
    settings.enableTools = true;
    settings.enableMockMode = false;
    settings.enableStreaming = false;
    settings.enableSessionHistory = true;
    settings.maxSessionHistoryCount = 50;
    settings.debugShowRawJson = false;
    settings.debugShowToolTrace = false;
    settings.debugLogToConsole = false;
    return settings;
}

QJsonObject PdfAgentSettings::toJson() const
{
    QJsonObject json;
    json["endpoint"] = endpoint;
    json["model"] = model;
    json["apiKey"] = apiKey;
    json["systemPrompt"] = systemPrompt;
    json["timeoutMs"] = timeoutMs;
    json["temperature"] = temperature;
    json["enableTools"] = enableTools;
    json["enableMockMode"] = enableMockMode;
    json["enableStreaming"] = enableStreaming;
    json["enableSessionHistory"] = enableSessionHistory;
    json["maxSessionHistoryCount"] = maxSessionHistoryCount;
    json["debugShowRawJson"] = debugShowRawJson;
    json["debugShowToolTrace"] = debugShowToolTrace;
    json["debugLogToConsole"] = debugLogToConsole;
    return json;
}

PdfAgentSettings PdfAgentSettings::fromJson(const QJsonObject& json)
{
    PdfAgentSettings settings;
    settings.endpoint = json.value("endpoint").toString();
    settings.model = json.value("model").toString();
    settings.apiKey = json.value("apiKey").toString();
    settings.systemPrompt = json.value("systemPrompt").toString();
    settings.timeoutMs = json.value("timeoutMs").toInt(60000);
    settings.temperature = json.value("temperature").toDouble(0.2);
    settings.enableTools = json.value("enableTools").toBool(true);
    settings.enableMockMode = json.value("enableMockMode").toBool(false);
    settings.enableStreaming = json.value("enableStreaming").toBool(false);
    settings.enableSessionHistory = json.value("enableSessionHistory").toBool(true);
    settings.maxSessionHistoryCount = json.value("maxSessionHistoryCount").toInt(50);
    settings.debugShowRawJson = json.value("debugShowRawJson").toBool(false);
    settings.debugShowToolTrace = json.value("debugShowToolTrace").toBool(false);
    settings.debugLogToConsole = json.value("debugLogToConsole").toBool(false);
    return settings;
}

PdfAgentSettingsManager::PdfAgentSettingsManager()
{
}

PdfAgentSettings PdfAgentSettingsManager::load() const
{
    QSettings settings;

    PdfAgentSettings result;

    // Load basic configuration
    result.endpoint = settings.value(kSettingsPrefix + "Endpoint", PdfAgentSettings::defaultSettings().endpoint).toString();
    result.model = settings.value(kSettingsPrefix + "Model", PdfAgentSettings::defaultSettings().model).toString();
    result.apiKey = settings.value(kSettingsPrefix + "ApiKey", PdfAgentSettings::defaultSettings().apiKey).toString();
    result.systemPrompt = settings.value(kSettingsPrefix + "SystemPrompt", PdfAgentSettings::defaultSettings().systemPrompt).toString();
    result.timeoutMs = settings.value(kSettingsPrefix + "TimeoutMs", PdfAgentSettings::defaultSettings().timeoutMs).toInt();
    result.temperature = settings.value(kSettingsPrefix + "Temperature", PdfAgentSettings::defaultSettings().temperature).toDouble();

    // Load feature flags
    result.enableTools = settings.value(kSettingsPrefix + "EnableTools", PdfAgentSettings::defaultSettings().enableTools).toBool();
    result.enableMockMode = settings.value(kSettingsPrefix + "EnableMockMode", PdfAgentSettings::defaultSettings().enableMockMode).toBool();
    result.enableStreaming = settings.value(kSettingsPrefix + "EnableStreaming", PdfAgentSettings::defaultSettings().enableStreaming).toBool();

    // Load history settings
    result.enableSessionHistory = settings.value(kSettingsPrefix + "EnableSessionHistory", PdfAgentSettings::defaultSettings().enableSessionHistory).toBool();
    result.maxSessionHistoryCount = settings.value(kSettingsPrefix + "MaxSessionHistoryCount", PdfAgentSettings::defaultSettings().maxSessionHistoryCount).toInt();

    // Load debug flags
    result.debugShowRawJson = settings.value(kSettingsPrefix + "DebugShowRawJson", PdfAgentSettings::defaultSettings().debugShowRawJson).toBool();
    result.debugShowToolTrace = settings.value(kSettingsPrefix + "DebugShowToolTrace", PdfAgentSettings::defaultSettings().debugShowToolTrace).toBool();
    result.debugLogToConsole = settings.value(kSettingsPrefix + "DebugLogToConsole", PdfAgentSettings::defaultSettings().debugLogToConsole).toBool();

    // Override with environment variables if set (for development convenience)
    if (qEnvironmentVariableIsSet("PDF4QT_AGENT_ENDPOINT"))
    {
        result.endpoint = qEnvironmentVariable("PDF4QT_AGENT_ENDPOINT");
    }
    if (qEnvironmentVariableIsSet("PDF4QT_AGENT_MODEL"))
    {
        result.model = qEnvironmentVariable("PDF4QT_AGENT_MODEL");
    }
    if (qEnvironmentVariableIsSet("PDF4QT_AGENT_API_KEY"))
    {
        result.apiKey = qEnvironmentVariable("PDF4QT_AGENT_API_KEY");
    }
    if (qEnvironmentVariableIsSet("PDF4QT_AGENT_SYSTEM_PROMPT"))
    {
        result.systemPrompt = qEnvironmentVariable("PDF4QT_AGENT_SYSTEM_PROMPT");
    }
    if (qEnvironmentVariableIsSet("PDF4QT_AGENT_TIMEOUT_MS"))
    {
        result.timeoutMs = qEnvironmentVariableIntValue("PDF4QT_AGENT_TIMEOUT_MS");
    }

    return result;
}

void PdfAgentSettingsManager::save(const PdfAgentSettings& settings)
{
    QSettings qsettings;

    // Save basic configuration
    qsettings.setValue(kSettingsPrefix + "Endpoint", settings.endpoint);
    qsettings.setValue(kSettingsPrefix + "Model", settings.model);
    qsettings.setValue(kSettingsPrefix + "ApiKey", settings.apiKey);
    qsettings.setValue(kSettingsPrefix + "SystemPrompt", settings.systemPrompt);
    qsettings.setValue(kSettingsPrefix + "TimeoutMs", settings.timeoutMs);
    qsettings.setValue(kSettingsPrefix + "Temperature", settings.temperature);

    // Save feature flags
    qsettings.setValue(kSettingsPrefix + "EnableTools", settings.enableTools);
    qsettings.setValue(kSettingsPrefix + "EnableMockMode", settings.enableMockMode);
    qsettings.setValue(kSettingsPrefix + "EnableStreaming", settings.enableStreaming);

    // Save history settings
    qsettings.setValue(kSettingsPrefix + "EnableSessionHistory", settings.enableSessionHistory);
    qsettings.setValue(kSettingsPrefix + "MaxSessionHistoryCount", settings.maxSessionHistoryCount);

    // Save debug flags
    qsettings.setValue(kSettingsPrefix + "DebugShowRawJson", settings.debugShowRawJson);
    qsettings.setValue(kSettingsPrefix + "DebugShowToolTrace", settings.debugShowToolTrace);
    qsettings.setValue(kSettingsPrefix + "DebugLogToConsole", settings.debugLogToConsole);
}

PdfAgentSettings PdfAgentSettingsManager::restoreDefaults()
{
    PdfAgentSettings defaults = PdfAgentSettings::defaultSettings();
    save(defaults);
    return defaults;
}

}   // namespace pdf
