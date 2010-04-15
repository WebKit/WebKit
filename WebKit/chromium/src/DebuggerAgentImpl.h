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

#ifndef DebuggerAgentImpl_h
#define DebuggerAgentImpl_h

#include "DebuggerAgent.h"

#include <v8.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace WebCore {
class Document;
class Frame;
class Node;
class Page;
class String;
}

namespace WebKit {

class WebDevToolsAgentImpl;
class WebViewImpl;

class DebuggerAgentImpl : public DebuggerAgent {
public:
    DebuggerAgentImpl(WebKit::WebViewImpl* webViewImpl,
                      DebuggerAgentDelegate* delegate,
                      WebDevToolsAgentImpl* webdevtoolsAgent);
    virtual ~DebuggerAgentImpl();

    // DebuggerAgent implementation.
    virtual void getContextId();
    virtual void processDebugCommands();
    virtual void setDebuggerScriptSource(const String&);

    void debuggerOutput(const WebCore::String& out);

    void setAutoContinueOnException(bool autoContinue) { m_autoContinueOnException = autoContinue; }

    bool autoContinueOnException() { return m_autoContinueOnException; }

    // Executes function with the given name in the utility context. Passes node
    // and json args as parameters. Note that the function called must be
    // implemented in the inject_dispatch.js file.
    WebCore::String executeUtilityFunction(
        v8::Handle<v8::Context> context,
        int callId,
        const char* object,
        const WebCore::String& functionName,
        const WebCore::String& jsonArgs,
        bool async,
        WebCore::String* exception);


    WebCore::Page* page();
    WebDevToolsAgentImpl* webdevtoolsAgent() { return m_webdevtoolsAgent; }

    WebKit::WebViewImpl* webView() { return m_webViewImpl; }

private:
    WebKit::WebViewImpl* m_webViewImpl;
    DebuggerAgentDelegate* m_delegate;
    WebDevToolsAgentImpl* m_webdevtoolsAgent;
    bool m_autoContinueOnException;
};

} // namespace WebKit

#endif
