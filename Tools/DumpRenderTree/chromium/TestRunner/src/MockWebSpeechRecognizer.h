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

#ifndef MockWebSpeechRecognizer_h
#define MockWebSpeechRecognizer_h

#include "TestCommon.h"
#include "WebSpeechRecognizer.h"
#include "WebTask.h"
#include <deque>
#include <vector>

namespace WebKit {
class WebSpeechRecognitionHandle;
class WebSpeechRecognitionParams;
class WebSpeechRecognizerClient;
}

namespace WebTestRunner {

class WebTestDelegate;

class MockWebSpeechRecognizer : public WebKit::WebSpeechRecognizer {
public:
    MockWebSpeechRecognizer();
    ~MockWebSpeechRecognizer();

    void setDelegate(WebTestDelegate*);

    // WebSpeechRecognizer implementation:
    virtual void start(const WebKit::WebSpeechRecognitionHandle&, const WebKit::WebSpeechRecognitionParams&, WebKit::WebSpeechRecognizerClient*) OVERRIDE;
    virtual void stop(const WebKit::WebSpeechRecognitionHandle&, WebKit::WebSpeechRecognizerClient*) OVERRIDE;
    virtual void abort(const WebKit::WebSpeechRecognitionHandle&, WebKit::WebSpeechRecognizerClient*) OVERRIDE;

    // Methods accessed by layout tests:
    void addMockResult(const WebKit::WebString& transcript, float confidence);
    void setError(const WebKit::WebString& error, const WebKit::WebString& message);
    bool wasAborted() const { return m_wasAborted; }

    // Methods accessed from Task objects:
    WebKit::WebSpeechRecognizerClient* client() { return m_client; }
    WebKit::WebSpeechRecognitionHandle& handle() { return m_handle; }
    WebTaskList* taskList() { return &m_taskList; }

    class Task {
    public:
        Task(MockWebSpeechRecognizer* recognizer) : m_recognizer(recognizer) { }
        virtual ~Task() { }
        virtual void run() = 0;
    protected:
        MockWebSpeechRecognizer* m_recognizer;
    };

private:
    void startTaskQueue();
    void clearTaskQueue();

    WebTaskList m_taskList;
    WebKit::WebSpeechRecognitionHandle m_handle;
    WebKit::WebSpeechRecognizerClient* m_client;
    std::vector<WebKit::WebString> m_mockTranscripts;
    std::vector<float> m_mockConfidences;
    bool m_wasAborted;

    // Queue of tasks to be run.
    std::deque<Task*> m_taskQueue;
    bool m_taskQueueRunning;

    WebTestDelegate* m_delegate;

    // Task for stepping the queue.
    class StepTask : public WebMethodTask<MockWebSpeechRecognizer> {
    public:
        StepTask(MockWebSpeechRecognizer* object) : WebMethodTask<MockWebSpeechRecognizer>(object) { }
        virtual void runIfValid() OVERRIDE;
    };
};

}

#endif // MockWebSpeechRecognizer_h
