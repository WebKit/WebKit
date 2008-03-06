/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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

// FIXME: This file and the classes in it should have the prefix "Web" instead
// of "WebCore". The best way to fix this is to merge WebCoreScriptCallFrame
// with WebScriptCallFrame and WebCoreScriptDebugger with WebScriptDebugger.

#import "WebCoreScriptDebugger.h"

#import "WebScriptDebugDelegate.h"
#import <JavaScriptCore/ExecState.h>
#import <JavaScriptCore/JSGlobalObject.h>
#import <JavaScriptCore/debugger.h>
#import <JavaScriptCore/function.h>
#import <JavaScriptCore/interpreter.h>
#import <WebCore/KURL.h>
#import <WebCore/PlatformString.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/runtime_root.h>

using namespace KJS;
using namespace WebCore;

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

// C++ interface to KJS debugger callbacks

class WebCoreScriptDebuggerImp : public KJS::Debugger {

  private:
    WebCoreScriptDebugger  *_objc;      // our ObjC half
    bool                    _nested;    // true => this is a nested call
    WebScriptCallFrame *_current;   // top stack frame (copy of same field from ObjC side)

  public:
    // constructor
    WebCoreScriptDebuggerImp(WebCoreScriptDebugger *objc, JSGlobalObject* globalObject) : _objc(objc) {
        _nested = true;
        _current = [[_objc delegate] enterFrame:globalObject->globalExec()];
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
            _current = [[_objc delegate] enterFrame:state];
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
            _current = [[_objc delegate] leaveFrame];
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
        _delegate  = delegate;
        _globalObj = [_delegate globalObject];
        _debugger  = new WebCoreScriptDebuggerImp(self, [_globalObj _rootObject]->globalObject());
    }
    return self;
}

- (void)dealloc
{
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
