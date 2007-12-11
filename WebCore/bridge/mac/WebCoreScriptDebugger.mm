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

#import "config.h"
#import "WebCoreScriptDebugger.h"

#import "DeprecatedString.h"
#import "KURL.h"
#import "PlatformString.h"
#import "WebCoreObjCExtras.h"
#import "WebScriptObjectPrivate.h"
#import <JavaScriptCore/ExecState.h>
#import <JavaScriptCore/JSGlobalObject.h>
#import <JavaScriptCore/debugger.h>
#import <JavaScriptCore/function.h>
#import <JavaScriptCore/interpreter.h>

using namespace KJS;
using namespace WebCore;

@interface WebCoreScriptDebugger (WebCoreScriptDebuggerInternal)

- (WebCoreScriptCallFrame *)_enterFrame:(ExecState *)state;
- (WebCoreScriptCallFrame *)_leaveFrame;

@end

@interface WebCoreScriptCallFrame (WebCoreScriptDebuggerInternal)

- (WebCoreScriptCallFrame *)_initWithGlobalObject:(WebScriptObject *)globalObj caller:(WebCoreScriptCallFrame *)caller state:(ExecState *)state;
- (void)_setWrapper:(id)wrapper;
- (id)_convertValueToObjcValue:(JSValue *)value;

@end

// convert UString to NSString
static NSString *toNSString(const UString &s)
{
    if (s.isEmpty()) return nil;
    return [NSString stringWithCharacters:(const unichar *)s.data() length:s.size()];
}

// convert UString to NSURL
static NSURL *toNSURL(const UString &s)
{
    if (s.isEmpty()) return nil;
    return KURL(DeprecatedString(s)).getNSURL();
}

// C++ interface to KJS debugger callbacks

class WebCoreScriptDebuggerImp : public KJS::Debugger {

  private:
    WebCoreScriptDebugger  *_objc;      // our ObjC half
    bool                    _nested;    // true => this is a nested call
    WebCoreScriptCallFrame *_current;   // top stack frame (copy of same field from ObjC side)

  public:
    // constructor
    WebCoreScriptDebuggerImp(WebCoreScriptDebugger *objc, JSGlobalObject* globalObject) : _objc(objc) {
        _nested = true;
        _current = [_objc _enterFrame:globalObject->globalExec()];
        attach(globalObject);
        [[_objc delegate] enteredFrame:_current sourceId:-1 line:-1];
        _nested = false;
    }

    // callbacks - relay to delegate
    virtual bool sourceParsed(ExecState *state, int sid, const UString &url, const UString &source, int lineNumber, int errorLine, const UString &errorMsg) {
        if (!_nested) {
            _nested = true;
            [[_objc delegate] parsedSource:toNSString(source) fromURL:toNSURL(url) sourceId:sid startLine:lineNumber errorLine:errorLine errorMessage:toNSString(errorMsg)];
            _nested = false;
        }
        return true;
    }
    virtual bool callEvent(ExecState *state, int sid, int lineno, JSObject *func, const List &args) {
        if (!_nested) {
            _nested = true;
            _current = [_objc _enterFrame:state];
            [[_objc delegate] enteredFrame:_current sourceId:sid line:lineno];
            _nested = false;
        }
        return true;
    }
    virtual bool atStatement(ExecState *state, int sid, int lineno, int lastLine) {
        if (!_nested) {
            _nested = true;
            [[_objc delegate] hitStatement:_current sourceId:sid line:lineno];
            _nested = false;
        }
        return true;
    }
    virtual bool returnEvent(ExecState *state, int sid, int lineno, JSObject *func) {
        if (!_nested) {
            _nested = true;
            [[_objc delegate] leavingFrame:_current sourceId:sid line:lineno];
            _current = [_objc _leaveFrame];
            _nested = false;
        }
        return true;
    }
    virtual bool exception(ExecState *state, int sid, int lineno, JSValue *exception) {
        if (!_nested) {
            _nested = true;
            [[_objc delegate] exceptionRaised:_current sourceId:sid line:lineno];
            _nested = false;
        }
        return true;
    }

};



// WebCoreScriptDebugger
//
// This is the main (behind-the-scenes) debugger class in WebCore.
//
// The WebCoreScriptDebugger has two faces, one for Objective-C (this class), and another (WebCoreScriptDebuggerImp)
// for C++.  The ObjC side creates the C++ side, which does the real work of attaching to the global object and
// forwarding the KJS debugger callbacks to the delegate.

@implementation WebCoreScriptDebugger

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

- (WebCoreScriptDebugger *)initWithDelegate:(id<WebScriptDebugger>)delegate
{
    if ((self = [super init])) {
        _delegate   = delegate;
        _globalObj = [_delegate globalObject];
        _debugger  = new WebCoreScriptDebuggerImp(self, [_globalObj _rootObject]->globalObject());
    }
    return self;
}

- (void)dealloc
{
    [_current release];
    delete _debugger;
    [super dealloc];
}

- (void)finalize
{
    delete _debugger;
    [super finalize];
}

- (id<WebScriptDebugger>)delegate
{
    return _delegate;
}

@end



@implementation WebCoreScriptDebugger (WebCoreScriptDebuggerInternal)

- (WebCoreScriptCallFrame *)_enterFrame:(ExecState *)state;
{
    WebCoreScriptCallFrame *callee = [[WebCoreScriptCallFrame alloc] _initWithGlobalObject:_globalObj caller:_current state:state];
    [callee _setWrapper:[_delegate newWrapperForFrame:callee]];
    return _current = callee;
}

- (WebCoreScriptCallFrame *)_leaveFrame;
{
    WebCoreScriptCallFrame *caller = [[_current caller] retain];
    [_current release];
    return _current = caller;
}

@end



// WebCoreScriptCallFrame
//
// One of these is created to represent each stack frame.  Additionally, there is a "global"
// frame to represent the outermost scope.  This global frame is always the last frame in
// the chain of callers.
//
// The delegate can assign a "wrapper" to each frame object so it can relay calls through its
// own exported interface.  This class is private to WebCore (and the delegate).

@implementation WebCoreScriptCallFrame (WebCoreScriptDebuggerInternal)

- (WebCoreScriptCallFrame *)_initWithGlobalObject:(WebScriptObject *)globalObj caller:(WebCoreScriptCallFrame *)caller state:(ExecState *)state
{
    if ((self = [super init])) {
        _globalObj = globalObj;
        _caller    = caller;    // (already retained)
        _state     = state;
    }
    return self;
}

- (void)_setWrapper:(id)wrapper
{
    _wrapper = wrapper;     // (already retained)
}

- (id)_convertValueToObjcValue:(JSValue *)value
{
    if (!value)
        return nil;

    if (value == [_globalObj _imp])
        return _globalObj;

    Bindings::RootObject* root1 = [_globalObj _originRootObject];
    if (!root1)
        return nil;

    Bindings::RootObject* root2 = [_globalObj _rootObject];
    if (!root2)
        return nil;

    return [WebScriptObject _convertValueToObjcValue:value originRootObject:root1 rootObject:root2];
}

@end



@implementation WebCoreScriptCallFrame

- (void)dealloc
{
    [_wrapper release];
    [_caller release];
    [super dealloc];
}

- (id)wrapper
{
    return _wrapper;
}

- (WebCoreScriptCallFrame *)caller
{
    return _caller;
}


// Returns an array of scope objects (most local first).
// The properties of each scope object are the variables for that scope.
// Note that the last entry in the array will _always_ be the global object (windowScriptObject),
// whose properties are the global variables.

- (NSArray *)scopeChain
{
    if (!_state->scopeNode()) {  // global frame
        return [NSArray arrayWithObject:_globalObj];
    }

    ScopeChain      chain  = _state->scopeChain();
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
    if (_state->scopeNode()) {
        FunctionImp *func = _state->function();
        if (func) {
            Identifier fn = func->functionName();
            return toNSString(fn.ustring());
        }
    }
    return nil;
}


// Returns the pending exception for this frame (nil if none).

- (id)exception
{
    if (!_state->hadException()) return nil;
    return [self _convertValueToObjcValue:_state->exception()];
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

    ExecState* state = _state;
    JSGlobalObject* globalObject = state->dynamicGlobalObject();

    // find "eval"
    JSObject *eval = NULL;
    if (state->scopeNode()) {  // "eval" won't work without context (i.e. at global scope)
        JSValue *v = globalObject->get(state, "eval");
        if (v->isObject() && static_cast<JSObject *>(v)->implementsCall())
            eval = static_cast<JSObject *>(v);
        else
            // no "eval" - fallback operates on global exec state
            state = globalObject->globalExec();
    }

    JSValue *savedException = state->exception();
    state->clearException();

    // evaluate
    JSValue *result;
    if (eval) {
        List args;
        args.append(jsString(code));
        result = eval->call(state, NULL, args);
    } else
        // no "eval", or no context (i.e. global scope) - use global fallback
        result = Interpreter::evaluate(state, UString(), 0, code.data(), code.size(), globalObject).value();

    if (state->hadException())
        result = state->exception();    // (may be redundant depending on which eval path was used)
    state->setException(savedException);

    return [self _convertValueToObjcValue:result];
}

@end
