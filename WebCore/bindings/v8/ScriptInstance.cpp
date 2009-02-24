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
#include "ScriptInstance.h"

#ifndef NDEBUG
#include "V8Proxy.h"
#endif
#include <wtf/Assertions.h>

namespace WebCore {

V8ScriptInstance::V8ScriptInstance()
{
}

V8ScriptInstance::V8ScriptInstance(v8::Handle<v8::Object> instance)
{
    set(instance);
}

V8ScriptInstance::~V8ScriptInstance()
{
    clear();
}

v8::Persistent<v8::Object> V8ScriptInstance::instance()
{
    return m_instance;
}

void V8ScriptInstance::clear()
{
    if (m_instance.IsEmpty())
        return;
#ifndef NDEBUG
    V8Proxy::UnregisterGlobalHandle(this, m_instance);
#endif
    m_instance.Dispose();
    m_instance.Clear();
}

void V8ScriptInstance::set(v8::Handle<v8::Object> instance)
{
    clear();
    if (instance.IsEmpty())
        return;

    m_instance = v8::Persistent<v8::Object>::New(instance);
#ifndef NDEBUG
    V8Proxy::RegisterGlobalHandle(SCRIPTINSTANCE, this, m_instance);
#endif
}

} // namespace WebCore
