/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "WebsitePoliciesData.h"
#include <wtf/WeakPtr.h>

namespace WebKit {
class LockdownModeObserver;
class WebUserContentControllerProxy;
class WebsiteDataStore;
struct WebsitePoliciesData;
}

namespace API {

class WebsitePolicies final : public API::ObjectImpl<API::Object::Type::WebsitePolicies>, public CanMakeWeakPtr<WebsitePolicies> {
public:
    static Ref<WebsitePolicies> create() { return adoptRef(*new WebsitePolicies); }
    WebsitePolicies();
    ~WebsitePolicies();

    Ref<WebsitePolicies> copy() const;

    WebKit::WebsitePoliciesData data();

    const WebCore::ContentExtensionEnablement& contentExtensionEnablement() const { return m_data.contentExtensionEnablement; }
    void setContentExtensionEnablement(WebCore::ContentExtensionEnablement&& enablement) { m_data.contentExtensionEnablement = WTFMove(enablement); }

    void setActiveContentRuleListActionPatterns(HashMap<WTF::String, Vector<WTF::String>>&& patterns) { m_data.activeContentRuleListActionPatterns = WTFMove(patterns); }
    const HashMap<WTF::String, Vector<WTF::String>>& activeContentRuleListActionPatterns() const { return m_data.activeContentRuleListActionPatterns; }
    
    OptionSet<WebKit::WebsiteAutoplayQuirk> allowedAutoplayQuirks() const { return m_data.allowedAutoplayQuirks; }
    void setAllowedAutoplayQuirks(OptionSet<WebKit::WebsiteAutoplayQuirk> quirks) { m_data.allowedAutoplayQuirks = quirks; }
    
    WebKit::WebsiteAutoplayPolicy autoplayPolicy() const { return m_data.autoplayPolicy; }
    void setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy policy) { m_data.autoplayPolicy = policy; }

#if ENABLE(DEVICE_ORIENTATION)
    WebCore::DeviceOrientationOrMotionPermissionState deviceOrientationAndMotionAccessState() const { return m_data.deviceOrientationAndMotionAccessState; }
    void setDeviceOrientationAndMotionAccessState(WebCore::DeviceOrientationOrMotionPermissionState state) { m_data.deviceOrientationAndMotionAccessState = state; }
#endif

    const Vector<WebCore::CustomHeaderFields>& customHeaderFields() const { return m_data.customHeaderFields; }
    void setCustomHeaderFields(Vector<WebCore::CustomHeaderFields>&& fields) { m_data.customHeaderFields = WTFMove(fields); }

    WebKit::WebsitePopUpPolicy popUpPolicy() const { return m_data.popUpPolicy; }
    void setPopUpPolicy(WebKit::WebsitePopUpPolicy policy) { m_data.popUpPolicy = policy; }

    WebKit::WebsiteDataStore* websiteDataStore() const { return m_websiteDataStore.get(); }
    void setWebsiteDataStore(RefPtr<WebKit::WebsiteDataStore>&&);
    
    WebKit::WebUserContentControllerProxy* userContentController() const { return m_userContentController.get(); }
    void setUserContentController(RefPtr<WebKit::WebUserContentControllerProxy>&&);

    void setCustomUserAgent(const WTF::String& customUserAgent) { m_data.customUserAgent = customUserAgent; }
    const WTF::String& customUserAgent() const { return m_data.customUserAgent; }

    void setCustomUserAgentAsSiteSpecificQuirks(const WTF::String& customUserAgent) { m_data.customUserAgentAsSiteSpecificQuirks = customUserAgent; }
    const WTF::String& customUserAgentAsSiteSpecificQuirks() const { return m_data.customUserAgentAsSiteSpecificQuirks; }

    void setCustomNavigatorPlatform(const WTF::String& customNavigatorPlatform) { m_data.customNavigatorPlatform = customNavigatorPlatform; }
    const WTF::String& customNavigatorPlatform() const { return m_data.customNavigatorPlatform; }

    WebKit::WebContentMode preferredContentMode() const { return m_data.preferredContentMode; }
    void setPreferredContentMode(WebKit::WebContentMode mode) { m_data.preferredContentMode = mode; }

    WebKit::WebsiteMetaViewportPolicy metaViewportPolicy() const { return m_data.metaViewportPolicy; }
    void setMetaViewportPolicy(WebKit::WebsiteMetaViewportPolicy policy) { m_data.metaViewportPolicy = policy; }

    WebKit::WebsiteMediaSourcePolicy mediaSourcePolicy() const { return m_data.mediaSourcePolicy; }
    void setMediaSourcePolicy(WebKit::WebsiteMediaSourcePolicy policy) { m_data.mediaSourcePolicy = policy; }

    WebKit::WebsiteSimulatedMouseEventsDispatchPolicy simulatedMouseEventsDispatchPolicy() const { return m_data.simulatedMouseEventsDispatchPolicy; }
    void setSimulatedMouseEventsDispatchPolicy(WebKit::WebsiteSimulatedMouseEventsDispatchPolicy policy) { m_data.simulatedMouseEventsDispatchPolicy = policy; }

    WebKit::WebsiteLegacyOverflowScrollingTouchPolicy legacyOverflowScrollingTouchPolicy() const { return m_data.legacyOverflowScrollingTouchPolicy; }
    void setLegacyOverflowScrollingTouchPolicy(WebKit::WebsiteLegacyOverflowScrollingTouchPolicy policy) { m_data.legacyOverflowScrollingTouchPolicy = policy; }

    bool allowSiteSpecificQuirksToOverrideContentMode() const { return m_data.allowSiteSpecificQuirksToOverrideContentMode; }
    void setAllowSiteSpecificQuirksToOverrideContentMode(bool value) { m_data.allowSiteSpecificQuirksToOverrideContentMode = value; }

    WTF::String applicationNameForDesktopUserAgent() const { return m_data.applicationNameForDesktopUserAgent; }
    void setApplicationNameForDesktopUserAgent(const WTF::String& applicationName) { m_data.applicationNameForDesktopUserAgent = applicationName; }

    WebCore::AllowsContentJavaScript allowsContentJavaScript() const { return m_data.allowsContentJavaScript; }
    void setAllowsContentJavaScript(WebCore::AllowsContentJavaScript allows) { m_data.allowsContentJavaScript = allows; }

    bool lockdownModeEnabled() const;
    void setLockdownModeEnabled(std::optional<bool> enabled) { m_lockdownModeEnabled = enabled; }
    bool isLockdownModeExplicitlySet() const { return !!m_lockdownModeEnabled; }

    WebCore::ColorSchemePreference colorSchemePreference() const { return m_data.colorSchemePreference; }
    void setColorSchemePreference(WebCore::ColorSchemePreference colorSchemePreference) { m_data.colorSchemePreference = colorSchemePreference; }

    WebCore::MouseEventPolicy mouseEventPolicy() const { return m_data.mouseEventPolicy; }
    void setMouseEventPolicy(WebCore::MouseEventPolicy policy) { m_data.mouseEventPolicy = policy; }

    WebCore::ModalContainerObservationPolicy modalContainerObservationPolicy() const { return m_data.modalContainerObservationPolicy; }
    void setModalContainerObservationPolicy(WebCore::ModalContainerObservationPolicy policy) { m_data.modalContainerObservationPolicy = policy; }

    OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections() const { return m_data.advancedPrivacyProtections; }
    void setAdvancedPrivacyProtections(OptionSet<WebCore::AdvancedPrivacyProtections> policy) { m_data.advancedPrivacyProtections = policy; }

    bool idempotentModeAutosizingOnlyHonorsPercentages() const { return m_data.idempotentModeAutosizingOnlyHonorsPercentages; }
    void setIdempotentModeAutosizingOnlyHonorsPercentages(bool idempotentModeAutosizingOnlyHonorsPercentages) { m_data.idempotentModeAutosizingOnlyHonorsPercentages = idempotentModeAutosizingOnlyHonorsPercentages; }

    bool allowPrivacyProxy() const { return m_data.allowPrivacyProxy; }
    void setAllowPrivacyProxy(bool allow) { m_data.allowPrivacyProxy = allow; }

    WebCore::HTTPSByDefaultMode httpsByDefaultMode() const { return m_data.httpsByDefaultMode; }
    void setHTTPSByDefault(WebCore::HTTPSByDefaultMode mode) { m_data.httpsByDefaultMode = mode; }
    bool isUpgradeWithUserMediatedFallbackEnabled() { return advancedPrivacyProtections().contains(WebCore::AdvancedPrivacyProtections::HTTPSOnly) || httpsByDefaultMode() == WebCore::HTTPSByDefaultMode::UpgradeWithUserMediatedFallback; }
    bool isUpgradeWithAutomaticFallbackEnabled() { return advancedPrivacyProtections().contains(WebCore::AdvancedPrivacyProtections::HTTPSFirst) || httpsByDefaultMode() == WebCore::HTTPSByDefaultMode::UpgradeWithAutomaticFallback; }

    const Vector<Vector<HashSet<WTF::String>>>& visibilityAdjustmentSelectors() const { return m_data.visibilityAdjustmentSelectors; }
    void setVisibilityAdjustmentSelectors(Vector<Vector<HashSet<WTF::String>>>&& selectors) { m_data.visibilityAdjustmentSelectors = WTFMove(selectors); }

    WebKit::WebsitePushAndNotificationsEnabledPolicy pushAndNotificationsEnabledPolicy() const { return m_data.pushAndNotificationsEnabledPolicy; }
    void setPushAndNotificationsEnabledPolicy(WebKit::WebsitePushAndNotificationsEnabledPolicy policy) { m_data.pushAndNotificationsEnabledPolicy = policy; }

#if ENABLE(TOUCH_EVENTS)
    void setOverrideTouchEventDOMAttributesEnabled(bool value) { m_data.overrideTouchEventDOMAttributesEnabled = value; }
#endif

private:
    WebKit::WebsitePoliciesData m_data;
    RefPtr<WebKit::WebsiteDataStore> m_websiteDataStore;
    RefPtr<WebKit::WebUserContentControllerProxy> m_userContentController;
    std::optional<bool> m_lockdownModeEnabled;
#if PLATFORM(COCOA)
    std::unique_ptr<WebKit::LockdownModeObserver> m_lockdownModeObserver;
#endif
};

} // namespace API
