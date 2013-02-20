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

#ifndef ScriptValue_h
#define ScriptValue_h

#include "ScriptState.h"
#include "SharedPersistent.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#ifndef NDEBUG 
#include "V8GCController.h" 
#endif 

namespace WTF {
class ArrayBuffer;
}

namespace WebCore {

class InspectorValue;
class MessagePort;
class SerializedScriptValue;
typedef Vector<RefPtr<MessagePort>, 1> MessagePortArray;
typedef Vector<RefPtr<WTF::ArrayBuffer>, 1> ArrayBufferArray;

class ScriptValue {
public:
    ScriptValue() { }
    virtual ~ScriptValue();

    ScriptValue(v8::Handle<v8::Value> value)
        : m_value(value.IsEmpty() ? 0 : SharedPersistent<v8::Value>::create(value))
    {
    }

    ScriptValue(const ScriptValue& value)
        : m_value(value.m_value)
    {
    }

    ScriptValue& operator=(const ScriptValue& value) 
    {
        if (this != &value)
            m_value = value.m_value;
        return *this;
    }

    bool operator==(const ScriptValue& value) const
    {
        return v8ValueRaw() == value.v8ValueRaw();
    }

    bool isEqual(ScriptState*, const ScriptValue& value) const
    {
        return operator==(value);
    }

    bool isFunction() const
    {
        ASSERT(!hasNoValue());
        return v8ValueRaw()->IsFunction();
    }

    bool operator!=(const ScriptValue& value) const
    {
        return !operator==(value);
    }

    bool isNull() const
    {
        ASSERT(!hasNoValue());
        return v8ValueRaw()->IsNull();
    }

    bool isUndefined() const
    {
        ASSERT(!hasNoValue());
        return v8ValueRaw()->IsUndefined();
    }

    bool isObject() const
    {
        ASSERT(!hasNoValue());
        return v8ValueRaw()->IsObject();
    }

    bool hasNoValue() const
    {
        return !m_value.get() || m_value->get().IsEmpty();
    }

    PassRefPtr<SerializedScriptValue> serialize(ScriptState*);
    PassRefPtr<SerializedScriptValue> serialize(ScriptState*, MessagePortArray*, ArrayBufferArray*, bool&);
    static ScriptValue deserialize(ScriptState*, SerializedScriptValue*);

    void clear()
    {
        m_value = 0;
    }

    v8::Handle<v8::Value> v8Value() const
    {
        return v8::Local<v8::Value>::New(v8ValueRaw());
    }

    // FIXME: This function should be private. 
    v8::Handle<v8::Value> v8ValueRaw() const
    {
        return m_value.get() ? m_value->get() : v8::Handle<v8::Value>();
    }

    bool getString(ScriptState*, String& result) const { return getString(result); }
    bool getString(String& result) const;
    String toString(ScriptState*) const;

    PassRefPtr<InspectorValue> toInspectorValue(ScriptState*) const;

private:
    RefPtr<SharedPersistent<v8::Value> > m_value;
};

} // namespace WebCore

#endif // ScriptValue_h
