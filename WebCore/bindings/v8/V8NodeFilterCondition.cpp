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
#include "V8NodeFilterCondition.h"

#include "Node.h"
#include "NodeFilter.h"
#include "ScriptState.h"
#include "V8Proxy.h"

#include <wtf/OwnArrayPtr.h>

namespace WebCore {

V8NodeFilterCondition::V8NodeFilterCondition(v8::Handle<v8::Value> filter)
    : m_filter(v8::Persistent<v8::Value>::New(filter))
{
#ifndef NDEBUG
    V8Proxy::RegisterGlobalHandle(NODE_FILTER, this, m_filter);
#endif
}

V8NodeFilterCondition::~V8NodeFilterCondition()
{
#ifndef NDEBUG
    V8Proxy::UnregisterGlobalHandle(this, m_filter);
#endif
    m_filter.Dispose();
    m_filter.Clear();
}

short V8NodeFilterCondition::acceptNode(ScriptState* state, Node* node) const
{
    ASSERT(v8::Context::InContext());

    if (!m_filter->IsFunction())
        return NodeFilter::FILTER_ACCEPT;

    v8::TryCatch exceptionCatcher;

    v8::Handle<v8::Object> object = v8::Context::GetCurrent()->Global();
    v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(m_filter);
    OwnArrayPtr<v8::Handle<v8::Value> > args(new v8::Handle<v8::Value>[1]);
    args[0] = V8Proxy::ToV8Object(V8ClassIndex::NODE, node);

    V8Proxy* proxy = V8Proxy::retrieve();
    ASSERT(proxy);

    v8::Handle<v8::Value> result = proxy->CallFunction(callback, object, 1, args.get());

    if (exceptionCatcher.HasCaught()) {
        state->setException(exceptionCatcher.Exception());
        return NodeFilter::FILTER_REJECT;
    }

    ASSERT(!result.IsEmpty());

    return result->Int32Value();
}

} // namespace WebCore
