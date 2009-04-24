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
#include "V8EventListenerList.h"

#include "V8CustomEventListener.h"

namespace WebCore {

V8EventListenerList::V8EventListenerList(const char* name)
{
    ASSERT(strlen(name) <= maxKeyNameLength);
    v8::HandleScope handleScope;

    // Write the name into a temporary buffer, leaving 1 space at the beginning
    // for a prefix we'll vary between the inline and non-inline keys.
    char keyBuffer[maxKeyNameLength + 2] = { 0 };
    strncpy(keyBuffer + 1, name, maxKeyNameLength);
    keyBuffer[0] = '1';
    m_inlineKey = v8::Persistent<v8::String>::New(v8::String::New(keyBuffer));
    keyBuffer[0] = '2';
    m_nonInlineKey = v8::Persistent<v8::String>::New(v8::String::New(keyBuffer));
}

V8EventListenerList::~V8EventListenerList()
{
    m_inlineKey.Dispose();
    m_nonInlineKey.Dispose();
}

V8EventListenerList::iterator V8EventListenerList::begin()
{
    return m_list.begin();
}

V8EventListenerList::iterator V8EventListenerList::end()
{
    return m_list.end();
}

v8::Handle<v8::String> V8EventListenerList::getKey(bool isInline)
{
    return isInline ? m_inlineKey : m_nonInlineKey;
}

// See comment in .h file for this function, and update accordingly if implementation details change here.
void V8EventListenerList::add(V8EventListener* listener)
{
    ASSERT(v8::Context::InContext());
    m_list.append(listener);

    v8::HandleScope handleScope;
    v8::Local<v8::Object> object = listener->getListenerObject();
    v8::Local<v8::Value> value = v8::External::Wrap(listener);
    object->SetHiddenValue(getKey(listener->isAttribute()), value);
}

void V8EventListenerList::remove(V8EventListener* listener)
{
    ASSERT(v8::Context::InContext());
    v8::HandleScope handleScope;
    v8::Handle<v8::String> key = getKey(listener->isAttribute());
    for (size_t i = 0; i < m_list.size(); i++) {
        V8EventListener* element = m_list.at(i);
        if (element->isAttribute() == listener->isAttribute() && element == listener) {
            v8::Local<v8::Object> object = listener->getListenerObject();

            // FIXME(asargent) this check for hidden value being !empty is a workaround for
            // http://code.google.com/p/v8/issues/detail?id=300
            // Once the fix for that is pulled into chromium we can remove the check here.
            if (!object->GetHiddenValue(key).IsEmpty())
                object->DeleteHiddenValue(getKey(listener->isAttribute()));
            m_list.remove(i);
            break;
        }
    }
}

void V8EventListenerList::clear()
{
    ASSERT(v8::Context::InContext());
    v8::HandleScope handleScope;
    for (size_t i = 0; i < m_list.size(); i++) {
        V8EventListener* element = m_list.at(i);
        v8::Local<v8::Object> object = element->getListenerObject();
        object->DeleteHiddenValue(getKey(element->isAttribute()));
    }
    m_list.clear();
}

V8EventListener* V8EventListenerList::find(v8::Local<v8::Object> object, bool isInline)
{
    v8::Local<v8::Value> value = object->GetHiddenValue(getKey(isInline));
    if (value.IsEmpty())
        return 0;
    return reinterpret_cast<V8EventListener*>(v8::External::Unwrap(value));
}

} // namespace WebCore
