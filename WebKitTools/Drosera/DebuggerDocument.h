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
#include <wtf/OwnPtr.h>

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

    // FIXME I want to restructure this somehow; maybe break out the callbacks into another class.
    JSValueRef breakpointEditorHTML(JSContextRef);
    JSValueRef isPaused(JSContextRef);
    JSValueRef pause(JSContextRef);
    JSValueRef resume(JSContextRef);
    JSValueRef stepInto(JSContextRef);
    JSValueRef evaluateScript(JSContextRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    JSValueRef currentFunctionStack(JSContextRef, JSValueRef* exception);
    JSValueRef localScopeVariableNamesForCallFrame(JSContextRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    JSValueRef valueForScopeVariableNamed(JSContextRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef log(JSContextRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    // Cross-platform functions
    void platformPause(JSContextRef);
    void platformResume(JSContextRef);
    void platformStepInto(JSContextRef);
    JSValueRef platformEvaluateScript(JSContextRef, JSStringRef script, int callFrame);
    void getPlatformCurrentFunctionStack(JSContextRef, Vector<JSValueRef>& currentStack);
    void getPlatformLocalScopeVariableNamesForCallFrame(JSContextRef, int callFrame, Vector<JSValueRef>& variableNames);
    JSValueRef platformValueForScopeVariableNamed(JSContextRef, JSStringRef key, int callFrame);
    static void platformLog(JSContextRef, JSStringRef msg);

    // These are the calls into the JS.
    static void toolbarPause(JSContextRef);
    static void toolbarResume(JSContextRef);
    static void toolbarStepInto(JSContextRef);
    static void toolbarStepOver(JSContextRef);
    static void toolbarStepOut(JSContextRef);
    static void toolbarShowConsole(JSContextRef);
    static void toolbarCloseCurrentFile(JSContextRef);
    static void updateFileSource(JSContextRef, JSStringRef documentSource, JSStringRef url);
    static void didParseScript(JSContextRef, JSStringRef source, JSStringRef documentSource, JSStringRef url, JSValueRef sourceId, JSValueRef baseLine);
    static void willExecuteStatement(JSContextRef, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception);
    static void didEnterCallFrame(JSContextRef, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception);
    static void willLeaveCallFrame(JSContextRef, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception);
    static void exceptionWasRaised(JSContextRef, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception);

    void windowScriptObjectAvailable(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);
    static JSValueRef toJSArray(JSContextRef, Vector<JSValueRef>&, JSValueRef* exception);

private:
    static JSValueRef callGlobalFunction(JSContextRef, const char* functionName, int argumentCount, JSValueRef arguments[], JSValueRef* exception);   // Implementation for calls into JS
    static JSValueRef callFunctionOnObject(JSContextRef, JSObjectRef object, const char* functionName, int argumentCount, JSValueRef arguments[], JSValueRef* exception);   // Implementation for calls into JS
    static JSClassRef getDroseraJSClass();
    static JSStaticFunction* staticFunctions();

    static void logException(JSContextRef, JSValueRef exception);

    OwnPtr<DebuggerClient> m_debuggerClient;
    bool m_paused;
};

#endif //DebuggerDocument_H

