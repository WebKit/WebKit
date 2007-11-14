/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebKitDLL.h"
#include "WebScriptDebugger.h"

#include "IWebView.h"
#include "WebFrame.h"
#include "WebScriptCallFrame.h"
#include "WebScriptDebugServer.h"

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/kjs_binding.h>
#include <WebCore/kjs_proxy.h>
#include <WebCore/PlatformString.h>
#pragma warning(pop)

using namespace WebCore;
using namespace KJS;

WebScriptDebugger::WebScriptDebugger(WebFrame* frame)
    : m_frame(frame)
{
    ASSERT(m_frame);

    KJSProxy* proxy = core(m_frame)->scriptProxy();
    if (!proxy)
        return;

    Interpreter* interp(proxy->interpreter());
    attach(interp);

    m_frame->webView(&m_webView);
    ASSERT(m_webView);

    callEvent(interp->globalExec(), -1, -1, 0, List());
}

bool WebScriptDebugger::sourceParsed(ExecState*, int sourceId, const UString& sourceURL,
                  const UString& source, int startingLineNumber, int errorLine, const UString& /*errorMsg*/)
{
    if (WebScriptDebugServer::listenerCount() <= 0)
        return true;

    BString bSource = String(source);
    BString bSourceURL = String(sourceURL);
    
    if (errorLine == -1) {
        WebScriptDebugServer::sharedWebScriptDebugServer()->didParseSource(m_webView.get(),
            bSource,
            startingLineNumber,
            bSourceURL,
            sourceId,
            m_frame);
    } else {
        // FIXME: the error var should be made with the information in the errorMsg.  It is not a simple
        // UString to BSTR conversion there is some logic involved that I don't fully understand yet.
        BString error(L"An Error Occurred.");
        WebScriptDebugServer::sharedWebScriptDebugServer()->failedToParseSource(m_webView.get(),
            bSource,
            startingLineNumber,
            bSourceURL,
            error,
            m_frame);
    }

    return true;
}

bool WebScriptDebugger::callEvent(ExecState* state, int sourceId, int lineno, JSObject* /*function*/, const List &/*args*/)
{
    enterFrame(state);
    WebScriptDebugServer::sharedWebScriptDebugServer()->didEnterCallFrame(m_webView.get(), m_topStackFrame.get(), sourceId, lineno, m_frame);
    return true;
}

bool WebScriptDebugger::atStatement(ExecState*, int sourceId, int firstLine, int /*lastLine*/)
{
    WebScriptDebugServer::sharedWebScriptDebugServer()->willExecuteStatement(m_webView.get(), m_topStackFrame.get(), sourceId, firstLine, m_frame);
    return true;
}

bool WebScriptDebugger::returnEvent(ExecState*, int sourceId, int lineno, JSObject* /*function*/)
{
    leaveFrame();
    WebScriptDebugServer::sharedWebScriptDebugServer()->willLeaveCallFrame(m_webView.get(), m_topStackFrame.get(), sourceId, lineno, m_frame);
    return true;
}

bool WebScriptDebugger::exception(ExecState*, int sourceId, int lineno, JSValue* /*exception */)
{
    WebScriptDebugServer::sharedWebScriptDebugServer()->exceptionWasRaised(m_webView.get(), m_topStackFrame.get(), sourceId, lineno, m_frame);
    return true;
}

void WebScriptDebugger::enterFrame(ExecState* state)
{
    m_topStackFrame = WebScriptCallFrame::createInstance(state, m_topStackFrame.get()); // Set the top as the caller
}

void WebScriptDebugger::leaveFrame()
{
    if (!m_topStackFrame)
        return;

    COMPtr<IWebScriptCallFrame> caller;
    if (FAILED(m_topStackFrame->caller(&caller)))
        return;

    if (caller)
        m_topStackFrame = caller;
}
