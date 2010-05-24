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

#include "ScriptScope.h"
#include "ScriptController.h"
#include "ScriptDebugServer.h"
#include "V8Binding.h"

#include <v8-debug.h>
#include <v8.h>

namespace WebCore {

ScriptCallStack* ScriptCallStack::create(const v8::Arguments& arguments, unsigned skipArgumentCount) {
    String sourceName;
    int sourceLineNumber;
    String funcName;
    if (!callLocation(&sourceName, &sourceLineNumber, &funcName))
      return 0;
    return new ScriptCallStack(arguments, skipArgumentCount, sourceName, sourceLineNumber, funcName);
}

bool ScriptCallStack::callLocation(String* sourceName, int* sourceLineNumber, String* functionName)
{
    v8::HandleScope scope;
    v8::Context::Scope contextScope(v8::Context::GetCurrent());
    v8::Handle<v8::StackTrace> stackTrace(v8::StackTrace::CurrentStackTrace(1));
    if (stackTrace.IsEmpty())
        return false;
    if (stackTrace->GetFrameCount() <= 0) {
        // Fallback to setting lineNumber to 0, and source and function name to "undefined".
        *sourceName = toWebCoreString(v8::Undefined());
        *sourceLineNumber = 0;
        *functionName = toWebCoreString(v8::Undefined());
        return true;
    }
    v8::Handle<v8::StackFrame> frame = stackTrace->GetFrame(0);
    *sourceName = toWebCoreString(frame->GetScriptName());
    *sourceLineNumber = frame->GetLineNumber();
    *functionName = toWebCoreString(frame->GetFunctionName());
    return true;
}

ScriptCallStack::ScriptCallStack(const v8::Arguments& arguments, unsigned skipArgumentCount, String sourceName, int sourceLineNumber, String functionName)
    : m_lastCaller(functionName, sourceName, sourceLineNumber, arguments, skipArgumentCount)
    , m_scriptState(ScriptState::current())
{
}

ScriptCallStack::~ScriptCallStack()
{
}

const ScriptCallFrame& ScriptCallStack::at(unsigned index) const
{
    // Currently, only one ScriptCallFrame is supported. When we can get
    // a full stack trace from V8, we can do this right.
    ASSERT(index == 0);
    return m_lastCaller;
}

bool ScriptCallStack::stackTrace(int frameLimit, ScriptState* state, ScriptArray& stackTrace)
{
    ScriptScope scope(state);
    v8::Handle<v8::StackTrace> trace(v8::StackTrace::CurrentStackTrace(frameLimit));
    if (trace.IsEmpty() || !trace->GetFrameCount())
        return false;
    stackTrace = ScriptArray(state, trace->AsArray());
    return true;
}

} // namespace WebCore
