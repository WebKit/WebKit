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

// FIXME This is a placeholder, replace with a real CallFrame structure when a cross-platform one exists.
typedef int CallFrame;

class DebuggerDocument {
public:
    DebuggerDocument()
        : m_paused(false)
    {
    }

    //-- Not sure of a good naming scheme here. --//
    //-- These are the calls out of the JS. --//

    bool isPaused();
    void pause();
    void resume();
    void stepInto();
    JSValueRef evaluateScript(JSContextRef context, CallFrame frame);
    Vector<CallFrame> currentFunctionStack();
    Vector<CallFrame> localScopeVariableNamesForCallFrame(JSContextRef /*context*/);
    JSStringRef valueForScopeVariableNamed(CallFrame frame, JSStringRef key);

    static JSValueRef breakpointEditorHTML(JSContextRef context);
    
    //-- Not sure of a good naming scheme here. --//
    //-- These are the calls into the JS. --//
    static void pause(JSContextRef);
    static void resume(JSContextRef);
    static void stepInto(JSContextRef);
    static void stepOver(JSContextRef);
    static void stepOut(JSContextRef);
    static void showConsole(JSContextRef);
    static void closeCurrentFile(JSContextRef);
    static void updateFileSource(JSContextRef, JSStringRef documentSource, JSStringRef url);
    static void didParseScript(JSContextRef, JSStringRef source, JSStringRef documentSource, JSStringRef url, JSValueRef sourceId, JSValueRef baseLine);
    static void willExecuteStatement(JSContextRef, JSValueRef sourceId, JSValueRef lineno);
    static void didEnterCallFrame(JSContextRef, JSValueRef sourceId, JSValueRef lineno);
    static void willLeaveCallFrame(JSContextRef, JSValueRef sourceId, JSValueRef lineno);
    static void exceptionWasRaised(JSContextRef, JSValueRef sourceId, JSValueRef lineno);

private:
    static void callGlobalFunction(JSContextRef, const char* functionName, int argumentCount, JSValueRef arguments[]);   // Implementation for calls into JS

    bool m_paused;
};

#endif //DebuggerDocument_H

