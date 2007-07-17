/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "DebuggerDocument.h"

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefCF.h>
#include <JavaScriptCore/RetainPtr.h>

// You can expand breakpoints by double clicking them.  This is where that HTML comes from.
JSValueRef DebuggerDocument::breakpointEditorHTML(JSContextRef context)
{
    RetainPtr<CFURLRef> htmlURLRef(AdoptCF, ::CFBundleCopyResourceURL(::CFBundleGetBundleWithIdentifier(CFSTR("org.webkit.drosera")), CFSTR("breakpointEditor"), CFSTR("html"), 0));
    if (!htmlURLRef)
        return 0;

    // FIXME: I'm open to a better way to do this.  We convert from UInt8 to CFString to JSString (3 string types!)
    RetainPtr<CFReadStreamRef> readStreamRef(AdoptCF, CFReadStreamCreateWithFile(0, htmlURLRef.get()));
    CFReadStreamRef readStream = readStreamRef.get();

    if (!CFReadStreamOpen(readStream))
        return 0;

    // Large enough for current BreakPointEditor.html but won't need to be changed if that file changes 
    // because we loop over the entire file and read it in bufferLength pieces at a time
    const CFIndex bufferLength = 740;
    UInt8 buffer[bufferLength];
    Vector<UInt8, bufferLength> charBuffer;
    CFIndex readResult = bufferLength;
    while (readResult == bufferLength) {
        readResult = CFReadStreamRead(readStream, buffer, bufferLength);

        // Error condition (-1) will not copy any data
        for (int i = 0; i < readResult; i++)
            charBuffer.append(buffer[i]);
    }

    CFReadStreamClose(readStream);
    if (readResult == -1)
        return 0;

    // FIXME: Is there a way to determine the encoding?
    RetainPtr<CFStringRef> fileContents(AdoptCF, CFStringCreateWithBytes(0, charBuffer.data(), charBuffer.size(), kCFStringEncodingUTF8, true));
    JSStringRef fileContentsJS = JSStringCreateWithCFString(fileContents.get());
    JSValueRef ret = JSValueMakeString(context, fileContentsJS);

    JSStringRelease(fileContentsJS);

    return ret;
}

bool DebuggerDocument::isPaused()
{
    return m_paused;
}

// ------------------------------------------------------------------------------------------------------------
// FIXME There is still some cross-platform work that needs to be done here, but first WebCore and the WebScriptDebugger stuff needs to be re-written
// to be cross-platform and we need to impmlement RPC on windows, then more can be moved into these functions
// ------------------------------------------------------------------------------------------------------------
void DebuggerDocument::pause()
{
    m_paused = true;
}

void DebuggerDocument::resume()
{
    m_paused = false;
}

void DebuggerDocument::stepInto()
{
}

JSValueRef DebuggerDocument::evaluateScript(JSContextRef context, CallFrame frame)
{
    return JSValueMakeNumber(context, frame);
}

Vector<CallFrame> DebuggerDocument::currentFunctionStack()
{
    Vector<CallFrame> ret;
    return ret;
}

Vector<CallFrame> DebuggerDocument::localScopeVariableNamesForCallFrame(JSContextRef /*context*/)
{
    Vector<CallFrame> ret;
    return ret;
}

JSStringRef DebuggerDocument::valueForScopeVariableNamed(CallFrame /*frame*/, JSStringRef key)
{
    return key;
}

//-- These are the calls into the JS. --//

void DebuggerDocument::callGlobalFunction(JSContextRef context, const char* functionName, int argumentCount, JSValueRef arguments[])
{
    JSObjectRef globalObject = JSContextGetGlobalObject(context);

    JSStringRef string = JSStringCreateWithUTF8CString(functionName);
    JSObjectRef function = JSValueToObject(context, JSObjectGetProperty(context, globalObject, string, 0), 0);
    JSStringRelease(string);

    JSObjectCallAsFunction(context, function, 0, argumentCount, arguments, 0);
}
    
void DebuggerDocument::pause(JSContextRef context)
{
    DebuggerDocument::callGlobalFunction(context, "pause", 0, 0);
}

void DebuggerDocument::resume(JSContextRef context)
{
    DebuggerDocument::callGlobalFunction(context, "resume", 0, 0);
}

void DebuggerDocument::stepInto(JSContextRef context)
{
    DebuggerDocument::callGlobalFunction(context, "stepInto", 0, 0);
}

void DebuggerDocument::stepOver(JSContextRef context)
{
    DebuggerDocument::callGlobalFunction(context, "stepOver", 0, 0);
}

void DebuggerDocument::stepOut(JSContextRef context)
{
    DebuggerDocument::callGlobalFunction(context, "stepOut", 0, 0);
}

void DebuggerDocument::showConsole(JSContextRef context)
{
    DebuggerDocument::callGlobalFunction(context, "showConsole", 0, 0);
}

void DebuggerDocument::closeCurrentFile(JSContextRef context)
{
    DebuggerDocument::callGlobalFunction(context, "closeCurrentFile", 0, 0);
}

void DebuggerDocument::updateFileSource(JSContextRef context, JSStringRef documentSource, JSStringRef url)
{
    JSValueRef documentSourceValue = JSValueMakeString(context, documentSource);
    JSValueRef urlValue = JSValueMakeString(context, url);
    JSValueRef forceValue = JSValueMakeBoolean(context, false);

    JSValueRef arguments[] = { documentSourceValue, urlValue, forceValue };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);

    DebuggerDocument::callGlobalFunction(context, "updateFileSource", argumentsSize, arguments);
}

void DebuggerDocument::didParseScript(JSContextRef context, JSStringRef source, JSStringRef documentSource, JSStringRef url, JSValueRef sourceId, JSValueRef baseLine)
{
    JSValueRef sourceValue = JSValueMakeString(context, source);
    JSValueRef documentSourceValue = JSValueMakeString(context, documentSource);
    JSValueRef urlValue = JSValueMakeString(context, url);

    JSValueRef arguments[] = { sourceValue, documentSourceValue, urlValue, sourceId, baseLine };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);
    DebuggerDocument::callGlobalFunction(context, "didParseScript", argumentsSize, arguments);
}

void DebuggerDocument::willExecuteStatement(JSContextRef context, JSValueRef sourceId, JSValueRef lineno)
{
    JSValueRef arguments[] = { sourceId, lineno };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);
    DebuggerDocument::callGlobalFunction(context, "willExecuteStatement", argumentsSize, arguments);
}

void DebuggerDocument::didEnterCallFrame(JSContextRef context, JSValueRef sourceId, JSValueRef lineno)
{
    JSValueRef arguments[] = { sourceId, lineno };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);
    DebuggerDocument::callGlobalFunction(context, "didEnterCallFrame", argumentsSize, arguments);
}

void DebuggerDocument::willLeaveCallFrame(JSContextRef context, JSValueRef sourceId, JSValueRef lineno)
{
    JSValueRef arguments[] = { sourceId, lineno };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);
    DebuggerDocument::callGlobalFunction(context, "willLeaveCallFrame", argumentsSize, arguments);
}

void DebuggerDocument::exceptionWasRaised(JSContextRef context, JSValueRef sourceId, JSValueRef lineno)
{
    JSValueRef arguments[] = { sourceId, lineno };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);
    DebuggerDocument::callGlobalFunction(context, "exceptionWasRaised", argumentsSize, arguments);
}

