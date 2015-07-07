/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPreferencesStore.h"

#include "FontSmoothingLevel.h"
#include "WebCoreArgumentCoders.h"
#include "WebPreferencesKeys.h"
#include <WebCore/Settings.h>
#include <WebCore/TextEncodingRegistry.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(IOS)
#import <WebKitSystemInterfaceIOS.h>
#endif

using namespace WebCore;

namespace WebKit {

typedef HashMap<String, bool> BoolOverridesMap;

static BoolOverridesMap& boolTestRunnerOverridesMap()
{
    static NeverDestroyed<BoolOverridesMap> map;
    return map;
}

void WebPreferencesStore::Value::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder.encodeEnum(m_type);
    
    switch (m_type) {
    case Type::None:
        break;
    case Type::String:
        encoder << m_string;
        break;
    case Type::Bool:
        encoder << m_bool;
        break;
    case Type::UInt32:
        encoder << m_uint32;
        break;
    case Type::Double:
        encoder << m_double;
        break;
    }
}

bool WebPreferencesStore::Value::decode(IPC::ArgumentDecoder& decoder, Value& result)
{
    Value::Type type;
    if (!decoder.decodeEnum(type))
        return false;
    
    switch (type) {
    case Type::None:
        break;
    case Type::String: {
        String value;
        if (!decoder.decode(value))
            return false;
        result = Value(value);
        break;
    }
    case Type::Bool: {
        bool value;
        if (!decoder.decode(value))
            return false;
        result = Value(value);
        break;
    }
    case Type::UInt32: {
        uint32_t value;
        if (!decoder.decode(value))
            return false;
        result = Value(value);
        break;
    }
    case Type::Double: {
        double value;
        if (!decoder.decode(value))
            return false;
        result = Value(value);
        break;
    }
    default:
        return false;
    }

    return true;
}

WebPreferencesStore::WebPreferencesStore()
{
}

void WebPreferencesStore::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_values;
    encoder << m_overridenDefaults;
}

bool WebPreferencesStore::decode(IPC::ArgumentDecoder& decoder, WebPreferencesStore& result)
{
    if (!decoder.decode(result.m_values))
        return false;
    if (!decoder.decode(result.m_overridenDefaults))
        return false;
    return true;
}

void WebPreferencesStore::overrideBoolValueForKey(const String& key, bool value)
{
    boolTestRunnerOverridesMap().set(key, value);
}

void WebPreferencesStore::removeTestRunnerOverrides()
{
    boolTestRunnerOverridesMap().clear();
}

template <typename T> struct ToType { };

template<> struct ToType<String> { static const auto value = WebPreferencesStore::Value::Type::String; };
template<> struct ToType<bool> { static const auto value = WebPreferencesStore::Value::Type::Bool; };
template<> struct ToType<uint32_t> { static const auto value = WebPreferencesStore::Value::Type::UInt32; };
template<> struct ToType<double> { static const auto value = WebPreferencesStore::Value::Type::Double; };


template<typename MappedType> MappedType as(const WebPreferencesStore::Value&);

template<> String as<String>(const WebPreferencesStore::Value& value) { return value.asString(); }
template<> bool as<bool>(const WebPreferencesStore::Value& value) { return value.asBool(); }
template<> uint32_t as<uint32_t>(const WebPreferencesStore::Value& value) { return value.asUInt32(); }
template<> double as<double>(const WebPreferencesStore::Value& value) { return value.asDouble(); }


static WebPreferencesStore::ValueMap& defaults()
{
    static NeverDestroyed<WebPreferencesStore::ValueMap> defaults;
    if (defaults.get().isEmpty()) {
#define DEFINE_DEFAULTS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) defaults.get().set(WebPreferencesKey::KeyLower##Key(), WebPreferencesStore::Value((Type)DefaultValue));
        FOR_EACH_WEBKIT_PREFERENCE(DEFINE_DEFAULTS)
        FOR_EACH_WEBKIT_DEBUG_PREFERENCE(DEFINE_DEFAULTS)
#undef DEFINE_DEFAULTS
    }

    return defaults;
}

template<typename MappedType>
static MappedType valueForKey(const WebPreferencesStore::ValueMap& values, const WebPreferencesStore::ValueMap& overridenDefaults, const String& key)
{
    auto valuesIt = values.find(key);
    if (valuesIt != values.end() && valuesIt->value.type() == ToType<MappedType>::value)
        return as<MappedType>(valuesIt->value);

    auto overridenDefaultsIt = overridenDefaults.find(key);
    if (overridenDefaultsIt != overridenDefaults.end() && overridenDefaultsIt->value.type() == ToType<MappedType>::value)
        return as<MappedType>(overridenDefaultsIt->value);

    auto defaultsMap = defaults();
    auto defaultsIt = defaultsMap.find(key);
    if (defaultsIt != defaultsMap.end() && defaultsIt->value.type() == ToType<MappedType>::value)
        return as<MappedType>(defaultsIt->value);

    return MappedType();
}

template<typename MappedType>
static bool setValueForKey(WebPreferencesStore::ValueMap& map, const WebPreferencesStore::ValueMap& overridenDefaults, const String& key, const MappedType& value)
{
    MappedType existingValue = valueForKey<MappedType>(map, overridenDefaults, key);
    if (existingValue == value)
        return false;

    map.set(key, WebPreferencesStore::Value(value));
    return true;
}

bool WebPreferencesStore::setStringValueForKey(const String& key, const String& value)
{
    return setValueForKey<String>(m_values, m_overridenDefaults, key, value);
}

String WebPreferencesStore::getStringValueForKey(const String& key) const
{
    return valueForKey<String>(m_values, m_overridenDefaults, key);
}

bool WebPreferencesStore::setBoolValueForKey(const String& key, bool value)
{
    return setValueForKey<bool>(m_values, m_overridenDefaults, key, value);
}

bool WebPreferencesStore::getBoolValueForKey(const String& key) const
{
    // FIXME: Extend overriding to other key types used from TestRunner.
    auto it = boolTestRunnerOverridesMap().find(key);
    if (it != boolTestRunnerOverridesMap().end())
        return it->value;

    return valueForKey<bool>(m_values, m_overridenDefaults, key);
}

bool WebPreferencesStore::setUInt32ValueForKey(const String& key, uint32_t value) 
{
    return setValueForKey<uint32_t>(m_values, m_overridenDefaults, key, value);
}

uint32_t WebPreferencesStore::getUInt32ValueForKey(const String& key) const
{
    return valueForKey<uint32_t>(m_values, m_overridenDefaults, key);
}

bool WebPreferencesStore::setDoubleValueForKey(const String& key, double value) 
{
    return setValueForKey<double>(m_values, m_overridenDefaults, key, value);
}

double WebPreferencesStore::getDoubleValueForKey(const String& key) const
{
    return valueForKey<double>(m_values, m_overridenDefaults, key);
}

// Overriden Defaults

void WebPreferencesStore::setOverrideDefaultsStringValueForKey(const String& key, String value)
{
    m_overridenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsBoolValueForKey(const String& key, bool value)
{
    m_overridenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsUInt32ValueForKey(const String& key, uint32_t value)
{
    m_overridenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsDoubleValueForKey(const String& key, double value)
{
    m_overridenDefaults.set(key, Value(value));
}

} // namespace WebKit
