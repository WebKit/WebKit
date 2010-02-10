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

#ifndef WebDevToolsAgentImpl_h
#define WebDevToolsAgentImpl_h

#include "APUAgentDelegate.h"
#include "DevToolsRPC.h"
#include "ToolsAgent.h"
#include "WebDevToolsAgentPrivate.h"

#include <v8.h>
#include <wtf/OwnPtr.h>

namespace WebCore {
class Document;
class InspectorController;
class Node;
class String;
}

namespace WebKit {

class DebuggerAgentDelegateStub;
class DebuggerAgentImpl;
class WebDevToolsAgentClient;
class WebFrame;
class WebFrameImpl;
class WebString;
class WebURLRequest;
class WebURLResponse;
class WebViewImpl;
struct WebURLError;
struct WebDevToolsMessageData;

class WebDevToolsAgentImpl : public WebDevToolsAgentPrivate,
                             public ToolsAgent,
                             public DevToolsRPC::Delegate {
public:
    WebDevToolsAgentImpl(WebViewImpl* webViewImpl, WebDevToolsAgentClient* client);
    virtual ~WebDevToolsAgentImpl();

    // ToolsAgent implementation.
    virtual void dispatchOnInspectorController(int callId, const WebCore::String& functionName, const WebCore::String& jsonArgs);
    virtual void dispatchOnInjectedScript(int callId, int injectedScriptId, const WebCore::String& functionName, const WebCore::String& jsonArgs, bool async);

    // WebDevToolsAgentPrivate implementation.
    virtual void didClearWindowObject(WebFrameImpl* frame);
    virtual void didCommitProvisionalLoad(WebFrameImpl* frame, bool isNewNavigation);

    // WebDevToolsAgent implementation.
    virtual void attach();
    virtual void detach();
    virtual void didNavigate();
    virtual void dispatchMessageFromFrontend(const WebDevToolsMessageData& data);
    virtual void inspectElementAt(const WebPoint& point);
    virtual void evaluateInWebInspector(long callId, const WebString& script);
    virtual void setRuntimeFeatureEnabled(const WebString& feature, bool enabled);
    virtual void setTimelineProfilingEnabled(bool enable);

    virtual void identifierForInitialRequest(unsigned long, WebFrame*, const WebURLRequest&);
    virtual void willSendRequest(unsigned long, const WebURLRequest&);
    virtual void didReceiveData(unsigned long, int length);
    virtual void didReceiveResponse(unsigned long, const WebURLResponse&);
    virtual void didFinishLoading(unsigned long);
    virtual void didFailLoading(unsigned long, const WebURLError&);

    // DevToolsRPC::Delegate implementation.
    virtual void sendRpcMessage(const WebDevToolsMessageData& data);

    void forceRepaint();

    int hostId() { return m_hostId; }

private:
    static v8::Handle<v8::Value> jsDispatchOnClient(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsDispatchToApu(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsEvaluateOnSelf(const v8::Arguments& args);
    static v8::Handle<v8::Value> jsOnRuntimeFeatureStateChanged(const v8::Arguments& args);

    void disposeUtilityContext();
    void unhideResourcesPanelIfNecessary();

    void compileUtilityScripts();
    void initDevToolsAgentHost();
    void resetInspectorFrontendProxy();
    void setApuAgentEnabled(bool enabled);

    WebCore::InspectorController* inspectorController();

    // Creates InspectorBackend v8 wrapper in the utility context so that it's
    // methods prototype is Function.protoype object from the utility context.
    // Otherwise some useful methods  defined on Function.prototype(such as bind)
    // are missing for InspectorController native methods.
    v8::Local<v8::Object> createInspectorBackendV8Wrapper();

    int m_hostId;
    WebDevToolsAgentClient* m_client;
    WebViewImpl* m_webViewImpl;
    OwnPtr<DebuggerAgentDelegateStub> m_debuggerAgentDelegateStub;
    OwnPtr<ToolsAgentDelegateStub> m_toolsAgentDelegateStub;
    OwnPtr<DebuggerAgentImpl> m_debuggerAgentImpl;
    OwnPtr<ApuAgentDelegateStub> m_apuAgentDelegateStub;
    bool m_apuAgentEnabled;
    bool m_resourceTrackingWasEnabled;
    bool m_attached;
    // TODO(pfeldman): This should not be needed once GC styles issue is fixed
    // for matching rules.
    v8::Persistent<v8::Context> m_utilityContext;
};

} // namespace WebKit

#endif
