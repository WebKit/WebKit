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

#pragma once

#include "APIExperimentalFeature.h"
#include "APIInternalDebugFeature.h"
#include "APIObject.h"
#include "WebPreferencesDefinitions.h"
#include "WebPreferencesStore.h"
#include <WebCore/LibWebRTCProvider.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakHashSet.h>

#define DECLARE_PREFERENCE_GETTER_AND_SETTERS(KeyUpper, KeyLower, TypeName, Type, DefaultValue, HumanReadableName, HumanReadableDescription) \
    void set##KeyUpper(const Type& value); \
    void delete##KeyUpper(); \
    Type KeyLower() const;

namespace WebKit {

class WebPageProxy;

class WebPreferences : public API::ObjectImpl<API::Object::Type::Preferences> {
public:
    static Ref<WebPreferences> create(const String& identifier, const String& keyPrefix, const String& globalDebugKeyPrefix);
    static Ref<WebPreferences> createWithLegacyDefaults(const String& identifier, const String& keyPrefix, const String& globalDebugKeyPrefix);

    explicit WebPreferences(const String& identifier, const String& keyPrefix, const String& globalDebugKeyPrefix);
    WebPreferences(const WebPreferences&);

    virtual ~WebPreferences();

    Ref<WebPreferences> copy() const;

    void addPage(WebPageProxy&);
    void removePage(WebPageProxy&);

    const WebPreferencesStore& store() const { return m_store; }

    // Implemented in generated file WebPreferencesGetterSetters.cpp.
    FOR_EACH_WEBKIT_PREFERENCE(DECLARE_PREFERENCE_GETTER_AND_SETTERS)

    static const Vector<RefPtr<API::Object>>& experimentalFeatures();
    bool isFeatureEnabled(const API::ExperimentalFeature&) const;
    void setFeatureEnabled(const API::ExperimentalFeature&, bool);
    void setExperimentalFeatureEnabledForKey(const String&, bool);
    void enableAllExperimentalFeatures();

    static const Vector<RefPtr<API::Object>>& internalDebugFeatures();
    bool isFeatureEnabled(const API::InternalDebugFeature&) const;
    void setFeatureEnabled(const API::InternalDebugFeature&, bool);
    void setInternalDebugFeatureEnabledForKey(const String&, bool);
    void resetAllInternalDebugFeatures();

    // Exposed for WebKitTestRunner use only.
    void setBoolValueForKey(const String&, bool value, bool ephemeral);
    void setDoubleValueForKey(const String&, double value, bool ephemeral);
    void setUInt32ValueForKey(const String&, uint32_t value, bool ephemeral);
    void setStringValueForKey(const String&, const String& value, bool ephemeral);
    void forceUpdate() { update(); }

    void startBatchingUpdates();
    void endBatchingUpdates();

private:
    void platformInitializeStore();

    void update();

    class UpdateBatch {
    public:
        explicit UpdateBatch(WebPreferences& preferences)
            : m_preferences(preferences)
        {
            m_preferences.startBatchingUpdates();
        }
        
        ~UpdateBatch()
        {
            m_preferences.endBatchingUpdates();
        }
        
    private:
        WebPreferences& m_preferences;
    };

    void updateStringValueForKey(const String& key, const String& value, bool ephemeral);
    void updateBoolValueForKey(const String& key, bool value, bool ephemeral);
    void updateUInt32ValueForKey(const String& key, uint32_t value, bool ephemeral);
    void updateDoubleValueForKey(const String& key, double value, bool ephemeral);
    void updateFloatValueForKey(const String& key, float value, bool ephemeral);
    void platformUpdateStringValueForKey(const String& key, const String& value);
    void platformUpdateBoolValueForKey(const String& key, bool value);
    void platformUpdateUInt32ValueForKey(const String& key, uint32_t value);
    void platformUpdateDoubleValueForKey(const String& key, double value);
    void platformUpdateFloatValueForKey(const String& key, float value);

    void deleteKey(const String& key);
    void platformDeleteKey(const String& key);

    void registerDefaultBoolValueForKey(const String&, bool);
    void registerDefaultUInt32ValueForKey(const String&, uint32_t);

    bool platformGetStringUserValueForKey(const String& key, String& userValue);
    bool platformGetBoolUserValueForKey(const String&, bool&);
    bool platformGetUInt32UserValueForKey(const String&, uint32_t&);
    bool platformGetDoubleUserValueForKey(const String&, double&);

    const String m_identifier;
    const String m_keyPrefix;
    const String m_globalDebugKeyPrefix;
    WebPreferencesStore m_store;

    WeakHashSet<WebPageProxy> m_pages;
    unsigned m_updateBatchCount { 0 };
    bool m_needUpdateAfterBatch { false };
};

} // namespace WebKit
