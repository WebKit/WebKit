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

#import "config.h"
#import "WebPreferences.h"

#import "StringUtilities.h"
#import "WebPreferencesKeys.h"
#import <WebCore/RealtimeMediaSourceCenter.h>
#import <wtf/text/StringConcatenate.h>

#if ENABLE(MEDIA_STREAM)
#include "UserMediaPermissionRequestManagerProxy.h"
#endif

namespace WebKit {

static inline NSString *makeKey(const String& identifier, const String& keyPrefix, const String& key)
{
    ASSERT(!identifier.isEmpty());
    return String(identifier + keyPrefix + key);
}

bool WebPreferences::platformGetStringUserValueForKey(const String& key, String& userValue)
{
    if (!m_identifier)
        return false;

    id object = [[NSUserDefaults standardUserDefaults] objectForKey:makeKey(m_identifier, m_keyPrefix, key)];
    if (!object)
        return false;
    if (![object isKindOfClass:[NSString class]])
        return false;

    userValue = (NSString *)object;
    return true;
}

bool WebPreferences::platformGetBoolUserValueForKey(const String& key, bool& userValue)
{
    if (!m_identifier)
        return false;

    id object = [[NSUserDefaults standardUserDefaults] objectForKey:makeKey(m_identifier, m_keyPrefix, key)];
    if (!object)
        return false;
    if (![object respondsToSelector:@selector(boolValue)])
        return false;

    userValue = [object boolValue];
    return true;
}

bool WebPreferences::platformGetUInt32UserValueForKey(const String& key, uint32_t& userValue)
{
    if (!m_identifier)
        return false;

    id object = [[NSUserDefaults standardUserDefaults] objectForKey:makeKey(m_identifier, m_keyPrefix, key)];
    if (!object)
        return false;
    if (![object respondsToSelector:@selector(intValue)])
        return false;

    userValue = [object intValue];
    return true;
}

bool WebPreferences::platformGetDoubleUserValueForKey(const String& key, double& userValue)
{
    if (!m_identifier)
        return false;

    id object = [[NSUserDefaults standardUserDefaults] objectForKey:makeKey(m_identifier, m_keyPrefix, key)];
    if (!object)
        return false;
    if (![object respondsToSelector:@selector(doubleValue)])
        return false;

    userValue = [object doubleValue];
    return true;
}

static id debugUserDefaultsValue(const String& identifier, const String& keyPrefix, const String& globalDebugKeyPrefix, const String& key)
{
    NSUserDefaults *standardUserDefaults = [NSUserDefaults standardUserDefaults];
    id object = nil;

    if (!identifier.isEmpty())
        object = [standardUserDefaults objectForKey:makeKey(identifier, keyPrefix, key)];

    if (!object) {
        // Allow debug preferences to be set globally, using the debug key prefix.
        object = [standardUserDefaults objectForKey:[globalDebugKeyPrefix stringByAppendingString:key]];
    }

    return object;
}

static void setDebugBoolValueIfInUserDefaults(const String& identifier, const String& keyPrefix, const String& globalDebugKeyPrefix, const String& key, WebPreferencesStore& store)
{
    id object = debugUserDefaultsValue(identifier, keyPrefix, globalDebugKeyPrefix, key);
    if (!object)
        return;
    if (![object respondsToSelector:@selector(boolValue)])
        return;

    store.setBoolValueForKey(key, [object boolValue]);
}

void WebPreferences::platformInitializeStore()
{
    @autoreleasepool {
#if ENABLE(MEDIA_STREAM)
        // NOTE: This is set here, and does not setting the default using the 'defaultValue' mechanism, because the
        // 'defaultValue' must be the same in both the UIProcess and WebProcess, which may not be true for audio
        // and video capture state as the WebProcess is not entitled to use the camera or microphone by default.
        // If other preferences need to dynamically set the initial value based on host app state, we should extended
        // the declarative format rather than adding more special cases here.
        m_store.setBoolValueForKey(WebPreferencesKey::mediaDevicesEnabledKey(), UserMediaPermissionRequestManagerProxy::permittedToCaptureAudio() || UserMediaPermissionRequestManagerProxy::permittedToCaptureVideo());
        m_store.setBoolValueForKey(WebPreferencesKey::interruptAudioOnPageVisibilityChangeEnabledKey(),  WebCore::RealtimeMediaSourceCenter::shouldInterruptAudioOnPageVisibilityChange());
#endif

#define INITIALIZE_DEFAULT_OVERRIDABLE_PREFERENCE_FROM_NSUSERDEFAULTS(KeyUpper, KeyLower, TypeName, Type, DefaultValue, HumanReadableName, HumanReadableDescription) \
        setDebug##TypeName##ValueIfInUserDefaults(m_identifier, m_keyPrefix, m_globalDebugKeyPrefix, WebPreferencesKey::KeyLower##Key(), m_store);

        FOR_EACH_DEFAULT_OVERRIDABLE_WEBKIT_PREFERENCE(INITIALIZE_DEFAULT_OVERRIDABLE_PREFERENCE_FROM_NSUSERDEFAULTS)

#undef INITIALIZE_DEFAULT_OVERRIDABLE_PREFERENCE_FROM_NSUSERDEFAULTS

        if (!m_identifier)
            return;

#define INITIALIZE_PREFERENCE_FROM_NSUSERDEFAULTS(KeyUpper, KeyLower, TypeName, Type, DefaultValue, HumanReadableName, HumanReadableDescription) \
        Type user##KeyUpper##Value; \
        if (platformGet##TypeName##UserValueForKey(WebPreferencesKey::KeyLower##Key(), user##KeyUpper##Value)) \
            m_store.set##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key(), user##KeyUpper##Value);

        FOR_EACH_PERSISTENT_WEBKIT_PREFERENCE(INITIALIZE_PREFERENCE_FROM_NSUSERDEFAULTS)

#undef INITIALIZE_PREFERENCE_FROM_NSUSERDEFAULTS
    }
}

void WebPreferences::platformUpdateStringValueForKey(const String& key, const String& value)
{
    if (!m_identifier)
        return;

    [[NSUserDefaults standardUserDefaults] setObject:nsStringFromWebCoreString(value) forKey:makeKey(m_identifier, m_keyPrefix, key)];
}

void WebPreferences::platformUpdateBoolValueForKey(const String& key, bool value)
{
    if (!m_identifier)
        return;

    [[NSUserDefaults standardUserDefaults] setBool:value forKey:makeKey(m_identifier, m_keyPrefix, key)];
}

void WebPreferences::platformUpdateUInt32ValueForKey(const String& key, uint32_t value)
{
    if (!m_identifier)
        return;

    [[NSUserDefaults standardUserDefaults] setInteger:value forKey:makeKey(m_identifier, m_keyPrefix, key)];
}

void WebPreferences::platformUpdateDoubleValueForKey(const String& key, double value)
{
    if (!m_identifier)
        return;

    [[NSUserDefaults standardUserDefaults] setDouble:value forKey:makeKey(m_identifier, m_keyPrefix, key)];
}

void WebPreferences::platformUpdateFloatValueForKey(const String& key, float value)
{
    if (!m_identifier)
        return;

    [[NSUserDefaults standardUserDefaults] setFloat:value forKey:makeKey(m_identifier, m_keyPrefix, key)];
}

void WebPreferences::platformDeleteKey(const String& key)
{
    if (!m_identifier)
        return;

    [[NSUserDefaults standardUserDefaults] removeObjectForKey:makeKey(m_identifier, m_keyPrefix, key)];
}

} // namespace WebKit
