/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SerializedScriptValue_h
#define SerializedScriptValue_h

#include "ScriptValue.h"

namespace WebCore {
    class SerializedObject;
    class SerializedArray;

    class SharedSerializedData : public RefCounted<SharedSerializedData> {
    public:
        virtual ~SharedSerializedData() { }
        SerializedArray* asArray();
        SerializedObject* asObject();
    };

    class SerializedScriptValue;

    class SerializedScriptValueData {
    public:
        enum SerializedType {
            EmptyType,
            DateType,
            NumberType,
            ImmediateType,
            ObjectType,
            ArrayType,
            StringType
        };

        SerializedType type() const { return m_type; }
        static SerializedScriptValueData serialize(JSC::ExecState*, JSC::JSValue);
        JSC::JSValue deserialize(JSC::ExecState*, bool mustCopy) const;

        ~SerializedScriptValueData()
        {
            if (m_sharedData)
                tearDownSerializedData();
        }

        SerializedScriptValueData()
            : m_type(EmptyType)
        {
        }

        explicit SerializedScriptValueData(const String& string)
            : m_type(StringType)
            , m_string(string.crossThreadString()) // FIXME: Should be able to just share the Rep
        {
        }

        explicit SerializedScriptValueData(JSC::JSValue value)
            : m_type(ImmediateType)
        {
            ASSERT(!value.isCell());
            m_data.m_immediate = JSC::JSValue::encode(value);
        }

        SerializedScriptValueData(SerializedType type, double value)
            : m_type(type)
        {
            m_data.m_double = value;
        }

        SerializedScriptValueData(RefPtr<SerializedObject>);
        SerializedScriptValueData(RefPtr<SerializedArray>);

        JSC::JSValue asImmediate() const
        {
            ASSERT(m_type == ImmediateType);
            return JSC::JSValue::decode(m_data.m_immediate);
        }

        double asDouble() const
        {
            ASSERT(m_type == NumberType || m_type == DateType);
            return m_data.m_double;
        }

        String asString() const
        {
            ASSERT(m_type == StringType);
            return m_string;
        }

        SerializedObject* asObject() const
        {
            ASSERT(m_type == ObjectType);
            ASSERT(m_sharedData);
            return m_sharedData->asObject();
        }

        SerializedArray* asArray() const
        {
            ASSERT(m_type == ArrayType);
            ASSERT(m_sharedData);
            return m_sharedData->asArray();
        }

        operator bool() const { return m_type != EmptyType; }

        SerializedScriptValueData release()
        {
            SerializedScriptValueData result = *this;
            *this = SerializedScriptValueData();
            return result;
        }

    private:
        void tearDownSerializedData();
        SerializedType m_type;
        RefPtr<SharedSerializedData> m_sharedData;
        String m_string;
        union {
            double m_double;
            JSC::EncodedJSValue m_immediate;
        } m_data;
    };

    class SerializedScriptValue : public RefCounted<SerializedScriptValue> {
    public:
        static PassRefPtr<SerializedScriptValue> create(JSC::ExecState* exec, JSC::JSValue value)
        {
            return adoptRef(new SerializedScriptValue(SerializedScriptValueData::serialize(exec, value)));
        }

        static PassRefPtr<SerializedScriptValue> create(String string)
        {
            return adoptRef(new SerializedScriptValue(SerializedScriptValueData(string)));
        }

        static PassRefPtr<SerializedScriptValue> create()
        {
            return adoptRef(new SerializedScriptValue(SerializedScriptValueData()));
        }

        PassRefPtr<SerializedScriptValue> release()
        {
            PassRefPtr<SerializedScriptValue> result = adoptRef(new SerializedScriptValue(m_value));
            m_value = SerializedScriptValueData();
            result->m_mustCopy = true;
            return result;
        }

        String toString()
        {
            if (m_value.type() != SerializedScriptValueData::StringType)
                return "";
            return m_value.asString();
        }

        JSC::JSValue deserialize(JSC::ExecState* exec)
        {
            if (!m_value)
                return JSC::jsNull();
            return m_value.deserialize(exec, m_mustCopy);
        }

        ~SerializedScriptValue() {}

    private:
        SerializedScriptValue(SerializedScriptValueData value)
            : m_value(value)
            , m_mustCopy(false)
        {

        SerializedScriptValueData m_value;
        bool m_mustCopy;
    };
}

#endif // SerializedScriptValue_h
