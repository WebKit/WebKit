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

#if ENABLE(INPUT_SPEECH)

#include "Task.h"
#include "platform/WebRect.h"
#include "WebSpeechInputController.h"
#include "WebSpeechInputResult.h"
#include <wtf/Compiler.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
class WebSecurityOrigin;
class WebSpeechInputListener;
class WebString;
}

class MockWebSpeechInputController : public WebKit::WebSpeechInputController {
public:
    static PassOwnPtr<MockWebSpeechInputController> create(WebKit::WebSpeechInputListener*);

    void addMockRecognitionResult(const WebKit::WebString& result, double confidence, const WebKit::WebString& language);
    void setDumpRect(bool);
    void clearResults();

    // WebSpeechInputController implementation:
    virtual bool startRecognition(int requestId, const WebKit::WebRect& elementRect, const WebKit::WebString& language, const WebKit::WebString& grammar, const WebKit::WebSecurityOrigin&) OVERRIDE;
    virtual void cancelRecognition(int requestId) OVERRIDE;
    virtual void stopRecording(int requestId) OVERRIDE;

    TaskList* taskList() { return &m_taskList; }

private:
    MockWebSpeechInputController(WebKit::WebSpeechInputListener*);
    void speechTaskFired();

    class SpeechTask : public MethodTask<MockWebSpeechInputController> {
    public:
        SpeechTask(MockWebSpeechInputController*);
        void stop();

    private:
        virtual void runIfValid() OVERRIDE;
    };

    WebKit::WebSpeechInputListener* m_listener;

    TaskList m_taskList;
    SpeechTask* m_speechTask;

    bool m_recording;
    int m_requestId;
    WebKit::WebRect m_requestRect;
    String m_language;

    HashMap<String, Vector<WebKit::WebSpeechInputResult> > m_recognitionResults;
    Vector<WebKit::WebSpeechInputResult> m_resultsForEmptyLanguage;
    bool m_dumpRect;
};

#endif // ENABLE(INPUT_SPEECH)

#endif // MockWebSpeechInputController_h
