/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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
#include "JavaScriptCallFrame.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "V8Binding.h"

namespace WebCore {

JavaScriptCallFrame::JavaScriptCallFrame(v8::Handle<v8::Context> debuggerContext, v8::Handle<v8::Object> callFrame)
    : m_debuggerContext(debuggerContext)
    , m_callFrame(callFrame)
{
}

JavaScriptCallFrame::~JavaScriptCallFrame()
{
}

JavaScriptCallFrame* JavaScriptCallFrame::caller()
{
    if (!m_caller) {
        v8::HandleScope handleScope;
        v8::Context::Scope contextScope(m_debuggerContext.get());
        v8::Handle<v8::Value> callerFrame = m_callFrame.get()->Get(v8String("caller"));
        if (!callerFrame->IsObject())
            return 0;
        m_caller = JavaScriptCallFrame::create(m_debuggerContext.get(), v8::Handle<v8::Object>::Cast(callerFrame));
    }
    return m_caller.get();
}

int JavaScriptCallFrame::sourceID() const
{
    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(m_debuggerContext.get());
    v8::Handle<v8::Value> result = m_callFrame.get()->Get(v8String("sourceID"));
    if (result->IsInt32())
        return result->Int32Value();
    return 0;
}

int JavaScriptCallFrame::line() const
{
    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(m_debuggerContext.get());
    v8::Handle<v8::Value> result = m_callFrame.get()->Get(v8String("line"));
    if (result->IsInt32())
        return result->Int32Value();
    return 0;
}

String JavaScriptCallFrame::functionName() const
{
    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(m_debuggerContext.get());
    v8::Handle<v8::Value> result = m_callFrame.get()->Get(v8String("functionName"));
    return toWebCoreString(result);
}

v8::Handle<v8::Value> JavaScriptCallFrame::scopeChain() const
{
    v8::Handle<v8::Array> scopeChain = v8::Handle<v8::Array>::Cast(m_callFrame.get()->Get(v8String("scopeChain")));
    v8::Handle<v8::Array> result = v8::Array::New(scopeChain->Length());
    for (uint32_t i = 0; i < scopeChain->Length(); i++)
        result->Set(i, scopeChain->Get(i));
    return result;
}

int JavaScriptCallFrame::scopeType(int scopeIndex) const
{
    v8::Handle<v8::Array> scopeType = v8::Handle<v8::Array>::Cast(m_callFrame.get()->Get(v8String("scopeType")));
    return scopeType->Get(scopeIndex)->Int32Value();
}

v8::Handle<v8::Value> JavaScriptCallFrame::thisObject() const
{
    return m_callFrame.get()->Get(v8String("thisObject"));
}

v8::Handle<v8::Value> JavaScriptCallFrame::evaluate(const String& expression)
{
    v8::Handle<v8::Function> evalFunction = v8::Handle<v8::Function>::Cast(m_callFrame.get()->Get(v8String("evaluate")));
    v8::Handle<v8::Value> argv[] = { v8String(expression) };
    return evalFunction->Call(m_callFrame.get(), 1, argv);
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
