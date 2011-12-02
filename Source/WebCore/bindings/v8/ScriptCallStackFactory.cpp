/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
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
#include "ScriptCallStackFactory.h"

#include "InspectorValues.h"
#include "ScriptArguments.h"
#include "ScriptCallFrame.h"
#include "ScriptCallStack.h"
#include "ScriptScope.h"
#include "ScriptValue.h"
#include "V8Binding.h"

#include <v8-debug.h>

namespace WebCore {

static ScriptCallFrame toScriptCallFrame(v8::Handle<v8::StackFrame> frame)
{
    String sourceName;
    v8::Local<v8::String> sourceNameValue(frame->GetScriptNameOrSourceURL());
    if (!sourceNameValue.IsEmpty())
        sourceName = toWebCoreString(sourceNameValue);

    String functionName;
    v8::Local<v8::String> functionNameValue(frame->GetFunctionName());
    if (!functionNameValue.IsEmpty())
        functionName = toWebCoreString(functionNameValue);

    int sourceLineNumber = frame->GetLineNumber();
    int sourceColumn = frame->GetColumn();
    return ScriptCallFrame(functionName, sourceName, sourceLineNumber, sourceColumn);
}

static void toScriptCallFramesVector(v8::Handle<v8::StackTrace> stackTrace, Vector<ScriptCallFrame>& scriptCallFrames, size_t maxStackSize, bool emptyStackIsAllowed)
{
    ASSERT(v8::Context::InContext());
    int frameCount = stackTrace->GetFrameCount();
    if (frameCount > static_cast<int>(maxStackSize))
        frameCount = maxStackSize;
    for (int i = 0; i < frameCount; i++) {
        v8::Local<v8::StackFrame> stackFrame = stackTrace->GetFrame(i);
        scriptCallFrames.append(toScriptCallFrame(stackFrame));
    }
    if (!frameCount && !emptyStackIsAllowed) {
        // Successfully grabbed stack trace, but there are no frames. It may happen in case
        // when a bound function is called from native code for example.
        // Fallback to setting lineNumber to 0, and source and function name to "undefined".
        scriptCallFrames.append(ScriptCallFrame("undefined", "undefined", 0));
    }
}

static PassRefPtr<ScriptCallStack> createScriptCallStack(v8::Handle<v8::StackTrace> stackTrace, size_t maxStackSize, bool emptyStackIsAllowed)
{
    ASSERT(v8::Context::InContext());
    v8::HandleScope scope;
    Vector<ScriptCallFrame> scriptCallFrames;
    toScriptCallFramesVector(stackTrace, scriptCallFrames, maxStackSize, emptyStackIsAllowed);
    return ScriptCallStack::create(scriptCallFrames);
}

PassRefPtr<ScriptCallStack> createScriptCallStack(v8::Handle<v8::StackTrace> stackTrace, size_t maxStackSize)
{
    return createScriptCallStack(stackTrace, maxStackSize, true);
}

PassRefPtr<ScriptCallStack> createScriptCallStack(size_t maxStackSize, bool emptyStackIsAllowed)
{
    if (!v8::Context::InContext())
        return 0;
    v8::HandleScope handleScope;
    v8::Handle<v8::StackTrace> stackTrace(v8::StackTrace::CurrentStackTrace(maxStackSize, stackTraceOptions));
    return createScriptCallStack(stackTrace, maxStackSize, emptyStackIsAllowed);
}

PassRefPtr<ScriptArguments> createScriptArguments(const v8::Arguments& v8arguments, unsigned skipArgumentCount)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> context = v8::Context::GetCurrent();
    ScriptState* state = ScriptState::forContext(context);

    Vector<ScriptValue> arguments;
    for (int i = skipArgumentCount; i < v8arguments.Length(); ++i)
        arguments.append(ScriptValue(v8arguments[i]));

    return ScriptArguments::create(state, arguments);
}

} // namespace WebCore
