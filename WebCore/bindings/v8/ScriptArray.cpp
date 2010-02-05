/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "ScriptArray.h"

#include "ScriptScope.h"
#include "ScriptState.h"

#include "Document.h"
#include "Frame.h"
#include "V8Binding.h"
#include "V8Proxy.h"

#include <v8.h>

namespace WebCore {

ScriptArray::ScriptArray(ScriptState* scriptState, v8::Handle<v8::Array> v8Array)
    : ScriptObject(scriptState, v8Array)
{
}

bool ScriptArray::set(unsigned index, const ScriptObject& value)
{
    if (value.scriptState() != m_scriptState) {
        ASSERT_NOT_REACHED();
        return false;
    }
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::Integer::New(index), value.v8Value());
    return scope.success();
}

bool ScriptArray::set(unsigned index, const String& value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::Integer::New(index), v8String(value));
    return scope.success();
}

bool ScriptArray::set(unsigned index, double value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::Integer::New(index), v8::Number::New(value));
    return scope.success();
}

bool ScriptArray::set(unsigned index, long long value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::Integer::New(index), v8::Number::New(value));
    return scope.success();
}

bool ScriptArray::set(unsigned index, int value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::Integer::New(index), v8::Number::New(value));
    return scope.success();
}

bool ScriptArray::set(unsigned index, bool value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::Integer::New(index), v8Boolean(value));
    return scope.success();
}

unsigned ScriptArray::length()
{
    ScriptScope scope(m_scriptState);
    return v8::Array::Cast(*v8Value())->Length();
}

ScriptArray ScriptArray::createNew(ScriptState* scriptState)
{
    ScriptScope scope(scriptState);
    return ScriptArray(scriptState, v8::Array::New());
}

} // namespace WebCore
