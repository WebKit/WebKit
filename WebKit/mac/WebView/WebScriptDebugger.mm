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

#import "WebScriptDebugger.h"

#import "WebFrameInternal.h"
#import "WebViewInternal.h"
#import "WebScriptDebugDelegate.h"
#import <runtime/JSGlobalObject.h>
#import <kjs/DebuggerCallFrame.h>
#import <WebCore/DOMWindow.h>
#import <WebCore/Frame.h>
#import <WebCore/JSDOMWindow.h>
#import <WebCore/KURL.h>
#import <WebCore/ScriptController.h>

using namespace JSC;
using namespace WebCore;

@interface WebScriptCallFrame (WebScriptDebugDelegateInternal)
- (WebScriptCallFrame *)_initWithGlobalObject:(WebScriptObject *)globalObj caller:(WebScriptCallFrame *)caller debuggerCallFrame:(const DebuggerCallFrame&)debuggerCallFrame;
- (void)_setDebuggerCallFrame:(const DebuggerCallFrame&)debuggerCallFrame;
- (void)_clearDebuggerCallFrame;
@end

NSString *toNSString(const UString& s)
{
    if (s.isEmpty())
        return nil;
    return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(s.data()) length:s.size()];
}

NSString *toNSString(const SourceCode& s)
{
    if (!s.length())
        return nil;
    return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(s.data()) length:s.length()];
}

// convert UString to NSURL
static NSURL *toNSURL(const UString& s)
{
    if (s.isEmpty())
        return nil;
    return KURL(s);
}

static WebFrame *toWebFrame(JSGlobalObject* globalObject)
{
    JSDOMWindow* window = static_cast<JSDOMWindow*>(globalObject);
    return kit(window->impl()->frame());
}

WebScriptDebugger::WebScriptDebugger(JSGlobalObject* globalObject)
    : m_callingDelegate(false)
{
    attach(globalObject);
    callEvent(globalObject->globalExec(), 0, -1);
}

// callbacks - relay to delegate
void WebScriptDebugger::sourceParsed(ExecState* exec, const SourceCode& source, int errorLine, const UString& errorMsg)
{
    if (m_callingDelegate)
        return;

    m_callingDelegate = true;

    NSString *nsSource = toNSString(source);
    NSURL *nsURL = toNSURL(source.provider()->url());

    WebFrame *webFrame = toWebFrame(exec->dynamicGlobalObject());
    WebView *webView = [webFrame webView];
    if (errorLine == -1) {
        [[webView _scriptDebugDelegateForwarder] webView:webView didParseSource:nsSource baseLineNumber:source.firstLine() fromURL:nsURL sourceId:static_cast<int>(source.provider()->asID()) forWebFrame:webFrame];
        [[webView _scriptDebugDelegateForwarder] webView:webView didParseSource:nsSource fromURL:[nsURL absoluteString] sourceId:static_cast<int>(source.provider()->asID()) forWebFrame:webFrame]; // deprecated delegate method
    } else {
        NSString* nsErrorMessage = toNSString(errorMsg);
        NSDictionary *info = [[NSDictionary alloc] initWithObjectsAndKeys:nsErrorMessage, WebScriptErrorDescriptionKey, [NSNumber numberWithUnsignedInt:errorLine], WebScriptErrorLineNumberKey, nil];
        NSError *error = [[NSError alloc] initWithDomain:WebScriptErrorDomain code:WebScriptGeneralErrorCode userInfo:info];
        [[webView _scriptDebugDelegateForwarder] webView:webView failedToParseSource:nsSource baseLineNumber:source.firstLine() fromURL:nsURL withError:error forWebFrame:webFrame];
        [error release];
        [info release];
    }

    m_callingDelegate = false;
}

void WebScriptDebugger::callEvent(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_callingDelegate)
        return;

    m_callingDelegate = true;

    WebFrame *webFrame = toWebFrame(debuggerCallFrame.dynamicGlobalObject());

    m_topCallFrame.adoptNS([[WebScriptCallFrame alloc] _initWithGlobalObject:core(webFrame)->script()->windowScriptObject() caller:m_topCallFrame.get() debuggerCallFrame:debuggerCallFrame]);

    WebView *webView = [webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView didEnterCallFrame:m_topCallFrame.get() sourceId:static_cast<int>(sourceID) line:lineNumber forWebFrame:webFrame];

    m_callingDelegate = false;
}

void WebScriptDebugger::atStatement(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_callingDelegate)
        return;

    m_callingDelegate = true;

    WebFrame *webFrame = toWebFrame(debuggerCallFrame.dynamicGlobalObject());
    WebView *webView = [webFrame webView];

    [m_topCallFrame.get() _setDebuggerCallFrame:debuggerCallFrame];
    [[webView _scriptDebugDelegateForwarder] webView:webView willExecuteStatement:m_topCallFrame.get() sourceId:static_cast<int>(sourceID) line:lineNumber forWebFrame:webFrame];

    m_callingDelegate = false;
}

void WebScriptDebugger::returnEvent(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_callingDelegate)
        return;

    m_callingDelegate = true;

    WebFrame *webFrame = toWebFrame(debuggerCallFrame.dynamicGlobalObject());
    WebView *webView = [webFrame webView];

    [m_topCallFrame.get() _setDebuggerCallFrame:debuggerCallFrame];
    [[webView _scriptDebugDelegateForwarder] webView:webView willLeaveCallFrame:m_topCallFrame.get() sourceId:static_cast<int>(sourceID) line:lineNumber forWebFrame:webFrame];

    [m_topCallFrame.get() _clearDebuggerCallFrame];
    m_topCallFrame = [m_topCallFrame.get() caller];

    m_callingDelegate = false;
}

void WebScriptDebugger::exception(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_callingDelegate)
        return;

    m_callingDelegate = true;

    WebFrame *webFrame = toWebFrame(debuggerCallFrame.dynamicGlobalObject());
    WebView *webView = [webFrame webView];
    [m_topCallFrame.get() _setDebuggerCallFrame:debuggerCallFrame];

    [[webView _scriptDebugDelegateForwarder] webView:webView exceptionWasRaised:m_topCallFrame.get() sourceId:static_cast<int>(sourceID) line:lineNumber forWebFrame:webFrame];

    m_callingDelegate = false;
}

void WebScriptDebugger::willExecuteProgram(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineno)
{
}

void WebScriptDebugger::didExecuteProgram(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineno)
{
}

void WebScriptDebugger::didReachBreakpoint(const DebuggerCallFrame&, intptr_t, int)
{
    return;
}
