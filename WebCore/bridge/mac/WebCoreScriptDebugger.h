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

#import <Foundation/Foundation.h>

@class WebScriptObject;         // from JavaScriptCore
@class WebCoreScriptCallFrame;  // below

#ifdef __cplusplus
class WebCoreScriptDebuggerImp;
namespace KJS { class ExecState; }
using KJS::ExecState;
#else
@class WebCoreScriptDebuggerImp;
@class ExecState;
#endif



// "WebScriptDebugger" protocol - must be implemented by a delegate

@protocol WebScriptDebugger

- (WebScriptObject *)globalObject;                          // return the WebView's windowScriptObject
- (id)newWrapperForFrame:(WebCoreScriptCallFrame *)frame;   // return a (retained) stack-frame object

// debugger callbacks
- (void)parsedSource:(NSString *)source fromURL:(NSString *)url sourceId:(int)sid;
- (void)enteredFrame:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno;
- (void)hitStatement:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno;
- (void)leavingFrame:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno;

@end



@interface WebCoreScriptDebugger : NSObject
{
@private
    id<WebScriptDebugger>     _delegate;      // interface to WebKit (not retained)
    WebScriptObject          *_globalObj;     // the global object's proxy (not retained)
    WebCoreScriptCallFrame   *_current;       // top of stack
    WebCoreScriptDebuggerImp *_debugger;      // [KJS::Debugger]
}

- (WebCoreScriptDebugger *)initWithDelegate:(id<WebScriptDebugger>)delegate;
- (id<WebScriptDebugger>)delegate;

@end



@interface WebCoreScriptCallFrame : NSObject
{
@private
    id                        _wrapper;       // WebKit's version of this object
    WebScriptObject          *_globalObj;     // the global object's proxy (not retained)
    WebCoreScriptCallFrame   *_caller;        // previous stack frame
    ExecState                *_state;         // [KJS::ExecState]
}

- (id)wrapper;
- (WebCoreScriptCallFrame *)caller;

- (NSArray *)scopeChain;
- (NSString *)functionName;
- (id)exception;
- (id)evaluateWebScript:(NSString *)script;

@end
