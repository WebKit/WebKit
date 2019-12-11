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

#include "WebCoreArgumentCoders.h"
#include "WebPreferencesKeys.h"
#include <wtf/NeverDestroyed.h>

namespace WebKit {

typedef HashMap<String, bool> BoolOverridesMap;

static BoolOverridesMap& boolTestRunnerOverridesMap()
{
    static NeverDestroyed<BoolOverridesMap> map;
    return map;
}

WebPreferencesStore::WebPreferencesStore()
{
}

void WebPreferencesStore::encode(IPC::Encoder& encoder) const
{
    encoder << m_values;
    encoder << m_overriddenDefaults;
}

bool WebPreferencesStore::decode(IPC::Decoder& decoder, WebPreferencesStore& result)
{
    Optional<HashMap<String, Value>> values;
    decoder >> values;
    if (!values)
        return false;
    result.m_values = WTFMove(*values);

    Optional<HashMap<String, Value>> overriddenDefaults;
    decoder >> overriddenDefaults;
    if (!overriddenDefaults)
        return false;
    result.m_overriddenDefaults = WTFMove(*overriddenDefaults);
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

template<typename MappedType>
static MappedType valueForKey(const WebPreferencesStore::ValueMap& values, const WebPreferencesStore::ValueMap& overriddenDefaults, const String& key)
{
    auto valuesIt = values.find(key);
    if (valuesIt != values.end() && WTF::holds_alternative<MappedType>(valuesIt->value))
        return WTF::get<MappedType>(valuesIt->value);

    auto overriddenDefaultsIt = overriddenDefaults.find(key);
    if (overriddenDefaultsIt != overriddenDefaults.end() && WTF::holds_alternative<MappedType>(overriddenDefaultsIt->value))
        return WTF::get<MappedType>(overriddenDefaultsIt->value);

    auto& defaultsMap = WebPreferencesStore::defaults();
    auto defaultsIt = defaultsMap.find(key);
    if (defaultsIt != defaultsMap.end() && WTF::holds_alternative<MappedType>(defaultsIt->value))
        return WTF::get<MappedType>(defaultsIt->value);

    return MappedType();
}

template<typename MappedType>
static bool setValueForKey(WebPreferencesStore::ValueMap& map, const WebPreferencesStore::ValueMap& overriddenDefaults, const String& key, const MappedType& value)
{
    MappedType existingValue = valueForKey<MappedType>(map, overriddenDefaults, key);
    if (existingValue == value)
        return false;

    map.set(key, WebPreferencesStore::Value(value));
    return true;
}

bool WebPreferencesStore::setStringValueForKey(const String& key, const String& value)
{
    return setValueForKey<String>(m_values, m_overriddenDefaults, key, value);
}

String WebPreferencesStore::getStringValueForKey(const String& key) const
{
    return valueForKey<String>(m_values, m_overriddenDefaults, key);
}

bool WebPreferencesStore::setBoolValueForKey(const String& key, bool value)
{
    return setValueForKey<bool>(m_values, m_overriddenDefaults, key, value);
}

bool WebPreferencesStore::getBoolValueForKey(const String& key) const
{
    // FIXME: Extend overriding to other key types used from TestRunner.
    auto it = boolTestRunnerOverridesMap().find(key);
    if (it != boolTestRunnerOverridesMap().end())
        return it->value;

    return valueForKey<bool>(m_values, m_overriddenDefaults, key);
}

bool WebPreferencesStore::setUInt32ValueForKey(const String& key, uint32_t value) 
{
    return setValueForKey<uint32_t>(m_values, m_overriddenDefaults, key, value);
}

uint32_t WebPreferencesStore::getUInt32ValueForKey(const String& key) const
{
    return valueForKey<uint32_t>(m_values, m_overriddenDefaults, key);
}

bool WebPreferencesStore::setDoubleValueForKey(const String& key, double value) 
{
    return setValueForKey<double>(m_values, m_overriddenDefaults, key, value);
}

double WebPreferencesStore::getDoubleValueForKey(const String& key) const
{
    return valueForKey<double>(m_values, m_overriddenDefaults, key);
}

// Overridden Defaults

void WebPreferencesStore::setOverrideDefaultsStringValueForKey(const String& key, String value)
{
    m_overriddenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsBoolValueForKey(const String& key, bool value)
{
    m_overriddenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsUInt32ValueForKey(const String& key, uint32_t value)
{
    m_overriddenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsDoubleValueForKey(const String& key, double value)
{
    m_overriddenDefaults.set(key, Value(value));
}

void WebPreferencesStore::deleteKey(const String& key)
{
    m_values.remove(key);
    m_overriddenDefaults.remove(key);
}

} // namespace WebKit
