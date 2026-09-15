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
#include "QuirkBehaviors.h"

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <utility>

namespace WebCore {

static constexpr std::array bbcDomains { "bbc.co.uk"_s, "bbc.com"_s };
static constexpr std::array expediaGroupDomains {
    "carrentals.com"_s, "cheaptickets.com"_s, "hoteis.com"_s, "hoteles.com"_s,
    "hotels.com"_s, "mrjet.se"_s, "orbitz.com"_s, "travelocity.ca"_s,
    "travelocity.com"_s, "wotif.co.nz"_s, "wotif.com"_s
};
static constexpr std::array microsoftTeamsHosts { "teams.live.com"_s, "teams.microsoft.com"_s };
static constexpr std::array naverHostsWithoutSimulatedMouseEvents { "tv.naver.com"_s, "mail.naver.com"_s, "m.naver.com"_s };
static constexpr std::array youTubeEmbedDomains { "youtube.com"_s, "youtube-nocookie.com"_s };
static constexpr std::array claudeDomains { "claude.ai"_s, "claude.com"_s };

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

static constexpr auto onSliderRole = "[role=slider], [role=slider] *"_s;
static constexpr auto onDockerPanelTabBar = ".lm-DockPanel-tabBar, .lm-DockPanel-tabBar *"_s;

namespace SiteSpecificQuirks {
using namespace QuirkBehaviors;
using namespace QuirkBehaviorConditions;
using namespace URLRefinement;
using namespace BuildCondition;

static constexpr auto anyclipPlayerScriptURL = URLMatch::host("player.anyclip.com"_s).when(lastPathComponentEndsWith("lre.js"_s));
static constexpr auto ceacBrowserCloseScriptURL = URLMatch::anyURL().when(lastPathComponentIs("CheckBrowserClose.js"_s));
static constexpr auto googleSignInClientScriptURL = URLMatch::host("accounts.google.com"_s).when(pathIs("/gsi/client"_s));
static constexpr auto webExPushDownloadScriptURL = URLMatch::anyURL().when(lastPathComponentStartsWith("pushdownload."_s));

static constexpr Quirk fullTable[] = {
    // 365scores.com rdar://116491386
    { .match = URLMatch::domain("365scores.com"_s),
        .behaviors = { shouldSilenceWindowResizeEventsDuringApplicationSnapshotting },
        .isAvailable = iOS || vision },

    // actesting.org rdar://124017544
    { .match = URLMatch::domain("actesting.org"_s),
        .behaviors = { shouldEnableLegacyGetUserMediaQuirk } },

    // airindiaexpress.com https://webkit.org/b/317375
    { .match = URLMatch::domain("airindiaexpress.com"_s),
        .behaviors = { needsAirIndiaExpressLayeringQuirk } },

    // Note: There is a userAgent override for rdar://117771731, see needsCustomUserAgentOverride()
    // airtable.com rdar://49124313
    { .match = URLMatch::domain("airtable.com"_s),
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk } },

    { .match = URLMatch::anyTopLevelDomain("amazon"_s),
        .behaviors = {
            // amazon.com rdar://49124529
            shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
            // amazon.com rdar://49124313
            shouldDispatchSimulatedMouseEventsQuirk,
        },
        .site = QuirkSite::Amazon },

    { .match = URLMatch::domain("amazon.design"_s),
        .behaviors = { needsAmazonDesignMenuViewportUnitQuirk } },

    // apple.com rdar://154434137
    // FIXME: Maybe EnsureCaptionVisibilityInFullscreenAndPictureInPicture should apply to apple.com.cn too?
    { .match = URLMatch::domain("apple.com"_s),
        .behaviors = { ensureCaptionVisibilityInFullscreenAndPictureInPicture } },

    // Quirk added for rdar://181007316, remove when rdar://182134549 is fixed.
    { .match = URLMatch::anyTopLevelDomain("apple"_s).when(pathContains("/retail"_s)),
        .behaviors = { shouldDisableScrollAnchoringQuirk } },

    // as.com: rdar://121014613
    { .match = URLMatch::domain("as.com"_s).when(smallScreen()),
        .behaviors = { shouldDisableElementFullscreenQuirk } },

    // att.com rdar://55185021
    { .match = URLMatch::domain("att.com"_s),
        .behaviors = { shouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk },
        .isAvailable = iOSFamily },

    // Login issue on bankofamerica.com (rdar://104938789).
    { .match = URLMatch::domain("bankofamerica.com"_s),
        .behaviors = {
            maybeBypassBackForwardCache,
        },
        .site = QuirkSite::BankOfAmerica },

    // bbc.co.uk rdar://126494734
    // bbc.com rdar://157499149
    { .match = URLMatch::domain(bbcDomains),
        .behaviors = { returnNullPictureInPictureElementDuringFullscreenChangeQuirk } },

    // bestbuy.com rdar://136235936
    { .match = URLMatch::domain("bestbuy.com"_s),
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(bestBuyLanguageScript)),
        } },

    // billpaysite.com rdar://141328971
    { .match = URLMatch::hostOrSubdomainOf("billpaysite.com"_s),
        .behaviors = { needsPartitionedCookiesQuirk } },

    { .match = URLMatch::domain("bing.com"_s),
        .behaviors = {
            // bing.com rdar://133223599
            maybeBypassBackForwardCache,
            // bing.com rdar://126573838
            needsMediaRewriteRangeRequestQuirk,
        },
        .site = QuirkSite::Bing },

    // bungalow.com rdar://61658940
    { .match = URLMatch::domain("bungalow.com"_s),
        .behaviors = { shouldBypassAsyncScriptDeferring } },

    // canva.com https://webkit.org/b/293886
    { .match = URLMatch::domain("canva.com"_s),
        .behaviors = { shouldTranscodeHeicImagesQuirk } },

    { .match = URLMatch::domain("capitalgroup.com"_s),
        .behaviors = { shouldDelayReloadWhenRegisteringServiceWorker } },

    // Remove this once rdar://139478801 is resolved.
    { .match = URLMatch::domain("cbssports.com"_s),
        .behaviors = {
            shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk,
        },
        .site = QuirkSite::CBSSports,
        .isAvailable = iOSFamily },

    { .match = URLMatch::hostOrSubdomainOf("ceac.state.gov"_s),
        .behaviors = {
            // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
            needsFormControlToBeMouseFocusableQuirk,
            // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=311383
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(ceacBeforeUnloadFixScript, ceacBrowserCloseScriptURL)),
        } },

    // secure.chase.com rdar://126715227
    { .match = URLMatch::host("secure.chase.com"_s),
        .behaviors = { shouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk },
        .isAvailable = touchEvents },

    { .match = URLMatch::domain("chess.com"_s).when(smallScreen()),
        .behaviors = { shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen },
        .isAvailable = iOS },

    { .match = URLMatch::domain("claude.ai"_s),
        .behaviors = {
            needsClaudeSidebarViewportUnitQuirk,
            // rdar://174779259 - logout flow leaves identification cookies
            // causing redirect loop on next /chat boot.
            // See Quirks::clearLogoutSurvivingIdentityCookiesIfNeeded().
            needsLogoutCookieCleanupQuirk,
        } },

    { .match = URLMatch::domain(claudeDomains),
        .behaviors = { needsHideSelectionDuringOverflowScrollQuirk } },

    { .match = URLMatch::domain("cnn.com"_s),
        .behaviors = {
            // cnn.com rdar://119640248
            needsFullscreenObjectFitQuirk,
            needsCNNCaptionQuirk,
            // cnn.com rdar://176539646
            shouldDisableThreadedAnimationsQuirk,
        },
        .isAvailable = iOSFamily },

    { .match = URLMatch::host("codepen.io"_s),
        .behaviors = { shouldEnableSpeakerSelectionPermissionsPolicyQuirk } },

    { .match = URLMatch::domain("crunchyroll.com"_s),
        .behaviors = { needsSuppressPostLayoutBoundaryEventsQuirk } },

    { .match = URLMatch::domain("dailymail.co.uk"_s),
        .behaviors = { shouldUnloadHeavyFrames } },

    { .match = URLMatch::host("digits.t-mobile.com"_s),
        .behaviors = { needsNavigatorUserAgentDataQuirk, needsCustomUserAgentData } },

    // descript.com rdar://156024693
    { .match = URLMatch::domain("descript.com"_s),
        .behaviors = { shouldDisableDOMAudioSession } },

    { .match = URLMatch::domain("dictionary.com"_s),
        .behaviors = { needsAnchorToBeMouseFocusableQuirk } },

    // player.anyclip.com rdar://138789765
    { .match = URLMatch::domain("dictionary.com"_s),
        .behaviors = { needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(chromeUserAgentScript, anyclipPlayerScriptURL)) },
        .isAvailable = iOSFamily },

    // digitaltrends.com rdar://121014613
    { .match = URLMatch::domain("digitaltrends.com"_s).when(smallScreen()),
        .behaviors = { shouldDisableElementFullscreenQuirk } },

    // discord.com rdar://162719481
    { .match = URLMatch::domain("discord.com"_s),
        .behaviors = { shouldUseLayoutViewportForClientRectsQuirk } },

    // disneyplus rdar://137613110
    { .match = URLMatch::domain("disneyplus.com"_s),
        .behaviors = { shouldHideCoarsePointerCharacteristicsQuirk } },

    // disneyplus rdar://151715964
    { .match = URLMatch::domain("disneyplus.com"_s),
        .behaviors = { needsZeroMaxTouchPointsQuirk },
        .isAvailable = iOSFamily && desktopContentModeQuirks },

    { .match = URLMatch::domain("ea.com"_s),
        .site = QuirkSite::EA },

    { .match = URLMatch::domain("espn.com"_s),
        .behaviors = {
            // espn.com rdar://184169028
            needsSuppressedPauseEventOnFullscreenExitQuirk,
            // espn.com rdar://problem/95651814
            allowLayeredFullscreenVideos,
            // espn.com rdar://problem/73227900
            shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
        } },

    // Expedia Group rdar://126631968
    { .match = URLMatch::domain(expediaGroupDomains),
        .behaviors = { needsExpediaGroupAnimationQuirk } },
    { .match = URLMatch::anyTopLevelDomain("ebookers"_s),
        .behaviors = { needsExpediaGroupAnimationQuirk } },
    { .match = URLMatch::anyTopLevelDomain("expedia"_s),
        .behaviors = { needsExpediaGroupAnimationQuirk } },

    { .match = URLMatch::domain("facebook.com"_s),
        .behaviors = {
            // facebook.com rdar://100871402
            needsFacebookRemoveNotSupportedQuirk,
            // facebook.com rdar://174179871
            shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
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
        },
        .site = QuirkSite::Facebook },

    // flipkart.com rdar://49648520
    { .match = URLMatch::domain("flipkart.com"_s),
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk } },

    // forbes.com rdar://67273166
    { .match = URLMatch::domain("forbes.com"_s),
        .behaviors = { requiresUserGestureToPauseInPictureInPictureQuirk } },

    { .match = URLMatch::host("play.geforcenow.com"_s),
        .behaviors = { needsGeforcenowWarningDisplayNoneQuirk } },

    // gizmodo.com rdar://102227302
    { .match = URLMatch::domain("gizmodo.com"_s),
        .behaviors = { needsFullscreenDisplayNoneQuirk } },

    { .match = URLMatch::anyTopLevelDomain("google"_s),
        .behaviors = {
            // docs.google.com rdar://59893415
            maybeBypassBackForwardCache,
        },
        .site = QuirkSite::GoogleProperty },

    // google.com https://bugs.webkit.org/show_bug.cgi?id=323851 rdar://181740296
    { .match = URLMatch::anyTopLevelDomain("google"_s).when(pathIs("/search"_s)),
        .behaviors = { needsAnchorToBeMouseFocusableQuirk },
        .site = QuirkSite::GoogleSearch },

    { .match = URLMatch::anyTopLevelDomain("google"_s).when(pathStartsWith("/maps/"_s)),
        .behaviors = {
            // maps.google.com rdar://152194074
            mayNeedToIgnoreContentObservation,
            // maps.google.com rdar://67358928
            needsGoogleMapsScrollingQuirk,
            // maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
            shouldAvoidResizingWhenInputViewBoundsChangeQuirk,
            // maps.google.com rdar://49124313
            shouldDispatchSimulatedMouseEventsQuirk,
        },
        .site = QuirkSite::GoogleMaps },

    { .match = URLMatch::host("docs.google.com"_s),
        .behaviors = {
            inputMethodUsesCorrectKeyEventOrder,
            inputMethodMustUseCompositionEvents,
            // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
            isTouchBarUpdateSuppressedForHiddenContentEditableQuirk,
            // docs.google.com rdar://49864669
            shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk,
        },
        .site = QuirkSite::GoogleDocs },

    // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
    { .match = URLMatch::host("docs.google.com"_s).when(pathStartsWith("/spreadsheets/"_s)),
        .behaviors = { needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk } },

    { .match = URLMatch::host("docs.google.com"_s).when(pathStartsWith("/presentation/"_s)),
        .behaviors = { shouldIgnoreInputModeNone } },

    // mail.google.com rdar://49403416
    { .match = URLMatch::host("mail.google.com"_s),
        .behaviors = { needsGMailOverflowScrollQuirk } },

    // translate.google.com rdar://106539018
    { .match = URLMatch::host("translate.google.com"_s),
        .behaviors = { needsGoogleTranslateScrollingQuirk } },

    { .match = URLMatch::host("translate.google.com"_s),
        .behaviors = { needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(chromeUserAgentScript)) },
        .isAvailable = iOSFamily },

    // sites.google.com rdar://58653069
    { .match = URLMatch::host("sites.google.com"_s),
        .behaviors = { shouldPreventDispatchOfTouchEventQuirk } },

    { .match = URLMatch::host("meet.google.com"_s),
        .behaviors = { shouldEnableCameraBackgroundPlayback } },

    // hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
    { .match = URLMatch::domain("hbomax.com"_s),
        .behaviors = { shouldEnableFontLoadingAPIQuirk } },

    { .match = URLMatch::host("play.hbomax.com"_s),
        .behaviors = {
            // play.hbomax.com rdar://158430821
            shouldDisableAdSkippingInPip,
            // hbomax.com: rdar://138806698
            shouldSupportHoverMediaQueriesQuirk,
        } },

    // hbomax.com: rdar://138424489
    { .match = URLMatch::host("play.hbomax.com"_s),
        .behaviors = { needsZeroMaxTouchPointsQuirk },
        .isAvailable = desktopContentModeQuirks },

    { .match = URLMatch::domain("hulu.com"_s),
        .behaviors = {
            // hulu.com rdar://55041979
            needsCanPlayAfterSeekedQuirk,
            // hulu.com rdar://100199996
            needsVideoShouldMaintainAspectRatioQuirk,
            // hulu.com rdar://126096361
            implicitMuteWhenVolumeSetToZero,
        } },

    // icloud.com rdar://131836301
    { .match = URLMatch::domain("icloud.com"_s).when(pathOrFragmentContains("mail"_s)),
        .behaviors = { shouldSilenceWindowResizeEventsDuringApplicationSnapshotting } },
    // icloud.com rdar://26013388
    { .match = URLMatch::domain("icloud.com"_s).when(pathOrFragmentContains("notes"_s)),
        .behaviors = { isNeverRichlyEditableForTouchBarQuirk } },

    { .match = URLMatch::domain("iheart.com"_s),
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(iHeartListenCookieScript)),
        } },

    { .match = URLMatch::domain("imdb.com"_s),
        .behaviors = {
            // imdb.com: rdar://137991466
            needsChromeMediaControlsPseudoElementQuirk,
            // imdb.com: rdar://162684936
            needsZeroMaxTouchPointsQuirk,
        } },

    // FIXME: Remove this quirk once <rdar://113978106> is no longer happening.
    { .match = URLMatch::host("www.indiatimes.com"_s),
        .behaviors = { needsIPadMiniUserAgentQuirk } },

    { .match = URLMatch::domain("instagram.com"_s),
        .behaviors = {
            // rdar://166400170
            needsInstagramResizingReelsQuirk,
            // instagram.com: rdar://174936655
            shouldSendFakeTouchForceChangeEvent,
        } },

    // instagram.com rdar://121014613
    { .match = URLMatch::domain("instagram.com"_s),
        .behaviors = { shouldDisableElementFullscreenQuirk } },

    // invideo.io rdar://171741842 https://webkit.org/b/311602
    { .match = URLMatch::domain("invideo.io"_s),
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(inVideoChromeObjectScript)),
        } },

    { .match = URLMatch::domain("linkedin.com"_s),
        .site = QuirkSite::LinkedIn },

    { .match = URLMatch::domain("live.com"_s),
        .behaviors = {
            // live.com: rdar://167489768
            needsChromeOSNavigatorUserAgentQuirk,
            // live.com rdar://52116170
            shouldAvoidResizingWhenInputViewBoundsChangeQuirk,
        } },

    { .match = URLMatch::host("outlook.live.com"_s),
        .behaviors = {
            // outlook.live.com: rdar://136624720
            needsMozillaFileTypeForDataTransferQuirk,
            // outlook.live.com: rdar://152277211
            mayNeedToIgnoreContentObservation,
        },
        .site = QuirkSite::Outlook },

    // outlook.live.com rdar://48008837
    { .match = URLMatch::host("outlook.live.com"_s),
        .behaviors = { shouldPreventDispatchOfTouchEventQuirk },
        .isAvailable = touchEvents },

    // Microsoft office online generates data URLs with incorrect padding on Safari only (rdar://114573089).
    { .match = URLMatch::hostOrSubdomainOf("officeapps.live.com"_s),
        .behaviors = { shouldDisableDataURLPaddingValidation } },
    { .match = URLMatch::hostOrSubdomainOf("onedrive.live.com"_s),
        .behaviors = { shouldDisableDataURLPaddingValidation } },

    // onedrive.live.com rdar://26013388
    { .match = URLMatch::host("onedrive.live.com"_s),
        .behaviors = { isNeverRichlyEditableForTouchBarQuirk } },

    // madisoncity.k12.al.us https://bugs.webkit.org/show_bug.cgi?id=296989
    { .match = URLMatch::domain("madisoncity.k12.al.us"_s),
        .behaviors = { needsFormControlToBeMouseFocusableQuirk } },

    // mailchimp.com rdar://47868965
    { .match = URLMatch::domain("mailchimp.com"_s),
        .behaviors = { shouldDisablePointerEventsQuirk } },

    { .match = URLMatch::domain("marcus.com"_s),
        .behaviors = {
            // Marcus: <rdar://101086391>.
            shouldExposeShowModalDialog,
            // marcus.com rdar://102959860
            shouldNavigatorPluginsBeEmpty,
        } },

    // medium.com rdar://50457837
    { .match = URLMatch::domain("medium.com"_s),
        .behaviors = { shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk } },

    // m365.cloud.microsoft rdar://157794706
    { .match = URLMatch::hostOrSubdomainOf("m365.cloud.microsoft"_s),
        .behaviors = { shouldAllowPopupFromMicrosoftOfficeToOneDrive } },

    // safe.menlosecurity.com rdar://135114489
    { .match = URLMatch::host("safe.menlosecurity.com"_s),
        .behaviors = { shouldDisableWritingSuggestionsByDefaultQuirk } },

    { .match = URLMatch::domain("messenger.com"_s),
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
    { .match = URLMatch::domain("nba.com"_s),
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(nbaSeekBarFixScript)),
        },
        .site = QuirkSite::NBA,
        .isAvailable = iOSFamily },

    { .match = URLMatch::domain("nba.com"_s).when(smallScreen()),
        .behaviors = { shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen },
        .isAvailable = iOS },

    // mybinder.org rdar://51770057
    { .match = URLMatch::domain("mybinder.org"_s),
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(onDockerPanelTabBar)) },
        .site = QuirkSite::MyBinder,
        .isAvailable = touchEvents || touchEventRegions },

    // naver.com rdar://48068610
    { .match = URLMatch::hostOrSubdomainOf("naver.com"_s).exceptWhen(hostIs(naverHostsWithoutSimulatedMouseEvents)),
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk } },

    { .match = URLMatch::domain("netflix.com"_s),
        .behaviors = {
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
            needsSeekingSupportDisabledQuirk,
            // netflix.com rdar://178545839
            needsNetflixVolumeSliderQuirk,
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=304608
            shouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick,
        },
        .site = QuirkSite::Netflix },

    { .match = URLMatch::domain("netflix.com"_s),
        .behaviors = { needsNowPlayingFullscreenSwapQuirk },
        .isAvailable = vision },

    { .match = URLMatch::domain("nfl.com"_s),
        .behaviors = { shouldSuppressHLSSubtitles } },

    { .match = URLMatch::domain("nhl.com"_s),
        .behaviors = { needsWebKitMediaTextTrackDisplayQuirk } },

    // nytimes.com: rdar://problem/5976384
    { .match = URLMatch::domain("nytimes.com"_s),
        .behaviors = { shouldSilenceWindowResizeEventsDuringApplicationSnapshotting },
        .isAvailable = iOS || vision },

    // Pandora: <rdar://100243111>.
    { .match = URLMatch::domain("pandora.com"_s),
        .behaviors = { shouldExposeShowModalDialog } },

    // pinterest.com rdar://104979314
    // FIXME: Remove this Quirk if Pinterest decides to trigger this notification from an user gesture (rdar://165745719)
    { .match = URLMatch::domain("pinterest.com"_s),
        .behaviors = { shouldAllowNotificationPermissionWithoutUserGesture } },

    { .match = URLMatch::domain("premierleague.com"_s),
        .behaviors = {
            // premierleague.com: rdar://123721211
            shouldIgnorePlaysInlineRequirementQuirk,
            // premierleague.com: rdar://68938833
            shouldDispatchPlayPauseEventsOnResume,
            // premierleague.com: rdar://136791737
            shouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
        } },

    // ralphlauren.com rdar://55629493
    { .match = URLMatch::domain("ralphlauren.com"_s),
        .behaviors = { shouldIgnoreAriaForFastPathContentObservationCheckQuirk } },

    // reddit.com: rdar://80550715
    { .match = URLMatch::domain("reddit.com"_s),
        .behaviors = { requiresUserGestureToPauseInPictureInPictureQuirk },
        .site = QuirkSite::Reddit,
        .isAvailable = videoPresentationMode || iOSFamily },

    // reddit.com with Sink It extension: rdar://176377447.
    { .match = URLMatch::domain("reddit.com"_s),
        .behaviors = { shouldDisableScrollAnchoringQuirk },
        .isAvailable = iOSFamily },

    // FIXME: Remove this quirk when <rdar://problem/61733101> is complete.
    { .match = URLMatch::hostOrSubdomainOf("roblox.com"_s),
        .behaviors = { needsIPadMiniUserAgentQuirk } },

    { .match = URLMatch::domain("scribd.com"_s),
        .behaviors = { needsReuseLiveRangeForSelectionUpdateQuirk } },

    // sfusd.edu: rdar://116292738
    { .match = URLMatch::domain("sfusd.edu"_s),
        .behaviors = { shouldBypassAsyncScriptDeferring } },

    // sharepoint.com rdar://52116170
    { .match = URLMatch::domain("sharepoint.com"_s),
        .behaviors = { shouldAvoidResizingWhenInputViewBoundsChangeQuirk } },

    { .match = URLMatch::host("shopee.sg"_s).when(pathIs("/payment/account-linking/landing"_s)),
        .behaviors = { needsIPhoneUserAgentQuirk },
        .isAvailable = iOSFamily },

    { .match = URLMatch::domain("slack.com"_s),
        .behaviors = {
            // slack.com: rdar://138614711
            shouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk,
            // slack.com: rdar://171190689
            shouldUseDynamicViewportUnitsAsDefaultQuirk,
        },
        .isAvailable = iOSFamily },

    { .match = URLMatch::domain("soundcloud.com"_s),
        .behaviors = {
            // soundcloud.com rdar://52915981
            shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
            // Soundcloud: rdar://102913500
            shouldExposeShowModalDialog,
            // soundcloud.com rdar://52915981
            shouldDispatchSimulatedMouseEventsQuirk,
        },
        .site = QuirkSite::SoundCloud },

    // soylent.*: rdar://113314067
    { .match = URLMatch::anyTopLevelDomain("soylent"_s),
        .behaviors = { shouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick } },

    // spotify.com rdar://138918575
    { .match = URLMatch::host("open.spotify.com"_s),
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
    { .match = URLMatch::domain("steampowered.com"_s),
        .behaviors = { shouldTreatAddingMouseOutEventListenerAsContentChange } },

    { .match = URLMatch::anyTopLevelDomain("theguardian"_s),
        .behaviors = { shouldHideSoftTopScrollEdgeEffectDuringFocusQuirk } },

    // theguardian.com rdar://166727225
    { .match = QuirkURLMatch::embeddedDocumentInTopMatch(URLMatch::anyTopLevelDomain("theguardian"_s), URLMatch::domain(youTubeEmbedDomains)),
        .behaviors = { needsYouTubeEmbedAutoplayQuirk } },

    // teams.live.com rdar://88678598
    // teams.microsoft.com rdar://90434296
    { .match = URLMatch::host(microsoftTeamsHosts),
        .behaviors = { shouldAllowMSTeamsProtocolWithoutUserGestureQuirk } },

    // teams.microsoft.com https://bugs.webkit.org/show_bug.cgi?id=219505
    { .match = URLMatch::host("teams.microsoft.com"_s).when(queryContains("Retried+3+times+without+success"_s)),
        .behaviors = { isMicrosoftTeamsRedirectURLQuirk } },

    { .match = URLMatch::domain("thesaurus.com"_s),
        .behaviors = { needsAnchorToBeMouseFocusableQuirk } },

    // player.anyclip.com rdar://138789765
    { .match = URLMatch::domain("thesaurus.com"_s),
        .behaviors = { needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(chromeUserAgentScript, anyclipPlayerScriptURL)) },
        .isAvailable = iOSFamily },

    { .match = URLMatch::domain("tiktok.com"_s),
        .behaviors = {
            needsTikTokOverflowingContentQuirk,
            // tiktok.com rdar://174179805
            shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
            // tiktok.com rdar://174179805
            shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(onSliderRole)),
        },
        .site = QuirkSite::TikTok },

    // trix-editor.org rdar://28242210
    { .match = URLMatch::domain("trix-editor.org"_s),
        .behaviors = { isNeverRichlyEditableForTouchBarQuirk } },

    // twitch.tv rdar://102420527
    { .match = URLMatch::domain("twitch.tv"_s),
        .behaviors = { shouldReportDocumentAsVisibleIfActivePIPQuirk } },

    // https://tympanus.net/Tutorials/WebGPUFluid/ does not load (rdar://143839620).
    { .match = URLMatch::domain("tympanus.net"_s),
        .behaviors = { shouldBlockFetchWithNewlineAndLessThan } },

    // uhc.com rdar://173206598
    { .match = URLMatch::domain("uhc.com"_s),
        .behaviors = { shouldTranscodeHeicImagesQuirk } },

    // unifi.ui.com rdar://180411019
    { .match = URLMatch::domain("ui.com"_s),
        .behaviors = { needsSupportsProgressMonitoringQuirk } },

    // Breaks express checkout on victoriassecret.com (rdar://104818312).
    { .match = URLMatch::domain("victoriassecret.com"_s),
        .behaviors = { shouldDisableFetchMetadata } },

    { .match = URLMatch::domain("vimeo.com"_s),
        .behaviors = {
            // vimeo.com rdar://56996057
            maybeBypassBackForwardCache,
            // vimeo.com rdar://55759025
            needsPreloadAutoQuirk,
            // vimeo.com: rdar://problem/73227900
            shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
            // vimeo.com: rdar://107592139
            blocksEnteringStandardFullscreenFromPictureInPictureQuirk,
            // vimeo.com: rdar://problem/70788878
            blocksReturnToFullscreenFromPictureInPictureQuirk,
        },
        .site = QuirkSite::Vimeo },

    // rdar://116531089
    { .match = URLMatch::domain("vimeo.com"_s).when(smallScreen()),
        .behaviors = { shouldDisableElementFullscreenQuirk } },

    // walmart.com: rdar://123734840
    { .match = URLMatch::domain("walmart.com"_s),
        .behaviors = {
            mayNeedToIgnoreContentObservation,
        },
        .site = QuirkSite::Walmart,
        .isAvailable = twoPhaseClicks },

    // weather.com rdar://139689157
    { .match = URLMatch::domain("weather.com"_s),
        .behaviors = { needsFormControlToBeMouseFocusableQuirk } },

    { .match = URLMatch::domain("webex.com"_s),
        .behaviors = {
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(webExUndefinedTouchScript, webExPushDownloadScriptURL)),
            // webex.com rdar://143715630
            needsWebExScrollabilityQuirk,
        },
        .isAvailable = iOSFamily && desktopContentModeQuirks },

    // weebly.com rdar://48003980
    { .match = URLMatch::domain("weebly.com"_s),
        .behaviors = { shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk } },

    { .match = URLMatch::domain("wikipedia.org"_s),
        .behaviors = {
            // wikipedia.org rdar://54856323
            shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk,
            // wikipedia.org https://webkit.org/b/247636
            shouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk,
        } },

    // rdar://170412045, https://bugs.webkit.org/show_bug.cgi?id=307933
    // wix.com rdar://49124313, except while picking a template.
    { .match = URLMatch::domain("wix.com"_s).exceptWhen(pathStartsWith("/website/templates/"_s)),
        .behaviors = { shouldDispatchSimulatedMouseEventsQuirk } },

    { .match = URLMatch::domain("workspaces.xyz"_s),
        .behaviors = { shouldComparareUsedValuesForBorderWidthForTriggeringTransitions } },

    // wpdevelopment.ca rdar://156109518
    { .match = URLMatch::domain("wpdevelopment.ca"_s),
        .behaviors = { needsFormControlToBeMouseFocusableQuirk } },

    { .match = URLMatch::domain("x.com"_s),
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
            needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirkParameters::fromScript(xGoogleSignInButtonFixScript, googleSignInClientScriptURL)),
        } },

    { .match = URLMatch::domain("x.com"_s),
        .behaviors = {
            // x.com: rdar://problem/58804852 and rdar://problem/61731801
            shouldSilenceWindowResizeEventsDuringApplicationSnapshotting,
            // x.com: rdar://175565114
            shouldAvoidProgrammaticScrollClampingQuirk,
        },
        .isAvailable = iOS || vision },

    { .match = URLMatch::anyTopLevelDomain("yahoo"_s),
        .behaviors = {
            // yahoo.com: rdar://170502516
            needsYahooVolumeSliderQuirk,
            // yahoo.com: rdar://136767005
            shouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
        } },

    // yahoo.com: rdar://148284059
    { .match = QuirkURLMatch::embeddedDocumentInTopMatch(URLMatch::anyTopLevelDomain("yahoo"_s), URLMatch::domain("yimg.com"_s)),
        .behaviors = { requiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk } },

    // yahoo.com : rdar://142894603
    { .match = URLMatch::anyTopLevelDomain("yahoo"_s),
        .behaviors = { shouldPreventDispatchOfTouchEventQuirk },
        .isAvailable = touchEvents },

    // news.ycombinator.com: rdar://127246368
    { .match = URLMatch::host("news.ycombinator.com"_s),
        .behaviors = { shouldIgnoreTextAutoSizingQuirk } },

    { .match = URLMatch::domain("youtube.com"_s),
        .behaviors = {
            // youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
            hasBrokenEncryptedMediaAPISupportQuirk,
            // youtube.com rdar://135886305
            needsScrollbarWidthThinDisabledQuirk,
            needsYouTubeCaptionQuirk,
            // youtube.com: rdar://110097836
            shouldSilenceResizeObservers,
        } },

    // Embedded youtube.com players need the caption quirk regardless of the embedding site.
    { .match = QuirkURLMatch::embeddedDocument(URLMatch::domain(youTubeEmbedDomains)),
        .behaviors = { needsYouTubeCaptionQuirk } },

    // YouTube.com does not provide AirPlay controls in fullscreen
    // (Ref: rdar://121471373)
    { .match = URLMatch::domain("youtube.com"_s).when(smallScreen()),
        .behaviors = { shouldDisableElementFullscreenQuirk } },

    // tiny. (Ref: rdar://121471373, rdar://121473410)
    { .match = QuirkURLMatch::embeddedDocument(URLMatch::domain(youTubeEmbedDomains).when(smallScreen())),
        .behaviors = { shouldDisableElementFullscreenQuirk } },

    { .match = QuirkURLMatch::embeddedDocument(URLMatch::domain("x.com"_s)),
        .behaviors = { shouldDisableElementFullscreenQuirk } },

    // youtube.com rdar://49582231
    { .match = URLMatch::host("www.youtube.com"_s),
        .behaviors = { needsYouTubeOverflowScrollQuirk } },

    { .match = URLMatch::domain("youtube.com"_s).when(tubularApp()),
        .behaviors = { shouldSuppressMediaSessionPauseActionOnInterruption },
        .isAvailable = iOSFamily },

    // www.youtube.com rdar://52361019
    { .match = URLMatch::host("www.youtube.com"_s),
        .behaviors = { needsYouTubeMouseOutQuirk } },

    // Lens.app rdar://178769976
    { .match = URLMatch::domain("youtube.com"_s).when(lensApp()),
        .behaviors = { requiresUserGestureToPlayInFullscreenQuirk },
        .isAvailable = vision },

    // zencastr.com rdar://143087016
    { .match = URLMatch::domain("zencastr.com"_s),
        .behaviors = { needsLimitedMatroskaSupportQuirk } },

    // zillow.com rdar://53103732
    { .match = URLMatch::host("www.zillow.com"_s),
        .behaviors = { shouldAvoidScrollingWhenFocusedContentIsVisibleQuirk } },

    { .match = URLMatch::domain("zillow.com"_s),
        .behaviors = {
            // zillow.com rdar://79872092
            shouldTranscodeHeicImagesQuirk,
            // zillow.com rdar://110097836
            shouldSilenceResizeObservers,
        } },

    { .match = URLMatch::domain("zomato.com"_s),
        .behaviors = { needsZomatoEmailLoginLabelQuirk } },

    { .match = URLMatch::domain("zoom.us"_s),
        .behaviors = {
            // zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
            shouldAutoplayWebAudioForArbitraryUserGestureQuirk,
            // zoom.us rdar://118185086
            shouldDisableImageCaptureQuirk,
            shouldAllowMediaStreamTrackSerializationQuirk,
        } },
};

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
            }

            if (behavior.elementSelectorCondition && !behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::ElementSelector))
                return false;
        }
    }

    return true;
}

static_assert(everyQuirkCarriesWhatItDeclares(), "A quirk in fullTable does not supply the parameters it declares, supplies parameters it does not declare, or applies a condition the behavior does not support");

consteval bool shouldEmit(const Quirk& quirk)
{
    if (!quirk.isAvailable)
        return false;

    const bool quirkWillSetSiteOnQuirkData = quirk.site.has_value();
    if (quirkWillSetSiteOnQuirkData)
        return true;

    return !quirk.behaviors.span().empty();
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

bool QuirkURLMatch::matches(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument isTopDocument) const
{
    switch (m_kind) {
    case Kind::TopURL:
        return m_match.matches(topContext);
    case Kind::EmbeddedDocument:
        if (isTopDocument == IsTopDocument::Yes)
            return false;
        return m_match.matches(documentContext);
    case Kind::EmbeddedDocumentInTopURL:
        if (isTopDocument == IsTopDocument::Yes)
            return false;
        ASSERT(m_topMatch);
        return m_topMatch->matches(topContext) && m_match.matches(documentContext);
    }

    ASSERT_NOT_REACHED();
    return false;
}

void Quirk::apply(QuirksData& quirksData) const
{
    for (const auto& behavior : behaviors.span())
        quirksData.addBehavior(behavior);

    if (site)
        quirksData.addSite(*site);
}

static QuirksData resolveSiteSpecificQuirks(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument documentIsTopDocument)
{
    QuirksData quirksData;
    for (auto& quirk : SiteSpecificQuirks::table) {
        if (quirk.match.matches(topContext, documentContext, documentIsTopDocument))
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

} // namespace WebCore
