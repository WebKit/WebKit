/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DRTDevToolsAgent_h
#define DRTDevToolsAgent_h

#include "DRTDevToolsCallArgs.h"
#include "Task.h"
#include "public/WebDevToolsAgentClient.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>

namespace WebKit {

class WebCString;
class WebDevToolsAgent;
class WebString;
class WebView;
struct WebDevToolsMessageData;

} // namespace WebKit

class DRTDevToolsClient;

class DRTDevToolsAgent : public WebKit::WebDevToolsAgentClient
                       , public Noncopyable {
public:
    DRTDevToolsAgent();
    virtual ~DRTDevToolsAgent() {}
    void reset();

    void setWebView(WebKit::WebView*);

    // WebDevToolsAgentClient implementation.
    virtual void sendMessageToInspectorFrontend(const WebKit::WebString&);
    virtual int hostIdentifier() { return m_routingID; }
    virtual void forceRepaint();
    virtual void runtimePropertyChanged(const WebKit::WebString& name, const WebKit::WebString& value);
    virtual WebKit::WebCString debuggerScriptSource();
    virtual WebKitClientMessageLoop* createClientMessageLoop();

    void asyncCall(const DRTDevToolsCallArgs&);

    void attach(DRTDevToolsClient*);
    void detach();
    void frontendLoaded();

    bool evaluateInWebInspector(long callID, const std::string& script);
    bool setTimelineProfilingEnabled(bool enable);
    TaskList* taskList() { return &m_taskList; }

private:
    void call(const DRTDevToolsCallArgs&);
    void delayedFrontendLoaded();
    static void dispatchMessageLoop();
    WebKit::WebDevToolsAgent* webDevToolsAgent();

    class AsyncCallTask: public MethodTask<DRTDevToolsAgent> {
    public:
        AsyncCallTask(DRTDevToolsAgent* object, const DRTDevToolsCallArgs& args)
            : MethodTask<DRTDevToolsAgent>(object), m_args(args) {}
        virtual void runIfValid() { m_object->call(m_args); }
    private:
        DRTDevToolsCallArgs m_args;
    };

    struct DelayedFrontendLoadedTask: public MethodTask<DRTDevToolsAgent> {
        DelayedFrontendLoadedTask(DRTDevToolsAgent* object) : MethodTask<DRTDevToolsAgent>(object) {}
        virtual void runIfValid() { m_object->delayedFrontendLoaded(); }
    };

    TaskList m_taskList;
    DRTDevToolsClient* m_drtDevToolsClient;
    int m_routingID;
    WebKit::WebDevToolsAgent* m_webDevToolsAgent;
    WebKit::WebView* m_webView;
};

#endif // DRTDevToolsAgent_h
