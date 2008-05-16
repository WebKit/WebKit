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

#import "WebScriptDebugger.h"
#import "WebDataSource.h"
#import "WebDataSourceInternal.h"
#import "WebFrameInternal.h"
#import "WebScriptDebugDelegate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/ExecState.h>
#import <JavaScriptCore/JSGlobalObject.h>
#import <JavaScriptCore/function.h>
#import <JavaScriptCore/interpreter.h>
#import <WebCore/Frame.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/kjs_proxy.h>
#import <WebCore/runtime_root.h>

using namespace KJS;
using namespace WebCore;

// FIXME: these error strings should be public for future use by WebScriptObject and in WebScriptObject.h
NSString * const WebScriptErrorDomain = @"WebScriptErrorDomain";
NSString * const WebScriptErrorDescriptionKey = @"WebScriptErrorDescription";
NSString * const WebScriptErrorLineNumberKey = @"WebScriptErrorLineNumber";

@interface WebScriptCallFrame (WebScriptDebugDelegateInternal)

- (id)_convertValueToObjcValue:(JSValue *)value;

@end

@interface WebScriptCallFramePrivate : NSObject {
@public
    WebScriptObject        *globalObject;   // the global object's proxy (not retained)
    WebScriptCallFrame     *caller;         // previous stack frame
    KJS::ExecState         *state;
}
@end

@implementation WebScriptCallFramePrivate
- (void)dealloc
{
    [caller release];
    [super dealloc];
}
@end

// WebScriptCallFrame
//
// One of these is created to represent each stack frame.  Additionally, there is a "global"
// frame to represent the outermost scope.  This global frame is always the last frame in
// the chain of callers.
//
// The delegate can assign a "wrapper" to each frame object so it can relay calls through its
// own exported interface.  This class is private to WebCore (and the delegate).

@implementation WebScriptCallFrame (WebScriptDebugDelegateInternal)

- (WebScriptCallFrame *)_initWithGlobalObject:(WebScriptObject *)globalObj caller:(WebScriptCallFrame *)caller state:(ExecState *)state
{
    if ((self = [super init])) {
        _private = [[WebScriptCallFramePrivate alloc] init];
        _private->globalObject = globalObj;
        _private->caller = [caller retain];
        _private->state = state;
    }
    return self;
}

- (id)_convertValueToObjcValue:(JSValue *)value
{
    if (!value)
        return nil;

    WebScriptObject *globalObject = _private->globalObject;
    if (value == [globalObject _imp])
        return globalObject;

    Bindings::RootObject* root1 = [globalObject _originRootObject];
    if (!root1)
        return nil;

    Bindings::RootObject* root2 = [globalObject _rootObject];
    if (!root2)
        return nil;

    return [WebScriptObject _convertValueToObjcValue:value originRootObject:root1 rootObject:root2];
}

@end



@implementation WebScriptCallFrame

- (void) dealloc
{
    [_userInfo release];
    [_private release];
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
    return _private->caller;
}

// Returns an array of scope objects (most local first).
// The properties of each scope object are the variables for that scope.
// Note that the last entry in the array will _always_ be the global object (windowScriptObject),
// whose properties are the global variables.

- (NSArray *)scopeChain
{
    ExecState* state = _private->state;
    if (!state->scopeNode())  // global frame
        return [NSArray arrayWithObject:_private->globalObject];

    ScopeChain      chain  = state->scopeChain();
    NSMutableArray *scopes = [[NSMutableArray alloc] init];

    while (!chain.isEmpty()) {
        [scopes addObject:[self _convertValueToObjcValue:chain.top()]];
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
    ExecState* state = _private->state;
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
    ExecState* state = _private->state;
    if (!state->hadException())
        return nil;
    return [self _convertValueToObjcValue:state->exception()];
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

    ExecState* state = _private->state;
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

    return [self _convertValueToObjcValue:result];
}

@end
