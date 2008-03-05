/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import "WebScriptDebugDelegatePrivate.h"

#import "WebCoreScriptDebugger.h"
#import "WebDataSource.h"
#import "WebDataSourceInternal.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/ExecState.h>
#import <JavaScriptCore/JSGlobalObject.h>
#import <JavaScriptCore/function.h>
#import <JavaScriptCore/interpreter.h>
#import <WebCore/Frame.h>

using namespace KJS;
using namespace WebCore;

// FIXME: these error strings should be public for future use by WebScriptObject and in WebScriptObject.h
NSString * const WebScriptErrorDomain = @"WebScriptErrorDomain";
NSString * const WebScriptErrorDescriptionKey = @"WebScriptErrorDescription";
NSString * const WebScriptErrorLineNumberKey = @"WebScriptErrorLineNumber";

@interface WebScriptCallFrame (WebScriptDebugDelegateInternal)

- (WebScriptCallFrame *)_initWithFrame:(WebCoreScriptCallFrame *)frame;

@end

@implementation WebScriptDebugger

- (WebScriptDebugger *)initWithWebFrame:(WebFrame *)webFrame
{
    if ((self = [super init])) {
        _webFrame = webFrame;
        _debugger = [[WebCoreScriptDebugger alloc] initWithDelegate:self];
    }
    return self;
}

- (void)dealloc
{
    [_debugger release];
    [super dealloc];
}

- (WebScriptObject *)globalObject
{
    return core(_webFrame)->windowScriptObject();
}

- (id)newWrapperForFrame:(WebCoreScriptCallFrame *)frame
{
    return [[WebScriptCallFrame alloc] _initWithFrame:frame];
}

- (void)parsedSource:(NSString *)source fromURL:(NSURL *)url sourceId:(int)sid startLine:(int)startLine errorLine:(int)errorLine errorMessage:(NSString *)errorMessage
{
    WebView *webView = [_webFrame webView];
    if (errorLine == -1) {
        [[webView _scriptDebugDelegateForwarder] webView:webView didParseSource:source baseLineNumber:startLine fromURL:url sourceId:sid forWebFrame:_webFrame];
        [[webView _scriptDebugDelegateForwarder] webView:webView didParseSource:source fromURL:[url absoluteString] sourceId:sid forWebFrame:_webFrame]; // deprecated delegate method
        if ([WebScriptDebugServer listenerCount])
            [[WebScriptDebugServer sharedScriptDebugServer] webView:webView didParseSource:source baseLineNumber:startLine fromURL:url sourceId:sid forWebFrame:_webFrame];
    } else {
        NSDictionary *info = [[NSDictionary alloc] initWithObjectsAndKeys:errorMessage, WebScriptErrorDescriptionKey, [NSNumber numberWithUnsignedInt:errorLine], WebScriptErrorLineNumberKey, nil];
        NSError *error = [[NSError alloc] initWithDomain:WebScriptErrorDomain code:WebScriptGeneralErrorCode userInfo:info];
        [[webView _scriptDebugDelegateForwarder] webView:webView failedToParseSource:source baseLineNumber:startLine fromURL:url withError:error forWebFrame:_webFrame];
        if ([WebScriptDebugServer listenerCount])
            [[WebScriptDebugServer sharedScriptDebugServer] webView:webView failedToParseSource:source baseLineNumber:startLine fromURL:url withError:error forWebFrame:_webFrame];
        [error release];
        [info release];
    }
}

- (void)enteredFrame:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno
{
    WebView *webView = [_webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView didEnterCallFrame:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:webView didEnterCallFrame:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
}

- (void)hitStatement:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno
{
    WebView *webView = [_webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView willExecuteStatement:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:webView willExecuteStatement:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
}

- (void)leavingFrame:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno
{
    WebView *webView = [_webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView willLeaveCallFrame:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:webView willLeaveCallFrame:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
}

- (void)exceptionRaised:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno
{
    WebView *webView = [_webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView exceptionWasRaised:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:webView exceptionWasRaised:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
}

@end



@implementation WebScriptCallFrame (WebScriptDebugDelegateInternal)

- (WebScriptCallFrame *)_initWithFrame:(WebCoreScriptCallFrame *)frame
{
    if ((self = [super init])) {
        _private = frame;
    }
    return self;
}

@end



@implementation WebScriptCallFrame

- (void) dealloc
{
    [_userInfo release];
    [super dealloc];
}

- (void)setUserInfo:(id)userInfo
{
    if (userInfo != _userInfo) {
        [_userInfo release];
        _userInfo = [userInfo retain];
    }
}

- (id)userInfo
{
    return _userInfo;
}

- (WebScriptCallFrame *)caller
{
    return [[_private caller] wrapper];
}

// Returns an array of scope objects (most local first).
// The properties of each scope object are the variables for that scope.
// Note that the last entry in the array will _always_ be the global object (windowScriptObject),
// whose properties are the global variables.

- (NSArray *)scopeChain
{
    ExecState* state = [_private state];
    if (!state->scopeNode())  // global frame
        return [NSArray arrayWithObject:[_private globalObject]];

    ScopeChain      chain  = state->scopeChain();
    NSMutableArray *scopes = [[NSMutableArray alloc] init];

    while (!chain.isEmpty()) {
        [scopes addObject:[_private _convertValueToObjcValue:chain.top()]];
        chain.pop();
    }

    NSArray *result = [NSArray arrayWithArray:scopes];
    [scopes release];
    return result;
}

// Returns the name of the function for this frame, if available.
// Returns nil for anonymous functions and for the global frame.

- (NSString *)functionName
{
    ExecState* state = [_private state];
    if (!state->scopeNode())
        return nil;

    FunctionImp* func = state->function();
    if (!func)
        return nil;

    Identifier fn = func->functionName();
    return toNSString(fn.ustring());
}

// Returns the pending exception for this frame (nil if none).

- (id)exception
{
    ExecState* state = [_private state];
    if (!state->hadException())
        return nil;
    return [_private _convertValueToObjcValue:state->exception()];
}

// Evaluate some JavaScript code in the context of this frame.
// The code is evaluated as if by "eval", and the result is returned.
// If there is an (uncaught) exception, it is returned as though _it_ were the result.
// Calling this method on the global frame is not quite the same as calling the WebScriptObject
// method of the same name, due to the treatment of exceptions.

// FIXME: If "script" contains var declarations, the machinery to handle local variables
// efficiently in JavaScriptCore will not work properly. This could lead to crashes or
// incorrect variable values. So this is not appropriate for evaluating arbitrary script.
- (id)evaluateWebScript:(NSString *)script
{
    JSLock lock;

    UString code = String(script);

    ExecState* state = [_private state];
    JSGlobalObject* globalObject = state->dynamicGlobalObject();

    // find "eval"
    JSObject* eval = NULL;
    if (state->scopeNode()) {  // "eval" won't work without context (i.e. at global scope)
        JSValue* v = globalObject->get(state, "eval");
        if (v->isObject() && static_cast<JSObject*>(v)->implementsCall())
            eval = static_cast<JSObject*>(v);
        else
            // no "eval" - fallback operates on global exec state
            state = globalObject->globalExec();
    }

    JSValue* savedException = state->exception();
    state->clearException();

    // evaluate
    JSValue* result;
    if (eval) {
        List args;
        args.append(jsString(code));
        result = eval->call(state, 0, args);
    } else
        // no "eval", or no context (i.e. global scope) - use global fallback
        result = Interpreter::evaluate(state, UString(), 0, code.data(), code.size(), globalObject).value();

    if (state->hadException())
        result = state->exception();    // (may be redundant depending on which eval path was used)
    state->setException(savedException);

    return [_private _convertValueToObjcValue:result];
}

@end
