/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptCallStack.h"

#include "InspectorValues.h"
#include "ScriptController.h"
#include "ScriptDebugServer.h"
#include "ScriptScope.h"
#include "V8Binding.h"

#include <v8-debug.h>

namespace WebCore {

static void getFrameLocation(v8::Handle<v8::StackFrame> frame, String* sourceName, int* sourceLineNumber, String* functionName)
{
    ASSERT(!frame.IsEmpty());
    v8::Local<v8::String> sourceNameValue(frame->GetScriptNameOrSourceURL());
    v8::Local<v8::String> functionNameValue(frame->GetFunctionName());
    *sourceName = sourceNameValue.IsEmpty() ? "" : toWebCoreString(sourceNameValue);
    *functionName = functionNameValue.IsEmpty() ? "" : toWebCoreString(functionNameValue);
    *sourceLineNumber = frame->GetLineNumber();
}

static void getTopFrameLocation(v8::Handle<v8::StackTrace> stackTrace, String* sourceName, int* sourceLineNumber, String* functionName)
{
    if (stackTrace->GetFrameCount() <= 0) {
        // Successfully grabbed stack trace, but there are no frames. It may happen in case of a syntax error for example.
        // Fallback to setting lineNumber to 0, and source and function name to "undefined".
        *sourceName = "undefined";
        *sourceLineNumber = 0;
        *functionName = "undefined";
    } else {
        v8::Handle<v8::StackFrame> frame = stackTrace->GetFrame(0);
        getFrameLocation(frame, sourceName, sourceLineNumber, functionName);
    }
}

static PassOwnPtr<ScriptCallFrame> toScriptCallFrame(v8::Handle<v8::StackFrame> frame)
{
    String sourceName;
    int sourceLineNumber;
    String functionName;
    getFrameLocation(frame, &sourceName, &sourceLineNumber, &functionName);
    return new ScriptCallFrame(functionName, sourceName, sourceLineNumber);
}

static void toScriptCallFramesVector(v8::Local<v8::Context> context, v8::Handle<v8::StackTrace> stackTrace, Vector<OwnPtr<ScriptCallFrame> >& scriptCallFrames)
{
    v8::Context::Scope contextScope(context);
    int frameCount = stackTrace->GetFrameCount();
    for (int i = 0; i < frameCount; i++) {
        v8::Local<v8::StackFrame> stackFrame = stackTrace->GetFrame(i);
        scriptCallFrames.append(toScriptCallFrame(stackFrame));
    }
}

const int ScriptCallStack::maxCallStackSizeToCapture = 200;
const v8::StackTrace::StackTraceOptions ScriptCallStack::stackTraceOptions = static_cast<v8::StackTrace::StackTraceOptions>(
     v8::StackTrace::kLineNumber
    | v8::StackTrace::kColumnOffset
    | v8::StackTrace::kScriptNameOrSourceURL
    | v8::StackTrace::kFunctionName);


PassOwnPtr<ScriptCallStack> ScriptCallStack::create(const v8::Arguments& arguments, unsigned skipArgumentCount, int framCountLimit)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> context = v8::Context::GetCurrent();
    v8::Context::Scope contextScope(context);
    v8::Handle<v8::StackTrace> stackTrace(v8::StackTrace::CurrentStackTrace(framCountLimit, ScriptCallStack::stackTraceOptions));

    if (stackTrace.IsEmpty())
        return 0;

    String sourceName;
    int sourceLineNumber;
    String functionName;
    getTopFrameLocation(stackTrace, &sourceName, &sourceLineNumber, &functionName);

    Vector<OwnPtr<ScriptCallFrame> > scriptCallFrames;
    if (framCountLimit > 1)
        toScriptCallFramesVector(context, stackTrace, scriptCallFrames);

    return new ScriptCallStack(ScriptState::forContext(context), new ScriptCallFrame(functionName, sourceName, sourceLineNumber, arguments, skipArgumentCount), scriptCallFrames);
}

PassOwnPtr<ScriptCallStack> ScriptCallStack::create(ScriptState* state, v8::Handle<v8::StackTrace> stackTrace)
{
    v8::HandleScope scope;
    Vector<OwnPtr<ScriptCallFrame> > scriptCallFrames;
    toScriptCallFramesVector(state->context(), stackTrace, scriptCallFrames);

    String sourceName;
    int sourceLineNumber;
    String functionName;
    getTopFrameLocation(stackTrace, &sourceName, &sourceLineNumber, &functionName);

    return new ScriptCallStack(state, new ScriptCallFrame(functionName, sourceName, sourceLineNumber), scriptCallFrames);
}

ScriptCallStack::ScriptCallStack(ScriptState* scriptState, PassOwnPtr<ScriptCallFrame> topFrame, Vector<OwnPtr<ScriptCallFrame> >& scriptCallFrames)
    : m_topFrame(topFrame)
    , m_scriptState(scriptState)
{
    m_scriptCallFrames.swap(scriptCallFrames);
}

ScriptCallStack::~ScriptCallStack()
{
}

const ScriptCallFrame& ScriptCallStack::at(unsigned index)
{
    if (!index && m_topFrame)
        return *m_topFrame;
    return *m_scriptCallFrames.at(index);
}

unsigned ScriptCallStack::size()
{
    if (m_scriptCallFrames.isEmpty())
        return 1;
    return m_scriptCallFrames.size();
}


bool ScriptCallStack::stackTrace(int frameLimit, const RefPtr<InspectorArray>& stackTrace)
{
#if ENABLE(INSPECTOR)
    if (!v8::Context::InContext())
        return false;
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return false;
    v8::HandleScope scope;
    v8::Context::Scope contextScope(context);
    v8::Handle<v8::StackTrace> trace(v8::StackTrace::CurrentStackTrace(frameLimit));
    int frameCount = trace->GetFrameCount();
    if (trace.IsEmpty() || !frameCount)
        return false;
    for (int i = 0; i < frameCount; ++i) {
        v8::Handle<v8::StackFrame> frame = trace->GetFrame(i);
        RefPtr<InspectorObject> frameObject = InspectorObject::create();
        v8::Local<v8::String> scriptName = frame->GetScriptName();
        frameObject->setString("scriptName", scriptName.IsEmpty() ? "" : toWebCoreString(scriptName));
        v8::Local<v8::String> functionName = frame->GetFunctionName();
        frameObject->setString("functionName", functionName.IsEmpty() ? "" : toWebCoreString(functionName));
        frameObject->setNumber("lineNumber", frame->GetLineNumber());
        frameObject->setNumber("column", frame->GetColumn());
        stackTrace->pushObject(frameObject);
    }
    return true;
#else
    return false;
#endif
}

} // namespace WebCore
