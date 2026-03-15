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

#ifndef PDFAGENTSETTINGSDIALOG_H
#define PDFAGENTSETTINGSDIALOG_H

#include <QDialog>

#include "agent/pdfagentsettings.h"

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QTextEdit;
class QCheckBox;
class QPushButton;
class QTabWidget;

namespace pdfplugin
{

class PdfAgentSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfAgentSettingsDialog(QWidget* parent = nullptr);
    ~PdfAgentSettingsDialog() override;

    // Get current settings
    pdf::PdfAgentSettings getSettings() const;

    // Set settings
    void setSettings(const pdf::PdfAgentSettings& settings);

signals:
    // Signal emitted when settings are applied
    void settingsApplied(const pdf::PdfAgentSettings& settings);

private slots:
    void onRestoreDefaults();
    void onApply();
    void onOk();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    // Basic config widgets
    QLineEdit* m_endpointEdit = nullptr;
    QLineEdit* m_modelEdit = nullptr;
    QLineEdit* m_apiKeyEdit = nullptr;
    QTextEdit* m_systemPromptEdit = nullptr;
    QSpinBox* m_timeoutSpinBox = nullptr;
    QDoubleSpinBox* m_temperatureSpinBox = nullptr;

    // Feature flag widgets
    QCheckBox* m_enableToolsCheckBox = nullptr;
    QCheckBox* m_enableMockModeCheckBox = nullptr;

    // Debug flag widgets
    QCheckBox* m_debugShowRawJsonCheckBox = nullptr;
    QCheckBox* m_debugShowToolTraceCheckBox = nullptr;
    QCheckBox* m_debugLogToConsoleCheckBox = nullptr;

    // Buttons
    QPushButton* m_restoreDefaultsButton = nullptr;
    QPushButton* m_applyButton = nullptr;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    // Current settings
    pdf::PdfAgentSettings m_currentSettings;
    pdf::PdfAgentSettingsManager m_settingsManager;
};

}   // namespace pdfplugin

#endif // PDFAGENTSETTINGSDIALOG_H
