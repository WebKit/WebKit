/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebsitePoliciesData.h"

#include "ArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/Page.h>

namespace WebKit {

void WebsitePoliciesData::encode(IPC::Encoder& encoder) const
{
    encoder << contentBlockersEnabled;
    encoder << activeContentRuleListActionPatterns;
    encoder << autoplayPolicy;
#if ENABLE(DEVICE_ORIENTATION)
    encoder << deviceOrientationAndMotionAccessState;
#endif
    encoder << allowedAutoplayQuirks;
    encoder << customHeaderFields;
    encoder << popUpPolicy;
    encoder << customUserAgent;
    encoder << customUserAgentAsSiteSpecificQuirks;
    encoder << customNavigatorPlatform;
    encoder << metaViewportPolicy;
    encoder << mediaSourcePolicy;
    encoder << simulatedMouseEventsDispatchPolicy;
    encoder << legacyOverflowScrollingTouchPolicy;
    encoder << allowContentChangeObserverQuirk;
    encoder << allowsContentJavaScript;
    encoder << mouseEventPolicy;
    encoder << modalContainerObservationPolicy;
    encoder << colorSchemePreference;
    encoder << networkConnectionIntegrityEnabled;
    encoder << idempotentModeAutosizingOnlyHonorsPercentages;
    encoder << allowPrivacyProxy;
}

std::optional<WebsitePoliciesData> WebsitePoliciesData::decode(IPC::Decoder& decoder)
{
    std::optional<bool> contentBlockersEnabled;
    decoder >> contentBlockersEnabled;
    if (!contentBlockersEnabled)
        return std::nullopt;

    std::optional<HashMap<WTF::String, Vector<WTF::String>>> activeContentRuleListActionPatterns;
    decoder >> activeContentRuleListActionPatterns;
    if (!activeContentRuleListActionPatterns)
        return std::nullopt;

    std::optional<WebsiteAutoplayPolicy> autoplayPolicy;
    decoder >> autoplayPolicy;
    if (!autoplayPolicy)
        return std::nullopt;

#if ENABLE(DEVICE_ORIENTATION)
    std::optional<WebCore::DeviceOrientationOrMotionPermissionState> deviceOrientationAndMotionAccessState;
    decoder >> deviceOrientationAndMotionAccessState;
    if (!deviceOrientationAndMotionAccessState)
        return std::nullopt;
#endif
    
    std::optional<OptionSet<WebsiteAutoplayQuirk>> allowedAutoplayQuirks;
    decoder >> allowedAutoplayQuirks;
    if (!allowedAutoplayQuirks)
        return std::nullopt;
    
    std::optional<Vector<WebCore::CustomHeaderFields>> customHeaderFields;
    decoder >> customHeaderFields;
    if (!customHeaderFields)
        return std::nullopt;

    std::optional<WebsitePopUpPolicy> popUpPolicy;
    decoder >> popUpPolicy;
    if (!popUpPolicy)
        return std::nullopt;

    std::optional<String> customUserAgent;
    decoder >> customUserAgent;
    if (!customUserAgent)
        return std::nullopt;

    std::optional<String> customUserAgentAsSiteSpecificQuirks;
    decoder >> customUserAgentAsSiteSpecificQuirks;
    if (!customUserAgentAsSiteSpecificQuirks)
        return std::nullopt;

    std::optional<String> customNavigatorPlatform;
    decoder >> customNavigatorPlatform;
    if (!customNavigatorPlatform)
        return std::nullopt;

    std::optional<WebsiteMetaViewportPolicy> metaViewportPolicy;
    decoder >> metaViewportPolicy;
    if (!metaViewportPolicy)
        return std::nullopt;

    std::optional<WebsiteMediaSourcePolicy> mediaSourcePolicy;
    decoder >> mediaSourcePolicy;
    if (!mediaSourcePolicy)
        return std::nullopt;
    
    std::optional<WebsiteSimulatedMouseEventsDispatchPolicy> simulatedMouseEventsDispatchPolicy;
    decoder >> simulatedMouseEventsDispatchPolicy;
    if (!simulatedMouseEventsDispatchPolicy)
        return std::nullopt;

    std::optional<WebsiteLegacyOverflowScrollingTouchPolicy> legacyOverflowScrollingTouchPolicy;
    decoder >> legacyOverflowScrollingTouchPolicy;
    if (!legacyOverflowScrollingTouchPolicy)
        return std::nullopt;

    std::optional<bool> allowContentChangeObserverQuirk;
    decoder >> allowContentChangeObserverQuirk;
    if (!allowContentChangeObserverQuirk)
        return std::nullopt;

    std::optional<WebCore::AllowsContentJavaScript> allowsContentJavaScript;
    decoder >> allowsContentJavaScript;
    if (!allowsContentJavaScript)
        return std::nullopt;

    std::optional<WebCore::MouseEventPolicy> mouseEventPolicy;
    decoder >> mouseEventPolicy;
    if (!mouseEventPolicy)
        return std::nullopt;

    std::optional<WebCore::ModalContainerObservationPolicy> modalContainerObservationPolicy;
    decoder >> modalContainerObservationPolicy;
    if (!modalContainerObservationPolicy)
        return std::nullopt;

    std::optional<WebCore::ColorSchemePreference> colorSchemePreference;
    decoder >> colorSchemePreference;
    if (!colorSchemePreference)
        return std::nullopt;

    std::optional<bool> networkConnectionIntegrityEnabled;
    decoder >> networkConnectionIntegrityEnabled;
    if (!networkConnectionIntegrityEnabled)
        return std::nullopt;

    std::optional<bool> idempotentModeAutosizingOnlyHonorsPercentages;
    decoder >> idempotentModeAutosizingOnlyHonorsPercentages;
    if (!idempotentModeAutosizingOnlyHonorsPercentages)
        return std::nullopt;

    std::optional<bool> allowPrivacyProxy;
    decoder >> allowPrivacyProxy;
    if (!allowPrivacyProxy)
        return std::nullopt;

    return { {
        WTFMove(*contentBlockersEnabled),
        WTFMove(*activeContentRuleListActionPatterns),
        WTFMove(*allowedAutoplayQuirks),
        WTFMove(*autoplayPolicy),
#if ENABLE(DEVICE_ORIENTATION)
        WTFMove(*deviceOrientationAndMotionAccessState),
#endif
        WTFMove(*customHeaderFields),
        WTFMove(*popUpPolicy),
        WTFMove(*customUserAgent),
        WTFMove(*customUserAgentAsSiteSpecificQuirks),
        WTFMove(*customNavigatorPlatform),
        WTFMove(*metaViewportPolicy),
        WTFMove(*mediaSourcePolicy),
        WTFMove(*simulatedMouseEventsDispatchPolicy),
        WTFMove(*legacyOverflowScrollingTouchPolicy),
        WTFMove(*allowContentChangeObserverQuirk),
        WTFMove(*allowsContentJavaScript),
        WTFMove(*mouseEventPolicy),
        WTFMove(*modalContainerObservationPolicy),
        WTFMove(*colorSchemePreference),
        WTFMove(*networkConnectionIntegrityEnabled),
        WTFMove(*idempotentModeAutosizingOnlyHonorsPercentages),
        WTFMove(*allowPrivacyProxy),
    } };
}

void WebsitePoliciesData::applyToDocumentLoader(WebsitePoliciesData&& websitePolicies, WebCore::DocumentLoader& documentLoader)
{
    documentLoader.setCustomHeaderFields(WTFMove(websitePolicies.customHeaderFields));
    documentLoader.setCustomUserAgent(websitePolicies.customUserAgent);
    documentLoader.setCustomUserAgentAsSiteSpecificQuirks(websitePolicies.customUserAgentAsSiteSpecificQuirks);
    documentLoader.setCustomNavigatorPlatform(websitePolicies.customNavigatorPlatform);
    documentLoader.setAllowPrivacyProxy(websitePolicies.allowPrivacyProxy);

#if ENABLE(DEVICE_ORIENTATION)
    documentLoader.setDeviceOrientationAndMotionAccessState(websitePolicies.deviceOrientationAndMotionAccessState);
#endif

    // Only setUserContentExtensionsEnabled if it hasn't already been disabled by reloading without content blockers.
    if (documentLoader.userContentExtensionsEnabled())
        documentLoader.setUserContentExtensionsEnabled(websitePolicies.contentBlockersEnabled);

    documentLoader.setActiveContentRuleListActionPatterns(websitePolicies.activeContentRuleListActionPatterns);

    OptionSet<WebCore::AutoplayQuirk> quirks;
    const auto& allowedQuirks = websitePolicies.allowedAutoplayQuirks;
    
    if (allowedQuirks.contains(WebsiteAutoplayQuirk::InheritedUserGestures))
        quirks.add(WebCore::AutoplayQuirk::InheritedUserGestures);
    
    if (allowedQuirks.contains(WebsiteAutoplayQuirk::SynthesizedPauseEvents))
        quirks.add(WebCore::AutoplayQuirk::SynthesizedPauseEvents);
    
    if (allowedQuirks.contains(WebsiteAutoplayQuirk::ArbitraryUserGestures))
        quirks.add(WebCore::AutoplayQuirk::ArbitraryUserGestures);

    if (allowedQuirks.contains(WebsiteAutoplayQuirk::PerDocumentAutoplayBehavior))
        quirks.add(WebCore::AutoplayQuirk::PerDocumentAutoplayBehavior);

    documentLoader.setAllowedAutoplayQuirks(quirks);

    switch (websitePolicies.autoplayPolicy) {
    case WebsiteAutoplayPolicy::Default:
        documentLoader.setAutoplayPolicy(WebCore::AutoplayPolicy::Default);
        break;
    case WebsiteAutoplayPolicy::Allow:
        documentLoader.setAutoplayPolicy(WebCore::AutoplayPolicy::Allow);
        break;
    case WebsiteAutoplayPolicy::AllowWithoutSound:
        documentLoader.setAutoplayPolicy(WebCore::AutoplayPolicy::AllowWithoutSound);
        break;
    case WebsiteAutoplayPolicy::Deny:
        documentLoader.setAutoplayPolicy(WebCore::AutoplayPolicy::Deny);
        break;
    }

    switch (websitePolicies.popUpPolicy) {
    case WebsitePopUpPolicy::Default:
        documentLoader.setPopUpPolicy(WebCore::PopUpPolicy::Default);
        break;
    case WebsitePopUpPolicy::Allow:
        documentLoader.setPopUpPolicy(WebCore::PopUpPolicy::Allow);
        break;
    case WebsitePopUpPolicy::Block:
        documentLoader.setPopUpPolicy(WebCore::PopUpPolicy::Block);
        break;
    }

    switch (websitePolicies.metaViewportPolicy) {
    case WebsiteMetaViewportPolicy::Default:
        documentLoader.setMetaViewportPolicy(WebCore::MetaViewportPolicy::Default);
        break;
    case WebsiteMetaViewportPolicy::Respect:
        documentLoader.setMetaViewportPolicy(WebCore::MetaViewportPolicy::Respect);
        break;
    case WebsiteMetaViewportPolicy::Ignore:
        documentLoader.setMetaViewportPolicy(WebCore::MetaViewportPolicy::Ignore);
        break;
    }

    switch (websitePolicies.mediaSourcePolicy) {
    case WebsiteMediaSourcePolicy::Default:
        documentLoader.setMediaSourcePolicy(WebCore::MediaSourcePolicy::Default);
        break;
    case WebsiteMediaSourcePolicy::Disable:
        documentLoader.setMediaSourcePolicy(WebCore::MediaSourcePolicy::Disable);
        break;
    case WebsiteMediaSourcePolicy::Enable:
        documentLoader.setMediaSourcePolicy(WebCore::MediaSourcePolicy::Enable);
        break;
    }

    switch (websitePolicies.simulatedMouseEventsDispatchPolicy) {
    case WebsiteSimulatedMouseEventsDispatchPolicy::Default:
        documentLoader.setSimulatedMouseEventsDispatchPolicy(WebCore::SimulatedMouseEventsDispatchPolicy::Default);
        break;
    case WebsiteSimulatedMouseEventsDispatchPolicy::Allow:
        documentLoader.setSimulatedMouseEventsDispatchPolicy(WebCore::SimulatedMouseEventsDispatchPolicy::Allow);
        break;
    case WebsiteSimulatedMouseEventsDispatchPolicy::Deny:
        documentLoader.setSimulatedMouseEventsDispatchPolicy(WebCore::SimulatedMouseEventsDispatchPolicy::Deny);
        break;
    }

    switch (websitePolicies.legacyOverflowScrollingTouchPolicy) {
    case WebsiteLegacyOverflowScrollingTouchPolicy::Default:
        documentLoader.setLegacyOverflowScrollingTouchPolicy(WebCore::LegacyOverflowScrollingTouchPolicy::Default);
        break;
    case WebsiteLegacyOverflowScrollingTouchPolicy::Disable:
        documentLoader.setLegacyOverflowScrollingTouchPolicy(WebCore::LegacyOverflowScrollingTouchPolicy::Disable);
        break;
    case WebsiteLegacyOverflowScrollingTouchPolicy::Enable:
        documentLoader.setLegacyOverflowScrollingTouchPolicy(WebCore::LegacyOverflowScrollingTouchPolicy::Enable);
        break;
    }

    switch (websitePolicies.mouseEventPolicy) {
    case WebCore::MouseEventPolicy::Default:
        documentLoader.setMouseEventPolicy(WebCore::MouseEventPolicy::Default);
        break;
#if ENABLE(IOS_TOUCH_EVENTS)
    case WebCore::MouseEventPolicy::SynthesizeTouchEvents:
        documentLoader.setMouseEventPolicy(WebCore::MouseEventPolicy::SynthesizeTouchEvents);
        break;
#endif
    }

    documentLoader.setModalContainerObservationPolicy(websitePolicies.modalContainerObservationPolicy);
    documentLoader.setColorSchemePreference(websitePolicies.colorSchemePreference);
    documentLoader.setAllowContentChangeObserverQuirk(websitePolicies.allowContentChangeObserverQuirk);
    documentLoader.setNetworkConnectionIntegrityEnabled(websitePolicies.networkConnectionIntegrityEnabled);
    documentLoader.setIdempotentModeAutosizingOnlyHonorsPercentages(websitePolicies.idempotentModeAutosizingOnlyHonorsPercentages);

    if (!documentLoader.frame())
        return;

    documentLoader.applyPoliciesToSettings();
}

}

