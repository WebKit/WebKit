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

#include "PlatformString.h"
#include "ScriptState.h"

#include <v8.h>
#include <wtf/PassRefPtr.h>

#ifndef NDEBUG
#include "V8Proxy.h"  // for register and unregister global handles.
#endif

namespace WebCore {

class SerializedScriptValue;

class ScriptValue {
public:
    ScriptValue() {}

    ScriptValue(v8::Handle<v8::Value> value) 
    {
        if (value.IsEmpty())
            return;

        m_value = v8::Persistent<v8::Value>::New(value);
#ifndef NDEBUG
        V8GCController::registerGlobalHandle(SCRIPTVALUE, this, m_value);
#endif
    }

    ScriptValue(const ScriptValue& value) 
    {
        if (value.m_value.IsEmpty())
            return;

        m_value = v8::Persistent<v8::Value>::New(value.m_value);
#ifndef NDEBUG
        V8GCController::registerGlobalHandle(SCRIPTVALUE, this, m_value);
#endif
    }

    ScriptValue& operator=(const ScriptValue& value) 
    {
        if (this == &value) 
            return *this;

        clear();

        if (value.m_value.IsEmpty())
            return *this;

        m_value = v8::Persistent<v8::Value>::New(value.m_value);
#ifndef NDEBUG
        V8GCController::registerGlobalHandle(SCRIPTVALUE, this, m_value);
#endif

        return *this;
    }

    bool operator==(const ScriptValue value) const
    {
        return m_value == value.m_value;
    }

    bool isEqual(ScriptState*, const ScriptValue& value) const
    {
        return m_value == value.m_value;
    }

    bool operator!=(const ScriptValue value) const
    {
        return !operator==(value);
    }

    bool isNull() const
    {
        return m_value->IsNull();
    }

    bool isUndefined() const
    {
        return m_value->IsUndefined();
    }

    bool isObject() const
    {
        return m_value->IsObject();
    }

    bool hasNoValue() const
    {
        return m_value.IsEmpty();
    }

    PassRefPtr<SerializedScriptValue> serialize(ScriptState*);
    static ScriptValue deserialize(ScriptState*, SerializedScriptValue*);

    void clear()
    {
        if (m_value.IsEmpty())
            return;

#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_value);
#endif
        m_value.Dispose();
        m_value.Clear();
    }

    virtual ~ScriptValue() 
    {
        clear();
    }

    v8::Handle<v8::Value> v8Value() const { return m_value; }
    bool getString(ScriptState*, String& result) const { return getString(result); }
    bool getString(String& result) const;
    String toString(ScriptState*) const;

private:
    mutable v8::Persistent<v8::Value> m_value;
};

} // namespace WebCore

#endif // ScriptValue_h
