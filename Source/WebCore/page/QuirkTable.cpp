/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "QuirkTable.h"
#include "QuirkBehaviorDefinitions.h"
#include "QuirkSelectors.h"
#include "RuntimeQuirkTable.h"

#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static constexpr std::array bbcPatterns { "*://*.bbc.co.uk/*"_s, "*://*.bbc.com/*"_s };
static constexpr std::array expediaGroupPatterns {
    "*://*.carrentals.com/*"_s, "*://*.cheaptickets.com/*"_s, "*://*.hoteis.com/*"_s, "*://*.hoteles.com/*"_s,
    "*://*.hotels.com/*"_s, "*://*.mrjet.se/*"_s, "*://*.orbitz.com/*"_s, "*://*.travelocity.ca/*"_s,
    "*://*.travelocity.com/*"_s, "*://*.wotif.co.nz/*"_s, "*://*.wotif.com/*"_s
};
static constexpr std::array facebookGroupCallPatterns { "*://*.facebook.com/groupcall/ROOM:*"_s, "*://*.messenger.com/groupcall/ROOM:*"_s };
static constexpr std::array googleMapsPatterns { "*://*.google.*/maps"_s, "*://*.google.*/maps?*"_s, "*://*.google.*/maps/*"_s };
static constexpr std::array googleSearchPatterns { "*://*.google.*/search"_s, "*://*.google.*/search?*"_s };
static constexpr std::array amazonVideoPatterns { "*://*.amazon.*/gp/video/"_s, "*://*.amazon.*/gp/video/?*"_s };
static constexpr std::array shopeeAccountLinkingPatterns { "*://shopee.sg/payment/account-linking/landing"_s, "*://shopee.sg/payment/account-linking/landing?*"_s };
static constexpr std::array appleRetailPatterns { "*://*.apple.*/retail*"_s, "*://*.apple.*/*/retail*"_s };
static constexpr std::array microsoftTeamsPatterns { "*://teams.live.com/*"_s, "*://teams.microsoft.com/*"_s };
static constexpr std::array naverPatternsWithoutSimulatedMouseEvents { "*://tv.naver.com/*"_s, "*://mail.naver.com/*"_s, "*://m.naver.com/*"_s };
static constexpr std::array outlookPatterns { "*://outlook.live.com/*"_s, "*://outlook.office.com/*"_s, "*://outlook.office365.com/*"_s, "*://outlook.cloud.microsoft/*"_s };
static constexpr std::array youTubeEmbedPatterns { "*://*.youtube.com/*"_s, "*://*.youtube-nocookie.com/*"_s };
static constexpr std::array claudePatterns { "*://*.claude.ai/*"_s, "*://*.claude.com/*"_s };
static constexpr std::array kinjaLoginPatterns { "*://*.jalopnik.com/*"_s, "*://*.kotaku.com/*"_s, "*://*.theroot.com/*"_s, "*://*.theinventory.com/*"_s };
static constexpr std::array playStationSignInPatterns { "*://www.playstation.com/*"_s, "*://my.playstation.com/*"_s };
static constexpr std::array claudeLogoutSurvivingCookieNames { "__ssid"_s, "__cf_bm"_s, "anthropic-device-id"_s, "lastActiveOrg"_s, "activitySessionId"_s };

static constexpr auto chromeUserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36"_s;
static constexpr auto chromeUserAgent152 = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/152.0.0.0 Safari/537.36"_s;
static constexpr auto safari13UserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_6) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/13.1.2 Safari/605.1.15"_s;
static constexpr auto safari18_6UserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.6 Safari/605.1.15"_s;

static constexpr auto bestBuyLanguageScript = "Object.defineProperty(navigator,'language',{get:function(){return'en-US'}});Object.defineProperty(navigator,'languages',{get:function(){return['en-US','en']}});"_s;

static constexpr auto chromeUserAgentScript = "(function() { let userAgent = navigator.userAgent; Object.defineProperty(navigator, 'userAgent', { get: () => { return userAgent + ' Chrome/130.0.0.0 Android/15.0'; }, configurable: true }); })();"_s;

static constexpr auto iHeartListenCookieScript = "document.cookie = 'app=listen:60; path=/; domain=.iheart.com';"_s;

static constexpr auto inVideoChromeObjectScript = "if(!window.chrome)window.chrome={};"_s;

static constexpr auto webExUndefinedTouchScript = "Object.defineProperty(window, 'Touch', { get: () => undefined });"_s;

static constexpr auto nbaSeekBarFixScript = R"js(if (!window.__nbaSeekFix) {
    window.__nbaSeekFix = true;
    document.addEventListener('touchmove', function({ target, touches }) {
        if (!target?.getAttribute
            || target.getAttribute('data-id') !== 'video-player:scrub-bar:controls'
            || !touches?.[0])
            return;
        const touch = touches[0];
        const rect = target.getBoundingClientRect();
        const event = new MouseEvent('mousemove', {
            clientX: touch.clientX,
            clientY: touch.clientY,
            screenX: touch.screenX,
            screenY: touch.screenY,
            bubbles: true,
            cancelable: true
        });
        Object.defineProperty(event, 'offsetX', { value: touch.clientX - rect.left, configurable: true });
        Object.defineProperty(event, 'offsetY', { value: touch.clientY - rect.top, configurable: true });
        target.dispatchEvent(event);
    }, false);
})js"_s;

static constexpr auto ceacBeforeUnloadFixScript = R"js((function() {
    if (window.__ceacBeforeUnloadFix) return;
    window.__ceacBeforeUnloadFix = true;
    var origAEL = window.addEventListener;
    window.addEventListener = function(type, fn, opts) {
        if (type === 'beforeunload') {
            return origAEL.call(this, type, function(e) {
                var ae = document.activeElement;
                if (ae && ae.tagName === 'INPUT') {
                    var t = (ae.type || '').toLowerCase();
                    if (t === 'radio' || t === 'checkbox' || t === 'submit' || t === 'button')
                        return;
                }
                if (typeof fn === 'function') fn.call(this, e);
            }, opts);
        }
        return origAEL.apply(this, arguments);
    };
})();)js"_s;

static constexpr auto xGoogleSignInButtonFixScript = R"js((function() {
    if (window.__xGoogleSignInButtonFix)
        return;
    window.__xGoogleSignInButtonFix = true;
    var style = document.createElement('style');
    style.textContent = '.jf-gsi-hit > div > div:first-child:empty:not([role]) { display: none !important }';
    (document.head || document.documentElement).appendChild(style);
})();
)js"_s;

namespace SiteSpecificQuirks {
using namespace QuirkBehaviors;
using namespace QuirkBehaviorConditions;
using namespace QuirkSelectors;
using namespace BuildCondition;

static constexpr std::array anyclipPlayerScriptURL { "*://player.anyclip.com/*lre.js"_s, "*://player.anyclip.com/*lre.js?*"_s };
static constexpr std::array ceacBrowserCloseScriptURL {
    "*://*/CheckBrowserClose.js"_s, "*://*/CheckBrowserClose.js?*"_s,
    "*://*/*/CheckBrowserClose.js"_s, "*://*/*/CheckBrowserClose.js?*"_s
};
static constexpr std::array googleSignInClientScriptURL { "*://accounts.google.com/gsi/client"_s, "*://accounts.google.com/gsi/client?*"_s };
static constexpr std::array webExPushDownloadScriptURL { "*://*/pushdownload.*"_s, "*://*/*/pushdownload.*"_s };
static constexpr std::array wordEditorScriptURL {
    "*://*/wordeditords.js"_s, "*://*/wordeditords.js?*"_s,
    "*://*/*/wordeditords.js"_s, "*://*/*/wordeditords.js?*"_s
};
static constexpr std::array wordEditorFrameURL {
    "*://*/wordeditorframe.aspx"_s, "*://*/wordeditorframe.aspx?*"_s,
    "*://*/*/wordeditorframe.aspx"_s, "*://*/*/wordeditorframe.aspx?*"_s
};
static constexpr std::array claudeLogoutURL { "*://claude.ai/api/auth/logout"_s, "*://claude.ai/api/auth/logout?*"_s };

static constexpr Quirk fullTable[] = {
    // 365scores.com rdar://116491386
    { .matches = "*://*.365scores.com/*"_s,
        .behaviors = { shouldSilenceWindowResizeEventsDuringApplicationSnapshotting },
        .isAvailable = iOS || vision },

    // actesting.org rdar://124017544
    { .matches = "*://*.actesting.org/*"_s,
        .behaviors = { shouldEnableLegacyGetUserMediaQuirk } },

    // airindiaexpress.com https://webkit.org/b/317375
    { .matches = "*://*.airindiaexpress.com/*"_s,
        .behaviors = { needsAirIndiaExpressLayeringQuirk } },

    // airtable.com rdar://49124313
    { .matches = "*://*.airtable.com/*"_s,
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk } },

    { .matches = "*://*.amazon.*/*"_s,
        .behaviors = {
            // amazon.com rdar://49124529
            shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk.when(elementMatchesSelector(onAmazonMagnifierLens)),
            // amazon.com rdar://49124313
            shouldDispatchSimulatedMouseEventsQuirk,
        } },

    // amazon.com rdar://117771731
    { .matches = amazonVideoPatterns,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent)) },
        .isAvailable = iOS },

    { .matches = "*://*.amazon.design/*"_s,
        .behaviors = { needsAmazonDesignMenuViewportUnitQuirk } },

    // apple.com rdar://154434137
    // FIXME: Maybe EnsureCaptionVisibilityInFullscreenAndPictureInPicture should apply to apple.com.cn too?
    { .matches = "*://*.apple.com/*"_s,
        .behaviors = { ensureCaptionVisibilityInFullscreenAndPictureInPicture } },

    // Quirk added for rdar://181007316, remove when rdar://182134549 is fixed.
    { .matches = appleRetailPatterns,
        .behaviors = { shouldDisableScrollAnchoringQuirk } },

    // studio.atomm.com rdar://157636545
    { .matches = "*://studio.atomm.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    // att.com rdar://55185021
    { .matches = "*://*.att.com/*"_s,
        .behaviors = { shouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk },
        .isAvailable = iOSFamily },

    // configurator.audi.com https://webkit.org/b/327442 rdar://181630431
    { .matches = "*://configurator.audi.com/*"_s,
        .behaviors = { shouldRenderModelElementFallbackContentQuirk } },

    // Login issue on bankofamerica.com (rdar://104938789).
    { .matches = "*://*.bankofamerica.com/*"_s,
        .behaviors = {
            shouldBypassBackForwardCacheWhenUnloadListenerAndElementMatchesQuirk.when(documentHasElementMatching(onBankOfAmericaLoadingSignInButton)),
        } },

    // bbc.co.uk rdar://126494734
    // bbc.com rdar://157499149
    { .matches = bbcPatterns,
        .behaviors = { returnNullPictureInPictureElementDuringFullscreenChangeQuirk } },

    // bestbuy.com rdar://136235936
    { .matches = "*://*.bestbuy.com/*"_s,
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(bestBuyLanguageScript)),
        } },

    // bilibili.com rdar://154408203
    { .matches = "*://*.bilibili.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(safari18_6UserAgent)) } },

    // billpaysite.com rdar://141328971
    { .matches = "*://*.billpaysite.com/*"_s,
        .behaviors = { needsPartitionedCookiesQuirk } },

    { .matches = "*://*.bing.com/*"_s,
        .behaviors = {
            // Spinner issue from image search for bing.com rdar://133223599
            shouldBypassBackForwardCacheWhenRenderedElementMatchesQuirk.when(documentHasElementMatching(onBingImageSearchDialog)),
            // bing.com rdar://126573838
            needsMediaRewriteRangeRequestQuirk.when(secondaryURLMatches("*://*.bing.com/*"_s)),
        } },

    // box.com rdar://187475153
    { .matches = "*://*.box.com/*"_s,
        .behaviors = { needsBoxAnnotationQuirk.when(elementMatchesSelector(onBoxRegionAnnotationCreator)) } },

    // bungalow.com rdar://61658940
    { .matches = "*://*.bungalow.com/*"_s,
        .behaviors = { shouldBypassAsyncScriptDeferring } },

    // canva.com https://webkit.org/b/293886
    { .matches = "*://*.canva.com/*"_s,
        .behaviors = { shouldTranscodeHeicImagesQuirk } },

    // capcut.com rdar://177597110
    { .matches = "*://*.capcut.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    { .matches = "*://*.capitalgroup.com/*"_s,
        .behaviors = { shouldDelayReloadWhenRegisteringServiceWorker } },

    // Remove this once rdar://139478801 is resolved.
    { .matches = "*://*.cbssports.com/*"_s,
        .behaviors = {
            shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk.when(elementMatchesSelector(onAviaButton)),
        },
        .isAvailable = iOSFamily },

    { .matches = "*://*.ceac.state.gov/*"_s,
        .behaviors = {
            // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
            needsFormControlToBeMouseFocusableQuirk,
            // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=311383
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(ceacBeforeUnloadFixScript)).when(secondaryURLMatches(ceacBrowserCloseScriptURL)),
        } },

    // secure.chase.com rdar://126715227
    { .matches = "*://secure.chase.com/*"_s,
        .behaviors = { shouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk },
        .isAvailable = touchEvents },

    { .matches = "*://*.chess.com/*"_s, .environment = URLEnvironment::SmallScreen,
        .behaviors = { shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen },
        .isAvailable = iOS },

    { .matches = "*://*.claude.ai/*"_s,
        .behaviors = {
            needsClaudeSidebarViewportUnitQuirk.when(elementMatchesSelector(onClaudeSidebar)),
            // rdar://174779259 - logout flow leaves identification cookies
            // causing redirect loop on next /chat boot.
            needsLogoutCookieCleanupQuirk(QuirkParameters::fromCookieNames(claudeLogoutSurvivingCookieNames)).when(secondaryURLMatches(claudeLogoutURL)),
        } },

    { .matches = claudePatterns,
        .behaviors = { needsHideSelectionDuringOverflowScrollQuirk } },

    { .matches = "*://*.cnn.com/*"_s,
        .behaviors = {
            // cnn.com rdar://119640248
            needsFullscreenObjectFitQuirk,
            needsCNNCaptionQuirk,
            // cnn.com rdar://176539646
            shouldDisableThreadedAnimationsQuirk,
        },
        .isAvailable = iOSFamily },

    { .matches = "*://codepen.io/*"_s,
        .behaviors = { shouldEnableSpeakerSelectionPermissionsPolicyQuirk } },

    // commitmono.com rdar://129051694
    { .matches = "*://*.commitmono.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    { .matches = "*://*.crunchyroll.com/*"_s,
        .behaviors = { needsSuppressPostLayoutBoundaryEventsQuirk } },

    { .matches = "*://*.dailymail.co.uk/*"_s,
        .behaviors = { shouldUnloadHeavyFrames } },

    // mypay.dfas.mil rdar://67081760
    { .matches = "*://mypay.dfas.mil/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    { .matches = "*://digits.t-mobile.com/*"_s,
        .behaviors = {
            needsNavigatorUserAgentDataQuirk,
            needsCustomUserAgentData,
            // digits.t-mobile.com rdar://158721595
            needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)),
        } },

    // descript.com rdar://156024693
    { .matches = "*://*.descript.com/*"_s,
        .behaviors = { shouldDisableDOMAudioSession } },

    { .matches = "*://*.dictionary.com/*"_s,
        .behaviors = { needsAnchorToBeMouseFocusableQuirk } },

    // player.anyclip.com rdar://138789765
    { .matches = "*://*.dictionary.com/*"_s,
        .behaviors = { needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(chromeUserAgentScript)).when(secondaryURLMatches(anyclipPlayerScriptURL)) },
        .isAvailable = iOSFamily },

    // digiposte.fr rdar://177229829
    { .matches = "*://*.digiposte.fr/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    // discord.com rdar://162719481
    { .matches = "*://*.discord.com/*"_s,
        .behaviors = { shouldUseLayoutViewportForClientRectsQuirk } },

    // disneyplus rdar://137613110
    { .matches = "*://*.disneyplus.com/*"_s,
        .behaviors = { shouldHideCoarsePointerCharacteristicsQuirk } },

    // disneyplus rdar://151715964
    { .matches = "*://*.disneyplus.com/*"_s,
        .behaviors = { needsZeroMaxTouchPointsQuirk },
        .isAvailable = iOSFamily && desktopContentModeQuirks },

    { .matches = "*://*.ea.com/*"_s,
        .behaviors = { shouldPreventKeyframeEffectAccelerationQuirk.when(elementMatchesSelector(onEANetworkNav)) } },

    { .matches = "*://*.espn.com/*"_s,
        .behaviors = {
            // espn.com rdar://184169028
            needsSuppressedPauseEventOnFullscreenExitQuirk,
            // espn.com rdar://problem/95651814
            allowLayeredFullscreenVideos,
            // espn.com rdar://problem/73227900
            shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
        } },

    // tool.european-calculator.eu rdar://59424306
    { .matches = "*://tool.european-calculator.eu/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    // Expedia Group rdar://126631968
    { .matches = expediaGroupPatterns,
        .behaviors = { needsExpediaGroupAnimationQuirk.when(elementMatchesSelector(onExpediaOpeningMenu)) } },
    { .matches = "*://*.ebookers.*/*"_s,
        .behaviors = { needsExpediaGroupAnimationQuirk.when(elementMatchesSelector(onExpediaOpeningMenu)) } },
    { .matches = "*://*.expedia.*/*"_s,
        .behaviors = { needsExpediaGroupAnimationQuirk.when(elementMatchesSelector(onExpediaOpeningMenu)) } },

    { .matches = "*://*.facebook.com/*"_s,
        .behaviors = {
            // facebook.com rdar://100871402
            needsFacebookRemoveNotSupportedQuirk,
            // facebook.com rdar://174179871
            shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk.when(elementMatchesSelector(onSliderRoleItself)),
            // facebook.com rdar://67273166
            requiresUserGestureToPauseInPictureInPictureQuirk,
            // facebook.com rdar://158736355
            shouldEnableCameraAndMicrophonePermissionStateQuirk,
            shouldEnableRemoteTrackLabelQuirk,
            // facebook.com rdar://41104397
            shouldEnableFacebookFlagQuirk,
            // facebook.com rdar://161269819
            shouldEnableEnumerateDeviceQuirk,
            // facebook.com rdar://158736355
            shouldEnableRTCEncodedStreamsQuirk,
            // facebook.com rdar://174179871
            shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(onSliderRole)),
            // facebook.com rdar://174179871
            shouldComputeSimulatedMouseEventMovementDeltaQuirk,
            // facebook.com rdar://141103350
            // Matched against the live top URL because quirks are not re-resolved on same-document navigations.
            needsFacebookStoriesCreationFormQuirk.when(secondaryURLMatches("*://*/stories/create*"_s)),
        } },

    // facebook.com and messenger.com group calls fall back to an unsupported-browser page for Safari.
    { .matches = facebookGroupCallPatterns,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent)) } },

    // flipkart.com rdar://49648520
    { .matches = "*://*.flipkart.com/*"_s,
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk } },

    // forbes.com rdar://67273166
    { .matches = "*://*.forbes.com/*"_s,
        .behaviors = { requiresUserGestureToPauseInPictureInPictureQuirk } },

    { .matches = "*://play.geforcenow.com/*"_s,
        .behaviors = { needsGeforcenowWarningDisplayNoneQuirk } },

    // github.com https://bugs.webkit.org/show_bug.cgi?id=319011 rdar://181825035
    // github.com serves Safari some JS that tries to adjust the scroll position, which interferes
    // with WebKit's scroll to fragment implementation. A Chrome-like UA takes the working code path.
    { .matches = "*://*.github.com/*"_s,
        .behaviors = { needsChromeCompatibilityUserAgentQuirk(QuirkParameters::fromChromeCompatibilityVersion("151"_s)) } },

    // gizmodo.com rdar://102227302
    { .matches = "*://*.gizmodo.com/*"_s,
        .behaviors = { needsFullscreenDisplayNoneQuirk } },

    // Google Docs used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
    // We started caching such content in r250437 but the Google Docs index page unfortunately is not currently compatible
    // because it puts an overlay over the page when navigating away and fails to remove it when coming back from the
    // back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/57670064>.
    // Note that this does not check for docs.google.com host because of hosted G Suite apps.
    // docs.google.com rdar://59893415
    { .matches = "*://*.google.*/*"_s,
        .behaviors = { shouldBypassBackForwardCacheWhenElementMatchesQuirk.when(documentHasElementMatching(onGoogleDocsHomescreenFreezeOverlay)) } },

    // google.com https://bugs.webkit.org/show_bug.cgi?id=323851 rdar://181740296
    { .matches = googleSearchPatterns,
        .behaviors = { needsAnchorToBeMouseFocusableQuirk.when(elementMatchesSelector(onExpandablePanel)) } },

    { .matches = googleMapsPatterns,
        .behaviors = {
            // maps.google.com rdar://152194074
            mayNeedToIgnoreContentObservation.when(elementMatchesSelector(onSuggestionsLabel)),
            // maps.google.com rdar://185857498
            needsGoogleMapsMagnificationWheelDeltaScalingQuirk,
            // maps.google.com rdar://67358928
            needsGoogleMapsScrollingQuirk,
            // maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
            shouldAvoidResizingWhenInputViewBoundsChangeQuirk,
            // maps.google.com rdar://49124313
            shouldDispatchSimulatedMouseEventsQuirk,
        } },

    // google.com/maps/embed rdar://184166392
    { .embeddedMatches = "*://*.google.*/maps/embed*"_s,
        .behaviors = { needsGoogleMapsEmbedManipulationSurfaceQuirk } },

    { .matches = "*://docs.google.com/*"_s,
        .behaviors = {
            inputMethodUsesCorrectKeyEventOrder,
            inputMethodMustUseCompositionEvents,
            // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
            isTouchBarUpdateSuppressedForHiddenContentEditableQuirk,
            // docs.google.com
            needsGoogleDocsNavigationWidgetScrollQuirk,
            // docs.google.com rdar://49864669
            shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk,
            // docs.google.com rdar://59402637
            shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk.when(elementMatchesSelector(onGoogleDocsMLPromotion)),
            // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199933
            shouldOpenAsAboutBlankQuirk,
        } },

    // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
    { .matches = "*://docs.google.com/spreadsheets/*"_s,
        .behaviors = { needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk } },

    { .matches = "*://docs.google.com/presentation/*"_s,
        .behaviors = { shouldIgnoreInputModeNone } },

    // mail.google.com rdar://49403416
    { .matches = "*://mail.google.com/*"_s,
        .behaviors = { needsGMailOverflowScrollQuirk } },

    // translate.google.com rdar://106539018
    { .matches = "*://translate.google.com/*"_s,
        .behaviors = { needsGoogleTranslateScrollingQuirk } },

    { .matches = "*://translate.google.com/*"_s,
        .behaviors = { needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(chromeUserAgentScript)) },
        .isAvailable = iOSFamily },

    // sites.google.com rdar://58653069
    { .matches = "*://sites.google.com/*"_s,
        .behaviors = { shouldPreventTouchEndDispatchQuirk.when(elementMatchesSelector(onGoogleSitesButton)) } },

    { .matches = "*://meet.google.com/*"_s,
        .behaviors = { shouldEnableCameraBackgroundPlayback } },

    // meet.goto.com rdar://112632639
    { .matches = "*://meet.goto.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    // gunbroker.com rdar://157388533
    { .matches = "*://*.gunbroker.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(safari18_6UserAgent)) } },

    // hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
    { .matches = "*://*.hbomax.com/*"_s,
        .behaviors = { shouldEnableFontLoadingAPIQuirk } },

    { .matches = "*://play.hbomax.com/*"_s,
        .behaviors = {
            // play.hbomax.com rdar://158430821
            shouldDisableAdSkippingInPip,
            // hbomax.com: rdar://138806698
            shouldSupportHoverMediaQueriesQuirk,
        } },

    // hbomax.com: rdar://138424489
    { .matches = "*://play.hbomax.com/*"_s,
        .behaviors = { needsZeroMaxTouchPointsQuirk },
        .isAvailable = desktopContentModeQuirks },

    // hfhs.org rdar://61788722
    { .matches = "*://*.hfhs.org/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    // security.us.hsbc.com rdar://65870401
    { .matches = "*://security.us.hsbc.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(safari13UserAgent)) } },

    { .matches = "*://*.hulu.com/*"_s,
        .behaviors = {
            // hulu.com rdar://55041979
            needsCanPlayAfterSeekedQuirk,
            // hulu.com rdar://100199996
            needsVideoShouldMaintainAspectRatioQuirk,
            // hulu.com rdar://126096361
            implicitMuteWhenVolumeSetToZero,
        } },

    // icloud.com rdar://187710972
    { .matches = "*://*.icloud.com/*"_s,
        .behaviors = { mayNeedToIgnoreContentObservation.when(elementMatchesSelector(onTreeItem)) } },
    // icloud.com rdar://188875593
    { .matches = "*://*.icloud.com/*"_s,
        .behaviors = { shouldTreatLongClickAsSecondaryClickQuirk.when(elementMatchesSelector(onICloudMailListItem)) } },
    // icloud.com rdar://131836301
    { .matches = "*://*.icloud.com/*mail*"_s,
        .behaviors = { shouldSilenceWindowResizeEventsDuringApplicationSnapshotting } },
    { .matches = "*://*.icloud.com/*"_s, .fragmentContains = "mail"_s,
        .behaviors = { shouldSilenceWindowResizeEventsDuringApplicationSnapshotting } },
    // icloud.com rdar://26013388
    { .matches = "*://*.icloud.com/*notes*"_s,
        .behaviors = { isNeverRichlyEditableForTouchBarQuirk } },
    { .matches = "*://*.icloud.com/*"_s, .fragmentContains = "notes"_s,
        .behaviors = { isNeverRichlyEditableForTouchBarQuirk } },

    { .matches = "*://*.iheart.com/*"_s,
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(iHeartListenCookieScript)),
        } },

    { .matches = "*://*.imdb.com/*"_s,
        .behaviors = {
            // imdb.com: rdar://137991466
            needsChromeMediaControlsPseudoElementQuirk,
            // imdb.com: rdar://162684936
            needsZeroMaxTouchPointsQuirk,
        } },

    // FIXME: Remove this quirk once <rdar://113978106> is no longer happening.
    { .matches = "*://www.indiatimes.com/*"_s,
        .behaviors = { needsIPadMiniUserAgentQuirk } },

    { .matches = "*://*.instacart.com/*"_s, .environment = URLEnvironment::SafariWebApp,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    { .matches = "*://*.instagram.com/*"_s,
        .behaviors = {
            // rdar://166400170
            needsInstagramResizingReelsQuirk.when(elementMatchesSelector(onElementContainingVideo)),
            // instagram.com: rdar://174936655
            shouldSendFakeTouchForceChangeEvent,
        } },

    // invideo.io rdar://171741842 https://webkit.org/b/311602
    { .matches = "*://*.invideo.io/*"_s,
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(inVideoChromeObjectScript)),
            needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)),
        } },

    // ippk.pl rdar://82221582
    { .matches = "*://*.ippk.pl/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    // irs.gov rdar://60782373
    { .matches = "*://*.irs.gov/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    // linkedin.com: native taps must reach the video.js player surface.
    { .matches = "*://*.linkedin.com/*"_s,
        .behaviors = { shouldAllowNativeTapsOnMediaElementsQuirk.when(elementMatchesSelector(onVideoJSTech)) } },

    // live.com: rdar://167489768
    { .matches = "*://*.live.com/*"_s, .embeddedMatches = wordEditorFrameURL,
        .behaviors = { needsChromeOSNavigatorUserAgentQuirk.when(secondaryURLMatches(wordEditorScriptURL)) } },

    // live.com rdar://52116170
    { .matches = "*://*.live.com/*"_s,
        .behaviors = { shouldAvoidResizingWhenInputViewBoundsChangeQuirk } },

    { .matches = "*://outlook.live.com/*"_s,
        .behaviors = {
            // outlook.live.com: rdar://136624720
            needsMozillaFileTypeForDataTransferQuirk,
            // outlook.live.com: rdar://152277211
            mayNeedToIgnoreContentObservation.when(elementMatchesSelector(onSwatchColorPicker)),
            // Outlook detects Safari and handles selections incorrectly in their rich text editor roosterjs.
            needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent)),
            // outlook.live.com: rdar://151851274
            shouldAllowTouchMoveToChangeSelectionQuirk,
        } },

    // Outlook on the web: rdar://187832523
    { .matches = outlookPatterns,
        .behaviors = { shouldTreatLongClickAsSecondaryClickQuirk.when(elementMatchesSelector(onOutlookMailListItem)) } },

    // outlook.live.com rdar://48008837
    { .matches = "*://outlook.live.com/*"_s,
        .behaviors = { shouldPreventTouchMoveDispatchQuirk.when(elementMatchesSelector(onOutlookSuggestions)) },
        .isAvailable = touchEvents },

    // Microsoft office online generates data URLs with incorrect padding on Safari only (rdar://114573089).
    { .matches = "*://*.officeapps.live.com/*"_s,
        .behaviors = { shouldDisableDataURLPaddingValidation } },
    { .matches = "*://*.onedrive.live.com/*"_s,
        .behaviors = { shouldDisableDataURLPaddingValidation } },

    // onedrive.live.com rdar://26013388
    { .matches = "*://onedrive.live.com/*"_s,
        .behaviors = { isNeverRichlyEditableForTouchBarQuirk } },

    // madisoncity.k12.al.us https://bugs.webkit.org/show_bug.cgi?id=296989
    { .matches = "*://*.madisoncity.k12.al.us/*"_s,
        .behaviors = { needsFormControlToBeMouseFocusableQuirk } },

    // mailchimp.com rdar://47868965
    { .matches = "*://*.mailchimp.com/*"_s,
        .behaviors = { shouldDisablePointerEventsQuirk } },

    { .matches = "*://*.marcus.com/*"_s,
        .behaviors = {
            // Marcus: <rdar://101086391>.
            shouldExposeShowModalDialog,
            // marcus.com rdar://102959860
            shouldNavigatorPluginsBeEmpty,
        } },

    // Kinja login flow rdar://60601895
    { .matches = kinjaLoginPatterns,
        .behaviors = { needsKinjaLoginStorageAccessQuirk.when(elementMatchesSelector(onKinjaLoginAvatar)) } },

    // medium.com rdar://50457837
    { .matches = "*://*.medium.com/*"_s,
        .behaviors = { shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk } },

    // m365.cloud.microsoft rdar://157794706
    { .matches = "*://*.m365.cloud.microsoft/*"_s,
        .behaviors = { shouldAllowPopupFromMicrosoftOfficeToOneDrive.when(secondaryURLMatches("*://*.onedrive.live.com/*"_s)) } },

    // safe.menlosecurity.com rdar://135114489
    { .matches = "*://safe.menlosecurity.com/*"_s,
        .behaviors = { shouldDisableWritingSuggestionsByDefaultQuirk } },

    { .matches = "*://*.messenger.com/*"_s,
        .behaviors = {
            // facebook.com rdar://158736355
            shouldEnableCameraAndMicrophonePermissionStateQuirk,
            shouldEnableRemoteTrackLabelQuirk,
            // facebook.com rdar://161269819
            shouldEnableEnumerateDeviceQuirk,
            // facebook.com rdar://158736355
            shouldEnableRTCEncodedStreamsQuirk,
        } },

    // rdar://147429596
    { .matches = "*://*.nba.com/*"_s,
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(nbaSeekBarFixScript)),
            needsPerDocumentAutoplayBehaviorQuirk,
        },
        .isAvailable = iOSFamily },

    { .matches = "*://*.nba.com/*"_s, .environment = URLEnvironment::SmallScreen,
        .behaviors = { shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen },
        .isAvailable = iOS },

    // mybinder.org rdar://51770057
    { .matches = "*://*.mybinder.org/*"_s,
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(onDockPanelTabBar)) },
        .isAvailable = touchEvents || touchEventRegions },

    // naver.com rdar://48068610
    { .matches = "*://*.naver.com/*"_s, .excludeMatches = naverPatternsWithoutSimulatedMouseEvents,
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk } },

    { .matches = "*://*.netflix.com/*"_s,
        .behaviors = {
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
            needsSeekingSupportDisabledQuirk,
            // netflix.com rdar://178545839
            needsNetflixVolumeSliderQuirk,
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=304608
            shouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick,
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=193301
            needsPerDocumentAutoplayBehaviorQuirk,
        } },

    { .matches = "*://*.netflix.com/*"_s,
        .behaviors = { needsNowPlayingFullscreenSwapQuirk },
        .isAvailable = vision },

    { .matches = "*://*.nfl.com/*"_s,
        .behaviors = { shouldSuppressHLSSubtitles } },

    { .matches = "*://*.nhl.com/*"_s,
        .behaviors = { needsWebKitMediaTextTrackDisplayQuirk } },

    // nytimes.com: rdar://problem/5976384
    { .matches = "*://*.nytimes.com/*"_s,
        .behaviors = { shouldSilenceWindowResizeEventsDuringApplicationSnapshotting },
        .isAvailable = iOS || vision },

    // Pandora: <rdar://100243111>.
    { .matches = "*://*.pandora.com/*"_s,
        .behaviors = { shouldExposeShowModalDialog } },

    // mms.pinduoduo.com https://bugs.webkit.org/b/318201
    { .matches = "*://mms.pinduoduo.com/*"_s,
        .behaviors = { needsChromeCompatibilityUserAgentQuirk(QuirkParameters::fromChromeCompatibilityVersion("149"_s)) } },

    // pinterest.com rdar://104979314
    // FIXME: Remove this Quirk if Pinterest decides to trigger this notification from an user gesture (rdar://165745719)
    { .matches = "*://*.pinterest.com/*"_s,
        .behaviors = { shouldAllowNotificationPermissionWithoutUserGesture } },

    { .matches = "*://*.premierleague.com/*"_s,
        .behaviors = {
            // premierleague.com: rdar://123721211
            shouldIgnorePlaysInlineRequirementQuirk,
            // premierleague.com: rdar://68938833
            shouldDispatchPlayPauseEventsOnResume,
            // premierleague.com: rdar://136791737
            shouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
        } },

    // ralphlauren.com rdar://55629493
    { .matches = "*://*.ralphlauren.com/*"_s,
        .behaviors = { shouldIgnoreAriaForFastPathContentObservationCheckQuirk } },

    // reddit.com: rdar://80550715
    { .matches = "*://*.reddit.com/*"_s,
        .behaviors = { requiresUserGestureToPauseInPictureInPictureQuirk },
        .isAvailable = videoPresentationMode || iOSFamily },

    // reddit.com with Sink It extension: rdar://176377447.
    { .matches = "*://*.reddit.com/*"_s,
        .behaviors = { shouldDisableScrollAnchoringQuirk.when(documentHasElementMatching(onRedditSinkItBackToTop)) },
        .isAvailable = iOSFamily },

    // FIXME: Remove this quirk when <rdar://problem/61733101> is complete.
    { .matches = "*://*.roblox.com/*"_s,
        .behaviors = { needsIPadMiniUserAgentQuirk } },

    // sapo.pt rdar://60314791
    { .matches = "*://*.sapo.pt/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    { .matches = "*://*.scribd.com/*"_s,
        .behaviors = { needsReuseLiveRangeForSelectionUpdateQuirk } },

    // sfusd.edu: rdar://116292738
    { .matches = "*://*.sfusd.edu/*"_s,
        .behaviors = { shouldBypassAsyncScriptDeferring } },

    // sharepoint.com rdar://52116170
    { .matches = "*://*.sharepoint.com/*"_s,
        .behaviors = { shouldAvoidResizingWhenInputViewBoundsChangeQuirk } },

    { .matches = shopeeAccountLinkingPatterns,
        .behaviors = { needsIPhoneUserAgentQuirk },
        .isAvailable = iOSFamily },

    { .matches = "*://*.slack.com/*"_s,
        .behaviors = {
            // slack.com: rdar://138614711
            shouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk,
            // slack.com: rdar://171190689
            shouldUseDynamicViewportUnitsAsDefaultQuirk,
        },
        .isAvailable = iOSFamily },

    { .matches = "*://*.soundcloud.com/*"_s,
        .behaviors = {
            // soundcloud.com rdar://52915981
            shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk.when(elementMatchesSelector(onSoundCloudSceneLayer)),
            // Soundcloud: rdar://102913500
            shouldExposeShowModalDialog,
            // soundcloud.com rdar://52915981
            shouldDispatchSimulatedMouseEventsQuirk,
        } },

    // soylent.*: rdar://113314067
    { .matches = "*://*.soylent.*/*"_s,
        .behaviors = { shouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick } },

    // spotify.com rdar://138918575
    { .matches = "*://open.spotify.com/*"_s,
        .behaviors = {
            needsBodyScrollbarWidthNoneDisabledQuirk,
            shouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
            shouldLimitHLSPlaybackRate,
            needsWebKitMediaTextTrackDisplayQuirk,
            shouldDeferIntersectionObserversDuringResize,
            shouldBlockAudiblePlaybackWhileAudioIsPlaying,
            needsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk,
        } },

    // Remove this once rdar://142573562 is resolved.
    { .matches = "*://*.steampowered.com/*"_s,
        .behaviors = { shouldTreatAddingMouseOutEventListenerAsContentChange } },

    // sutterhealth.org rdar://61788722
    { .matches = "*://*.sutterhealth.org/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    { .matches = "*://*.theguardian.*/*"_s,
        .behaviors = { shouldHideSoftTopScrollEdgeEffectDuringFocusQuirk.when(elementMatchesSelector(onCrosswordID)) } },

    // theguardian.com rdar://166727225
    { .matches = "*://*.theguardian.*/*"_s, .embeddedMatches = youTubeEmbedPatterns,
        .behaviors = { needsYouTubeEmbedAutoplayQuirk } },

    // teams.live.com rdar://88678598
    // teams.microsoft.com rdar://90434296
    { .matches = microsoftTeamsPatterns,
        .behaviors = { shouldAllowMSTeamsProtocolWithoutUserGestureQuirk } },

    // www.microsoft.com sign-in FIXME(218779): remove once the login flow redesign ships.
    { .matches = "*://www.microsoft.com/*"_s,
        .behaviors = { needsStorageAccessOnLoginButtonClickQuirk.when(elementMatchesSelector(onMicrosoftSignInButton)) } },

    // playstation.com sign-in FIXME(218760): remove once the login flow redesign ships.
    { .matches = playStationSignInPatterns,
        .behaviors = { needsStorageAccessOnLoginButtonClickQuirk.when(elementMatchesSelector(onPlayStationSignInButton)) } },

    // teams.microsoft.com https://bugs.webkit.org/show_bug.cgi?id=219505
    { .matches = "*://teams.microsoft.com/*"_s, .queryContains = "Retried+3+times+without+success"_s,
        .behaviors = { isMicrosoftTeamsRedirectURLQuirk } },

    // santaclaracounty.telleronline.net rdar://165844163
    { .matches = "*://santaclaracounty.telleronline.net/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    { .matches = "*://*.thesaurus.com/*"_s,
        .behaviors = { needsAnchorToBeMouseFocusableQuirk } },

    // player.anyclip.com rdar://138789765
    { .matches = "*://*.thesaurus.com/*"_s,
        .behaviors = { needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(chromeUserAgentScript)).when(secondaryURLMatches(anyclipPlayerScriptURL)) },
        .isAvailable = iOSFamily },

    // dev.ti.com rdar://126629168
    { .matches = "*://dev.ti.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    { .matches = "*://*.tiktok.com/*"_s,
        .behaviors = {
            needsTikTokCommentsOverflowingContentQuirk.when(elementMatchesSelector(onTikTokCommentsContainer)),
            needsTikTokVideoOverflowingContentQuirk.when(elementMatchesSelector(onTikTokVideoContainer)),
            // tiktok.com rdar://174179805
            shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk.when(elementMatchesSelector(onSliderRoleItself)),
            // tiktok.com rdar://174179805
            shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(onSliderRole)),
            // tiktok.com rdar://174179805
            shouldComputeSimulatedMouseEventMovementDeltaQuirk,
            // tiktok.com rdar://154893648
            needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(safari18_6UserAgent)),
            // FIXME(rdar://148759791): Remove this once TikTok removes the outdated error message.
            needsChromeCompatibilityUserAgentQuirk(QuirkParameters::fromChromeCompatibilityVersion("136"_s)),
            // tiktok.com rdar://problem/183445905
            needsTikTokCaptchaSliderTouchActionQuirk.when(elementMatchesSelector(onTikTokCaptchaDragWrapper)),
        } },

    // trix-editor.org rdar://28242210
    { .matches = "*://*.trix-editor.org/*"_s,
        .behaviors = { isNeverRichlyEditableForTouchBarQuirk } },

    // twitch.tv rdar://102420527
    { .matches = "*://*.twitch.tv/*"_s,
        .behaviors = { shouldReportDocumentAsVisibleIfActivePIPQuirk } },

    // https://tympanus.net/Tutorials/WebGPUFluid/ does not load (rdar://143839620).
    { .matches = "*://*.tympanus.net/*"_s,
        .behaviors = { shouldBlockFetchWithNewlineAndLessThan } },

    // uhc.com rdar://173206598
    { .matches = "*://*.uhc.com/*"_s,
        .behaviors = { shouldTranscodeHeicImagesQuirk } },

    // unifi.ui.com rdar://180411019
    { .matches = "*://*.ui.com/*"_s,
        .behaviors = { needsSupportsProgressMonitoringQuirk } },

    // upgrad.com rdar://170751919
    { .matches = "*://*.upgrad.com/*"_s,
        .behaviors = { needsUserAgentStringOverrideQuirk(QuirkParameters::fromUserAgent(chromeUserAgent152)) } },

    // Breaks express checkout on victoriassecret.com (rdar://104818312).
    { .matches = "*://*.victoriassecret.com/*"_s,
        .behaviors = { shouldDisableFetchMetadata } },

    { .matches = "*://*.vimeo.com/*"_s,
        .behaviors = {
            // Vimeo.com used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
            // We started caching such content in r250437 but the vimeo.com content unfortunately is not currently compatible
            // because it changes the opacity of its body to 0 when navigating away and fails to restore the original opacity
            // when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/56996057>.
            shouldBypassBackForwardCacheForNoStoreQuirk,
            // vimeo.com rdar://55759025
            needsPreloadAutoQuirk,
            // vimeo.com: rdar://problem/73227900
            shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
            // vimeo.com: rdar://107592139
            blocksEnteringStandardFullscreenFromPictureInPictureQuirk,
            // vimeo.com: rdar://problem/70788878
            blocksReturnToFullscreenFromPictureInPictureQuirk,
        } },

    // walmart.com: rdar://123734840
    { .matches = "*://*.walmart.com/*"_s,
        .behaviors = {
            mayNeedToIgnoreContentObservation.when(elementMatchesSelector(onButtonInListItem)),
        },
        .isAvailable = twoPhaseClicks },

    // weather.com rdar://139689157
    { .matches = "*://*.weather.com/*"_s,
        .behaviors = { needsFormControlToBeMouseFocusableQuirk } },

    { .matches = "*://*.webex.com/*"_s,
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(webExUndefinedTouchScript)).when(secondaryURLMatches(webExPushDownloadScriptURL)),
            // webex.com rdar://143715630
            needsWebExScrollabilityQuirk,
        },
        .isAvailable = iOSFamily && desktopContentModeQuirks },

    // weebly.com rdar://48003980
    { .matches = "*://*.weebly.com/*"_s,
        .behaviors = { shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk } },

    // wellnessliving.com rdar://185639386
    { .matches = "*://*.wellnessliving.com/*"_s,
        .behaviors = { needsPartitionedCookiesQuirk } },

    { .matches = "*://*.wikipedia.org/*"_s,
        .behaviors = {
            // wikipedia.org rdar://54856323
            shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk,
            // wikipedia.org https://webkit.org/b/247636
            shouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk,
        } },

    // rdar://170412045, https://bugs.webkit.org/show_bug.cgi?id=307933
    // wix.com rdar://49124313, except while picking a template.
    { .matches = "*://*.wix.com/*"_s, .excludeMatches = "*://*.wix.com/website/templates/*"_s,
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk } },

    { .matches = "*://*.workspaces.xyz/*"_s,
        .behaviors = { shouldComparareUsedValuesForBorderWidthForTriggeringTransitions } },

    // wpdevelopment.ca rdar://156109518
    { .matches = "*://*.wpdevelopment.ca/*"_s,
        .behaviors = { needsFormControlToBeMouseFocusableQuirk } },

    { .matches = "*://*.x.com/*"_s,
        .behaviors = {
            // x.com: rdar://132850672
            shouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk,
            // rdar://121473410
            shouldSilenceMediaQueryListChangeEvents,
            // x.com: rdar://73369869
            requiresUserGestureToLoadInPictureInPictureQuirk,
            // x.com: rdar://73369869
            requiresUserGestureToPauseInPictureInPictureQuirk,
            // x.com: https://bugs.webkit.org/show_bug.cgi?id=323931 rdar://183399060
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(xGoogleSignInButtonFixScript)).when(secondaryURLMatches(googleSignInClientScriptURL)),
        } },

    { .matches = "*://*.x.com/*"_s,
        .behaviors = {
            // x.com: rdar://problem/58804852 and rdar://problem/61731801
            shouldSilenceWindowResizeEventsDuringApplicationSnapshotting,
            // x.com: rdar://175565114
            shouldAvoidProgrammaticScrollClampingQuirk,
        },
        .isAvailable = iOS || vision },

    { .matches = "*://*.yahoo.*/*"_s,
        .behaviors = {
            // yahoo.com: rdar://170502516
            needsYahooVolumeSliderQuirk,
            // yahoo.com: rdar://136767005
            shouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
        } },

    // yahoo.com: rdar://148284059
    { .matches = "*://*.yahoo.*/*"_s, .embeddedMatches = "*://*.yimg.com/*"_s,
        .behaviors = { requiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk } },

    // yahoo.com : rdar://142894603
    { .matches = "*://*.yahoo.*/*"_s,
        .behaviors = { shouldPreventTouchEndDispatchQuirk.when(elementMatchesSelector(onYahooButton)) },
        .isAvailable = touchEvents },

    // news.ycombinator.com: rdar://127246368
    { .matches = "*://news.ycombinator.com/*"_s,
        .behaviors = { shouldIgnoreTextAutoSizingQuirk } },

    // Embedded youtube.com players need storage access for the "Watch later" button. rdar://64549429
    { .embeddedMatches = "*://*.youtube.com/*"_s,
        .behaviors = { needsStorageAccessForYouTubeWatchLaterQuirk.when(elementMatchesSelector(onYouTubeWatchLaterIcon)) } },

    { .matches = "*://*.youtube.com/*"_s,
        .behaviors = {
            // youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
            hasBrokenEncryptedMediaAPISupportQuirk,
            // youtube.com rdar://135886305
            needsScrollbarWidthThinDisabledQuirk,
            needsYouTubeCaptionQuirk,
            // youtube.com: rdar://110097836
            shouldSilenceResizeObservers,
            // youtube.com https://bugs.webkit.org/show_bug.cgi?id=325264
            shouldDisableThreadedAnimationsQuirk,
        } },

    // Embedded youtube.com players need the caption quirk regardless of the embedding site.
    { .embeddedMatches = youTubeEmbedPatterns,
        .behaviors = { needsYouTubeCaptionQuirk } },

    // youtube.com rdar://49582231
    { .matches = "*://www.youtube.com/*"_s,
        .behaviors = { needsYouTubeOverflowScrollQuirk } },

    { .matches = "*://*.youtube.com/*"_s, .environment = URLEnvironment::TubularApp,
        .behaviors = { shouldSuppressMediaSessionPauseActionOnInterruption },
        .isAvailable = iOSFamily },

    // www.youtube.com rdar://52361019
    { .matches = "*://www.youtube.com/*"_s,
        .behaviors = { needsYouTubeMouseOutQuirk } },

    // Lens.app rdar://178769976
    { .matches = "*://*.youtube.com/*"_s, .environment = URLEnvironment::LensApp,
        .behaviors = { requiresUserGestureToPlayInFullscreenQuirk },
        .isAvailable = vision },

    // zencastr.com rdar://143087016
    { .matches = "*://*.zencastr.com/*"_s,
        .behaviors = { needsLimitedMatroskaSupportQuirk } },

    // zillow.com rdar://53103732
    { .matches = "*://www.zillow.com/*"_s,
        .behaviors = { shouldAvoidScrollingWhenFocusedContentIsVisibleQuirk } },

    { .matches = "*://*.zillow.com/*"_s,
        .behaviors = {
            // zillow.com rdar://79872092
            shouldTranscodeHeicImagesQuirk,
            // zillow.com rdar://110097836
            shouldSilenceResizeObservers,
        } },

    { .matches = "*://*.zomato.com/*"_s,
        .behaviors = { needsZomatoEmailLoginLabelQuirk } },

    { .matches = "*://*.zoom.us/*"_s,
        .behaviors = {
            // zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
            shouldAutoplayWebAudioForArbitraryUserGestureQuirk,
            // zoom.us rdar://118185086
            shouldDisableImageCaptureQuirk,
            shouldAllowMediaStreamTrackSerializationQuirk,
        } },
};

consteval bool everyQuirkNamesAURL()
{
    return std::ranges::all_of(fullTable, [](auto& quirk) {
        return !quirk.matches.isEmpty() || !quirk.embeddedMatches.isEmpty();
    });
}

static_assert(everyQuirkNamesAURL(), "A quirk in fullTable has neither matches nor embeddedMatches");

consteval bool everyQuirkCarriesWhatItDeclares()
{
    for (auto& quirk : fullTable) {
        for (auto& behavior : quirk.behaviors.span()) {
            auto parametersNeeded = behavior.quirkParametersNeeded;
            if (parametersNeeded.isEmpty()) {
                if (behavior.parameters)
                    return false;
            } else {
                if (!behavior.parameters)
                    return false;

                if (parametersNeeded.contains(QuirkParametersNeeded::NeedsScript) && behavior.parameters->script.isEmpty())
                    return false;

                if (parametersNeeded.contains(QuirkParametersNeeded::NeedsUserAgent) && behavior.parameters->userAgent.isEmpty())
                    return false;

                if (parametersNeeded.contains(QuirkParametersNeeded::NeedsChromeCompatibilityVersion) && behavior.parameters->chromeCompatibilityVersion.isEmpty())
                    return false;

                if (parametersNeeded.contains(QuirkParametersNeeded::NeedsCookieNames) && behavior.parameters->cookieNames.empty())
                    return false;
            }

            if (behavior.conditions.elementSelector && !behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::ElementSelector))
                return false;

            if (!behavior.conditions.secondaryURL.isEmpty() && !behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::SecondaryURL))
                return false;

            if (behavior.conditions.documentSelector && !behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::DocumentSelector))
                return false;

            if (behavior.quirkConditionsNeeded.contains(QuirkConditionsSupported::ElementSelector) && !behavior.conditions.elementSelector)
                return false;

            if (behavior.quirkConditionsNeeded.contains(QuirkConditionsSupported::SecondaryURL) && behavior.conditions.secondaryURL.isEmpty())
                return false;

            if (behavior.quirkConditionsNeeded.contains(QuirkConditionsSupported::DocumentSelector) && !behavior.conditions.documentSelector)
                return false;

            if (!behavior.quirkConditionsSupported.hasExactlyOneBitSet() && !behavior.quirkConditionsSupported.isEmpty())
                return false;
        }
    }

    return true;
}

static_assert(everyQuirkCarriesWhatItDeclares(), "A quirk in fullTable does not supply the parameters it declares, supplies parameters it does not declare, applies a condition the behavior does not support, or omits a condition the behavior needs");

consteval bool shouldEmit(const Quirk& quirk)
{
    return quirk.isAvailable && !quirk.behaviors.span().empty();
}

consteval size_t emittedQuirkCount()
{
    return std::ranges::count_if(fullTable, shouldEmit);
}

consteval auto emittedQuirkIndices()
{
    std::array<size_t, emittedQuirkCount()> indices { };
    size_t index = 0;
    size_t next = 0;
    for (auto& quirk : fullTable) {
        if (shouldEmit(quirk))
            indices[next++] = index;
        ++index;
    }
    return indices;
}

template<size_t... indices> consteval auto prunedTable(std::index_sequence<indices...>)
{
    constexpr auto emitted = emittedQuirkIndices();
    constexpr auto quirks = std::span { fullTable };
    return std::array<Quirk, sizeof...(indices)> { quirks[emitted[indices]]... };
}

static constexpr auto table = prunedTable(std::make_index_sequence<emittedQuirkCount()> { });

} // namespace SiteSpecificQuirks

static Vector<QuirkMatchPattern> parseCompiledPatterns(URLPatternList patterns)
{
    auto literals = patterns.span();
    return WTF::compactMap(literals, [](ASCIILiteral pattern) {
        auto parsed = QuirkMatchPattern::parse(pattern);
        ASSERT_WITH_MESSAGE(parsed, "Invalid pattern in the compiled quirk table: %s", pattern.characters());
        return parsed;
    });
}

RuntimeQuirkBehavior RuntimeQuirkBehavior::from(const QuirkBehavior& behavior)
{
    RuntimeQuirkBehavior result { };
    result.id = behavior.id;
    if (auto& parameters = behavior.parameters) {
        result.script = parameters->script;
        result.userAgent = parameters->userAgent;
        result.chromeCompatibilityVersion = parameters->chromeCompatibilityVersion;
        result.cookieNames = WTF::map(parameters->cookieNames, [](ASCIILiteral name) {
            return String { name };
        });
    }

    auto& conditions = behavior.conditions;
    if (conditions.elementSelector)
        result.elementSelector = *conditions.elementSelector;
    if (conditions.documentSelector)
        result.documentSelector = *conditions.documentSelector;
    result.secondaryURL = parseCompiledPatterns(conditions.secondaryURL);
    return result;
}

RuntimeQuirk RuntimeQuirk::from(const Quirk& quirk)
{
    return RuntimeQuirk {
        .matches = parseCompiledPatterns(quirk.matches),
        .embeddedMatches = parseCompiledPatterns(quirk.embeddedMatches),
        .excludeMatches = parseCompiledPatterns(quirk.excludeMatches),
        .queryContains = quirk.queryContains,
        .fragmentContains = quirk.fragmentContains,
        .environment = quirk.environment,
        .behaviors = WTF::map(quirk.behaviors.span(), RuntimeQuirkBehavior::from),
    };
}

static QuirksData resolveSiteSpecificQuirks(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument documentIsTopDocument)
{
    QuirksData quirksData;
    for (auto& quirk : quirkTable().quirks) {
        if (quirk.appliesTo(topContext, documentContext, documentIsTopDocument))
            quirk.apply(quirksData);
    }
    return quirksData;
}

QuirksData resolveSiteSpecificQuirks(const URL& topURL, const URL& documentURL, IsTopDocument documentIsTopDocument)
{
    return resolveSiteSpecificQuirks(URLMatchContext { topURL }, URLMatchContext { documentURL }, documentIsTopDocument);
}

QuirksData resolveTopURLQuirks(const URL& url)
{
    URLMatchContext context { url };
    return resolveSiteSpecificQuirks(context, context, IsTopDocument::Yes);
}

std::span<const Quirk> compiledQuirks()
{
    return SiteSpecificQuirks::table;
}

} // namespace WebCore
