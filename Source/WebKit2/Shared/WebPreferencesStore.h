/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPreferencesStore_h
#define WebPreferencesStore_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct WebPreferencesStore {
    WebPreferencesStore();

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebPreferencesStore&);

    // NOTE: The getters in this class have non-standard names to aid in the use of the preference macros.

    bool setStringValueForKey(const String& key, const String& value);
    String getStringValueForKey(const String& key) const;

    bool setBoolValueForKey(const String& key, bool value);
    bool getBoolValueForKey(const String& key) const;

    bool setUInt32ValueForKey(const String& key, uint32_t value);
    uint32_t getUInt32ValueForKey(const String& key) const;

    bool setDoubleValueForKey(const String& key, double value);
    double getDoubleValueForKey(const String& key) const;

    void setOverrideDefaultsStringValueForKey(const String& key, String value);
    void setOverrideDefaultsBoolValueForKey(const String& key, bool value);
    void setOverrideDefaultsUInt32ValueForKey(const String& key, uint32_t value);
    void setOverrideDefaultsDoubleValueForKey(const String& key, double value);

    // For WebKitTestRunner usage.
    static void overrideBoolValueForKey(const String& key, bool value);
    static void removeTestRunnerOverrides();

    struct Value {
        enum class Type {
            None,
            String,
            Bool,
            UInt32,
            Double,
        };

        void encode(IPC::ArgumentEncoder&) const;
        static bool decode(IPC::ArgumentDecoder&, Value&);

        explicit Value() : m_type(Type::None) { }
        explicit Value(const String& value) : m_type(Type::String), m_string(value) { }
        explicit Value(bool value) : m_type(Type::Bool), m_bool(value) { }
        explicit Value(uint32_t value) : m_type(Type::UInt32), m_uint32(value) { }
        explicit Value(double value) : m_type(Type::Double), m_double(value) { }

        Value(Value&& value)
            : m_type(value.m_type)
        {
            switch (m_type) {
            case Type::None:
                break;
            case Type::String:
                new (&m_string) String(WTF::move(value.m_string));
                break;
            case Type::Bool:
                m_bool = value.m_bool;
                break;
            case Type::UInt32:
                m_uint32 = value.m_uint32;
                break;
            case Type::Double:
                m_double = value.m_double;
                break;
            }
        }

        Value& operator=(const Value& other)
        {
            if (this == &other)
                return *this;
                
            destroy();

            m_type = other.m_type;
            switch (m_type) {
            case Type::None:
                break;
            case Type::String:
                new (&m_string) String(other.m_string);
                break;
            case Type::Bool:
                m_bool = other.m_bool;
                break;
            case Type::UInt32:
                m_uint32 = other.m_uint32;
                break;
            case Type::Double:
                m_double = other.m_double;
                break;
            }
    
            return *this;
        }

        ~Value()
        {
            destroy();
        }

        Type type() const { return m_type; }

        String asString() const
        {
            ASSERT(m_type == Type::String);
            return m_string;
        }

        bool asBool() const
        {
            ASSERT(m_type == Type::Bool);
            return m_bool;
        }

        uint32_t asUInt32() const
        {
            ASSERT(m_type == Type::UInt32);
            return m_uint32;
        }

        double asDouble() const
        {
            ASSERT(m_type == Type::Double);
            return m_double;
        }

    private:
        void destroy()
        {
            if (m_type == Type::String)
                m_string.~String();
        }

        Type m_type;
        union {
            String m_string;
            bool m_bool;
            uint32_t m_uint32;
            double m_double;
        };
    };

    typedef HashMap<String, Value> ValueMap;
    ValueMap m_values;
    ValueMap m_overridenDefaults;
};

} // namespace WebKit

#endif // WebPreferencesStore_h
