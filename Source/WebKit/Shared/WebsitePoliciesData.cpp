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
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>

namespace WebKit {

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

    // Only disable content blockers if it hasn't already been disabled by reloading without content blockers.
    auto& [defaultEnablement, exceptions] = documentLoader.contentExtensionEnablement();
    if (defaultEnablement == WebCore::ContentExtensionDefaultEnablement::Enabled && exceptions.isEmpty())
        documentLoader.setContentExtensionEnablement(WTFMove(websitePolicies.contentExtensionEnablement));

    documentLoader.setActiveContentRuleListActionPatterns(websitePolicies.activeContentRuleListActionPatterns);
    documentLoader.setVisibilityAdjustmentSelectors(WTFMove(websitePolicies.visibilityAdjustmentSelectors));

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
    documentLoader.setAdvancedPrivacyProtections(websitePolicies.advancedPrivacyProtections);
    documentLoader.setOriginatorAdvancedPrivacyProtections(websitePolicies.advancedPrivacyProtections);
    documentLoader.setIdempotentModeAutosizingOnlyHonorsPercentages(websitePolicies.idempotentModeAutosizingOnlyHonorsPercentages);

    if (!documentLoader.frame())
        return;

    documentLoader.applyPoliciesToSettings();
}

}

