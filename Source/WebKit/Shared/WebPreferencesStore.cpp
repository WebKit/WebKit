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

#include "Logging.h"
#include "WebPreferencesKeys.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

typedef HashMap<RefPtr<StringImpl>, bool> BoolOverridesMap;

NODELETE static BoolOverridesMap& boolTestRunnerOverridesMap()
{
    static NeverDestroyed<BoolOverridesMap> map;
    return map;
}

void WebPreferencesStore::overrideBoolValueForKey(StringImpl* key, bool value)
{
    boolTestRunnerOverridesMap().set(key, value);
}

void WebPreferencesStore::removeTestRunnerOverrides()
{
    boolTestRunnerOverridesMap().clear();
}

template<typename MappedType>
NODELETE SUPPRESS_NODELETE static MappedType valueForKey(const WebPreferencesStore::ValueMap& values, const WebPreferencesStore::ValueMap& overriddenDefaults, StringImpl* key)
{
    auto valuesIt = values.find(key);
    if (valuesIt != values.end() && std::holds_alternative<MappedType>(valuesIt->value))
        return std::get<MappedType>(valuesIt->value);

    auto overriddenDefaultsIt = overriddenDefaults.find(key);
    if (overriddenDefaultsIt != overriddenDefaults.end() && std::holds_alternative<MappedType>(overriddenDefaultsIt->value))
        return std::get<MappedType>(overriddenDefaultsIt->value);

    auto& defaultsMap = WebPreferencesStore::defaults();
    auto defaultsIt = defaultsMap.find(key);
    if (defaultsIt != defaultsMap.end() && std::holds_alternative<MappedType>(defaultsIt->value))
        return std::get<MappedType>(defaultsIt->value);

    return MappedType();
}

template<typename MappedType>
static bool valuesAreEqual(const MappedType& a, const MappedType& b)
{
    return a == b;
}

static bool valuesAreEqual(const RefPtr<StringImpl>& a, const RefPtr<StringImpl>& b)
{
    return equal(a, b);
}

template<typename MappedType>
static bool setValueForKey(WebPreferencesStore::ValueMap& map, const WebPreferencesStore::ValueMap& overriddenDefaults, StringImpl* key, const MappedType& value)
{
    MappedType existingValue = valueForKey<MappedType>(map, overriddenDefaults, key);
    if (valuesAreEqual(existingValue, value))
        return false;

    map.set(key, WebPreferencesStore::Value(value));
    return true;
}

bool WebPreferencesStore::setStringValueForKey(StringImpl* key, StringImpl* value)
{
    return setValueForKey<RefPtr<StringImpl>>(m_values, m_overriddenDefaults, key, value);
}

RefPtr<StringImpl> WebPreferencesStore::getStringValueForKey(StringImpl* key) const
{
    return valueForKey<RefPtr<StringImpl>>(m_values, m_overriddenDefaults, key);
}

bool WebPreferencesStore::setBoolValueForKey(StringImpl* key, bool value)
{
    return setValueForKey<bool>(m_values, m_overriddenDefaults, key, value);
}

SUPPRESS_NODELETE bool WebPreferencesStore::getBoolValueForKey(StringImpl* key) const
{
    // FIXME: Extend overriding to other key types used from TestRunner.
    auto it = boolTestRunnerOverridesMap().find(key);
    if (it != boolTestRunnerOverridesMap().end())
        return it->value;

    return valueForKey<bool>(m_values, m_overriddenDefaults, key);
}

bool WebPreferencesStore::setUInt32ValueForKey(StringImpl* key, uint32_t value) 
{
    return setValueForKey<uint32_t>(m_values, m_overriddenDefaults, key, value);
}

uint32_t WebPreferencesStore::getUInt32ValueForKey(StringImpl* key) const
{
    return valueForKey<uint32_t>(m_values, m_overriddenDefaults, key);
}

bool WebPreferencesStore::setDoubleValueForKey(StringImpl* key, double value) 
{
    return setValueForKey<double>(m_values, m_overriddenDefaults, key, value);
}

double WebPreferencesStore::getDoubleValueForKey(StringImpl* key) const
{
    return valueForKey<double>(m_values, m_overriddenDefaults, key);
}

// Overridden Defaults

void WebPreferencesStore::setOverrideDefaultsStringValueForKey(StringImpl* key, RefPtr<StringImpl> value)
{
    m_overriddenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsBoolValueForKey(StringImpl* key, bool value)
{
    m_overriddenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsUInt32ValueForKey(StringImpl* key, uint32_t value)
{
    m_overriddenDefaults.set(key, Value(value));
}

void WebPreferencesStore::setOverrideDefaultsDoubleValueForKey(StringImpl* key, double value)
{
    m_overriddenDefaults.set(key, Value(value));
}

void WebPreferencesStore::deleteKey(StringImpl* key)
{
    m_values.remove(key);
    m_overriddenDefaults.remove(key);
}

#if !RELEASE_LOG_DISABLED
static bool valuesAreEqual(const WebPreferencesStore::Value& a, const WebPreferencesStore::Value& b)
{
    if (a.index() != b.index())
        return false;
    return WTF::switchOn(a, [&]<typename T>(const T& value) {
        return valuesAreEqual(value, std::get<T>(b));
    });
}

static void appendValueToStringBuilder(StringBuilder& builder, const WebPreferencesStore::Value& value)
{
    WTF::switchOn(value,
        [&](bool b) { builder.append(b ? "true"_s : "false"_s); },
        [&](uint32_t u) { builder.append(u); },
        [&](double d) { builder.append(d); },
        [&](const RefPtr<StringImpl>& s) { builder.append('"', s, '"'); }
    );
}
#endif

void WebPreferencesStore::logNonDefaultValues() const
{
#if !RELEASE_LOG_DISABLED
    if (m_values.isEmpty() && m_overriddenDefaults.isEmpty())
        return;

    auto& defaultsMap = defaults();

    StringBuilder nonDefaultPrefs;

    auto checkEntry = [&](StringImpl* key, const Value& value) {
        auto it = defaultsMap.find(key);
        if (it != defaultsMap.end() && valuesAreEqual(it->value, value))
            return;
        if (!nonDefaultPrefs.isEmpty())
            nonDefaultPrefs.append(", "_s);
        nonDefaultPrefs.append(key, '=');
        appendValueToStringBuilder(nonDefaultPrefs, value);
    };

    for (auto& [key, value] : m_overriddenDefaults) {
        if (!m_values.contains(key))
            checkEntry(key, value);
    }

    for (auto& [key, value] : m_values)
        checkEntry(key, value);

    if (!nonDefaultPrefs.isEmpty())
        RELEASE_LOG(Loading, "WebPreferencesStore: non-default preferences: %" PUBLIC_LOG_STRING, utf8(nonDefaultPrefs.toString()));
#endif
}

} // namespace WebKit
