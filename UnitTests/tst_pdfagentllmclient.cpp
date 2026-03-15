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
#include <QProcessEnvironment>
#include <QSignalSpy>

#include "agent/pdfagentllmclient.h"

class PDFAgentLlmClientTest : public QObject
{
    Q_OBJECT

private slots:
    void test_parseValidResponse();
    void test_parseInvalidJson();
    void test_parseMissingChoices();
    void test_parseToolCallsOnlyResponse();
    void test_validateConfig();
    void test_normalizeResponse();
    void test_liveChatCompletion();
};

void PDFAgentLlmClientTest::test_parseValidResponse()
{
    const QByteArray body = R"({
        "id": "7129456f-f211-41ec-8304-2172e85d455b",
        "object": "chat.completion",
        "created": 1773506304,
        "model": "deepseek-chat",
        "choices": [
            {
                "index": 0,
                "message": {
                    "role": "assistant",
                    "content": "Hello! How can I assist you today? 😊"
                },
                "logprobs": null,
                "finish_reason": "stop"
            }
        ],
        "usage": {
            "prompt_tokens": 12,
            "completion_tokens": 11,
            "total_tokens": 23,
            "prompt_tokens_details": {
                "cached_tokens": 0
            },
            "prompt_cache_hit_tokens": 0,
            "prompt_cache_miss_tokens": 12
        },
        "system_fingerprint": "fp_eaab8d114b_prod0820_fp8_kvcache"
    })";

    const pdf::PDFAgentLlmResponse response = pdf::PDFAgentLlmClient::parseChatResponse(body, 200);
    QVERIFY(response.success);
    QCOMPARE(response.assistantText, QString::fromUtf8("Hello! How can I assist you today? 😊"));
    QVERIFY(response.errorMessage.isEmpty());
    QCOMPARE(response.httpStatusCode, 200);
    QCOMPARE(response.rawResponseBody, body);
    QCOMPARE(response.rawResponseText, QString::fromUtf8(body));
    QCOMPARE(response.rawJson.value("model").toString(), QString("deepseek-chat"));
}

void PDFAgentLlmClientTest::test_parseInvalidJson()
{
    const pdf::PDFAgentLlmResponse response = pdf::PDFAgentLlmClient::parseChatResponse("{invalid", 200);
    QVERIFY(!response.success);
    QVERIFY(!response.errorMessage.isEmpty());
    QCOMPARE(response.rawResponseText, QString("{invalid"));
}

void PDFAgentLlmClientTest::test_parseMissingChoices()
{
    const QByteArray body = R"({
        "id": "chatcmpl-test",
        "object": "chat.completion",
        "created": 1773506304,
        "model": "deepseek-chat"
    })";

    const pdf::PDFAgentLlmResponse response = pdf::PDFAgentLlmClient::parseChatResponse(body, 200);
    QVERIFY(!response.success);
    QCOMPARE(response.errorMessage, QString("LLM response did not contain any choices."));
}

void PDFAgentLlmClientTest::test_parseToolCallsOnlyResponse()
{
    const QByteArray body = R"({
        "id": "7129456f-f211-41ec-8304-2172e85d455b",
        "object": "chat.completion",
        "created": 1773506304,
        "model": "deepseek-chat",
        "choices": [
            {
                "index": 0,
                "message": {
                    "role": "assistant",
                    "content": null,
                    "tool_calls": [
                        {
                            "id": "call_1",
                            "type": "function",
                            "function": {
                                "name": "get_document_summary",
                                "arguments": "{}"
                            }
                        }
                    ]
                },
                "logprobs": null,
                "finish_reason": "tool_calls"
            }
        ],
        "usage": {
            "prompt_tokens": 12,
            "completion_tokens": 11,
            "total_tokens": 23
        },
        "system_fingerprint": "fp_eaab8d114b_prod0820_fp8_kvcache"
    })";

    const pdf::PDFAgentLlmResponse response = pdf::PDFAgentLlmClient::parseChatResponse(body, 200);
    QVERIFY(!response.success);
    QCOMPARE(response.errorMessage, QString("Tool calling is not supported in phase 1."));
}

void PDFAgentLlmClientTest::test_validateConfig()
{
    pdf::PDFAgentLlmConfig config;
    QVector<pdf::PDFAgentChatMessage> messages;

    QCOMPARE(pdf::PDFAgentLlmClient::validateChatRequest(messages, config), QString("LLM endpoint is empty."));

    config.endpoint = "https://example.com/v1/chat/completions";
    QCOMPARE(pdf::PDFAgentLlmClient::validateChatRequest(messages, config), QString("LLM model is empty."));

    config.model = "test-model";
    QCOMPARE(pdf::PDFAgentLlmClient::validateChatRequest(messages, config), QString("At least one chat message is required."));

    messages.push_back({ QString(), QString("Hello") });
    QCOMPARE(pdf::PDFAgentLlmClient::validateChatRequest(messages, config), QString("Chat message role is empty."));

    messages[0].role = "user";
    messages[0].content = " ";
    QCOMPARE(pdf::PDFAgentLlmClient::validateChatRequest(messages, config), QString("Chat message content is empty."));

    messages[0].content = "Hello";
    QVERIFY(pdf::PDFAgentLlmClient::validateChatRequest(messages, config).isEmpty());
}

void PDFAgentLlmClientTest::test_normalizeResponse()
{
    const QByteArray body = R"({
        "id": "56651d7f-97c1-457b-abdb-8c87aed5226a",
        "object": "chat.completion",
        "created": 1773506876,
        "model": "deepseek-chat",
        "choices": [
            {
                "index": 0,
                "message": {
                    "role": "assistant",
                    "content": "Hello! How can I assist you today?"
                },
                "logprobs": null,
                "finish_reason": "stop"
            }
        ],
        "usage": {
            "prompt_tokens": 19,
            "completion_tokens": 9,
            "total_tokens": 28,
            "prompt_tokens_details": {
                "cached_tokens": 0
            },
            "prompt_cache_hit_tokens": 0,
            "prompt_cache_miss_tokens": 19
        },
        "system_fingerprint": "fp_eaab8d114b_prod0820_fp8_kvcache"
    })";

    const pdf::PDFAgentLlmResponse response = pdf::PDFAgentLlmClient::parseChatResponse(body, 200);
    const pdf::PDFAgentNormalizedResponse normalized =
        pdf::PDFAgentLlmClient::normalizeChatResponse(response, "https://api.deepseek.com/chat/completions");

    QVERIFY(normalized.ok);
    QCOMPARE(normalized.responseId, QString("56651d7f-97c1-457b-abdb-8c87aed5226a"));
    QCOMPARE(normalized.modelName, QString("deepseek-chat"));
    QCOMPARE(normalized.assistantRole, QString("assistant"));
    QCOMPARE(normalized.assistantText, QString("Hello! How can I assist you today?"));
    QCOMPARE(normalized.finishReason, QString("stop"));
    QCOMPARE(normalized.totalTokens, 28);
    QCOMPARE(normalized.httpStatusCode, 200);
    QVERIFY(!pdf::PDFAgentLlmClient::formatNormalizedResponse(normalized).isEmpty());
}

void PDFAgentLlmClientTest::test_liveChatCompletion()
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString endpoint = environment.value("PDF4QT_AGENT_ENDPOINT", "https://api.deepseek.com/chat/completions");
    const QString model = environment.value("PDF4QT_AGENT_MODEL", "deepseek-chat");
    const QString apiKey = environment.value("PDF4QT_AGENT_API_KEY");

    if (apiKey.trimmed().isEmpty())
    {
        QSKIP("Set PDF4QT_AGENT_API_KEY environment variable to run the live chat completion test.");
    }

    pdf::PDFAgentLlmConfig config;
    config.endpoint = endpoint;
    config.model = model;
    config.apiKey = apiKey;
    config.systemPrompt = "You are a helpful assistant.";
    config.timeoutMs = 60000;
    config.temperature = 0.2;

    pdf::PDFAgentLlmClient client;
    QSignalSpy spy(&client, &pdf::PDFAgentLlmClient::chatFinished);
    QVERIFY(spy.isValid());

    const QVector<pdf::PDFAgentChatMessage> messages = {
        { "system", "You are a helpful assistant." },
        { "user", "Hello! Please reply with a long greeting in Chinese." }
    };

    client.sendChat(messages, config);

    QVERIFY2(spy.wait(70000), "Timed out while waiting for live LLM response.");
    QCOMPARE(spy.count(), 1);

    const QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.size(), 1);

    const pdf::PDFAgentLlmResponse response = qvariant_cast<pdf::PDFAgentLlmResponse>(arguments.at(0));

    qInfo().noquote() << "Live endpoint:" << endpoint;
    qInfo().noquote() << "Live model:" << model;
    qInfo().noquote() << "Live raw response:";
    qInfo().noquote() << response.rawResponseText;

    const pdf::PDFAgentNormalizedResponse normalized =
        pdf::PDFAgentLlmClient::normalizeChatResponse(response, endpoint);
    qInfo().noquote() << "Live normalized response:";
    qInfo().noquote() << pdf::PDFAgentLlmClient::formatNormalizedResponse(normalized);

    QVERIFY2(response.httpStatusCode > 0, "Live request did not produce a valid HTTP status code.");
    QVERIFY2(!response.rawResponseBody.isEmpty(), "Live request returned an empty response body.");
    QVERIFY2(response.errorMessage.isEmpty(), qPrintable(QString("Live request failed: %1").arg(response.errorMessage)));
    QVERIFY2(response.success, "Live request completed but parsing did not produce a successful result.");
    QVERIFY2(!response.assistantText.trimmed().isEmpty(), "Live request did not return assistant text.");
    QVERIFY2(normalized.ok, "Normalized live response was not marked successful.");
    QVERIFY2(!normalized.responseId.trimmed().isEmpty(), "Normalized live response did not contain a response id.");
    QVERIFY2(!normalized.modelName.trimmed().isEmpty(), "Normalized live response did not contain a model name.");
}

QTEST_GUILESS_MAIN(PDFAgentLlmClientTest)

#include "tst_pdfagentllmclient.moc"
