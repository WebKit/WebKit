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
#include "WebScriptDebugServer.h"

#include <WebCore/BString.h>
#include <WebCore/kjs_binding.h>
#include <WebCore/kjs_proxy.h>
#include <WebCore/PlatformString.h>

#include <WebCore/COMPtr.h>

using namespace WebCore;

WebScriptDebugger::WebScriptDebugger(WebFrame* frame)
    : m_frame(frame)
{
    ASSERT(m_frame);
    if (KJSProxy* proxy = core(m_frame)->scriptProxy())
        attach(static_cast<KJS::Interpreter*>(proxy->interpreter()));
}

bool WebScriptDebugger::sourceParsed(KJS::ExecState*, int sourceId, const KJS::UString& sourceURL,
                  const KJS::UString& source, int startingLineNumber, int errorLine, const KJS::UString& /*errorMsg*/)
{
    if (WebScriptDebugServer::listenerCount() <= 0)
        return true;

    COMPtr<IWebView> webView;
    if (FAILED(m_frame->webView(&webView)))
        return false;

    BString bSource = String(source);
    BString bSourceURL = String(sourceURL);
    
    if (errorLine == -1) {
        WebScriptDebugServer::sharedWebScriptDebugServer()->didParseSource(webView.get(),
            bSource,
            startingLineNumber,
            bSourceURL,
            sourceId,
            m_frame);
    } else {
        // FIXME: the error var should be made with the information in the errorMsg.  It is not a simple
        // UString to BSTR conversion there is some logic involved that I don't fully understand yet.
        BString error(L"An Error Occurred.");
        WebScriptDebugServer::sharedWebScriptDebugServer()->failedToParseSource(webView.get(),
            bSource,
            startingLineNumber,
            bSourceURL,
            error,
            m_frame);
    }

    return true;
}

