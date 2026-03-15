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

#ifndef PDFAGENTLLMCLIENT_H
#define PDFAGENTLLMCLIENT_H

#include "agent/pdfagenttypes.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

namespace pdf
{

class PDF4QTLIBCORESHARED_EXPORT PDFAgentLlmClient : public QObject
{
    Q_OBJECT

public:
    explicit PDFAgentLlmClient(QObject* parent = nullptr);

    void sendChat(const QVector<PDFAgentChatMessage>& messages,
                  const PDFAgentLlmConfig& config);
    void sendChatWithTools(const QVector<PDFAgentChatMessage>& messages,
                           const QJsonArray& tools,
                           const PDFAgentLlmConfig& config);

    static QString validateChatRequest(const QVector<PDFAgentChatMessage>& messages,
                                       const PDFAgentLlmConfig& config);
    static PDFAgentLlmResponse parseChatResponse(const QByteArray& body,
                                                int httpStatusCode = -1);
    static PDFAgentAssistantTurn parseAssistantTurn(const QByteArray& body,
                                                     int httpStatusCode = -1);
    static PDFAgentNormalizedResponse normalizeChatResponse(const PDFAgentLlmResponse& response,
                                                            const QString& endpoint = QString());
    static QString formatNormalizedResponse(const PDFAgentNormalizedResponse& response);

signals:
    void chatFinished(const pdf::PDFAgentLlmResponse& response);

private:
    [[nodiscard]] static QNetworkRequest buildRequest(const PDFAgentLlmConfig& config);
    [[nodiscard]] static QJsonObject buildPayload(const QVector<PDFAgentChatMessage>& messages,
                                                  const PDFAgentLlmConfig& config);
    [[nodiscard]] static QJsonObject buildPayloadWithTools(const QVector<PDFAgentChatMessage>& messages,
                                                           const QJsonArray& tools,
                                                           const PDFAgentLlmConfig& config);
    void finishWithResponse(const PDFAgentLlmResponse& response);
    void onReplyFinished();
    void onRequestTimedOut();

    QNetworkAccessManager* m_networkAccessManager;
    QPointer<QNetworkReply> m_activeReply;
    QTimer m_requestTimer;
    bool m_requestTimedOut;
};

}   // namespace pdf

#endif // PDFAGENTLLMCLIENT_H
