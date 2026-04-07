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

#include "pdfagentsettingsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QTabWidget>
#include <QMessageBox>
#include <QDebug>

namespace pdfplugin
{

PdfAgentSettingsDialog::PdfAgentSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("AI Agent Settings"));
    setMinimumWidth(500);
    setMinimumHeight(400);

    setupUi();
    loadSettings();
}

PdfAgentSettingsDialog::~PdfAgentSettingsDialog()
{
}

void PdfAgentSettingsDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create tab widget
    QTabWidget* tabWidget = new QTabWidget(this);

    // Basic configuration tab
    QWidget* basicTab = new QWidget(this);
    QFormLayout* basicLayout = new QFormLayout(basicTab);

    m_endpointEdit = new QLineEdit(this);
    m_endpointEdit->setPlaceholderText("https://dashscope.aliyuncs.com/compatible-mode/v1");
    basicLayout->addRow(tr("Endpoint:"), m_endpointEdit);

    m_modelEdit = new QLineEdit(this);
    m_modelEdit->setPlaceholderText("qwen3.6-plus");
    basicLayout->addRow(tr("Model:"), m_modelEdit);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("Enter API Key");
    basicLayout->addRow(tr("API Key:"), m_apiKeyEdit);

    m_systemPromptEdit = new QTextEdit(this);
    m_systemPromptEdit->setMaximumHeight(100);
    m_systemPromptEdit->setPlaceholderText("You are a helpful PDF assistant.");
    basicLayout->addRow(tr("System Prompt:"), m_systemPromptEdit);

    m_timeoutSpinBox = new QSpinBox(this);
    m_timeoutSpinBox->setRange(1000, 300000);
    m_timeoutSpinBox->setSuffix(" ms");
    m_timeoutSpinBox->setSingleStep(1000);
    basicLayout->addRow(tr("Timeout:"), m_timeoutSpinBox);

    m_temperatureSpinBox = new QDoubleSpinBox(this);
    m_temperatureSpinBox->setRange(0.0, 2.0);
    m_temperatureSpinBox->setSingleStep(0.1);
    m_temperatureSpinBox->setDecimals(1);
    basicLayout->addRow(tr("Temperature:"), m_temperatureSpinBox);

    tabWidget->addTab(basicTab, tr("Basic"));

    // Advanced tab (feature flags)
    QWidget* advancedTab = new QWidget(this);
    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedTab);

    QGroupBox* featureGroup = new QGroupBox(tr("Feature Flags"), this);
    QVBoxLayout* featureLayout = new QVBoxLayout(featureGroup);

    m_enableToolsCheckBox = new QCheckBox(tr("Enable Tools (allow agent to use PDF functions)"), this);
    featureLayout->addWidget(m_enableToolsCheckBox);

    m_enableMockModeCheckBox = new QCheckBox(tr("Enable Mock Mode (for testing without API calls)"), this);
    featureLayout->addWidget(m_enableMockModeCheckBox);

    advancedLayout->addWidget(featureGroup);

    // Debug flags group
    QGroupBox* debugGroup = new QGroupBox(tr("Debug Flags"), this);
    QVBoxLayout* debugLayout = new QVBoxLayout(debugGroup);

    m_debugShowRawJsonCheckBox = new QCheckBox(tr("Show Raw JSON (in diagnostics panel)"), this);
    debugLayout->addWidget(m_debugShowRawJsonCheckBox);

    m_debugShowToolTraceCheckBox = new QCheckBox(tr("Show Tool Trace (in diagnostics panel)"), this);
    debugLayout->addWidget(m_debugShowToolTraceCheckBox);

    m_debugLogToConsoleCheckBox = new QCheckBox(tr("Log to Console (debug output)"), this);
    debugLayout->addWidget(m_debugLogToConsoleCheckBox);

    advancedLayout->addWidget(debugGroup);
    advancedLayout->addStretch();

    tabWidget->addTab(advancedTab, tr("Advanced"));

    mainLayout->addWidget(tabWidget);

    // Button box
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_restoreDefaultsButton = new QPushButton(tr("Restore Defaults"), this);
    buttonLayout->addWidget(m_restoreDefaultsButton);
    connect(m_restoreDefaultsButton, &QPushButton::clicked, this, &PdfAgentSettingsDialog::onRestoreDefaults);

    buttonLayout->addStretch();

    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    buttonLayout->addWidget(m_okButton);
    connect(m_okButton, &QPushButton::clicked, this, &PdfAgentSettingsDialog::onOk);

    m_applyButton = new QPushButton(tr("Apply"), this);
    buttonLayout->addWidget(m_applyButton);
    connect(m_applyButton, &QPushButton::clicked, this, &PdfAgentSettingsDialog::onApply);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addWidget(m_cancelButton);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    mainLayout->addLayout(buttonLayout);
}

void PdfAgentSettingsDialog::loadSettings()
{
    m_currentSettings = m_settingsManager.load();

    // Basic config
    m_endpointEdit->setText(m_currentSettings.endpoint);
    m_modelEdit->setText(m_currentSettings.model);
    m_apiKeyEdit->setText(m_currentSettings.apiKey);
    m_systemPromptEdit->setText(m_currentSettings.systemPrompt);
    m_timeoutSpinBox->setValue(m_currentSettings.timeoutMs);
    m_temperatureSpinBox->setValue(m_currentSettings.temperature);

    // Feature flags
    m_enableToolsCheckBox->setChecked(m_currentSettings.enableTools);
    m_enableMockModeCheckBox->setChecked(m_currentSettings.enableMockMode);

    // Debug flags
    m_debugShowRawJsonCheckBox->setChecked(m_currentSettings.debugShowRawJson);
    m_debugShowToolTraceCheckBox->setChecked(m_currentSettings.debugShowToolTrace);
    m_debugLogToConsoleCheckBox->setChecked(m_currentSettings.debugLogToConsole);
}

void PdfAgentSettingsDialog::saveSettings()
{
    // Basic config
    m_currentSettings.endpoint = m_endpointEdit->text();
    m_currentSettings.model = m_modelEdit->text();
    m_currentSettings.apiKey = m_apiKeyEdit->text();
    m_currentSettings.systemPrompt = m_systemPromptEdit->toPlainText();
    m_currentSettings.timeoutMs = m_timeoutSpinBox->value();
    m_currentSettings.temperature = m_temperatureSpinBox->value();

    // Feature flags
    m_currentSettings.enableTools = m_enableToolsCheckBox->isChecked();
    m_currentSettings.enableMockMode = m_enableMockModeCheckBox->isChecked();

    // Debug flags
    m_currentSettings.debugShowRawJson = m_debugShowRawJsonCheckBox->isChecked();
    m_currentSettings.debugShowToolTrace = m_debugShowToolTraceCheckBox->isChecked();
    m_currentSettings.debugLogToConsole = m_debugLogToConsoleCheckBox->isChecked();

    // Save to QSettings
    m_settingsManager.save(m_currentSettings);
}

pdf::PdfAgentSettings PdfAgentSettingsDialog::getSettings() const
{
    return m_currentSettings;
}

void PdfAgentSettingsDialog::setSettings(const pdf::PdfAgentSettings& settings)
{
    m_currentSettings = settings;

    // Basic config
    m_endpointEdit->setText(settings.endpoint);
    m_modelEdit->setText(settings.model);
    m_apiKeyEdit->setText(settings.apiKey);
    m_systemPromptEdit->setText(settings.systemPrompt);
    m_timeoutSpinBox->setValue(settings.timeoutMs);
    m_temperatureSpinBox->setValue(settings.temperature);

    // Feature flags
    m_enableToolsCheckBox->setChecked(settings.enableTools);
    m_enableMockModeCheckBox->setChecked(settings.enableMockMode);

    // Debug flags
    m_debugShowRawJsonCheckBox->setChecked(settings.debugShowRawJson);
    m_debugShowToolTraceCheckBox->setChecked(settings.debugShowToolTrace);
    m_debugLogToConsoleCheckBox->setChecked(settings.debugLogToConsole);
}

void PdfAgentSettingsDialog::onRestoreDefaults()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Restore Defaults"),
        tr("Are you sure you want to restore default settings?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        m_currentSettings = m_settingsManager.restoreDefaults();
        loadSettings();
    }
}

void PdfAgentSettingsDialog::onApply()
{
    saveSettings();
    Q_EMIT settingsApplied(m_currentSettings);
}

void PdfAgentSettingsDialog::onOk()
{
    saveSettings();
    Q_EMIT settingsApplied(m_currentSettings);
    accept();
}

}   // namespace pdfplugin
