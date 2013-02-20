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

#ifndef MockWebSpeechInputController_h
#define MockWebSpeechInputController_h

#include "TestCommon.h"
#include "WebSpeechInputController.h"
#include "WebSpeechInputResult.h"
#include "WebTask.h"
#include <map>
#include <public/WebRect.h>
#include <string>
#include <vector>

namespace WebKit {
class WebSecurityOrigin;
class WebSpeechInputListener;
class WebString;
}

namespace WebTestRunner {

class WebTestDelegate;

class MockWebSpeechInputController : public WebKit::WebSpeechInputController {
public:
    explicit MockWebSpeechInputController(WebKit::WebSpeechInputListener*);
    ~MockWebSpeechInputController();

    void addMockRecognitionResult(const WebKit::WebString& result, double confidence, const WebKit::WebString& language);
    void setDumpRect(bool);
    void clearResults();
    void setDelegate(WebTestDelegate*);

    // WebSpeechInputController implementation:
    virtual bool startRecognition(int requestId, const WebKit::WebRect& elementRect, const WebKit::WebString& language, const WebKit::WebString& grammar, const WebKit::WebSecurityOrigin&) OVERRIDE;
    virtual void cancelRecognition(int requestId) OVERRIDE;
    virtual void stopRecording(int requestId) OVERRIDE;

    WebTaskList* taskList() { return &m_taskList; }

private:
    void speechTaskFired();

    class SpeechTask : public WebMethodTask<MockWebSpeechInputController> {
    public:
        SpeechTask(MockWebSpeechInputController*);
        void stop();

    private:
        virtual void runIfValid() OVERRIDE;
    };

    WebKit::WebSpeechInputListener* m_listener;

    WebTaskList m_taskList;
    SpeechTask* m_speechTask;

    bool m_recording;
    int m_requestId;
    WebKit::WebRect m_requestRect;
    std::string m_language;

    std::map<std::string, std::vector<WebKit::WebSpeechInputResult> > m_recognitionResults;
    std::vector<WebKit::WebSpeechInputResult> m_resultsForEmptyLanguage;
    bool m_dumpRect;

    WebTestDelegate* m_delegate;
};

}

#endif // MockWebSpeechInputController_h
