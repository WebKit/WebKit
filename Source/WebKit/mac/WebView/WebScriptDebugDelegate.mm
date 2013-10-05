/*
 * Copyright (C) 2005-2013 Apple Computer, Inc.  All rights reserved.
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

#import "WebDataSource.h"
#import "WebDataSourceInternal.h"
#import "WebFrameInternal.h"
#import "WebScriptDebugDelegate.h"
#import "WebScriptDebugger.h"
#import "WebViewInternal.h"
#import <WebCore/Frame.h>
#import <WebCore/ScriptController.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/runtime_root.h>
#import <debugger/Debugger.h>
#import <debugger/DebuggerActivation.h>
#import <interpreter/CallFrame.h>
#import <runtime/Completion.h>
#import <runtime/JSFunction.h>
#import <runtime/JSGlobalObject.h>
#import <runtime/JSLock.h>

using namespace JSC;
using namespace WebCore;

// FIXME: these error strings should be public for future use by WebScriptObject and in WebScriptObject.h
NSString * const WebScriptErrorDomain = @"WebScriptErrorDomain";
NSString * const WebScriptErrorDescriptionKey = @"WebScriptErrorDescription";
NSString * const WebScriptErrorLineNumberKey = @"WebScriptErrorLineNumber";

@interface WebScriptCallFrame (WebScriptDebugDelegateInternal)

- (id)_convertValueToObjcValue:(JSC::JSValue)value;

@end

@interface WebScriptCallFramePrivate : NSObject {
@public
    WebScriptObject        *globalObject;   // the global object's proxy (not retained)
    String functionName;
    JSC::JSValue exceptionValue;
}
@end

@implementation WebScriptCallFramePrivate
- (void)dealloc
{
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

- (WebScriptCallFrame *)_initWithGlobalObject:(WebScriptObject *)globalObj functionName:(String)functionName exceptionValue:(JSC::JSValue)exceptionValue
{
    if ((self = [super init])) {
        _private = [[WebScriptCallFramePrivate alloc] init];
        _private->globalObject = globalObj;
        _private->functionName = functionName;
        _private->exceptionValue = exceptionValue;
    }
    return self;
}

- (id)_convertValueToObjcValue:(JSC::JSValue)value
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

// Returns the name of the function for this frame, if available.
// Returns nil for anonymous functions and for the global frame.

- (NSString *)functionName
{
    if (_private->functionName.isEmpty())
        return nil;

    String functionName = _private->functionName;
    return nsStringNilIfEmpty(functionName);
}

// Returns the pending exception for this frame (nil if none).

- (id)exception
{
    if (!_private->exceptionValue)
        return nil;

    JSC::JSValue exception = _private->exceptionValue;
    return exception ? [self _convertValueToObjcValue:exception] : nil;
}

@end
