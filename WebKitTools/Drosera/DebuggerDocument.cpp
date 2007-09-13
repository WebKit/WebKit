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

#include "DebuggerClient.h"

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefCF.h>
#include <JavaScriptCore/RetainPtr.h>
#include <JavaScriptCore/Vector.h>

DebuggerDocument::DebuggerDocument(DebuggerClient* debugger)
    : m_paused(false)
    , m_debuggerClient(debugger)
{
    ASSERT(m_debuggerClient);
}

//-- Callbacks

JSValueRef DebuggerDocument::breakpointEditorHTMLCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    RetainPtr<CFURLRef> htmlURLRef(AdoptCF, ::CFBundleCopyResourceURL(::CFBundleGetBundleWithIdentifier(CFSTR("org.webkit.drosera")), CFSTR("breakpointEditor"), CFSTR("html"), 0));
    if (!htmlURLRef)
        return JSValueMakeUndefined(context);

    // FIXME: I'm open to a better way to do this.  We convert from UInt8 to CFString to JSString (3 string types!)
    RetainPtr<CFReadStreamRef> readStreamRef(AdoptCF, CFReadStreamCreateWithFile(0, htmlURLRef.get()));
    CFReadStreamRef readStream = readStreamRef.get();

    if (!CFReadStreamOpen(readStream))
        return JSValueMakeUndefined(context);

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
        return JSValueMakeUndefined(context);

    // FIXME: Is there a way to determine the encoding?
    RetainPtr<CFStringRef> fileContents(AdoptCF, CFStringCreateWithBytes(0, charBuffer.data(), charBuffer.size(), kCFStringEncodingUTF8, true));
    JSRetainPtr<JSStringRef> fileContentsJS(Adopt, JSStringCreateWithCFString(fileContents.get()));
    JSValueRef ret = JSValueMakeString(context, fileContentsJS.get());

    return ret;
}

JSValueRef DebuggerDocument::isPausedCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    DebuggerDocument* debuggerDocument = reinterpret_cast<DebuggerDocument*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(context, debuggerDocument->m_paused);
}

JSValueRef DebuggerDocument::pauseCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    DebuggerDocument* debuggerDocument = reinterpret_cast<DebuggerDocument*>(JSObjectGetPrivate(thisObject));

    debuggerDocument->m_paused = true;
    debuggerDocument->platformPause();
    return JSValueMakeUndefined(context);
}

JSValueRef DebuggerDocument::resumeCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    DebuggerDocument* debuggerDocument = reinterpret_cast<DebuggerDocument*>(JSObjectGetPrivate(thisObject));
    debuggerDocument->m_paused = false;
    debuggerDocument->platformResume();
    return JSValueMakeUndefined(context);
}

JSValueRef DebuggerDocument::stepIntoCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    DebuggerDocument* debuggerDocument = reinterpret_cast<DebuggerDocument*>(JSObjectGetPrivate(thisObject));
    debuggerDocument->platformStepInto();
    return JSValueMakeUndefined(context);
}

JSValueRef DebuggerDocument::evaluateScriptCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    DebuggerDocument* debuggerDocument = reinterpret_cast<DebuggerDocument*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    if (!JSValueIsNumber(context, arguments[1]))
        return JSValueMakeUndefined(context);
    
    double callFrame = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);

    JSRetainPtr<JSStringRef> script(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    JSValueRef ret = debuggerDocument->platformEvaluateScript(context, script.get(), (int)callFrame);

    return ret;
}

JSValueRef DebuggerDocument::currentFunctionStackCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* exception)
{
    DebuggerDocument* debuggerDocument = reinterpret_cast<DebuggerDocument*>(JSObjectGetPrivate(thisObject));
    Vector<JSValueRef> stack;
    debuggerDocument->getPlatformCurrentFunctionStack(context, stack);
    return DebuggerDocument::toJSArray(context, stack, exception);
}

JSValueRef DebuggerDocument::localScopeVariableNamesForCallFrameCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    DebuggerDocument* debuggerDocument = reinterpret_cast<DebuggerDocument*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    if (!JSValueIsNumber(context, arguments[0]))
        return JSValueMakeUndefined(context);    

    double callFrame = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);

    // Get the variable names
    Vector<JSValueRef> localVariableNames;

    debuggerDocument->getPlatformLocalScopeVariableNamesForCallFrame(context, static_cast<int>(callFrame), localVariableNames);
    return DebuggerDocument::toJSArray(context, localVariableNames, exception);
}

JSValueRef DebuggerDocument::valueForScopeVariableNamedCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    DebuggerDocument* debuggerDocument = reinterpret_cast<DebuggerDocument*>(JSObjectGetPrivate(thisObject));

    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    if (!JSValueIsString(context, arguments[0]))
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> key(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    if (!JSValueIsNumber(context, arguments[1]))
        return JSValueMakeUndefined(context);

    double callFrame = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);

    return debuggerDocument->platformValueForScopeVariableNamed(context, key.get(), (int)callFrame);
}

JSValueRef DebuggerDocument::logCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    if (!JSValueIsString(context, arguments[0]))
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> msg(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    DebuggerDocument::platformLog(msg.get());
    return JSValueMakeUndefined(context);
}

//-- These are the calls into the JS. --//    

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

void DebuggerDocument::willExecuteStatement(JSContextRef context, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception)
{
    JSValueRef arguments[] = { sourceId, lineno };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);

    DebuggerDocument::callGlobalFunction(context, "willExecuteStatement", argumentsSize, arguments, exception);
    if (exception)
        logException(context, *exception);
}

void DebuggerDocument::didEnterCallFrame(JSContextRef context, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception)
{
    JSValueRef arguments[] = { sourceId, lineno };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);

    DebuggerDocument::callGlobalFunction(context, "didEnterCallFrame", argumentsSize, arguments, exception);
    if (exception)
        logException(context, *exception);
}

void DebuggerDocument::willLeaveCallFrame(JSContextRef context, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception)
{
    JSValueRef arguments[] = { sourceId, lineno };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);

    DebuggerDocument::callGlobalFunction(context, "willLeaveCallFrame", argumentsSize, arguments, exception);
    if (exception)
        logException(context, *exception);
}

void DebuggerDocument::exceptionWasRaised(JSContextRef context, JSValueRef sourceId, JSValueRef lineno, JSValueRef* exception)
{
    JSValueRef arguments[] = { sourceId, lineno };
    int argumentsSize = sizeof(arguments)/sizeof(arguments[0]);

    DebuggerDocument::callGlobalFunction(context, "exceptionWasRaised", argumentsSize, arguments, exception);
}

void DebuggerDocument::windowScriptObjectAvailable(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> droseraStr(Adopt, JSStringCreateWithUTF8CString("DebuggerDocument"));
    JSValueRef droseraObject = JSObjectMake(context, getDroseraJSClass(), this);

    JSObjectSetProperty(context, windowObject, droseraStr.get(), droseraObject, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
    if (exception)
        logException(context, *exception);
}

JSValueRef DebuggerDocument::toJSArray(JSContextRef context, Vector<JSValueRef>& vectorValues, JSValueRef* exception)
{
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    JSRetainPtr<JSStringRef> constructorString(Adopt, JSStringCreateWithUTF8CString("Array"));
    JSValueRef constructorProperty = JSObjectGetProperty(context, globalObject, constructorString.get(), exception);
    ASSERT(!*exception);

    JSObjectRef arrayConstructor = JSValueToObject(context, constructorProperty, exception);
    ASSERT(!*exception);

    JSObjectRef array = JSObjectCallAsConstructor(context, arrayConstructor, 0, 0, exception);
    ASSERT(!*exception);

    JSRetainPtr<JSStringRef> pushString(Adopt, JSStringCreateWithUTF8CString("push"));
    JSValueRef pushValue = JSObjectGetProperty(context, array, pushString.get(), exception);
    ASSERT(!*exception);

    JSObjectRef push = JSValueToObject(context, pushValue, exception);
    ASSERT(!*exception);

    for (Vector<JSValueRef>::iterator it = vectorValues.begin(); it != vectorValues.end(); ++it) {
        JSObjectCallAsFunction(context, push, array, 1, it, exception);
        ASSERT(!*exception);
    }
    
    return array;
}

NSString* DebuggerDocument::NSStringCreateWithJSStringRef(JSStringRef jsString)
{
    CFStringRef cfString = JSStringCopyCFString(kCFAllocatorDefault, jsString);
    return (NSString *)cfString;
}

JSValueRef DebuggerDocument::JSValueRefCreateWithNSString(JSContextRef context, NSString* nsString)
{
    JSRetainPtr<JSStringRef> jsString(Adopt, JSStringCreateWithCFString((CFStringRef)nsString));
    return JSValueMakeString(context, jsString.get());
}

// Private
JSValueRef DebuggerDocument::callGlobalFunction(JSContextRef context, const char* functionName, int argumentCount, JSValueRef arguments[], JSValueRef* exception)
{
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    return callFunctionOnObject(context, globalObject, functionName, argumentCount, arguments, exception);
}

JSValueRef DebuggerDocument::callFunctionOnObject(JSContextRef context, JSObjectRef object, const char* functionName, int argumentCount, JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> string(Adopt, JSStringCreateWithUTF8CString(functionName));
    JSValueRef objectProperty = JSObjectGetProperty(context, object, string.get(), exception);

    JSObjectRef function = JSValueToObject(context, objectProperty, exception);
    ASSERT(JSObjectIsFunction(context, function));
 
    JSValueRef returnValue = JSObjectCallAsFunction(context, function, 0, argumentCount, arguments, exception);
    if (exception)
        logException(context, *exception);

    return returnValue;
}

JSClassRef DebuggerDocument::getDroseraJSClass()
{
    static JSClassRef droseraClass = 0;

    if (!droseraClass) {
        JSClassDefinition classDefinition = {0};
        classDefinition.staticFunctions = DebuggerDocument::staticFunctions();

        droseraClass = JSClassCreate(&classDefinition);
    }

    return droseraClass;
}

JSStaticFunction* DebuggerDocument::staticFunctions()
{
    static JSStaticFunction staticFunctions[] = {
        { "breakpointEditorHTML", DebuggerDocument::breakpointEditorHTMLCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "currentFunctionStack", DebuggerDocument::currentFunctionStackCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "evaluateScript", DebuggerDocument::evaluateScriptCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isPaused", DebuggerDocument::isPausedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "localScopeVariableNamesForCallFrame", DebuggerDocument::localScopeVariableNamesForCallFrameCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pause", DebuggerDocument::pauseCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "resume", DebuggerDocument::resumeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stepInto", DebuggerDocument::stepIntoCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "valueForScopeVariableNamed", DebuggerDocument::valueForScopeVariableNamedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "log", DebuggerDocument::logCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    return staticFunctions;
}

void DebuggerDocument::logException(JSContextRef context, JSValueRef exception)
{
    if (!exception)
        return;
    
    JSRetainPtr<JSStringRef> msg(Adopt, JSValueToStringCopy(context, exception, 0));
    DebuggerDocument::platformLog(msg.get());
}

