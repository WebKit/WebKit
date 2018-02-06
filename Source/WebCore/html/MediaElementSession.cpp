/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "MediaElementSession.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLAudioElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "HitTestResult.h"
#include "Logging.h"
#include "MainFrame.h"
#include "Page.h"
#include "PlatformMediaSessionManager.h"
#include "RenderMedia.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "Settings.h"
#include "SourceBuffer.h"
#include <wtf/CurrentTime.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(IOS)
#include "AudioSession.h"
#include "RuntimeApplicationChecks.h"
#include <wtf/spi/darwin/dyldSPI.h>
#endif

namespace WebCore {

static const Seconds elementMainContentCheckInterval { 250_ms };

static bool isElementRectMostlyInMainFrame(const HTMLMediaElement&);
static bool isElementLargeEnoughForMainContent(const HTMLMediaElement&, MediaSessionMainContentPurpose);

#if !LOG_DISABLED
static String restrictionName(MediaElementSession::BehaviorRestrictions restriction)
{
    StringBuilder restrictionBuilder;
#define CASE(restrictionType) \
    if (restriction & MediaElementSession::restrictionType) { \
        if (!restrictionBuilder.isEmpty()) \
            restrictionBuilder.appendLiteral(", "); \
        restrictionBuilder.append(#restrictionType); \
    } \

    CASE(NoRestrictions);
    CASE(RequireUserGestureForLoad);
    CASE(RequireUserGestureForVideoRateChange);
    CASE(RequireUserGestureForAudioRateChange);
    CASE(RequireUserGestureForFullscreen);
    CASE(RequirePageConsentToLoadMedia);
    CASE(RequirePageConsentToResumeMedia);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    CASE(RequireUserGestureToShowPlaybackTargetPicker);
    CASE(WirelessVideoPlaybackDisabled);
#endif
    CASE(InvisibleAutoplayNotPermitted);
    CASE(OverrideUserGestureRequirementForMainContent);

    return restrictionBuilder.toString();
}
#endif

static bool pageExplicitlyAllowsElementToAutoplayInline(const HTMLMediaElement& element)
{
    Document& document = element.document();
    Page* page = document.page();
    return document.isMediaDocument() && !document.ownerElement() && page && page->allowsMediaDocumentInlinePlayback();
}

MediaElementSession::MediaElementSession(HTMLMediaElement& element)
    : PlatformMediaSession(element)
    , m_element(element)
    , m_restrictions(NoRestrictions)
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    , m_targetAvailabilityChangedTimer(*this, &MediaElementSession::targetAvailabilityChangedTimerFired)
#endif
    , m_mainContentCheckTimer(*this, &MediaElementSession::mainContentCheckTimerFired)
{
}

void MediaElementSession::registerWithDocument(Document& document)
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    document.addPlaybackTargetPickerClient(*this);
#else
    UNUSED_PARAM(document);
#endif
}

void MediaElementSession::unregisterWithDocument(Document& document)
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    document.removePlaybackTargetPickerClient(*this);
#else
    UNUSED_PARAM(document);
#endif
}

void MediaElementSession::addBehaviorRestriction(BehaviorRestrictions restriction)
{
    LOG(Media, "MediaElementSession::addBehaviorRestriction - adding %s", restrictionName(restriction).utf8().data());
    m_restrictions |= restriction;

    if (restriction & OverrideUserGestureRequirementForMainContent)
        m_mainContentCheckTimer.startRepeating(elementMainContentCheckInterval);
}

void MediaElementSession::removeBehaviorRestriction(BehaviorRestrictions restriction)
{
    if (restriction & RequireUserGestureToControlControlsManager) {
        m_mostRecentUserInteractionTime = monotonicallyIncreasingTime();
        if (auto page = m_element.document().page())
            page->setAllowsPlaybackControlsForAutoplayingAudio(true);
    }

    LOG(Media, "MediaElementSession::removeBehaviorRestriction - removing %s", restrictionName(restriction).utf8().data());
    m_restrictions &= ~restriction;
}

#if PLATFORM(MAC)
static bool needsArbitraryUserGestureAutoplayQuirk(const Document& document)
{
    if (!document.settings().needsSiteSpecificQuirks())
        return false;

    auto loader = makeRefPtr(document.loader());
    return loader && loader->allowedAutoplayQuirks().contains(AutoplayQuirk::ArbitraryUserGestures);
}
#endif // PLATFORM(MAC)

SuccessOr<MediaPlaybackDenialReason> MediaElementSession::playbackPermitted(const HTMLMediaElement& element) const
{
    if (m_element.isSuspended())
        return { };

    if (element.document().isMediaDocument() && !element.document().ownerElement())
        return { };

    if (pageExplicitlyAllowsElementToAutoplayInline(element))
        return { };

    if (requiresFullscreenForVideoPlayback(element) && !fullscreenPermitted(element)) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because of fullscreen restriction");
        return MediaPlaybackDenialReason::FullscreenRequired;
    }

    if (m_restrictions & OverrideUserGestureRequirementForMainContent && updateIsMainContent())
        return { };

#if ENABLE(MEDIA_STREAM)
    if (element.hasMediaStreamSrcObject()) {
        if (element.document().isCapturing())
            return { };
        if (element.document().mediaState() & MediaProducer::IsPlayingAudio)
            return { };
    }
#endif

#if PLATFORM(MAC)
    // FIXME <https://webkit.org/b/175856>: Make this dependent on a runtime flag for desktop autoplay restrictions.
    const auto& topDocument = element.document().topDocument();
    if (topDocument.mediaState() & MediaProducer::HasUserInteractedWithMediaElement && topDocument.settings().needsSiteSpecificQuirks())
        return { };

    if (element.document().hasHadUserInteraction() && needsArbitraryUserGestureAutoplayQuirk(element.document()))
        return { };
#endif

    if (m_restrictions & RequireUserGestureForVideoRateChange && element.isVideo() && !element.document().processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because a user gesture is required for video rate change restriction");
        return MediaPlaybackDenialReason::UserGestureRequired;
    }

    if (m_restrictions & RequireUserGestureForAudioRateChange && (!element.isVideo() || element.hasAudio()) && !element.muted() && element.volume() && !element.document().processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because a user gesture is required for audio rate change restriction");
        return MediaPlaybackDenialReason::UserGestureRequired;
    }

    if (m_restrictions & RequireUserGestureForVideoDueToLowPowerMode && element.isVideo() && !element.document().processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because of video low power mode restriction");
        return MediaPlaybackDenialReason::UserGestureRequired;
    }

    return { };
}

bool MediaElementSession::autoplayPermitted() const
{
    const Document& document = m_element.document();
    if (document.pageCacheState() != Document::NotInPageCache)
        return false;
    if (document.activeDOMObjectsAreSuspended())
        return false;

    if (!hasBehaviorRestriction(MediaElementSession::InvisibleAutoplayNotPermitted))
        return true;

    // If the media element is audible, allow autoplay even when not visible as pausing it would be observable by the user.
    if ((!m_element.isVideo() || m_element.hasAudio()) && !m_element.muted() && m_element.volume())
        return true;

    auto* renderer = m_element.renderer();
    if (!renderer) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because element has no renderer");
        return false;
    }
    if (renderer->style().visibility() != VISIBLE) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because element is not visible");
        return false;
    }
    if (renderer->view().frameView().isOffscreen()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because frame is offscreen");
        return false;
    }
    if (renderer->visibleInViewportState() != VisibleInViewportState::Yes) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because element is not visible in the viewport");
        return false;
    }
    return true;
}

bool MediaElementSession::dataLoadingPermitted(const HTMLMediaElement& element) const
{
    if (m_restrictions & OverrideUserGestureRequirementForMainContent && updateIsMainContent())
        return true;

    if (m_restrictions & RequireUserGestureForLoad && !element.document().processingUserGestureForMedia()) {
        LOG(Media, "MediaElementSession::dataLoadingPermitted - returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::fullscreenPermitted(const HTMLMediaElement& element) const
{
    if (m_restrictions & RequireUserGestureForFullscreen && !element.document().processingUserGestureForMedia()) {
        LOG(Media, "MediaElementSession::fullscreenPermitted - returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::pageAllowsDataLoading(const HTMLMediaElement& element) const
{
    Page* page = element.document().page();
    if (m_restrictions & RequirePageConsentToLoadMedia && page && !page->canStartMedia()) {
        LOG(Media, "MediaElementSession::pageAllowsDataLoading - returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::pageAllowsPlaybackAfterResuming(const HTMLMediaElement& element) const
{
    Page* page = element.document().page();
    if (m_restrictions & RequirePageConsentToResumeMedia && page && !page->canStartMedia()) {
        LOG(Media, "MediaElementSession::pageAllowsPlaybackAfterResuming - returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::canShowControlsManager(PlaybackControlsPurpose purpose) const
{
    if (m_element.isSuspended() || !m_element.inActiveDocument()) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: isSuspended()");
        return false;
    }

    if (m_element.isFullscreen()) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning TRUE: Is fullscreen");
        return true;
    }

    if (m_element.muted()) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: Muted");
        return false;
    }

    if (m_element.document().isMediaDocument() && (m_element.document().frame() && m_element.document().frame()->isMainFrame())) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning TRUE: Is media document");
        return true;
    }

    if (client().presentationType() == Audio) {
        if (!hasBehaviorRestriction(RequireUserGestureToControlControlsManager) || m_element.document().processingUserGestureForMedia()) {
            LOG(Media, "MediaElementSession::canShowControlsManager - returning TRUE: Audio element with user gesture");
            return true;
        }

        if (m_element.isPlaying() && allowsPlaybackControlsForAutoplayingAudio()) {
            LOG(Media, "MediaElementSession::canShowControlsManager - returning TRUE: User has played media before");
            return true;
        }

        LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: Audio element is not suitable");
        return false;
    }

    if (purpose == PlaybackControlsPurpose::ControlsManager && !isElementRectMostlyInMainFrame(m_element)) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: Not in main frame");
        return false;
    }

    if (!m_element.hasAudio() && !m_element.hasEverHadAudio()) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: No audio");
        return false;
    }

    if (!playbackPermitted(m_element)) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: Playback not permitted");
        return false;
    }

    if (!hasBehaviorRestriction(RequireUserGestureToControlControlsManager) || m_element.document().processingUserGestureForMedia()) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning TRUE: No user gesture required");
        return true;
    }

    if (purpose == PlaybackControlsPurpose::ControlsManager && hasBehaviorRestriction(RequirePlaybackToControlControlsManager) && !m_element.isPlaying()) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: Needs to be playing");
        return false;
    }

    if (!m_element.hasEverNotifiedAboutPlaying()) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: Hasn't fired playing notification");
        return false;
    }

    // Only allow the main content heuristic to forbid videos from showing up if our purpose is the controls manager.
    if (purpose == PlaybackControlsPurpose::ControlsManager && m_element.isVideo()) {
        if (!m_element.renderer()) {
            LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: No renderer");
            return false;
        }

        if (!m_element.hasVideo() && !m_element.hasEverHadVideo()) {
            LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: No video");
            return false;
        }

        if (isLargeEnoughForMainContent(MediaSessionMainContentPurpose::MediaControls)) {
            LOG(Media, "MediaElementSession::canShowControlsManager - returning TRUE: Is main content");
            return true;
        }
    }

    if (purpose == PlaybackControlsPurpose::NowPlaying) {
        LOG(Media, "MediaElementSession::canShowControlsManager - returning TRUE: Potentially plays audio");
        return true;
    }

    LOG(Media, "MediaElementSession::canShowControlsManager - returning FALSE: No user gesture");
    return false;
}

bool MediaElementSession::isLargeEnoughForMainContent(MediaSessionMainContentPurpose purpose) const
{
    return isElementLargeEnoughForMainContent(m_element, purpose);
}

double MediaElementSession::mostRecentUserInteractionTime() const
{
    return m_mostRecentUserInteractionTime;
}

bool MediaElementSession::wantsToObserveViewportVisibilityForMediaControls() const
{
    return isLargeEnoughForMainContent(MediaSessionMainContentPurpose::MediaControls);
}

bool MediaElementSession::wantsToObserveViewportVisibilityForAutoplay() const
{
    if (!m_element.isVideo())
        return false;
    return hasBehaviorRestriction(InvisibleAutoplayNotPermitted) || hasBehaviorRestriction(OverrideUserGestureRequirementForMainContent);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaElementSession::showPlaybackTargetPicker(const HTMLMediaElement& element)
{
    LOG(Media, "MediaElementSession::showPlaybackTargetPicker");

    if (m_restrictions & RequireUserGestureToShowPlaybackTargetPicker && !element.document().processingUserGestureForMedia()) {
        LOG(Media, "MediaElementSession::showPlaybackTargetPicker - returning early because of permissions");
        return;
    }

    if (!element.document().page()) {
        LOG(Media, "MediaElementSession::showingPlaybackTargetPickerPermitted - returning early because page is NULL");
        return;
    }

#if !PLATFORM(IOS)
    if (element.readyState() < HTMLMediaElementEnums::HAVE_METADATA) {
        LOG(Media, "MediaElementSession::showPlaybackTargetPicker - returning early because element is not playable");
        return;
    }
#endif

    element.document().showPlaybackTargetPicker(*this, is<HTMLVideoElement>(element));
}

bool MediaElementSession::hasWirelessPlaybackTargets(const HTMLMediaElement&) const
{
#if PLATFORM(IOS)
    // FIXME: consolidate Mac and iOS implementations
    m_hasPlaybackTargets = PlatformMediaSessionManager::sharedManager().hasWirelessTargetsAvailable();
#endif

    LOG(Media, "MediaElementSession::hasWirelessPlaybackTargets - returning %s", m_hasPlaybackTargets ? "TRUE" : "FALSE");

    return m_hasPlaybackTargets;
}

bool MediaElementSession::wirelessVideoPlaybackDisabled(const HTMLMediaElement& element) const
{
    if (!element.document().settings().allowsAirPlayForMediaPlayback()) {
        LOG(Media, "MediaElementSession::wirelessVideoPlaybackDisabled - returning TRUE because of settings");
        return true;
    }

    if (element.hasAttributeWithoutSynchronization(HTMLNames::webkitwirelessvideoplaybackdisabledAttr)) {
        LOG(Media, "MediaElementSession::wirelessVideoPlaybackDisabled - returning TRUE because of attribute");
        return true;
    }

#if PLATFORM(IOS)
    auto& legacyAirplayAttributeValue = element.attributeWithoutSynchronization(HTMLNames::webkitairplayAttr);
    if (equalLettersIgnoringASCIICase(legacyAirplayAttributeValue, "deny")) {
        LOG(Media, "MediaElementSession::wirelessVideoPlaybackDisabled - returning TRUE because of legacy attribute");
        return true;
    }
    if (equalLettersIgnoringASCIICase(legacyAirplayAttributeValue, "allow")) {
        LOG(Media, "MediaElementSession::wirelessVideoPlaybackDisabled - returning FALSE because of legacy attribute");
        return false;
    }
#endif

    auto player = element.player();
    if (!player)
        return true;

    bool disabled = player->wirelessVideoPlaybackDisabled();
    LOG(Media, "MediaElementSession::wirelessVideoPlaybackDisabled - returning %s because media engine says so", disabled ? "TRUE" : "FALSE");
    
    return disabled;
}

void MediaElementSession::setWirelessVideoPlaybackDisabled(const HTMLMediaElement& element, bool disabled)
{
    if (disabled)
        addBehaviorRestriction(WirelessVideoPlaybackDisabled);
    else
        removeBehaviorRestriction(WirelessVideoPlaybackDisabled);

    auto player = element.player();
    if (!player)
        return;

    LOG(Media, "MediaElementSession::setWirelessVideoPlaybackDisabled - disabled %s", disabled ? "TRUE" : "FALSE");
    player->setWirelessVideoPlaybackDisabled(disabled);
}

void MediaElementSession::setHasPlaybackTargetAvailabilityListeners(const HTMLMediaElement& element, bool hasListeners)
{
    LOG(Media, "MediaElementSession::setHasPlaybackTargetAvailabilityListeners - hasListeners %s", hasListeners ? "TRUE" : "FALSE");

#if PLATFORM(IOS)
    UNUSED_PARAM(element);
    m_hasPlaybackTargetAvailabilityListeners = hasListeners;
    PlatformMediaSessionManager::sharedManager().configureWireLessTargetMonitoring();
#else
    UNUSED_PARAM(hasListeners);
    element.document().playbackTargetPickerClientStateDidChange(*this, element.mediaState());
#endif
}

void MediaElementSession::setPlaybackTarget(Ref<MediaPlaybackTarget>&& device)
{
    m_playbackTarget = WTFMove(device);
    client().setWirelessPlaybackTarget(*m_playbackTarget.copyRef());
}

void MediaElementSession::targetAvailabilityChangedTimerFired()
{
    client().wirelessRoutesAvailableDidChange();
}

void MediaElementSession::externalOutputDeviceAvailableDidChange(bool hasTargets)
{
    if (m_hasPlaybackTargets == hasTargets)
        return;

    LOG(Media, "MediaElementSession::externalOutputDeviceAvailableDidChange(%p) - hasTargets %s", this, hasTargets ? "TRUE" : "FALSE");

    m_hasPlaybackTargets = hasTargets;
    m_targetAvailabilityChangedTimer.startOneShot(0_s);
}

bool MediaElementSession::canPlayToWirelessPlaybackTarget() const
{
#if !PLATFORM(IOS)
    if (!m_playbackTarget || !m_playbackTarget->hasActiveRoute())
        return false;
#endif

    return client().canPlayToWirelessPlaybackTarget();
}

bool MediaElementSession::isPlayingToWirelessPlaybackTarget() const
{
#if !PLATFORM(IOS)
    if (!m_playbackTarget || !m_playbackTarget->hasActiveRoute())
        return false;
#endif

    return client().isPlayingToWirelessPlaybackTarget();
}

void MediaElementSession::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    LOG(Media, "MediaElementSession::setShouldPlayToPlaybackTarget - shouldPlay %s", shouldPlay ? "TRUE" : "FALSE");
    m_shouldPlayToPlaybackTarget = shouldPlay;
    client().setShouldPlayToPlaybackTarget(shouldPlay);
}

void MediaElementSession::mediaStateDidChange(const HTMLMediaElement& element, MediaProducer::MediaStateFlags state)
{
    element.document().playbackTargetPickerClientStateDidChange(*this, state);
}
#endif

MediaPlayer::Preload MediaElementSession::effectivePreloadForElement(const HTMLMediaElement& element) const
{
    MediaPlayer::Preload preload = element.preloadValue();

    if (pageExplicitlyAllowsElementToAutoplayInline(element))
        return preload;

    if (m_restrictions & MetadataPreloadingNotPermitted)
        return MediaPlayer::None;

    if (m_restrictions & AutoPreloadingNotPermitted) {
        if (preload > MediaPlayer::MetaData)
            return MediaPlayer::MetaData;
    }

    return preload;
}

bool MediaElementSession::requiresFullscreenForVideoPlayback(const HTMLMediaElement& element) const
{
    if (pageExplicitlyAllowsElementToAutoplayInline(element))
        return false;

    if (is<HTMLAudioElement>(element))
        return false;

    if (element.document().isMediaDocument()) {
        ASSERT(is<HTMLVideoElement>(element));
        const HTMLVideoElement& videoElement = *downcast<const HTMLVideoElement>(&element);
        if (element.readyState() < HTMLVideoElement::HAVE_METADATA || !videoElement.hasEverHadVideo())
            return false;
    }

    if (element.isTemporarilyAllowingInlinePlaybackAfterFullscreen())
        return false;

    if (!element.document().settings().allowsInlineMediaPlayback())
        return true;

    if (!element.document().settings().inlineMediaPlaybackRequiresPlaysInlineAttribute())
        return false;

#if PLATFORM(IOS)
    if (IOSApplication::isIBooks())
        return !element.hasAttributeWithoutSynchronization(HTMLNames::webkit_playsinlineAttr) && !element.hasAttributeWithoutSynchronization(HTMLNames::playsinlineAttr);
    if (dyld_get_program_sdk_version() < DYLD_IOS_VERSION_10_0)
        return !element.hasAttributeWithoutSynchronization(HTMLNames::webkit_playsinlineAttr);
#endif

    if (element.document().isMediaDocument() && element.document().ownerElement())
        return false;

    return !element.hasAttributeWithoutSynchronization(HTMLNames::playsinlineAttr);
}

bool MediaElementSession::allowsAutomaticMediaDataLoading(const HTMLMediaElement& element) const
{
    if (pageExplicitlyAllowsElementToAutoplayInline(element))
        return true;

    if (element.document().settings().mediaDataLoadsAutomatically())
        return true;

    return false;
}

void MediaElementSession::mediaEngineUpdated(const HTMLMediaElement& element)
{
    LOG(Media, "MediaElementSession::mediaEngineUpdated");

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (m_restrictions & WirelessVideoPlaybackDisabled)
        setWirelessVideoPlaybackDisabled(element, true);
    if (m_playbackTarget)
        client().setWirelessPlaybackTarget(*m_playbackTarget.copyRef());
    if (m_shouldPlayToPlaybackTarget)
        client().setShouldPlayToPlaybackTarget(true);
#else
    UNUSED_PARAM(element);
#endif
    
}

void MediaElementSession::resetPlaybackSessionState()
{
    m_mostRecentUserInteractionTime = 0;
    addBehaviorRestriction(RequireUserGestureToControlControlsManager | RequirePlaybackToControlControlsManager);
}

bool MediaElementSession::allowsPictureInPicture(const HTMLMediaElement& element) const
{
    return element.document().settings().allowsPictureInPictureMediaPlayback() && !element.webkitCurrentPlaybackTargetIsWireless();
}

#if PLATFORM(IOS)
bool MediaElementSession::requiresPlaybackTargetRouteMonitoring() const
{
    return m_hasPlaybackTargetAvailabilityListeners && !client().elementIsHidden();
}
#endif

#if ENABLE(MEDIA_SOURCE)
size_t MediaElementSession::maximumMediaSourceBufferSize(const SourceBuffer& buffer) const
{
    // A good quality 1080p video uses 8,000 kbps and stereo audio uses 384 kbps, so assume 95% for video and 5% for audio.
    const float bufferBudgetPercentageForVideo = .95;
    const float bufferBudgetPercentageForAudio = .05;

    size_t maximum = buffer.document().settings().maximumSourceBufferSize();

    // Allow a SourceBuffer to buffer as though it is audio-only even if it doesn't have any active tracks (yet).
    size_t bufferSize = static_cast<size_t>(maximum * bufferBudgetPercentageForAudio);
    if (buffer.hasVideo())
        bufferSize += static_cast<size_t>(maximum * bufferBudgetPercentageForVideo);

    // FIXME: we might want to modify this algorithm to:
    // - decrease the maximum size for background tabs
    // - decrease the maximum size allowed for inactive elements when a process has more than one
    //   element, eg. so a page with many elements which are played one at a time doesn't keep
    //   everything buffered after an element has finished playing.

    return bufferSize;
}
#endif

static bool isMainContentForPurposesOfAutoplay(const HTMLMediaElement& element)
{
    Document& document = element.document();
    if (!document.hasLivingRenderTree() || document.activeDOMObjectsAreStopped() || element.isSuspended() || !element.hasAudio() || !element.hasVideo())
        return false;

    // Elements which have not yet been laid out, or which are not yet in the DOM, cannot be main content.
    auto* renderer = element.renderer();
    if (!renderer)
        return false;

    if (!isElementLargeEnoughForMainContent(element, MediaSessionMainContentPurpose::Autoplay))
        return false;

    // Elements which are hidden by style, or have been scrolled out of view, cannot be main content.
    // But elements which have audio & video and are already playing should not stop playing because
    // they are scrolled off the page.
    if (renderer->style().visibility() != VISIBLE)
        return false;
    if (renderer->visibleInViewportState() != VisibleInViewportState::Yes && !element.isPlaying())
        return false;

    // Main content elements must be in the main frame.
    if (!document.frame() || !document.frame()->isMainFrame())
        return false;

    MainFrame& mainFrame = document.frame()->mainFrame();
    if (!mainFrame.view() || !mainFrame.view()->renderView())
        return false;

    RenderView& mainRenderView = *mainFrame.view()->renderView();

    // Hit test the area of the main frame where the element appears, to determine if the element is being obscured.
    IntRect rectRelativeToView = element.clientRect();
    ScrollPosition scrollPosition = mainFrame.view()->documentScrollPositionRelativeToViewOrigin();
    IntRect rectRelativeToTopDocument(rectRelativeToView.location() + scrollPosition, rectRelativeToView.size());
    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowChildFrameContent | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowUserAgentShadowContent);
    HitTestResult result(rectRelativeToTopDocument.center());

    // Elements which are obscured by other elements cannot be main content.
    mainRenderView.hitTest(request, result);
    result.setToNonUserAgentShadowAncestor();
    RefPtr<Element> hitElement = result.targetElement();
    if (hitElement != &element)
        return false;

    return true;
}

static bool isElementRectMostlyInMainFrame(const HTMLMediaElement& element)
{
    if (!element.renderer())
        return false;

    auto documentFrame = makeRefPtr(element.document().frame());
    if (!documentFrame)
        return false;

    auto mainFrameView = documentFrame->mainFrame().view();
    if (!mainFrameView)
        return false;

    IntRect mainFrameRectAdjustedForScrollPosition = IntRect(-mainFrameView->documentScrollPositionRelativeToViewOrigin(), mainFrameView->contentsSize());
    IntRect elementRectInMainFrame = element.clientRect();
    auto totalElementArea = elementRectInMainFrame.area<RecordOverflow>();
    if (totalElementArea.hasOverflowed())
        return false;

    elementRectInMainFrame.intersect(mainFrameRectAdjustedForScrollPosition);

    return elementRectInMainFrame.area().unsafeGet() > totalElementArea.unsafeGet() / 2;
}

static bool isElementLargeRelativeToMainFrame(const HTMLMediaElement& element)
{
    static const double minimumPercentageOfMainFrameAreaForMainContent = 0.9;
    auto* renderer = element.renderer();
    if (!renderer)
        return false;

    auto documentFrame = makeRefPtr(element.document().frame());
    if (!documentFrame)
        return false;

    if (!documentFrame->mainFrame().view())
        return false;

    auto& mainFrameView = *documentFrame->mainFrame().view();
    auto maxVisibleClientWidth = std::min(renderer->clientWidth().toInt(), mainFrameView.visibleWidth());
    auto maxVisibleClientHeight = std::min(renderer->clientHeight().toInt(), mainFrameView.visibleHeight());

    return maxVisibleClientWidth * maxVisibleClientHeight > minimumPercentageOfMainFrameAreaForMainContent * mainFrameView.visibleWidth() * mainFrameView.visibleHeight();
}

static bool isElementLargeEnoughForMainContent(const HTMLMediaElement& element, MediaSessionMainContentPurpose purpose)
{
    static const double elementMainContentAreaMinimum = 400 * 300;
    static const double maximumAspectRatio = purpose == MediaSessionMainContentPurpose::MediaControls ? 3 : 1.8;
    static const double minimumAspectRatio = .5; // Slightly smaller than 9:16.

    // Elements which have not yet been laid out, or which are not yet in the DOM, cannot be main content.
    auto* renderer = element.renderer();
    if (!renderer)
        return false;

    double width = renderer->clientWidth();
    double height = renderer->clientHeight();
    double area = width * height;
    double aspectRatio = width / height;

    if (area < elementMainContentAreaMinimum)
        return false;

    if (aspectRatio >= minimumAspectRatio && aspectRatio <= maximumAspectRatio)
        return true;

    return isElementLargeRelativeToMainFrame(element);
}

void MediaElementSession::mainContentCheckTimerFired()
{
    if (!hasBehaviorRestriction(OverrideUserGestureRequirementForMainContent))
        return;

    updateIsMainContent();
}

bool MediaElementSession::updateIsMainContent() const
{
    if (m_element.isSuspended())
        return false;

    bool wasMainContent = m_isMainContent;
    m_isMainContent = isMainContentForPurposesOfAutoplay(m_element);

    if (m_isMainContent != wasMainContent)
        m_element.updateShouldPlay();

    return m_isMainContent;
}

bool MediaElementSession::allowsNowPlayingControlsVisibility() const
{
    auto page = m_element.document().page();
    return page && !page->isVisibleAndActive();
}

bool MediaElementSession::allowsPlaybackControlsForAutoplayingAudio() const
{
    auto page = m_element.document().page();
    return page && page->allowsPlaybackControlsForAutoplayingAudio();
}

bool MediaElementSession::willLog(WTFLogLevel level) const
{
    return m_element.willLog(level);
}

#if !RELEASE_LOG_DISABLED
const Logger& MediaElementSession::logger() const
{
    return m_element.logger();
}

const void* MediaElementSession::logIdentifier() const
{
    return m_element.logIdentifier();
}

WTFLogChannel& MediaElementSession::logChannel() const
{
    return m_element.logChannel();
}
#endif

}

#endif // ENABLE(VIDEO)
