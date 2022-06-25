/*
 * Copyright (C) 2008-2013 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "WebDelegateImplementationCaching.h"
#import "WebFrameInternal.h"
#import "WebScriptDebugDelegate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/Breakpoint.h>
#import <JavaScriptCore/DebuggerCallFrame.h>
#import <JavaScriptCore/JSGlobalObject.h>
#import <JavaScriptCore/SourceProvider.h>
#import <JavaScriptCore/StrongInlines.h>
#import <WebCore/DOMWindow.h>
#import <WebCore/Frame.h>
#import <WebCore/JSDOMWindow.h>
#import <WebCore/ScriptController.h>
#import <wtf/URL.h>

@interface WebScriptCallFrame (WebScriptDebugDelegateInternalForDebugger)
- (WebScriptCallFrame *)_initWithGlobalObject:(WebScriptObject *)globalObj functionName:(String)functionName exceptionValue:(JSC::JSValue)exceptionValue;
@end

static NSString *toNSString(JSC::SourceProvider* sourceProvider)
{
    const String& sourceString = sourceProvider->source().toString();
    if (sourceString.isEmpty())
        return nil;
    return sourceString;
}

static WebFrame *toWebFrame(JSC::JSGlobalObject* globalObject)
{
    WebCore::JSDOMWindow* window = static_cast<WebCore::JSDOMWindow*>(globalObject);
    return kit(window->wrapped().frame());
}

WebScriptDebugger::WebScriptDebugger(JSC::JSGlobalObject* globalObject)
    : Debugger(globalObject->vm())
    , m_callingDelegate(false)
    , m_globalObject(globalObject->vm(), globalObject)
{
    setPauseOnAllExceptionsBreakpoint(JSC::Breakpoint::create(JSC::noBreakpointID));
    deactivateBreakpoints();
    attach(globalObject);
}

// callbacks - relay to delegate
void WebScriptDebugger::sourceParsed(JSC::JSGlobalObject* lexicalGlobalObject, JSC::SourceProvider* sourceProvider, int errorLine, const String& errorMsg)
{
    if (m_callingDelegate)
        return;

    m_callingDelegate = true;

    NSString *nsSource = toNSString(sourceProvider);
    NSURL *nsURL = sourceProvider->sourceOrigin().url();
    int firstLine = sourceProvider->startPosition().m_line.oneBasedInt();

    JSC::VM& vm = lexicalGlobalObject->vm();
    WebFrame *webFrame = toWebFrame(vm.deprecatedVMEntryGlobalObject(lexicalGlobalObject));
    WebView *webView = [webFrame webView];
    WebScriptDebugDelegateImplementationCache* implementations = WebViewGetScriptDebugDelegateImplementations(webView);

    if (errorLine == -1) {
        if (implementations->didParseSourceFunc) {
            if (implementations->didParseSourceExpectsBaseLineNumber)
                CallScriptDebugDelegate(implementations->didParseSourceFunc, webView, @selector(webView:didParseSource:baseLineNumber:fromURL:sourceId:forWebFrame:), nsSource, firstLine, nsURL, sourceProvider->asID(), webFrame);
            else
                CallScriptDebugDelegate(implementations->didParseSourceFunc, webView, @selector(webView:didParseSource:fromURL:sourceId:forWebFrame:), nsSource, [nsURL absoluteString], sourceProvider->asID(), webFrame);
        }
    } else {
        NSDictionary *info;
        if (errorMsg.isEmpty()) {
            info = @{
                WebScriptErrorLineNumberKey: @(errorLine),
            };
        } else {
            info = @{
                WebScriptErrorDescriptionKey: (NSString *)errorMsg,
                WebScriptErrorLineNumberKey: @(errorLine),
            };
        }
        auto error = adoptNS([[NSError alloc] initWithDomain:WebScriptErrorDomain code:WebScriptGeneralErrorCode userInfo:info]);

        if (implementations->failedToParseSourceFunc)
            CallScriptDebugDelegate(implementations->failedToParseSourceFunc, webView, @selector(webView:failedToParseSource:baseLineNumber:fromURL:withError:forWebFrame:), nsSource, firstLine, nsURL, error.get(), webFrame);
    }

    m_callingDelegate = false;
}

void WebScriptDebugger::handlePause(JSC::JSGlobalObject* globalObject, Debugger::ReasonForPause reason)
{
    if (m_callingDelegate)
        return;

    if (reason != Debugger::PausedForException)
        return;

    m_callingDelegate = true;

    JSC::VM& vm = globalObject->vm();
    WebFrame *webFrame = toWebFrame(globalObject);
    WebView *webView = [webFrame webView];
    JSC::DebuggerCallFrame& debuggerCallFrame = currentDebuggerCallFrame();
    JSC::JSValue exceptionValue = currentException();
    String functionName = debuggerCallFrame.functionName(vm);
    RetainPtr<WebScriptCallFrame> webCallFrame = adoptNS([[WebScriptCallFrame alloc] _initWithGlobalObject:core(webFrame)->script().windowScriptObject() functionName:functionName exceptionValue:exceptionValue]);

    WebScriptDebugDelegateImplementationCache* cache = WebViewGetScriptDebugDelegateImplementations(webView);
    if (cache->exceptionWasRaisedFunc) {
        if (cache->exceptionWasRaisedExpectsHasHandlerFlag) {
            bool hasHandler = hasHandlerForExceptionCallback();
            CallScriptDebugDelegate(cache->exceptionWasRaisedFunc, webView, @selector(webView:exceptionWasRaised:hasHandler:sourceId:line:forWebFrame:), webCallFrame.get(), hasHandler, debuggerCallFrame.sourceID(), debuggerCallFrame.line(), webFrame);
        } else
            CallScriptDebugDelegate(cache->exceptionWasRaisedFunc, webView, @selector(webView:exceptionWasRaised:sourceId:line:forWebFrame:), webCallFrame.get(), debuggerCallFrame.sourceID(), debuggerCallFrame.line(), webFrame);
    }

    m_callingDelegate = false;
}
