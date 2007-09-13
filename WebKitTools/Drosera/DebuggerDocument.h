/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef DebuggerDocument_H
#define DebuggerDocument_H

#pragma warning(push)
#pragma warning(disable: 4510 4512 4610)
#include <JavaScriptCore/JSObjectRef.h>
#pragma warning(pop)

#include <JavaScriptCore/Vector.h>

// Forward Declarations
#if PLATFORM(MAC)
@class DebuggerClient;
#else if PLATFORM(WIN)
class DebuggerClient;
#endif

typedef struct OpaqueJSString* JSStringRef;
typedef struct OpaqueJSValue* JSObjectRef;

class DebuggerDocument {
public:
    DebuggerDocument(DebuggerClient*);

    // These are all calls out of the JS
    static JSValueRef breakpointEditorHTMLCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef isPausedCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef pauseCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef resumeCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef stepIntoCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef evaluateScriptCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef currentFunctionStackCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef localScopeVariableNamesForCallFrameCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef valueForScopeVariableNamedCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);
    static JSValueRef logCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/);

    // Cross-platform functions
    void platformPause();
    void platformResume();
    void platformStepInto();
    JSValueRef platformEvaluateScript(JSContextRef, JSStringRef script, int callFrame);
    void getPlatformCurrentFunctionStack(JSContextRef, Vector<JSValueRef>& currentStack);
    void getPlatformLocalScopeVariableNamesForCallFrame(JSContextRef, int callFrame, Vector<JSValueRef>& variableNames);
    JSValueRef platformValueForScopeVariableNamed(JSContextRef, JSStringRef key, int callFrame);
    static void platformLog(JSStringRef msg);

    // These are the calls into the JS.
    static void updateFileSource(JSContextRef, JSStringRef documentSource, JSStringRef url);
    static void didParseScript(JSContextRef, JSStringRef source, JSStringRef documentSource, JSStringRef url, JSValueRef sourceId, JSValueRef baseLine);
    static void willExecuteStatement(JSContextRef, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception = 0);
    static void didEnterCallFrame(JSContextRef, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception = 0);
    static void willLeaveCallFrame(JSContextRef, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception = 0);
    static void exceptionWasRaised(JSContextRef, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception = 0);

    void windowScriptObjectAvailable(JSContextRef, JSObjectRef windowObject, JSValueRef* exception = 0);
    static JSValueRef toJSArray(JSContextRef, Vector<JSValueRef>&, JSValueRef* exception);

    // Converting string types
    static NSString* NSStringCreateWithJSStringRef(JSStringRef);
    static JSValueRef JSValueRefCreateWithNSString(JSContextRef, NSString*);

    static JSValueRef callGlobalFunction(JSContextRef, const char* functionName, int argumentCount, JSValueRef arguments[], JSValueRef* exception = 0);   // Implementation for calls into JS

    bool getPaused() const { return m_paused; }

private:
    static JSValueRef callFunctionOnObject(JSContextRef, JSObjectRef object, const char* functionName, int argumentCount, JSValueRef arguments[], JSValueRef* exception = 0);   // Implementation for calls into JS
    static JSClassRef getDroseraJSClass();
    static JSStaticFunction* staticFunctions();

    static void logException(JSContextRef, JSValueRef exception);

    DebuggerClient* m_debuggerClient;   //DebuggerClient owns the DebuggerDocument so don't delete it.  It will delete you!

    bool m_paused;
};

#endif //DebuggerDocument_H

