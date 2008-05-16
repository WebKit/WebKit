/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "WebScriptDebugger.h"

#include "WebFrameInternal.h"
#include "WebViewInternal.h"
#include "WebScriptDebugDelegate.h"
#include <JavaScriptCore/JSGlobalObject.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/Frame.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/KURL.h>

using namespace KJS;
using namespace WebCore;

@interface WebScriptCallFrame (WebScriptDebugDelegateInternal)

- (WebScriptCallFrame *)_initWithGlobalObject:(WebScriptObject *)globalObj caller:(WebScriptCallFrame *)caller state:(ExecState *)state;

@end

// convert UString to NSString
NSString *toNSString(const UString& s)
{
    if (s.isEmpty())
        return nil;
    return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(s.data()) length:s.size()];
}

// convert UString to NSURL
static NSURL *toNSURL(const UString& s)
{
    if (s.isEmpty())
        return nil;
    return KURL(s);
}

static WebFrame *toWebFrame(ExecState* state)
{
    JSDOMWindow* window = static_cast<JSDOMWindow*>(state->dynamicGlobalObject());
    return kit(window->impl()->frame());
}

WebScriptDebugger::WebScriptDebugger(JSGlobalObject* globalObject)
    : m_callingDelegate(false)
{
    attach(globalObject);
    List emptyList;
    callEvent(globalObject->globalExec(), -1, -1, 0, emptyList);
}

// callbacks - relay to delegate
bool WebScriptDebugger::sourceParsed(ExecState* state, int sourceID, const UString& url, const UString& source, int lineNumber, int errorLine, const UString& errorMsg)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;

    NSString *nsSource = toNSString(source);
    NSURL *nsURL = toNSURL(url);

    WebFrame *webFrame = toWebFrame(state);
    WebView *webView = [webFrame webView];
    if (errorLine == -1) {
        [[webView _scriptDebugDelegateForwarder] webView:webView didParseSource:nsSource baseLineNumber:lineNumber fromURL:nsURL sourceId:sourceID forWebFrame:webFrame];
        [[webView _scriptDebugDelegateForwarder] webView:webView didParseSource:nsSource fromURL:[nsURL absoluteString] sourceId:sourceID forWebFrame:webFrame]; // deprecated delegate method
    } else {
        NSString* nsErrorMessage = toNSString(errorMsg);
        NSDictionary *info = [[NSDictionary alloc] initWithObjectsAndKeys:nsErrorMessage, WebScriptErrorDescriptionKey, [NSNumber numberWithUnsignedInt:errorLine], WebScriptErrorLineNumberKey, nil];
        NSError *error = [[NSError alloc] initWithDomain:WebScriptErrorDomain code:WebScriptGeneralErrorCode userInfo:info];
        [[webView _scriptDebugDelegateForwarder] webView:webView failedToParseSource:nsSource baseLineNumber:lineNumber fromURL:nsURL withError:error forWebFrame:webFrame];
        [error release];
        [info release];
    }

    m_callingDelegate = false;

    return true;
}

bool WebScriptDebugger::callEvent(ExecState* state, int sourceID, int lineNumber, JSObject*, const List&)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;

    WebFrame *webFrame = toWebFrame(state);

    m_topCallFrame.adoptNS([[WebScriptCallFrame alloc] _initWithGlobalObject:core(webFrame)->windowScriptObject() caller:m_topCallFrame.get() state:state]);

    WebView *webView = [webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView didEnterCallFrame:m_topCallFrame.get() sourceId:sourceID line:lineNumber forWebFrame:webFrame];

    m_callingDelegate = false;

    return true;
}

bool WebScriptDebugger::atStatement(ExecState* state, int sourceID, int lineNumber, int)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;

    WebFrame *webFrame = toWebFrame(state);
    WebView *webView = [webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView willExecuteStatement:m_topCallFrame.get() sourceId:sourceID line:lineNumber forWebFrame:webFrame];

    m_callingDelegate = false;

    return true;
}

bool WebScriptDebugger::returnEvent(ExecState* state, int sourceID, int lineNumber, JSObject*)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;

    WebFrame *webFrame = toWebFrame(state);
    WebView *webView = [webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView willLeaveCallFrame:m_topCallFrame.get() sourceId:sourceID line:lineNumber forWebFrame:webFrame];

    m_topCallFrame = [m_topCallFrame.get() caller];

    m_callingDelegate = false;

    return true;
}

bool WebScriptDebugger::exception(ExecState* state, int sourceID, int lineNumber, JSValue*)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;

    WebFrame *webFrame = toWebFrame(state);
    WebView *webView = [webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView exceptionWasRaised:m_topCallFrame.get() sourceId:sourceID line:lineNumber forWebFrame:webFrame];

    m_callingDelegate = false;

    return true;
}
