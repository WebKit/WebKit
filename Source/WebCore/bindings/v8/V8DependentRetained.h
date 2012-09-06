/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef V8DependentRetained_h
#define V8DependentRetained_h

#include "ScopedPersistent.h"
#include "V8HiddenPropertyName.h"
#include "V8PerIsolateData.h"
#include <v8.h>
#include <wtf/Forward.h>
#include <wtf/UnusedParam.h>
#include <wtf/dtoa.h>

namespace WebCore {

class V8DependentRetained {
public:
    V8DependentRetained(v8::Handle<v8::Object> owner, v8::Handle<v8::Object> value)
        : m_owner(owner)
        , m_value(value)
        , m_propertyName(createPropertyName())
    {
        ASSERT(!m_owner.IsEmpty());
        ASSERT(!m_value.IsEmpty());
        owner->SetHiddenValue(m_propertyName.get(), value);
        m_owner.get().MakeWeak(this, &V8DependentRetained::ownerWeakCallback);
        m_value.get().MakeWeak(this, &V8DependentRetained::valueWeakCallback);
    }

    ~V8DependentRetained()
    {
        release();
    }

    v8::Handle<v8::Object> get() const
    {
        return m_value.get();
    }

    bool isEmpty() const
    {
        return m_owner.isEmpty() || m_value.isEmpty();
    }

    // FIXME: We might add an explicit retain(v8::Handle<v8::Object>) method to allow
    // creating one of these and choosing the owner later. Such behavior is required
    // in JSC, but not in v8 right now.

private:
    static v8::Handle<v8::String> createPropertyName()
    {
        static const char* prefix = "V8DependentRetained";
        NumberToStringBuffer buffer;
        Vector<char, 64> name;
        const char* id = numberToString(V8PerIsolateData::current()->nextDependentRetainedId(), buffer);
        name.append(prefix, sizeof(prefix) - 1);
        name.append(id, strlen(id) + 1);
        return V8HiddenPropertyName::hiddenReferenceName(name.data(), NewString);
    }

    static void ownerWeakCallback(v8::Persistent<v8::Value> object, void* parameter)
    {
        V8DependentRetained* value = static_cast<V8DependentRetained*>(parameter);
        value->release();
    }

    static void valueWeakCallback(v8::Persistent<v8::Value> object, void* parameter)
    {
        V8DependentRetained* value = static_cast<V8DependentRetained*>(parameter);
        // The owner callback should always be called first, since it retains the value.
        ASSERT_UNUSED(value, value->isEmpty());
    }

    void release()
    {
        if (!m_owner.isEmpty())
            m_owner->DeleteHiddenValue(m_propertyName.get());
        m_value.clear();
        m_owner.clear();
        m_propertyName.clear();
    }

    ScopedPersistent<v8::Object> m_owner;
    ScopedPersistent<v8::Object> m_value;
    ScopedPersistent<v8::String> m_propertyName;
};

} // namespace WebCore

#endif // V8DependentRetained_h
