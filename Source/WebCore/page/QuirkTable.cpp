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

#include <array>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

#if PLATFORM(IOS_FAMILY)
// nba.com rdar://147429596
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
#endif

// ceac.state.gov rdar://170258502
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

static constexpr std::array bbcDomains { "bbc.co.uk"_s, "bbc.com"_s };
static constexpr std::array expediaGroupDomains {
    "carrentals.com"_s, "cheaptickets.com"_s, "hoteis.com"_s, "hoteles.com"_s,
    "hotels.com"_s, "mrjet.se"_s, "orbitz.com"_s, "travelocity.ca"_s,
    "travelocity.com"_s, "wotif.co.nz"_s, "wotif.com"_s
};
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
static constexpr std::array naverHostsWithoutSimulatedMouseEvents { "tv.naver.com"_s, "mail.naver.com"_s, "m.naver.com"_s };
#endif
#if PLATFORM(COCOA)
static constexpr std::array youTubeEmbedDomains { "youtube.com"_s, "youtube-nocookie.com"_s };
#endif
#if PLATFORM(IOS_FAMILY)
static constexpr std::array claudeDomains { "claude.ai"_s, "claude.com"_s };
#endif

namespace SiteSpecificQuirks {
using enum SiteSpecificQuirk;
using namespace ParameterizedQuirk;
using namespace QuirkRefinement;

#if PLATFORM(IOS_FAMILY)
// The AnyClip player only needs the quirk for the script that sniffs the user agent.
static constexpr auto anyClipPlayerScript = QuirkMatch::host("player.anyclip.com"_s).when(lastPathComponentEndsWith("lre.js"_s));

static constexpr auto chromeUserAgentScript = "(function() { let userAgent = navigator.userAgent; Object.defineProperty(navigator, 'userAgent', { get: () => { return userAgent + ' Chrome/130.0.0.0 Android/15.0'; }, configurable: true }); })();"_s;

// dictionary.com and thesaurus.com both embed the AnyClip player. player.anyclip.com rdar://138789765
// nba.com rdar://147429596
#endif

static Vector<Quirk> makeTable()
{
    return {
#if PLATFORM(IOS) || PLATFORM(VISION)
    // 365scores.com rdar://116491386
    { .topMatch = QuirkMatch::domain("365scores.com"_s),
        .behaviors = { ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting } },
#endif

#if ENABLE(MEDIA_STREAM)
    // actesting.org rdar://124017544
    { .topMatch = QuirkMatch::domain("actesting.org"_s),
        .behaviors = { ShouldEnableLegacyGetUserMediaQuirk } },
#endif

    // airindiaexpress.com https://webkit.org/b/317375
    { .topMatch = QuirkMatch::domain("airindiaexpress.com"_s),
        .behaviors = { NeedsAirIndiaExpressLayeringQuirk } },

    // Note: There is a userAgent override for rdar://117771731, see needsCustomUserAgentOverride()
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    // airtable.com rdar://49124313
    { .topMatch = QuirkMatch::domain("airtable.com"_s),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk } },
#endif

    { .topMatch = QuirkMatch::anyTopLevelDomain("amazon"_s),
        .behaviors = {
            // amazon.com rdar://49124529
            ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
#if PLATFORM(MAC)
            // amazon.com rdar://128962002
            NeedsPrimeVideoUserSelectNoneQuirk,
#endif
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
            // amazon.com rdar://49124313
            ShouldDispatchSimulatedMouseEventsQuirk,
#endif
        },
        .site = QuirkSite::Amazon },

#if PLATFORM(IOS_FAMILY)
    { .topMatch = QuirkMatch::domain("amazon.design"_s),
        .behaviors = { NeedsAmazonDesignMenuViewportUnitQuirk } },
#endif

    // apple.com rdar://154434137
    // FIXME: Maybe EnsureCaptionVisibilityInFullscreenAndPictureInPicture should apply to apple.com.cn too?
    { .topMatch = QuirkMatch::domain("apple.com"_s),
        .behaviors = { EnsureCaptionVisibilityInFullscreenAndPictureInPicture } },

    // Quirk added for rdar://181007316, remove when rdar://182134549 is fixed.
    { .topMatch = QuirkMatch::anyTopLevelDomain("apple"_s).when(pathContains("/retail"_s)),
        .behaviors = { ShouldDisableScrollAnchoringQuirk } },

#if PLATFORM(IOS_FAMILY)
    // as.com: rdar://121014613
    { .topMatch = QuirkMatch::domain("as.com"_s).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // att.com rdar://55185021
    { .topMatch = QuirkMatch::domain("att.com"_s),
        .behaviors = { ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk } },
#endif

    // Login issue on bankofamerica.com (rdar://104938789).
    { .topMatch = QuirkMatch::domain("bankofamerica.com"_s),
        .behaviors = {
            MaybeBypassBackForwardCache,
        },
        .site = QuirkSite::BankOfAmerica },

    // bbc.co.uk rdar://126494734
    // bbc.com rdar://157499149
    { .topMatch = QuirkMatch::domain(bbcDomains),
        .behaviors = { ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk } },

    // bestbuy.com rdar://136235936
    { .topMatch = QuirkMatch::domain("bestbuy.com"_s),
        .behaviors = {
            EvaluateScriptBeforeRunningScript::create("Object.defineProperty(navigator,'language',{get:function(){return'en-US'}});Object.defineProperty(navigator,'languages',{get:function(){return['en-US','en']}});"_s),
        } },

    { .topMatch = QuirkMatch::domain("bing.com"_s),
        .behaviors = {
            // bing.com rdar://133223599
            MaybeBypassBackForwardCache,
            // bing.com rdar://126573838
            NeedsMediaRewriteRangeRequestQuirk,
        },
        .site = QuirkSite::Bing },

    // bungalow.com rdar://61658940
    { .topMatch = QuirkMatch::domain("bungalow.com"_s),
        .behaviors = { ShouldBypassAsyncScriptDeferring } },

    { .topMatch = QuirkMatch::domain("capitalgroup.com"_s),
        .behaviors = { ShouldDelayReloadWhenRegisteringServiceWorker } },

#if PLATFORM(IOS_FAMILY)
    // Remove this once rdar://139478801 is resolved.
    { .topMatch = QuirkMatch::domain("cbssports.com"_s),
        .behaviors = {
            ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk,
        },
        .site = QuirkSite::CBSSports },
#endif

    { .topMatch = QuirkMatch::hostOrSubdomainOf("ceac.state.gov"_s),
        .behaviors = {
#if PLATFORM(MAC)
            // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
            NeedsFormControlToBeMouseFocusableQuirk,
#endif
            // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=311383
            EvaluateScriptBeforeRunningScript::create(ceacBeforeUnloadFixScript, QuirkMatch::anyURL().when(lastPathComponentIs("CheckBrowserClose.js"_s))),
        } },
#if PLATFORM(IOS)
    { .topMatch = QuirkMatch::domain("chess.com"_s).when(smallScreen()),
        .behaviors = { ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen } },
#endif
    { .topMatch = QuirkMatch::domain("claude.ai"_s),
        .behaviors = {
#if PLATFORM(IOS_FAMILY)
            NeedsClaudeSidebarViewportUnitQuirk,
#endif
            // rdar://174779259 - logout flow leaves identification cookies
            // causing redirect loop on next /chat boot.
            // See Quirks::clearLogoutSurvivingIdentityCookiesIfNeeded().
            NeedsLogoutCookieCleanupQuirk,
        } },

#if PLATFORM(IOS_FAMILY)
    { .topMatch = QuirkMatch::domain(claudeDomains),
        .behaviors = { NeedsHideSelectionDuringOverflowScrollQuirk } },
#endif

#if PLATFORM(IOS_FAMILY)
    { .topMatch = QuirkMatch::domain("cnn.com"_s),
        .behaviors = {
            // cnn.com rdar://119640248
            NeedsFullscreenObjectFitQuirk,
#if PLATFORM(COCOA)
            NeedsCNNCaptionQuirk,
#endif
#if ENABLE(THREADED_ANIMATIONS)
            // cnn.com rdar://176539646
            ShouldDisableThreadedAnimationsQuirk,
#endif
        } },
#endif

#if ENABLE(MEDIA_STREAM)
    { .topMatch = QuirkMatch::host("codepen.io"_s),
        .behaviors = { ShouldEnableSpeakerSelectionPermissionsPolicyQuirk } },
#endif

    { .topMatch = QuirkMatch::domain("crunchyroll.com"_s),
        .behaviors = { NeedsSuppressPostLayoutBoundaryEventsQuirk } },

    { .topMatch = QuirkMatch::domain("dailymail.co.uk"_s),
        .behaviors = { ShouldUnloadHeavyFrames } },

    { .topMatch = QuirkMatch::host("digits.t-mobile.com"_s),
        .behaviors = { NeedsNavigatorUserAgentDataQuirk, NeedsCustomUserAgentData } },

    // descript.com rdar://156024693
    { .topMatch = QuirkMatch::domain("descript.com"_s),
        .behaviors = { ShouldDisableDOMAudioSession } },

    { .topMatch = QuirkMatch::domain("dictionary.com"_s),
        .behaviors = {
#if PLATFORM(IOS_FAMILY)
            EvaluateScriptBeforeRunningScript::create(chromeUserAgentScript, anyClipPlayerScript),
#endif
#if PLATFORM(COCOA)
            NeedsAnchorToBeMouseFocusableQuirk,
#endif
        } },

#if PLATFORM(IOS_FAMILY)
    // digitaltrends.com rdar://121014613
    { .topMatch = QuirkMatch::domain("digitaltrends.com"_s).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // discord.com rdar://162719481
    { .topMatch = QuirkMatch::domain("discord.com"_s),
        .behaviors = { ShouldUseLayoutViewportForClientRectsQuirk } },

    { .topMatch = QuirkMatch::domain("disneyplus.com"_s),
        .behaviors = {
            // disneyplus rdar://137613110
            ShouldHideCoarsePointerCharacteristicsQuirk,
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
            // disneyplus rdar://151715964
            NeedsZeroMaxTouchPointsQuirk,
#endif
        } },
#endif

    { .topMatch = QuirkMatch::domain("ea.com"_s),
        .site = QuirkSite::EA },

    { .topMatch = QuirkMatch::domain("espn.com"_s),
        .behaviors = {
#if PLATFORM(IOS)
            // espn.com rdar://184169028
            NeedsSuppressedPauseEventOnFullscreenExitQuirk,
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
            // espn.com rdar://problem/95651814
            AllowLayeredFullscreenVideos,
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
            // espn.com rdar://problem/73227900
            ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
#endif
        } },

    // Expedia Group rdar://126631968
    { .topMatch = QuirkMatch::domain(expediaGroupDomains),
        .behaviors = { NeedsExpediaGroupAnimationQuirk } },
    { .topMatch = QuirkMatch::anyTopLevelDomain("ebookers"_s),
        .behaviors = { NeedsExpediaGroupAnimationQuirk } },
    { .topMatch = QuirkMatch::anyTopLevelDomain("expedia"_s),
        .behaviors = { NeedsExpediaGroupAnimationQuirk } },

    { .topMatch = QuirkMatch::domain("facebook.com"_s),
        .behaviors = {
            // facebook.com rdar://100871402
            NeedsFacebookRemoveNotSupportedQuirk,
            // facebook.com rdar://174179871
            ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
#if ENABLE(VIDEO_PRESENTATION_MODE)
            // facebook.com rdar://67273166
            RequiresUserGestureToPauseInPictureInPictureQuirk,
#endif
#if ENABLE(MEDIA_STREAM)
            // facebook.com rdar://158736355
            ShouldEnableCameraAndMicrophonePermissionStateQuirk,
            ShouldEnableRemoteTrackLabelQuirk,
            // facebook.com rdar://41104397
            ShouldEnableFacebookFlagQuirk,
            // facebook.com rdar://161269819
            ShouldEnableEnumerateDeviceQuirk,
#endif
#if ENABLE(WEB_RTC)
            // facebook.com rdar://158736355
            ShouldEnableRTCEncodedStreamsQuirk,
#endif
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
            // facebook.com rdar://174179871
            ShouldDispatchSimulatedMouseEventsQuirk,
#endif
        },
        .site = QuirkSite::Facebook },

#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    // flipkart.com rdar://49648520
    { .topMatch = QuirkMatch::domain("flipkart.com"_s),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk } },
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    // forbes.com rdar://67273166
    { .topMatch = QuirkMatch::domain("forbes.com"_s),
        .behaviors = { RequiresUserGestureToPauseInPictureInPictureQuirk } },
#endif

    { .topMatch = QuirkMatch::host("play.geforcenow.com"_s),
        .behaviors = { NeedsGeforcenowWarningDisplayNoneQuirk } },

#if PLATFORM(IOS_FAMILY)
    // gizmodo.com rdar://102227302
    { .topMatch = QuirkMatch::domain("gizmodo.com"_s),
        .behaviors = { NeedsFullscreenDisplayNoneQuirk } },
#endif

    { .topMatch = QuirkMatch::anyTopLevelDomain("google"_s),
        .behaviors = {
            // docs.google.com rdar://59893415
            MaybeBypassBackForwardCache,
        },
        .site = QuirkSite::GoogleProperty },

    { .topMatch = QuirkMatch::anyTopLevelDomain("google"_s).when(pathStartsWith("/maps/"_s)),
        .behaviors = {
#if ENABLE(TWO_PHASE_CLICKS)
            // maps.google.com rdar://152194074
            MayNeedToIgnoreContentObservation,
#endif
#if PLATFORM(IOS_FAMILY)
            // maps.google.com rdar://67358928
            NeedsGoogleMapsScrollingQuirk,
#endif
            // maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
            ShouldAvoidResizingWhenInputViewBoundsChangeQuirk,
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
            // maps.google.com rdar://49124313
            ShouldDispatchSimulatedMouseEventsQuirk,
#endif
        },
        .site = QuirkSite::GoogleMaps },

    { .topMatch = QuirkMatch::host("docs.google.com"_s),
        .behaviors = {
            InputMethodUsesCorrectKeyEventOrder,
#if PLATFORM(MAC)
            InputMethodMustUseCompositionEvents,
            // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
            IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk,
#endif
#if PLATFORM(IOS_FAMILY)
            // docs.google.com rdar://49864669
            ShouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk,
#endif
        },
        .site = QuirkSite::GoogleDocs },

#if PLATFORM(IOS_FAMILY)
    // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
    { .topMatch = QuirkMatch::host("docs.google.com"_s).when(pathStartsWith("/spreadsheets/"_s)),
        .behaviors = { NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk } },

    { .topMatch = QuirkMatch::host("docs.google.com"_s).when(pathStartsWith("/presentation/"_s)),
        .behaviors = { ShouldIgnoreInputModeNone } },

    // mail.google.com rdar://49403416
    { .topMatch = QuirkMatch::host("mail.google.com"_s),
        .behaviors = { NeedsGMailOverflowScrollQuirk } },

    { .topMatch = QuirkMatch::host("translate.google.com"_s),
        .behaviors = {
            // translate.google.com rdar://106539018
            NeedsGoogleTranslateScrollingQuirk,
            EvaluateScriptBeforeRunningScript::create("(function() { let userAgent = navigator.userAgent; Object.defineProperty(navigator, 'userAgent', { get: () => { return userAgent + ' Chrome/130.0.0.0 Android/15.0'; }, configurable: true }); })();"_s),
        } },
#endif

#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    // sites.google.com rdar://58653069
    { .topMatch = QuirkMatch::host("sites.google.com"_s),
        .behaviors = { ShouldPreventDispatchOfTouchEventQuirk } },
#endif

#if ENABLE(MEDIA_STREAM)
    { .topMatch = QuirkMatch::host("meet.google.com"_s),
        .behaviors = { ShouldEnableCameraBackgroundPlayback } },
#endif

    // hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
    { .topMatch = QuirkMatch::domain("hbomax.com"_s),
        .behaviors = { ShouldEnableFontLoadingAPIQuirk } },

    { .topMatch = QuirkMatch::host("play.hbomax.com"_s),
        .behaviors = {
#if HAVE(PIP_SKIP_PREROLL)
            // play.hbomax.com rdar://158430821
            ShouldDisableAdSkippingInPip,
#endif
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
            // hbomax.com: rdar://138424489
            NeedsZeroMaxTouchPointsQuirk,
            // hbomax.com: rdar://138806698
            ShouldSupportHoverMediaQueriesQuirk,
#endif
        } },

    { .topMatch = QuirkMatch::domain("hulu.com"_s),
        .behaviors = {
            // hulu.com rdar://55041979
            NeedsCanPlayAfterSeekedQuirk,
            // hulu.com rdar://100199996
            NeedsVideoShouldMaintainAspectRatioQuirk,
            // hulu.com rdar://126096361
            ImplicitMuteWhenVolumeSetToZero,
        } },

#if PLATFORM(IOS_FAMILY)
    // icloud.com rdar://131836301
    { .topMatch = QuirkMatch::domain("icloud.com"_s).when(pathOrFragmentContains("mail"_s)),
        .behaviors = { ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting } },
#endif
#if PLATFORM(MAC)
    // icloud.com rdar://26013388
    { .topMatch = QuirkMatch::domain("icloud.com"_s).when(pathOrFragmentContains("notes"_s)),
        .behaviors = { IsNeverRichlyEditableForTouchBarQuirk } },
#endif

    // iheart.com rdar://171198911
    { .topMatch = QuirkMatch::domain("iheart.com"_s),
        .behaviors = {
            EvaluateScriptBeforeRunningScript::create("document.cookie = 'app=listen:60; path=/; domain=.iheart.com';"_s),
        } },

    { .topMatch = QuirkMatch::domain("imdb.com"_s),
        .behaviors = {
            // imdb.com: rdar://137991466
            NeedsChromeMediaControlsPseudoElementQuirk,
            // imdb.com: rdar://162684936
            NeedsZeroMaxTouchPointsQuirk,
        } },

    { .topMatch = QuirkMatch::domain("instagram.com"_s),
        .behaviors = {
            // rdar://166400170
            NeedsInstagramResizingReelsQuirk,
#if PLATFORM(IOS_FAMILY)
            // instagram.com: rdar://174936655
            ShouldSendFakeTouchForceChangeEvent,
#endif
        } },

#if PLATFORM(IOS_FAMILY)
    // instagram.com rdar://121014613
    { .topMatch = QuirkMatch::domain("instagram.com"_s),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },
#endif

    // invideo.io rdar://171741842 https://webkit.org/b/311602
    { .topMatch = QuirkMatch::domain("invideo.io"_s),
        .behaviors = {
            EvaluateScriptBeforeRunningScript::create("if(!window.chrome)window.chrome={};"_s),
        } },

    { .topMatch = QuirkMatch::domain("linkedin.com"_s),
        .site = QuirkSite::LinkedIn },

    { .topMatch = QuirkMatch::domain("live.com"_s),
        .behaviors = {
#if PLATFORM(IOS_FAMILY)
            // live.com: rdar://167489768
            NeedsChromeOSNavigatorUserAgentQuirk,
#endif
            // live.com rdar://52116170
            ShouldAvoidResizingWhenInputViewBoundsChangeQuirk,
        } },

    { .topMatch = QuirkMatch::host("outlook.live.com"_s),
        .behaviors = {
            // outlook.live.com: rdar://136624720
            NeedsMozillaFileTypeForDataTransferQuirk,
#if ENABLE(TWO_PHASE_CLICKS)
            // outlook.live.com: rdar://152277211
            MayNeedToIgnoreContentObservation,
#endif
#if ENABLE(TOUCH_EVENTS)
            // outlook.live.com rdar://48008837
            ShouldPreventDispatchOfTouchEventQuirk,
#endif
        },
        .site = QuirkSite::Outlook },

    // Microsoft office online generates data URLs with incorrect padding on Safari only (rdar://114573089).
    { .topMatch = QuirkMatch::hostOrSubdomainOf("officeapps.live.com"_s),
        .behaviors = { ShouldDisableDataURLPaddingValidation } },
    { .topMatch = QuirkMatch::hostOrSubdomainOf("onedrive.live.com"_s),
        .behaviors = { ShouldDisableDataURLPaddingValidation } },

#if PLATFORM(MAC)
    // onedrive.live.com rdar://26013388
    { .topMatch = QuirkMatch::host("onedrive.live.com"_s),
        .behaviors = { IsNeverRichlyEditableForTouchBarQuirk } },

    // madisoncity.k12.al.us https://bugs.webkit.org/show_bug.cgi?id=296989
    { .topMatch = QuirkMatch::domain("madisoncity.k12.al.us"_s),
        .behaviors = { NeedsFormControlToBeMouseFocusableQuirk } },
#endif

#if PLATFORM(IOS_FAMILY)
    // mailchimp.com rdar://47868965
    { .topMatch = QuirkMatch::domain("mailchimp.com"_s),
        .behaviors = { ShouldDisablePointerEventsQuirk } },
#endif

    { .topMatch = QuirkMatch::domain("marcus.com"_s),
        .behaviors = {
            // Marcus: <rdar://101086391>.
            ShouldExposeShowModalDialog,
#if PLATFORM(IOS_FAMILY)
            // marcus.com rdar://102959860
            ShouldNavigatorPluginsBeEmpty,
#endif
        } },

    // medium.com rdar://50457837
    { .topMatch = QuirkMatch::domain("medium.com"_s),
        .behaviors = { ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk } },

#if PLATFORM(IOS_FAMILY)
    // m365.cloud.microsoft rdar://157794706
    { .topMatch = QuirkMatch::hostOrSubdomainOf("m365.cloud.microsoft"_s),
        .behaviors = { ShouldAllowPopupFromMicrosoftOfficeToOneDrive } },
#endif

    // safe.menlosecurity.com rdar://135114489
    { .topMatch = QuirkMatch::host("safe.menlosecurity.com"_s),
        .behaviors = { ShouldDisableWritingSuggestionsByDefaultQuirk } },

    { .topMatch = QuirkMatch::domain("messenger.com"_s),
        .behaviors = {
#if ENABLE(MEDIA_STREAM)
            // facebook.com rdar://158736355
            ShouldEnableCameraAndMicrophonePermissionStateQuirk,
            ShouldEnableRemoteTrackLabelQuirk,
            // facebook.com rdar://161269819
            ShouldEnableEnumerateDeviceQuirk,
#endif
#if ENABLE(WEB_RTC)
            // facebook.com rdar://158736355
            ShouldEnableRTCEncodedStreamsQuirk,
#endif
        } },

#if PLATFORM(IOS_FAMILY)
    // rdar://147429596
    { .topMatch = QuirkMatch::domain("nba.com"_s),
        .behaviors = {
            EvaluateScriptBeforeRunningScript::create(nbaSeekBarFixScript),
        },
        .site = QuirkSite::NBA },
#if PLATFORM(IOS)
    { .topMatch = QuirkMatch::domain("nba.com"_s).when(smallScreen()),
        .behaviors = { ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen } },
#endif
#endif

#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    // mybinder.org rdar://51770057
    { .topMatch = QuirkMatch::domain("mybinder.org"_s),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk },
        .site = QuirkSite::MyBinder },

    // naver.com rdar://48068610
    { .topMatch = QuirkMatch::hostOrSubdomainOf("naver.com"_s).exceptWhen(hostIs(naverHostsWithoutSimulatedMouseEvents)),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk } },
#endif

    { .topMatch = QuirkMatch::domain("netflix.com"_s),
        .behaviors = {
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
            NeedsSeekingSupportDisabledQuirk,
#if PLATFORM(VISION)
            NeedsNowPlayingFullscreenSwapQuirk,
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
            // netflix.com rdar://178545839
            NeedsNetflixVolumeSliderQuirk,
#endif
#if ENABLE(TOUCH_EVENTS)
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=304608
            ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick,
#endif
        },
        .site = QuirkSite::Netflix },

    { .topMatch = QuirkMatch::domain("nfl.com"_s),
        .behaviors = { ShouldSuppressHLSSubtitles } },

    { .topMatch = QuirkMatch::domain("nhl.com"_s),
        .behaviors = { NeedsWebKitMediaTextTrackDisplayQuirk } },

#if PLATFORM(IOS) || PLATFORM(VISION)
    // nytimes.com: rdar://problem/5976384
    { .topMatch = QuirkMatch::domain("nytimes.com"_s),
        .behaviors = { ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting } },
#endif

    // Pandora: <rdar://100243111>.
    { .topMatch = QuirkMatch::domain("pandora.com"_s),
        .behaviors = { ShouldExposeShowModalDialog } },

    // pinterest.com rdar://104979314
    // FIXME: Remove this Quirk if Pinterest decides to trigger this notification from an user gesture (rdar://165745719)
    { .topMatch = QuirkMatch::domain("pinterest.com"_s),
        .behaviors = { ShouldAllowNotificationPermissionWithoutUserGesture } },

    { .topMatch = QuirkMatch::domain("premierleague.com"_s),
        .behaviors = {
            // premierleague.com: rdar://123721211
            ShouldIgnorePlaysInlineRequirementQuirk,
            // premierleague.com: rdar://68938833
            ShouldDispatchPlayPauseEventsOnResume,
            // premierleague.com: rdar://136791737
            ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
        } },

#if PLATFORM(IOS_FAMILY)
    // ralphlauren.com rdar://55629493
    { .topMatch = QuirkMatch::domain("ralphlauren.com"_s),
        .behaviors = { ShouldIgnoreAriaForFastPathContentObservationCheckQuirk } },
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE) || PLATFORM(IOS_FAMILY)
    { .topMatch = QuirkMatch::domain("reddit.com"_s),
        .behaviors = {
#if ENABLE(VIDEO_PRESENTATION_MODE)
            // reddit.com: rdar://80550715
            RequiresUserGestureToPauseInPictureInPictureQuirk,
#endif
#if PLATFORM(IOS_FAMILY)
            // reddit.com with Sink It extension: rdar://176377447.
            ShouldDisableScrollAnchoringQuirk,
#endif
        },
        .site = QuirkSite::Reddit },
#endif

    { .topMatch = QuirkMatch::domain("scribd.com"_s),
        .behaviors = { NeedsReuseLiveRangeForSelectionUpdateQuirk } },

    // sfusd.edu: rdar://116292738
    { .topMatch = QuirkMatch::domain("sfusd.edu"_s),
        .behaviors = { ShouldBypassAsyncScriptDeferring } },

    // sharepoint.com rdar://52116170
    { .topMatch = QuirkMatch::domain("sharepoint.com"_s),
        .behaviors = { ShouldAvoidResizingWhenInputViewBoundsChangeQuirk } },

#if PLATFORM(IOS_FAMILY) && ENABLE(META_VIEWPORT)
    { .topMatch = QuirkMatch::domain("slack.com"_s),
        .behaviors = {
            // slack.com: rdar://138614711
            ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk,
            // slack.com: rdar://171190689
            ShouldUseDynamicViewportUnitsAsDefaultQuirk,
        } },
#endif

    { .topMatch = QuirkMatch::domain("soundcloud.com"_s),
        .behaviors = {
            // soundcloud.com rdar://52915981
            ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
            // Soundcloud: rdar://102913500
            ShouldExposeShowModalDialog,
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
            // soundcloud.com rdar://52915981
            ShouldDispatchSimulatedMouseEventsQuirk,
#endif
        },
        .site = QuirkSite::SoundCloud },

#if ENABLE(TOUCH_EVENTS)
    // soylent.*: rdar://113314067
    { .topMatch = QuirkMatch::anyTopLevelDomain("soylent"_s),
        .behaviors = { ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick } },
#endif

    // spotify.com rdar://138918575
    { .topMatch = QuirkMatch::host("open.spotify.com"_s),
        .behaviors = {
            NeedsBodyScrollbarWidthNoneDisabledQuirk,
            ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
            ShouldLimitHLSPlaybackRate,
            NeedsWebKitMediaTextTrackDisplayQuirk,
            ShouldDeferIntersectionObserversDuringResize,
            ShouldBlockAudiblePlaybackWhileAudioIsPlaying,
#if PLATFORM(COCOA)
            NeedsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk,
#endif
        } },

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    // Remove this once rdar://142573562 is resolved.
    { .topMatch = QuirkMatch::domain("steampowered.com"_s),
        .behaviors = { ShouldTreatAddingMouseOutEventListenerAsContentChange } },
#endif

#if PLATFORM(IOS_FAMILY)
    { .topMatch = QuirkMatch::anyTopLevelDomain("theguardian"_s),
        .behaviors = { ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk } },

    // theguardian.com rdar://166727225
    { .topMatch = QuirkMatch::anyTopLevelDomain("theguardian"_s),
        .documentMatch = QuirkMatch::domain(youTubeEmbedDomains),
        .behaviors = { NeedsYouTubeEmbedAutoplayQuirk } },
#endif

    { .topMatch = QuirkMatch::domain("thesaurus.com"_s),
        .behaviors = {
#if PLATFORM(IOS_FAMILY)
            EvaluateScriptBeforeRunningScript::create(chromeUserAgentScript, anyClipPlayerScript),
#endif
#if PLATFORM(COCOA)
            NeedsAnchorToBeMouseFocusableQuirk,
#endif
        } },

    { .topMatch = QuirkMatch::domain("tiktok.com"_s),
        .behaviors = {
            NeedsTikTokOverflowingContentQuirk,
            // tiktok.com rdar://174179805
            ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
            // tiktok.com rdar://174179805
            ShouldDispatchSimulatedMouseEventsQuirk,
#endif
        },
        .site = QuirkSite::TikTok },

#if PLATFORM(MAC)
    // trix-editor.org rdar://28242210
    { .topMatch = QuirkMatch::domain("trix-editor.org"_s),
        .behaviors = { IsNeverRichlyEditableForTouchBarQuirk } },
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
    // twitch.tv rdar://102420527
    { .topMatch = QuirkMatch::domain("twitch.tv"_s),
        .behaviors = { ShouldReportDocumentAsVisibleIfActivePIPQuirk } },
#endif

    // https://tympanus.net/Tutorials/WebGPUFluid/ does not load (rdar://143839620).
    { .topMatch = QuirkMatch::domain("tympanus.net"_s),
        .behaviors = { ShouldBlockFetchWithNewlineAndLessThan } },

#if ENABLE(MEDIA_SOURCE)
    // unifi.ui.com rdar://180411019
    { .topMatch = QuirkMatch::domain("ui.com"_s),
        .behaviors = { NeedsSupportsProgressMonitoringQuirk } },
#endif

    // Breaks express checkout on victoriassecret.com (rdar://104818312).
    { .topMatch = QuirkMatch::domain("victoriassecret.com"_s),
        .behaviors = { ShouldDisableFetchMetadata } },

    { .topMatch = QuirkMatch::domain("vimeo.com"_s),
        .behaviors = {
            // vimeo.com rdar://56996057
            MaybeBypassBackForwardCache,
#if PLATFORM(IOS_FAMILY)
            // vimeo.com rdar://55759025
            NeedsPreloadAutoQuirk,
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
            // vimeo.com: rdar://problem/73227900
            ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
#endif
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
            // vimeo.com: rdar://107592139
            BlocksEnteringStandardFullscreenFromPictureInPictureQuirk,
            // vimeo.com: rdar://problem/70788878
            BlocksReturnToFullscreenFromPictureInPictureQuirk,
#endif
        },
        .site = QuirkSite::Vimeo },

#if PLATFORM(IOS_FAMILY)
    // rdar://116531089
    { .topMatch = QuirkMatch::domain("vimeo.com"_s).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },
#endif

#if ENABLE(TWO_PHASE_CLICKS)
    // walmart.com: rdar://123734840
    { .topMatch = QuirkMatch::domain("walmart.com"_s),
        .behaviors = {
            MayNeedToIgnoreContentObservation,
        },
        .site = QuirkSite::Walmart },
#endif

#if PLATFORM(MAC)
    // weather.com rdar://139689157
    { .topMatch = QuirkMatch::domain("weather.com"_s),
        .behaviors = { NeedsFormControlToBeMouseFocusableQuirk } },
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    { .topMatch = QuirkMatch::domain("webex.com"_s),
        .behaviors = {
            EvaluateScriptBeforeRunningScript::create(
                "Object.defineProperty(window, 'Touch', { get: () => undefined });"_s,
                QuirkMatch::anyURL().when(lastPathComponentStartsWith("pushdownload."_s))
            ),
        } },
#endif

    // weebly.com rdar://48003980
    { .topMatch = QuirkMatch::domain("weebly.com"_s),
        .behaviors = { ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk } },

    { .topMatch = QuirkMatch::domain("wikipedia.org"_s),
        .behaviors = {
            // wikipedia.org rdar://54856323
            ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk,
#if ENABLE(META_VIEWPORT)
            // wikipedia.org https://webkit.org/b/247636
            ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk,
#endif
        } },

    // rdar://170412045, https://bugs.webkit.org/show_bug.cgi?id=307933
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    // wix.com rdar://49124313, except while picking a template.
    { .topMatch = QuirkMatch::domain("wix.com"_s).exceptWhen(pathStartsWith("/website/templates/"_s)),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk } },
#endif

    { .topMatch = QuirkMatch::domain("workspaces.xyz"_s),
        .behaviors = { ShouldComparareUsedValuesForBorderWidthForTriggeringTransitions } },

#if PLATFORM(MAC)
    // wpdevelopment.ca rdar://156109518
    { .topMatch = QuirkMatch::domain("wpdevelopment.ca"_s),
        .behaviors = { NeedsFormControlToBeMouseFocusableQuirk } },
#endif

    { .topMatch = QuirkMatch::domain("x.com"_s),
        .behaviors = {
#if PLATFORM(VISION)
            // x.com: rdar://132850672
            ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk,
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
            // rdar://121473410
            ShouldSilenceMediaQueryListChangeEvents,
            // x.com: rdar://problem/58804852 and rdar://problem/61731801
            ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting,
            // x.com: rdar://175565114
            ShouldAvoidProgrammaticScrollClampingQuirk,
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
            // x.com: rdar://73369869
            RequiresUserGestureToLoadInPictureInPictureQuirk,
            // x.com: rdar://73369869
            RequiresUserGestureToPauseInPictureInPictureQuirk,
#endif
        } },

    { .topMatch = QuirkMatch::anyTopLevelDomain("yahoo"_s),
        .behaviors = {
            // yahoo.com: rdar://170502516
            NeedsYahooVolumeSliderQuirk,
            // yahoo.com: rdar://136767005
            ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
#if ENABLE(TOUCH_EVENTS)
            // yahoo.com : rdar://142894603
            ShouldPreventDispatchOfTouchEventQuirk,
#endif
        } },

#if ENABLE(TEXT_AUTOSIZING)
    // news.ycombinator.com: rdar://127246368
    { .topMatch = QuirkMatch::host("news.ycombinator.com"_s),
        .behaviors = { ShouldIgnoreTextAutoSizingQuirk } },
#endif

    { .topMatch = QuirkMatch::domain("youtube.com"_s),
        .behaviors = {
            // youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
            HasBrokenEncryptedMediaAPISupportQuirk,
            // youtube.com rdar://135886305
            NeedsScrollbarWidthThinDisabledQuirk,
#if PLATFORM(COCOA)
            NeedsYouTubeCaptionQuirk,
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
            // youtube.com: rdar://110097836
            ShouldSilenceResizeObservers,
#endif
        } },

#if PLATFORM(COCOA)
    // Embedded youtube.com players need the caption quirk regardless of the embedding site.
    { .documentMatch = QuirkMatch::domain(youTubeEmbedDomains),
        .behaviors = { NeedsYouTubeCaptionQuirk } },
#endif

#if PLATFORM(IOS_FAMILY)
    // YouTube.com does not provide AirPlay controls in fullscreen
    // (Ref: rdar://121471373)
    { .topMatch = QuirkMatch::domain("youtube.com"_s).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // tiny. (Ref: rdar://121471373, rdar://121473410)
    { .documentMatch = QuirkMatch::domain(youTubeEmbedDomains).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    { .documentMatch = QuirkMatch::domain("x.com"_s),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // youtube.com rdar://49582231
    { .topMatch = QuirkMatch::host("www.youtube.com"_s),
        .behaviors = { NeedsYouTubeOverflowScrollQuirk } },

    { .topMatch = QuirkMatch::domain("youtube.com"_s).when(tubularApp()),
        .behaviors = { ShouldSuppressMediaSessionPauseActionOnInterruption } },
#endif

#if ENABLE(TWO_PHASE_CLICKS)
    // www.youtube.com rdar://52361019
    { .topMatch = QuirkMatch::host("www.youtube.com"_s),
        .behaviors = { NeedsYouTubeMouseOutQuirk } },
#endif

#if PLATFORM(VISION) && ENABLE(FULLSCREEN_API)
    // Lens.app rdar://178769976
    { .topMatch = QuirkMatch::domain("youtube.com"_s).when(lensApp()),
        .behaviors = { RequiresUserGestureToPlayInFullscreenQuirk } },
#endif

#if ENABLE(MEDIA_RECORDER) && ENABLE(COCOA_WEBM_PLAYER)
    // zencastr.com rdar://143087016
    { .topMatch = QuirkMatch::domain("zencastr.com"_s),
        .behaviors = { NeedsLimitedMatroskaSupportQuirk } },
#endif

    // zillow.com rdar://53103732
    { .topMatch = QuirkMatch::host("www.zillow.com"_s),
        .behaviors = { ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk } },

#if PLATFORM(IOS) || PLATFORM(VISION)
    // zillow.com rdar://110097836
    { .topMatch = QuirkMatch::domain("zillow.com"_s),
        .behaviors = { ShouldSilenceResizeObservers } },
#endif

#if PLATFORM(MAC)
    { .topMatch = QuirkMatch::domain("zomato.com"_s),
        .behaviors = { NeedsZomatoEmailLoginLabelQuirk } },
#endif

    { .topMatch = QuirkMatch::domain("zoom.us"_s),
        .behaviors = {
            // zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
            ShouldAutoplayWebAudioForArbitraryUserGestureQuirk,
#if ENABLE(MEDIA_STREAM)
            // zoom.us rdar://118185086
            ShouldDisableImageCaptureQuirk,
            ShouldAllowMediaStreamTrackSerializationQuirk,
#endif
        } },
};
}

static void validateTable(const Vector<Quirk>& table)
{
    for (auto& quirk : table) {
        RELEASE_ASSERT_WITH_MESSAGE(quirk.namesAURL(), "Every quirk must name the page URL, the embedded document URL, or both.");

        unsigned scriptCount = 0;
        for (auto& behavior : quirk.behaviors) {
            if (auto* script = std::get_if<EvaluateScriptBeforeRunningScript>(&behavior)) {
                RELEASE_ASSERT_WITH_MESSAGE(!script->script.isNull(), "A script behavior must name a script.");
                ++scriptCount;
            }
        }

        RELEASE_ASSERT_WITH_MESSAGE(scriptCount <= 1, "A quirk must not name more than one script to evaluate.");
    }
}

static const Vector<Quirk>& table()
{
    static NeverDestroyed<Vector<Quirk>> table = [] {
        auto entries = makeTable();
        validateTable(entries);
        return entries;
    }();
    return table.get();
}

} // namespace SiteSpecificQuirks

void validateQuirkTable()
{
    // Building the table validates it.
    SiteSpecificQuirks::table();
}

void Quirk::apply(QuirksData& quirksData) const
{
    for (auto& behavior : behaviors) {
        WTF::switchOn(behavior,
            [&](SiteSpecificQuirk quirk) {
                quirksData.enableQuirk(quirk);
            },
            [&](const IsParameterizedQuirk auto& payload) {
                quirksData.behaviors.append(payload);
            });
    }

    if (site)
        quirksData.addSite(*site);
}

bool Quirk::matches(const QuirkMatchContext& page, const std::optional<QuirkMatchContext>& embeddedDocument) const
{
    if (topMatch && !topMatch->matches(page)) [[likely]]
        return false;

    if (documentMatch && (!embeddedDocument || !documentMatch->matches(*embeddedDocument)))
        return false;

    return true;
}

QuirksData resolveSiteSpecificQuirks(const URL& topURL, const URL& documentURL, IsTopDocument isTopDocument)
{
    QuirkMatchContext page { topURL };

    std::optional<QuirkMatchContext> embeddedDocument;
    if (isTopDocument == IsTopDocument::No)
        embeddedDocument.emplace(documentURL);

    QuirksData quirksData;
    for (auto& quirk : SiteSpecificQuirks::table()) {
        if (quirk.matches(page, embeddedDocument))
            quirk.apply(quirksData);
    }
    return quirksData;
}

} // namespace WebCore
