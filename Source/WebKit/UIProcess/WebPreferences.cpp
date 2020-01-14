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
#include "WebPreferences.h"

#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferencesKeys.h"
#include "WebProcessPool.h"
#include <WebCore/LibWebRTCProvider.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadingPrimitives.h>

#if !PLATFORM(COCOA)
#include <WebCore/NotImplemented.h>
#endif

namespace WebKit {

Ref<WebPreferences> WebPreferences::create(const String& identifier, const String& keyPrefix, const String& globalDebugKeyPrefix)
{
    return adoptRef(*new WebPreferences(identifier, keyPrefix, globalDebugKeyPrefix));
}

Ref<WebPreferences> WebPreferences::createWithLegacyDefaults(const String& identifier, const String& keyPrefix, const String& globalDebugKeyPrefix)
{
    auto preferences = WebPreferences::create(identifier, keyPrefix, globalDebugKeyPrefix);
    // FIXME: The registerDefault...ValueForKey machinery is unnecessarily heavyweight and complicated.
    // We can just compute different defaults for modern and legacy APIs in WebPreferencesDefinitions.h macros.
    preferences->registerDefaultBoolValueForKey(WebPreferencesKey::javaEnabledKey(), true);
    preferences->registerDefaultBoolValueForKey(WebPreferencesKey::javaEnabledForLocalFilesKey(), true);
    preferences->registerDefaultBoolValueForKey(WebPreferencesKey::pluginsEnabledKey(), true);
    preferences->registerDefaultUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey(), WebCore::SecurityOrigin::AllowAllStorage);
    return preferences;
}

WebPreferences::WebPreferences(const String& identifier, const String& keyPrefix, const String& globalDebugKeyPrefix)
    : m_identifier(identifier)
    , m_keyPrefix(keyPrefix)
    , m_globalDebugKeyPrefix(globalDebugKeyPrefix)
{
    platformInitializeStore();
}

WebPreferences::WebPreferences(const WebPreferences& other)
    : m_identifier()
    , m_keyPrefix(other.m_keyPrefix)
    , m_globalDebugKeyPrefix(other.m_globalDebugKeyPrefix)
    , m_store(other.m_store)
{
    platformInitializeStore();
}

WebPreferences::~WebPreferences()
{
    ASSERT(m_pages.isEmpty());
}

Ref<WebPreferences> WebPreferences::copy() const
{
    return adoptRef(*new WebPreferences(*this));
}

void WebPreferences::addPage(WebPageProxy& webPageProxy)
{
    ASSERT(!m_pages.contains(&webPageProxy));
    m_pages.add(&webPageProxy);
}

void WebPreferences::removePage(WebPageProxy& webPageProxy)
{
    ASSERT(m_pages.contains(&webPageProxy));
    m_pages.remove(&webPageProxy);
}

void WebPreferences::update()
{
    for (auto& webPageProxy : m_pages)
        webPageProxy->preferencesDidChange();
}

void WebPreferences::updateStringValueForKey(const String& key, const String& value)
{
    platformUpdateStringValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateBoolValueForKey(const String& key, bool value)
{
    platformUpdateBoolValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateBoolValueForInternalDebugFeatureKey(const String& key, bool value)
{
    if (key == WebPreferencesKey::processSwapOnCrossSiteNavigationEnabledKey()) {
        for (auto* page : m_pages)
            page->process().processPool().configuration().setProcessSwapsOnNavigation(value);

        return;
    }
    if (key == WebPreferencesKey::captureAudioInUIProcessEnabledKey()) {
        for (auto* processPool : WebProcessPool::allProcessPools())
            processPool->configuration().setShouldCaptureAudioInUIProcess(value);

        return;
    }
    if (key == WebPreferencesKey::captureAudioInGPUProcessEnabledKey()) {
        for (auto* processPool : WebProcessPool::allProcessPools())
            processPool->configuration().setShouldCaptureAudioInGPUProcess(value);

        return;
    }
    if (key == WebPreferencesKey::captureVideoInGPUProcessEnabledKey()) {
        for (auto* processPool : WebProcessPool::allProcessPools())
            processPool->configuration().setShouldCaptureVideoInGPUProcess(value);

        return;
    }

    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateBoolValueForExperimentalFeatureKey(const String& key, bool value)
{
    if (key == WebPreferencesKey::captureVideoInUIProcessEnabledKey()) {
        for (auto* processPool : WebProcessPool::allProcessPools())
            processPool->configuration().setShouldCaptureVideoInUIProcess(value);

        return;
    }
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateUInt32ValueForKey(const String& key, uint32_t value)
{
    platformUpdateUInt32ValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateDoubleValueForKey(const String& key, double value)
{
    platformUpdateDoubleValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateFloatValueForKey(const String& key, float value)
{
    platformUpdateFloatValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::deleteKey(const String& key)
{
    m_store.deleteKey(key);
    platformDeleteKey(key);
    update(); // FIXME: Only send over the changed key and value.
}

#define DEFINE_PREFERENCE_GETTER_AND_SETTERS(KeyUpper, KeyLower, TypeName, Type, DefaultValue, HumanReadableName, HumanReadableDescription) \
    void WebPreferences::set##KeyUpper(const Type& value) \
    { \
        if (!m_store.set##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key(), value)) \
            return; \
        update##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key(), value); \
    } \
    \
    void WebPreferences::delete##KeyUpper() \
    { \
        deleteKey(WebPreferencesKey::KeyLower##Key()); \
    } \
    \
    Type WebPreferences::KeyLower() const \
    { \
        return m_store.get##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key()); \
    } \

FOR_EACH_WEBKIT_PREFERENCE(DEFINE_PREFERENCE_GETTER_AND_SETTERS)
FOR_EACH_WEBKIT_DEBUG_PREFERENCE(DEFINE_PREFERENCE_GETTER_AND_SETTERS)

#undef DEFINE_PREFERENCE_GETTER_AND_SETTERS


void WebPreferences::registerDefaultBoolValueForKey(const String& key, bool value)
{
    m_store.setOverrideDefaultsBoolValueForKey(key, value);
    bool userValue;
    if (platformGetBoolUserValueForKey(key, userValue))
        m_store.setBoolValueForKey(key, userValue);
}

void WebPreferences::registerDefaultUInt32ValueForKey(const String& key, uint32_t value)
{
    m_store.setOverrideDefaultsUInt32ValueForKey(key, value);
    uint32_t userValue;
    if (platformGetUInt32UserValueForKey(key, userValue))
        m_store.setUInt32ValueForKey(key, userValue);
}

#if !PLATFORM(COCOA) && !PLATFORM(GTK)
void WebPreferences::platformInitializeStore()
{
    notImplemented();
}
#endif

#if !PLATFORM(COCOA)
void WebPreferences::platformUpdateStringValueForKey(const String&, const String&)
{
    notImplemented();
}

void WebPreferences::platformUpdateBoolValueForKey(const String&, bool)
{
    notImplemented();
}

void WebPreferences::platformUpdateUInt32ValueForKey(const String&, uint32_t)
{
    notImplemented();
}

void WebPreferences::platformUpdateDoubleValueForKey(const String&, double)
{
    notImplemented();
}

void WebPreferences::platformUpdateFloatValueForKey(const String&, float)
{
    notImplemented();
}

void WebPreferences::platformDeleteKey(const String&)
{
    notImplemented();
}

bool WebPreferences::platformGetStringUserValueForKey(const String&, String&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetBoolUserValueForKey(const String&, bool&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetUInt32UserValueForKey(const String&, uint32_t&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetDoubleUserValueForKey(const String&, double&)
{
    notImplemented();
    return false;
}
#endif

} // namespace WebKit
