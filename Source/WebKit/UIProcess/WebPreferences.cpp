/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "APIPageConfiguration.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferencesKeys.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/LibWebRTCProvider.h>
#include <WebCore/StorageBlockingPolicy.h>
#include <wtf/Borrow.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadingPrimitives.h>

#if !PLATFORM(COCOA)
#include <WebCore/NotImplemented.h>
#endif

namespace WebKit {

static bool forceSiteIsolationAlwaysOnForTesting;

bool WebPreferences::forcedSiteIsolationAlwaysOnForTesting()
{
    return WebKit::forceSiteIsolationAlwaysOnForTesting;
}

Ref<WebPreferences> WebPreferences::create(StringImpl* identifier, StringImpl* keyPrefix, StringImpl* globalDebugKeyPrefix)
{
    return adoptRef(*new WebPreferences(identifier, keyPrefix, globalDebugKeyPrefix));
}

Ref<WebPreferences> WebPreferences::createWithLegacyDefaults(StringImpl* identifier, StringImpl* keyPrefix, StringImpl* globalDebugKeyPrefix)
{
    auto preferences = WebPreferences::create(identifier, keyPrefix, globalDebugKeyPrefix);
    // FIXME: The registerDefault...ValueForKey machinery is unnecessarily heavyweight and complicated.
    // We can just compute different defaults for modern and legacy APIs in WebPreferencesDefinitions.h macros.
    preferences->registerDefaultUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey(), static_cast<uint32_t>(WebCore::StorageBlockingPolicy::AllowAll));
    return preferences;
}

WebPreferences::WebPreferences(StringImpl* identifier, StringImpl* keyPrefix, StringImpl* globalDebugKeyPrefix)
    : m_identifier(identifier)
    , m_keyPrefix(keyPrefix)
    , m_globalDebugKeyPrefix(globalDebugKeyPrefix)
{
    platformInitializeStore();

    if (WebKit::forceSiteIsolationAlwaysOnForTesting)
        setSiteIsolationEnabled(true);
}

WebPreferences::WebPreferences(const WebPreferences& other)
    : m_identifier()
    , m_keyPrefix(other.m_keyPrefix)
    , m_globalDebugKeyPrefix(other.m_globalDebugKeyPrefix)
    , m_store(other.m_store)
{
    platformInitializeStore();

    if (WebKit::forceSiteIsolationAlwaysOnForTesting)
        setSiteIsolationEnabled(true);
}

WebPreferences::~WebPreferences()
{
    ASSERT(m_pages.isEmptyIgnoringNullReferences());
}

void WebPreferences::forceSiteIsolationAlwaysOnForTesting()
{
    WebKit::forceSiteIsolationAlwaysOnForTesting = true;
}

bool WebPreferences::shouldUseUIProcessForBackForwardItemLoading() const
{
    // Site Isolation spreads a page's frames across Web processes, so no single Web process can
    // coordinate a back/forward traversal by itself; the UI process must always drive it.
    return useUIProcessForBackForwardItemLoading() || siteIsolationEnabled();
}

const Vector<RefPtr<API::Object>>& WebPreferences::experimentalFeatures()
{
    static auto experimentalFeatures = NeverDestroyed([]() {
        Vector<RefPtr<API::Object>> result;
        for (auto& object : borrow(features())) {
            API::FeatureStatus status = downcast<API::Feature>(object)->status();
            if (status == API::FeatureStatus::Developer || status == API::FeatureStatus::Testable || status == API::FeatureStatus::Preview || status == API::FeatureStatus::Stable)
                result.append(object);
        }
        return result;
    }());
    return experimentalFeatures;
}

const Vector<RefPtr<API::Object>>& WebPreferences::internalDebugFeatures()
{
    static auto internalDebugFeatures = NeverDestroyed([]() {
        Vector<RefPtr<API::Object>> result;
        for (auto& object : borrow(features())) {
            API::FeatureStatus status = downcast<API::Feature>(object)->status();
            if (status == API::FeatureStatus::Unstable || status == API::FeatureStatus::Internal)
                result.append(object);
        }
        return result;
    }());
    return internalDebugFeatures;
}

Ref<WebPreferences> WebPreferences::copy() const
{
    return adoptRef(*new WebPreferences(*this));
}

void WebPreferences::addPage(WebPageProxy& webPageProxy)
{
    ASSERT(!m_pages.contains(webPageProxy));
    m_pages.add(webPageProxy);
}

void WebPreferences::removePage(WebPageProxy& webPageProxy)
{
    ASSERT(m_pages.contains(webPageProxy));
    m_pages.remove(webPageProxy);
}

void WebPreferences::update()
{
    if (m_updateBatchCount) {
        m_needUpdateAfterBatch = true;
        return;
    }

    for (Ref page : m_pages)
        page->preferencesDidChange();
}

void WebPreferences::startBatchingUpdates()
{
    if (!m_updateBatchCount)
        m_needUpdateAfterBatch = false;

    ++m_updateBatchCount;
}

void WebPreferences::endBatchingUpdates()
{
    ASSERT(m_updateBatchCount > 0);
    --m_updateBatchCount;
    if (!m_updateBatchCount && m_needUpdateAfterBatch)
        update();
}

void WebPreferences::setBoolValueForKey(StringImpl* key, bool value, bool ephemeral)
{
    if (!m_store.setBoolValueForKey(key, value))
        return;
    updateBoolValueForKey(key, value, ephemeral);
}

void WebPreferences::setDoubleValueForKey(StringImpl* key, double value, bool ephemeral)
{
    if (!m_store.setDoubleValueForKey(key, value))
        return;
    updateDoubleValueForKey(key, value, ephemeral);
}

void WebPreferences::setUInt32ValueForKey(StringImpl* key, uint32_t value, bool ephemeral)
{
    if (!m_store.setUInt32ValueForKey(key, value))
        return;
    updateUInt32ValueForKey(key, value, ephemeral);
}

void WebPreferences::setStringValueForKey(StringImpl* key, StringImpl* value, bool ephemeral)
{
    if (!m_store.setStringValueForKey(key, value))
        return;
    updateStringValueForKey(key, value, ephemeral);
}

void WebPreferences::updateStringValueForKey(StringImpl* key, StringImpl* value, bool ephemeral)
{
    platformUpdateStringValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateBoolValueForKey(StringImpl* key, bool value, bool ephemeral)
{
    if (!ephemeral)
        platformUpdateBoolValueForKey(key, value);
    
    if (equal(key, WebPreferencesKey::processSwapOnCrossSiteNavigationEnabledKey())) {
        for (Ref page : m_pages)
            page->configuration().processPool().configuration().setProcessSwapsOnNavigation(value);

        return;
    }

    if (equal(key, WebPreferencesKey::exemptAllowlistedCookiesFromItpKey())) {
        WebsiteDataStore::forEachWebsiteDataStore([value](WebsiteDataStore& dataStore) {
            dataStore.setExemptAllowlistedCookiesFromITP(value);
        });
    }

    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateUInt32ValueForKey(StringImpl* key, uint32_t value, bool ephemeral)
{
    platformUpdateUInt32ValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateDoubleValueForKey(StringImpl* key, double value, bool ephemeral)
{
    platformUpdateDoubleValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateFloatValueForKey(StringImpl* key, float value, bool ephemeral)
{
    platformUpdateFloatValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::deleteKey(StringImpl* key)
{
    m_store.deleteKey(key);
    platformDeleteKey(key);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::registerDefaultBoolValueForKey(StringImpl* key, bool value)
{
    m_store.setOverrideDefaultsBoolValueForKey(key, value);
    bool userValue;
    if (platformGetBoolUserValueForKey(key, userValue))
        m_store.setBoolValueForKey(key, userValue);
}

void WebPreferences::registerDefaultUInt32ValueForKey(StringImpl* key, uint32_t value)
{
    m_store.setOverrideDefaultsUInt32ValueForKey(key, value);
    uint32_t userValue;
    if (platformGetUInt32UserValueForKey(key, userValue))
        m_store.setUInt32ValueForKey(key, userValue);
}

#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WPE) && !PLATFORM(PLAYSTATION)
void WebPreferences::platformInitializeStore()
{
    notImplemented();
}
#endif

#if !PLATFORM(COCOA)
void WebPreferences::platformUpdateStringValueForKey(StringImpl*, StringImpl*)
{
    notImplemented();
}

void WebPreferences::platformUpdateBoolValueForKey(StringImpl*, bool)
{
    notImplemented();
}

void WebPreferences::platformUpdateUInt32ValueForKey(StringImpl*, uint32_t)
{
    notImplemented();
}

void WebPreferences::platformUpdateDoubleValueForKey(StringImpl*, double)
{
    notImplemented();
}

void WebPreferences::platformUpdateFloatValueForKey(StringImpl*, float)
{
    notImplemented();
}

void WebPreferences::platformDeleteKey(StringImpl*)
{
    notImplemented();
}

bool WebPreferences::platformGetStringUserValueForKey(StringImpl*, RefPtr<StringImpl>&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetBoolUserValueForKey(StringImpl*, bool&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetUInt32UserValueForKey(StringImpl*, uint32_t&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetDoubleUserValueForKey(StringImpl*, double&)
{
    notImplemented();
    return false;
}
#endif

} // namespace WebKit
