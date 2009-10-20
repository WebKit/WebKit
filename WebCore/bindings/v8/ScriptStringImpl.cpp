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
#include "ScriptStringImpl.h"

#include "V8Binding.h"

namespace WebCore {

ScriptStringImpl::ScriptStringImpl(const String& s)
{
    v8::HandleScope scope;
    m_handle.set(v8String(s));
}

ScriptStringImpl::ScriptStringImpl(const char* s)
{
    v8::HandleScope scope;
    m_handle.set(v8::String::New(s));
}

String ScriptStringImpl::toString() const
{
    return v8StringToWebCoreString(m_handle.get());
}

bool ScriptStringImpl::isNull() const
{
    return m_handle.get().IsEmpty();
}

size_t ScriptStringImpl::size() const
{
    return m_handle.get()->Length();
}

void ScriptStringImpl::append(const String& s)
{
    v8::HandleScope scope;
    if (m_handle.get().IsEmpty())
        m_handle.set(v8String(s));
    else
        m_handle.set(v8::String::Concat(m_handle.get(), v8String(s)));
}

} // namespace WebCore
