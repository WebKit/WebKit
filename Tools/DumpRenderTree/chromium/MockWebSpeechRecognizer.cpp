/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockWebSpeechRecognizer.h"

#if ENABLE(SCRIPTED_SPEECH)

#include "WebSpeechRecognitionResult.h"
#include "WebSpeechRecognizerClient.h"

using namespace WebKit;

namespace {

// Task class for calling a client function that does not take any parameters.
typedef void (WebSpeechRecognizerClient::*ClientFunctionPointer)(const WebSpeechRecognitionHandle&);
class ClientCallTask : public MethodTask<MockWebSpeechRecognizer> {
public:
    ClientCallTask(MockWebSpeechRecognizer* mock, ClientFunctionPointer function)
        : MethodTask<MockWebSpeechRecognizer>(mock)
        , m_function(function)
    {
    }

    virtual void runIfValid() OVERRIDE { (m_object->client()->*m_function)(m_object->handle()); }

private:
    ClientFunctionPointer m_function;
};

// Task for delivering a result event.
class ResultTask : public MethodTask<MockWebSpeechRecognizer> {
public:
    ResultTask(MockWebSpeechRecognizer* mock, const WebString transcript, float confidence)
        : MethodTask<MockWebSpeechRecognizer>(mock)
        , m_transcript(transcript)
        , m_confidence(confidence)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        WebVector<WebString> transcripts(static_cast<size_t>(1));
        WebVector<float> confidences(static_cast<size_t>(1));
        transcripts[0] = m_transcript;
        confidences[0] = m_confidence;
        WebSpeechRecognitionResult res;
        res.assign(transcripts, confidences, true);

        m_object->client()->didReceiveResult(m_object->handle(), res, 0, WebVector<WebSpeechRecognitionResult>());
    }

private:
    WebString m_transcript;
    float m_confidence;
};

// Task for delivering a nomatch event.
class NoMatchTask : public MethodTask<MockWebSpeechRecognizer> {
public:
    NoMatchTask(MockWebSpeechRecognizer* mock) : MethodTask<MockWebSpeechRecognizer>(mock) { }
    virtual void runIfValid() OVERRIDE { m_object->client()->didReceiveNoMatch(m_object->handle(), WebSpeechRecognitionResult()); }
};

// Task for delivering an error event.
class ErrorTask : public MethodTask<MockWebSpeechRecognizer> {
public:
    ErrorTask(MockWebSpeechRecognizer* mock, int code, const WebString& message)
        : MethodTask<MockWebSpeechRecognizer>(mock)
        , m_code(code)
        , m_message(message)
    {
    }

    virtual void runIfValid() OVERRIDE { m_object->client()->didReceiveError(m_object->handle(), m_message, static_cast<WebSpeechRecognizerClient::ErrorCode>(m_code)); }

private:
    int m_code;
    WebString m_message;
};

} // namespace

PassOwnPtr<MockWebSpeechRecognizer> MockWebSpeechRecognizer::create()
{
    return adoptPtr(new MockWebSpeechRecognizer());
}

void MockWebSpeechRecognizer::start(const WebSpeechRecognitionHandle& handle, const WebSpeechRecognitionParams& params, WebSpeechRecognizerClient* client)
{
    m_handle = handle;
    m_client = client;

    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didStart));
    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didStartAudio));
    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didStartSound));
    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didStartSpeech));

    if (!m_mockTranscripts.isEmpty()) {
        ASSERT(m_mockTranscripts.size() == m_mockConfidences.size());

        for (size_t i = 0; i < m_mockTranscripts.size(); ++i)
            postTask(new ResultTask(this, m_mockTranscripts[i], m_mockConfidences[i]));

        m_mockTranscripts.clear();
        m_mockConfidences.clear();
    } else
        postTask(new NoMatchTask(this));

    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didEndSpeech));
    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didEndSound));
    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didEndAudio));
    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didEnd));
}

void MockWebSpeechRecognizer::stop(const WebSpeechRecognitionHandle& handle, WebSpeechRecognizerClient* client)
{
    m_handle = handle;
    m_client = client;

    // FIXME: Implement.
    ASSERT_NOT_REACHED();
}

void MockWebSpeechRecognizer::abort(const WebSpeechRecognitionHandle& handle, WebSpeechRecognizerClient* client)
{
    m_handle = handle;
    m_client = client;

    // FIXME: Implement.
    ASSERT_NOT_REACHED();
}

void MockWebSpeechRecognizer::addMockResult(const WebString& transcript, float confidence)
{
    m_mockTranscripts.append(transcript);
    m_mockConfidences.append(confidence);
}

void MockWebSpeechRecognizer::setError(int code, const WebString& message)
{
    m_taskList.revokeAll();
    postTask(new ErrorTask(this, code, message));
    postTask(new ClientCallTask(this, &WebSpeechRecognizerClient::didEnd));
}

MockWebSpeechRecognizer::MockWebSpeechRecognizer()
{
}

MockWebSpeechRecognizer::~MockWebSpeechRecognizer()
{
}


#endif // ENABLE(SCRIPTED_SPEECH)
