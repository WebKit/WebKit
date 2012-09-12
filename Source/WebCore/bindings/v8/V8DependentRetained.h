/*
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
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
        : m_value(value)
        , m_propertyName(createPropertyName())
    {
        ASSERT(!m_value.isEmpty());
        m_value.get().MakeWeak(this, &V8DependentRetained::valueWeakCallback);
        if (!owner.IsEmpty())
            retain(owner);
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

    void retain(v8::Handle<v8::Object> owner)
    {
        ASSERT(m_owner.isEmpty() && !owner.IsEmpty());
        ASSERT(!m_value.isEmpty());
        owner->SetHiddenValue(m_propertyName.get(), get());
        m_owner = owner;
        m_owner.get().MakeWeak(this, &V8DependentRetained::ownerWeakCallback);
    }

private:
    static v8::Handle<v8::String> createPropertyName()
    {
        StringBuilder name;
        name.appendLiteral("V8DependentRetained");
        name.append(String::numberToStringECMAScript(V8PerIsolateData::current()->nextDependentRetainedId()));
        return V8HiddenPropertyName::hiddenReferenceName(reinterpret_cast<const char*>(name.characters8()), name.length(), NewString);
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
