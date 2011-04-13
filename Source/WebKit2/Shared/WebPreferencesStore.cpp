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
#include <WebCore/Settings.h>

namespace WebKit {

namespace WebPreferencesKey {

#define DEFINE_KEY_GETTERS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) \
        const String& KeyLower##Key() \
        { \
            DEFINE_STATIC_LOCAL(String, key, (#KeyUpper)); \
            return key; \
        }

    FOR_EACH_WEBKIT_PREFERENCE(DEFINE_KEY_GETTERS)

#undef DEFINE_KEY_GETTERS

} // namespace WebPreferencesKey


static bool hasXSSAuditorEnabledTestRunnerOverride;
static bool xssAuditorEnabledTestRunnerOverride;
static bool hasAllowUniversalAccessFromFileURLsTestRunnerOverride;
static bool allowUniversalAccessFromFileURLsTestRunnerOverride;
static bool hasAllowFileAccessFromFileURLsTestRunnerOverride;
static bool allowFileAccessFromFileURLsTestRunnerOverride;

WebPreferencesStore::WebPreferencesStore()
{
}

void WebPreferencesStore::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(CoreIPC::In(m_stringValues, m_boolValues, m_uint32Values, m_doubleValues));
}

bool WebPreferencesStore::decode(CoreIPC::ArgumentDecoder* decoder, WebPreferencesStore& s)
{
    if (!decoder->decode(CoreIPC::Out(s.m_stringValues, s.m_boolValues, s.m_uint32Values, s.m_doubleValues)))
        return false;

    if (hasXSSAuditorEnabledTestRunnerOverride)
        s.m_boolValues.set(WebPreferencesKey::xssAuditorEnabledKey(), xssAuditorEnabledTestRunnerOverride);

    if (hasAllowUniversalAccessFromFileURLsTestRunnerOverride)
        s.m_boolValues.set(WebPreferencesKey::allowUniversalAccessFromFileURLsKey(), allowUniversalAccessFromFileURLsTestRunnerOverride);

    if (hasAllowFileAccessFromFileURLsTestRunnerOverride)
        s.m_boolValues.set(WebPreferencesKey::allowFileAccessFromFileURLsKey(), allowFileAccessFromFileURLsTestRunnerOverride);

    return true;
}

void WebPreferencesStore::overrideXSSAuditorEnabledForTestRunner(bool enabled)
{
    hasXSSAuditorEnabledTestRunnerOverride = true;
    xssAuditorEnabledTestRunnerOverride = enabled;
}

void WebPreferencesStore::overrideAllowUniversalAccessFromFileURLsForTestRunner(bool enabled)
{
    hasAllowUniversalAccessFromFileURLsTestRunnerOverride = true;
    allowUniversalAccessFromFileURLsTestRunnerOverride = enabled;
}

void WebPreferencesStore::overrideAllowFileAccessFromFileURLsForTestRunner(bool enabled)
{
    hasAllowFileAccessFromFileURLsTestRunnerOverride = true;
    allowFileAccessFromFileURLsTestRunnerOverride = enabled;
}

void WebPreferencesStore::removeTestRunnerOverrides()
{
    hasXSSAuditorEnabledTestRunnerOverride = false;
}


template<typename MappedType>
MappedType defaultValueForKey(const String&);

template<>
String defaultValueForKey(const String& key)
{
    static HashMap<String, String>& defaults = *new HashMap<String, String>;
    if (defaults.isEmpty()) {
#define DEFINE_STRING_DEFAULTS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) defaults.set(WebPreferencesKey::KeyLower##Key(), DefaultValue);
        FOR_EACH_WEBKIT_STRING_PREFERENCE(DEFINE_STRING_DEFAULTS)
#undef DEFINE_STRING_DEFAULTS
    }

    return defaults.get(key);
}

template<>
bool defaultValueForKey(const String& key)
{
    static HashMap<String, bool>& defaults = *new HashMap<String, bool>;
    if (defaults.isEmpty()) {
#define DEFINE_BOOL_DEFAULTS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) defaults.set(WebPreferencesKey::KeyLower##Key(), DefaultValue);
        FOR_EACH_WEBKIT_BOOL_PREFERENCE(DEFINE_BOOL_DEFAULTS)
#undef DEFINE_BOOL_DEFAULTS
    }

    return defaults.get(key);
}

template<>
uint32_t defaultValueForKey(const String& key)
{
    static HashMap<String, uint32_t>& defaults = *new HashMap<String, uint32_t>;
    if (defaults.isEmpty()) {
#define DEFINE_UINT32_DEFAULTS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) defaults.set(WebPreferencesKey::KeyLower##Key(), DefaultValue);
        FOR_EACH_WEBKIT_UINT32_PREFERENCE(DEFINE_UINT32_DEFAULTS)
#undef DEFINE_UINT32_DEFAULTS
    }

    return defaults.get(key);
}

template<>
double defaultValueForKey(const String& key)
{
    static HashMap<String, double>& defaults = *new HashMap<String, double>;
    if (defaults.isEmpty()) {
#define DEFINE_DOUBLE_DEFAULTS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) defaults.set(WebPreferencesKey::KeyLower##Key(), DefaultValue);
        FOR_EACH_WEBKIT_DOUBLE_PREFERENCE(DEFINE_DOUBLE_DEFAULTS)
#undef DEFINE_DOUBLE_DEFAULTS
    }

    return defaults.get(key);
}

template<typename MapType>
static typename MapType::MappedType valueForKey(const MapType& map, const typename MapType::KeyType& key)
{
    typename MapType::const_iterator it = map.find(key);
    if (it != map.end())
        return it->second;

    return defaultValueForKey<typename MapType::MappedType>(key);
}

template<typename MapType>
static bool setValueForKey(MapType& map, const typename MapType::KeyType& key, const typename MapType::MappedType& value)
{
    typename MapType::MappedType existingValue = valueForKey(map, key);
    if (existingValue == value)
        return false;
    
    map.set(key, value);
    return true;
}

bool WebPreferencesStore::setStringValueForKey(const String& key, const String& value)
{
    return setValueForKey(m_stringValues, key, value);
}

String WebPreferencesStore::getStringValueForKey(const String& key) const
{
    return valueForKey(m_stringValues, key);
}

bool WebPreferencesStore::setBoolValueForKey(const String& key, bool value)
{
    return setValueForKey(m_boolValues, key, value);
}

bool WebPreferencesStore::getBoolValueForKey(const String& key) const
{
    return valueForKey(m_boolValues, key);
}

bool WebPreferencesStore::setUInt32ValueForKey(const String& key, uint32_t value) 
{
    return setValueForKey(m_uint32Values, key, value);
}

uint32_t WebPreferencesStore::getUInt32ValueForKey(const String& key) const
{
    return valueForKey(m_uint32Values, key);
}

bool WebPreferencesStore::setDoubleValueForKey(const String& key, double value) 
{
    return setValueForKey(m_doubleValues, key, value);
}

double WebPreferencesStore::getDoubleValueForKey(const String& key) const
{
    return valueForKey(m_doubleValues, key);
}

} // namespace WebKit
