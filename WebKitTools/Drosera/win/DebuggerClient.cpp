/*
 * Copyright (C) 2007 Apple, Inc.  All rights reserved.
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
#include "DebuggerDocumentWin.h"

#include "DebuggerDocument.h"

static DebuggerDocument callbacks;

static JSValueRef breakpointEditorHTMLCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    return DebuggerDocument::breakpointEditorHTML(context);
}

static JSValueRef currentFunctionStackCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    callbacks.currentFunctionStack();
    return 0; //FIXME: the return value will need to change when the above function is finished
}

static JSValueRef evaluateScript_inCallFrame_Callback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    return callbacks.evaluateScript(context, 0);  //FIXME: the input values will change when this function is completed
}

static JSValueRef isPausedCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    return JSValueMakeBoolean(context, callbacks.isPaused());
}

static JSValueRef localScopeVariableNamesForCallFrame_Callback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    callbacks.localScopeVariableNamesForCallFrame(context);
    return 0; //FIXME: the return value will need to change when the above function is finished
}

static JSValueRef pauseCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    callbacks.pause();
    return 0; //FIXME: the return value will need to change when the above function is finished
}

static JSValueRef resumeCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    callbacks.resume();
    return 0; //FIXME: the return value will need to change when the above function is finished
}

static JSValueRef stepIntoCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    callbacks.stepInto();
    return 0; //FIXME: the return value will need to change when the above function is finished
}

static JSValueRef valueForScopeVariableNamed_inCallFrame_Callback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    callbacks.valueForScopeVariableNamed(0, 0); //FIXME: the input values will change when this function is completed
    return 0; //FIXME: the return value will need to change when the above function is finished
}

JSStaticFunction* staticFunctions()
{
    static JSStaticFunction staticFunctions[] = {
        { "breakpointEditorHTML", breakpointEditorHTMLCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "currentFunctionStack", currentFunctionStackCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "evaluateScript_inCallFrame_", evaluateScript_inCallFrame_Callback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isPaused", isPausedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "localScopeVariableNamesForCallFrame_", localScopeVariableNamesForCallFrame_Callback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pause", pauseCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "resume", resumeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stepInto", stepIntoCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "valueForScopeVariableNamed_inCallFrame_", valueForScopeVariableNamed_inCallFrame_Callback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    return staticFunctions;
}
