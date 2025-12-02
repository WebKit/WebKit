/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#include "AudioTrack.h"
#include "AudioTrackConfiguration.h"
#include "AudioTrackList.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentFullscreen.h"
#include "DocumentLoader.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "HTMLAudioElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MediaUsageInfo.h"
#include "Navigator.h"
#include "NowPlayingInfo.h"
#include "Page.h"
#include "PlatformMediaSessionManager.h"
#include "RenderMediaInlines.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "Settings.h"
#include "SourceBuffer.h"
#include "TextTrack.h"
#include "TextTrackList.h"
#include "VideoProjectionMetadata.h"
#include "VideoTrack.h"
#include "VideoTrackConfiguration.h"
#include "VideoTrackList.h"
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(MEDIA_SESSION)
#include "MediaMetadata.h"
#include "MediaPositionState.h"
#include "MediaSession.h"
#include "MediaSessionPlaybackState.h"
#include "NavigatorMediaSession.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "AudioSession.h"
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaElementSession);

static const Seconds clientDataBufferingTimerThrottleDelay { 100_ms };
static const Seconds elementMainContentCheckInterval { 250_ms };

static bool isElementRectMostlyInMainFrame(const HTMLMediaElement&);
static bool isElementLargeEnoughForMainContent(const HTMLMediaElement&, MediaSessionMainContentPurpose);
static bool isElementMainContentForPurposesOfAutoplay(const HTMLMediaElement&, bool shouldHitTestMainFrame);
static bool isElementLongEnoughForMainContent(const HTMLMediaElement&);

#if !RELEASE_LOG_DISABLED

static String restrictionNames(MediaElementSession::BehaviorRestrictions restriction)
{
    StringBuilder restrictionBuilder;
#define CASE(restrictionType) \
    if (restriction & MediaElementSession::restrictionType) { \
        if (!restrictionBuilder.isEmpty()) \
            restrictionBuilder.append(", "_s); \
        restrictionBuilder.append(#restrictionType ## _s); \
    } \

    CASE(NoRestrictions)
    CASE(RequireUserGestureForLoad)
    CASE(RequireUserGestureForVideoRateChange)
    CASE(RequireUserGestureForAudioRateChange)
    CASE(RequireUserGestureForFullscreen)
    CASE(RequirePageConsentToLoadMedia)
    CASE(RequirePageConsentToResumeMedia)
    CASE(RequireUserGestureToShowPlaybackTargetPicker)
    CASE(WirelessVideoPlaybackDisabled)
    CASE(RequireUserGestureToAutoplayToExternalDevice)
    CASE(AutoPreloadingNotPermitted)
    CASE(InvisibleAutoplayNotPermitted)
    CASE(OverrideUserGestureRequirementForMainContent)
    CASE(RequireUserGestureToControlControlsManager)
    CASE(RequirePlaybackToControlControlsManager)
    CASE(RequireUserGestureForVideoDueToLowPowerMode)
    CASE(RequireUserGestureForVideoDueToAggressiveThermalMitigation)

    return restrictionBuilder.toString();
}

#endif

static bool pageExplicitlyAllowsElementToAutoplayInline(const HTMLMediaElement& element)
{
    Document& document = element.document();
    Page* page = document.page();
    return document.isMediaDocument() && !document.ownerElement() && page && page->allowsMediaDocumentInlinePlayback();
}

#if ENABLE(MEDIA_SESSION)
class MediaElementSessionObserver : public MediaSessionObserver, public RefCounted<MediaElementSessionObserver> {
    WTF_MAKE_TZONE_ALLOCATED(MediaElementSessionObserver);
public:
    static Ref<MediaElementSessionObserver> create(MediaElementSession& session, const Ref<MediaSession>& mediaSession)
    {
        return adoptRef(*new MediaElementSessionObserver(session, mediaSession));
    }

    ~MediaElementSessionObserver()
    {
        m_mediaSession->removeObserver(*this);
    }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void metadataChanged(const RefPtr<MediaMetadata>& metadata) final
    {
        if (m_session)
            m_session->metadataChanged(metadata);
    }
    void positionStateChanged(const std::optional<MediaPositionState>& state) final
    {
        if (m_session)
            m_session->positionStateChanged(state);
    }
    void playbackStateChanged(MediaSessionPlaybackState state) final
    {
        if (m_session)
            m_session->playbackStateChanged(state);
    }
    void actionHandlersChanged()
    {
        if (m_session)
            m_session->actionHandlersChanged();
    }
private:
    MediaElementSessionObserver(MediaElementSession& session, const Ref<MediaSession>& mediaSession)
        : m_session(session), m_mediaSession(mediaSession)
    {
        m_mediaSession->addObserver(*this);
    }

    WeakPtr<MediaElementSession> m_session;
    const Ref<MediaSession> m_mediaSession;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaElementSessionObserver);
#endif

MediaElementSession::MediaElementSession(HTMLMediaElement& element)
    : PlatformMediaSession(element)
    , m_element(element)
    , m_restrictions(NoRestrictions)
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    , m_targetAvailabilityChangedTimer(*this, &MediaElementSession::targetAvailabilityChangedTimerFired)
#endif
    , m_mainContentCheckTimer(*this, &MediaElementSession::mainContentCheckTimerFired)
    , m_clientDataBufferingTimer(*this, &MediaElementSession::clientDataBufferingTimerFired)
{
}

MediaElementSession::~MediaElementSession()
{
#if ENABLE(MEDIA_USAGE)
    RefPtr element = m_element.get();
    if (!element)
        return;

    auto page = element->document().page();
    if (page && m_haveAddedMediaUsageManagerSession)
        page->chrome().client().removeMediaUsageManagerSession(mediaSessionIdentifier());
#endif
}

RefPtr<HTMLMediaElement> MediaElementSession::protectedElement() const
{
    return m_element.get();
}

void MediaElementSession::addMediaUsageManagerSessionIfNecessary()
{
#if ENABLE(MEDIA_USAGE)
    RefPtr element = m_element.get();
    if (!element)
        return;

    if (m_haveAddedMediaUsageManagerSession)
        return;

    auto page = element->document().page();
    if (!page)
        return;

    m_haveAddedMediaUsageManagerSession = true;
    page->chrome().client().addMediaUsageManagerSession(mediaSessionIdentifier(), element->sourceApplicationIdentifier(), element->document().url());
#endif
}

void MediaElementSession::registerWithDocument(Document& document)
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    document.addPlaybackTargetPickerClient(*this);
#else
    UNUSED_PARAM(document);
#endif
    ensureIsObservingMediaSession();
}

void MediaElementSession::unregisterWithDocument(Document& document)
{
    if (RefPtr manager = sessionManager())
        manager->removeSession(*this);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    document.removePlaybackTargetPickerClient(*this);
#else
    UNUSED_PARAM(document);
#endif
#if ENABLE(MEDIA_SESSION)
    m_observer = nullptr;
#endif
}

void MediaElementSession::clientWillBeginAutoplaying()
{
    PlatformMediaSession::clientWillBeginAutoplaying();
    m_elementIsHiddenBecauseItWasRemovedFromDOM = false;
    updateClientDataBuffering();
}

bool MediaElementSession::clientWillBeginPlayback()
{
    if (!PlatformMediaSession::clientWillBeginPlayback())
        return false;

    m_elementIsHiddenBecauseItWasRemovedFromDOM = false;
    updateClientDataBuffering();

#if ENABLE(MEDIA_SESSION)
    if (auto* session = mediaSession())
        session->willBeginPlayback();
#endif

    return true;
}

bool MediaElementSession::clientWillPausePlayback()
{
    if (!PlatformMediaSession::clientWillPausePlayback())
        return false;

    updateClientDataBuffering();

#if ENABLE(MEDIA_SESSION)
    if (auto* session = mediaSession())
        session->willPausePlayback();
#endif

    return true;
}

void MediaElementSession::visibilityChanged()
{
    scheduleClientDataBufferingCheck();

    RefPtr element = m_element.get();
    if (!element)
        return;

    bool elementIsHidden = element->elementIsHidden();

    if (elementIsHidden)
        m_elementIsHiddenUntilVisibleInViewport = true;
    else if (element->isVisibleInViewport())
        m_elementIsHiddenUntilVisibleInViewport = false;

    bool isPlayingAudio = element->isPlaying() && element->hasAudio() && !element->muted() && element->volume();
    if (!isPlayingAudio) {
        if (elementIsHidden) {
            ALWAYS_LOG(LOGIDENTIFIER, "Suspending silent playback after page visibility: hidden");
            beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
        } else {
            ALWAYS_LOG(LOGIDENTIFIER, "Resuming silent playback after page visibility: showing");
            endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
        }
        return;
    }

    if (hasBehaviorRestriction(RequirePageVisibilityToPlayAudio)) {
        if (elementIsHidden) {
            ALWAYS_LOG(LOGIDENTIFIER, "Suspending audible playback after page visibility: hidden");
            beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
        } else {
            ALWAYS_LOG(LOGIDENTIFIER, "Resuming audible playback after page visibility: showing");
            endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
        }
    }
}

void MediaElementSession::isVisibleInViewportChanged()
{
    scheduleClientDataBufferingCheck();

    RefPtr element = m_element.get();
    if (!element || element->isFullscreen() || element->isVisibleInViewport())
        m_elementIsHiddenUntilVisibleInViewport = false;

#if PLATFORM(COCOA) && !HAVE(CGS_FIX_FOR_RADAR_97530095)
    if (RefPtr manager = sessionManager())
        manager->scheduleSessionStatusUpdate();
#endif
}

void MediaElementSession::inActiveDocumentChanged()
{
    RefPtr element = m_element.get();
    m_elementIsHiddenBecauseItWasRemovedFromDOM = !element || !element->inActiveDocument();
    scheduleClientDataBufferingCheck();
}

void MediaElementSession::scheduleClientDataBufferingCheck()
{
    if (!m_clientDataBufferingTimer.isActive())
        m_clientDataBufferingTimer.startOneShot(clientDataBufferingTimerThrottleDelay);
}

void MediaElementSession::clientDataBufferingTimerFired()
{
    RefPtr element = m_element.get();
    if (!element)
        return;

    INFO_LOG(LOGIDENTIFIER, "visible = ", element->elementIsHidden());

    updateClientDataBuffering();

    if (state() != PlatformMediaSession::State::Playing || !element->elementIsHidden())
        return;

    RefPtr manager = sessionManager();
    if (!manager)
        return;

    auto restrictions = manager->restrictions(mediaType());
    if ((restrictions & MediaSessionRestriction::BackgroundTabPlaybackRestricted) == MediaSessionRestriction::BackgroundTabPlaybackRestricted)
        pauseSession();
}

void MediaElementSession::updateClientDataBuffering()
{
    if (m_clientDataBufferingTimer.isActive())
        m_clientDataBufferingTimer.stop();

    if (RefPtr element = m_element.get())
        element->setBufferingPolicy(preferredBufferingPolicy());

#if PLATFORM(IOS_FAMILY)
    if (RefPtr manager = sessionManager())
        manager->configureWirelessTargetMonitoring();
#endif
}

void MediaElementSession::addBehaviorRestriction(BehaviorRestrictions restrictions)
{
    if (restrictions & ~m_restrictions)
        INFO_LOG(LOGIDENTIFIER, "adding ", restrictionNames(restrictions & ~m_restrictions));

    m_restrictions |= restrictions;

    if (restrictions & OverrideUserGestureRequirementForMainContent)
        m_mainContentCheckTimer.startRepeating(elementMainContentCheckInterval);
}

void MediaElementSession::removeBehaviorRestriction(BehaviorRestrictions restriction)
{
    RefPtr element = m_element.get();
    if (!element)
        return;

    if (restriction & RequireUserGestureToControlControlsManager) {
        m_mostRecentUserInteractionTime = MonotonicTime::now();
        if (auto page = element->document().page())
            page->setAllowsPlaybackControlsForAutoplayingAudio(true);
    }

    if (!(m_restrictions & restriction))
        return;

    INFO_LOG(LOGIDENTIFIER, "removed ", restrictionNames(m_restrictions & restriction));
    m_restrictions &= ~restriction;
}

Expected<void, MediaPlaybackDenialReason> MediaElementSession::playbackStateChangePermitted(MediaPlaybackState state) const
{
    RefPtr element = m_element.get();

    INFO_LOG(LOGIDENTIFIER, "state = ", state);
    if (!element || element->isSuspended()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because element is suspended");
        return makeUnexpected(MediaPlaybackDenialReason::InvalidState);
    }

    Ref document = element->document();
    RefPtr page = document->page();
    if (!page || page->mediaPlaybackIsSuspended()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because media playback is suspended");
        return makeUnexpected(MediaPlaybackDenialReason::PageConsentRequired);
    }

    if (document->isMediaDocument() && !document->ownerElement())
        return { };

    if (pageExplicitlyAllowsElementToAutoplayInline(*element))
        return { };

    if (requiresFullscreenForVideoPlayback() && !fullscreenPermitted()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because of fullscreen restriction");
        return makeUnexpected(MediaPlaybackDenialReason::FullscreenRequired);
    }

    if (m_restrictions & OverrideUserGestureRequirementForMainContent && updateIsMainContent())
        return { };

#if ENABLE(MEDIA_STREAM)
    if (element->hasMediaStreamSrcObject()) {
        if (document->isCapturing())
            return { };
        if (document->mediaState() & MediaProducerMediaState::IsPlayingAudio)
            return { };
    }
#endif

    // FIXME: Why are we checking top-level document only for PerDocumentAutoplayBehavior?
    RefPtr mainFrameDocument = document->mainFrameDocument();
    if (!mainFrameDocument) {
        LOG_ONCE(SiteIsolation, "Unable to properly calculate MediaElementSession::playbackStateChangePermitted() without access to the main frame document ");
    }

    if (mainFrameDocument
        && mainFrameDocument->quirks().requiresUserGestureToPauseInPictureInPicture()
        && element->fullscreenMode() & HTMLMediaElementEnums::VideoFullscreenModePictureInPicture
        && !element->paused() && state == MediaPlaybackState::Paused
        && !document->processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because a quirk requires a user gesture to pause while in Picture-in-Picture");
        return makeUnexpected(MediaPlaybackDenialReason::UserGestureRequired);
    }

    if (mainFrameDocument
        && mainFrameDocument->mediaState() & MediaProducerMediaState::HasUserInteractedWithMediaElement
        && mainFrameDocument->quirks().needsPerDocumentAutoplayBehavior())
        return { };

    if (m_restrictions & RequireUserGestureForVideoRateChange && element->isVideo() && !document->processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because a user gesture is required for video rate change restriction");
        return makeUnexpected(MediaPlaybackDenialReason::UserGestureRequired);
    }

    if (m_restrictions & RequireUserGestureForAudioRateChange && (!element->isVideo() || element->hasAudio()) && !element->muted() && element->volume() && !document->processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because a user gesture is required for audio rate change restriction");
        return makeUnexpected(MediaPlaybackDenialReason::UserGestureRequired);
    }

    if (m_restrictions & RequirePageVisibilityToPlayAudio && (!element->isVideo() || element->hasAudio()) && !element->muted() && element->volume() && element->elementIsHidden()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because page visibility required for audio rate change restriction");
        return makeUnexpected(MediaPlaybackDenialReason::UserGestureRequired);
    }

    if (m_restrictions & RequireUserGestureForVideoDueToLowPowerMode && element->isVideo() && !document->processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because of video low power mode restriction");
        return makeUnexpected(MediaPlaybackDenialReason::UserGestureRequired);
    }

    if (m_restrictions & RequireUserGestureForVideoDueToAggressiveThermalMitigation && element->isVideo() && !document->processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because of video aggressive thermal mitigation restriction");
        return makeUnexpected(MediaPlaybackDenialReason::UserGestureRequired);
    }

    return { };
}

bool MediaElementSession::autoplayPermitted() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    Ref document = element->document();
    if (document->backForwardCacheState() != Document::NotInBackForwardCache)
        return false;
    if (document->activeDOMObjectsAreSuspended())
        return false;

    if (!hasBehaviorRestriction(MediaElementSession::InvisibleAutoplayNotPermitted))
        return true;

    // If the media element is audible, allow autoplay even when not visible as pausing it would be observable by the user.
    if ((!element->isVideo() || element->hasAudio()) && !element->muted() && element->volume())
        return true;

    CheckedPtr renderer = element->renderer();
    if (!renderer) {
        ALWAYS_LOG(LOGIDENTIFIER, "Returning FALSE because element has no renderer");
        return false;
    }
    if (renderer->style().visibility() != Visibility::Visible) {
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

bool MediaElementSession::dataLoadingPermitted() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    if (m_restrictions & OverrideUserGestureRequirementForMainContent && updateIsMainContent())
        return true;

    if (m_restrictions & RequireUserGestureForLoad && !element->document().processingUserGestureForMedia()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE");
        return false;
    }

    return true;
}

MediaPlayer::BufferingPolicy MediaElementSession::preferredBufferingPolicy() const
{
    RefPtr element = m_element.get();
    if (!element)
        return MediaPlayer::BufferingPolicy::Default;

    auto currentPolicy = element->bufferingPolicy();
    auto isPlaying = state() == PlatformMediaSession::State::Playing;
    MediaPlayer::BufferingPolicy newPolicy = [&] {

        if (isSuspended())
            return MediaPlayer::BufferingPolicy::MakeResourcesPurgeable;

        if (bufferingSuspended())
            return MediaPlayer::BufferingPolicy::LimitReadAhead;

        if (isPlaying)
            return MediaPlayer::BufferingPolicy::Default;

        if (shouldOverrideBackgroundLoadingRestriction())
            return MediaPlayer::BufferingPolicy::Default;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (m_shouldPlayToPlaybackTarget)
            return MediaPlayer::BufferingPolicy::Default;
#endif

        if (m_elementIsHiddenUntilVisibleInViewport || m_elementIsHiddenBecauseItWasRemovedFromDOM || element->elementIsHidden())
            return MediaPlayer::BufferingPolicy::MakeResourcesPurgeable;

        return currentPolicy;
    }();

    if (currentPolicy == MediaPlayer::BufferingPolicy::PurgeResources && !isPlaying && newPolicy != MediaPlayer::BufferingPolicy::Default)
        return MediaPlayer::BufferingPolicy::PurgeResources;

    return newPolicy;
}

bool MediaElementSession::fullscreenPermitted() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    if (m_restrictions & RequireUserGestureForFullscreen && !element->document().processingUserGestureForMedia()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::pageAllowsDataLoading() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    Page* page = element->document().page();
    if (m_restrictions & RequirePageConsentToLoadMedia && page && !page->canStartMedia()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::pageAllowsPlaybackAfterResuming() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    Page* page = element->document().page();
    if (m_restrictions & RequirePageConsentToResumeMedia && page && !page->canStartMedia()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::canShowControlsManager(PlaybackControlsPurpose purpose) const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    if (element->isSuspended() || !element->inActiveDocument()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE: isSuspended()");
        return false;
    }

#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
    if (purpose == MediaElementSession::PlaybackControlsPurpose::NowPlaying
        && hasBehaviorRestriction(RequirePageVisibilityForVideoToBeNowPlaying)
        && element->isVideo()
        && !element->protectedDocument()->protectedPage()->isVisibleAndActive()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE: NowPlaying restricted for video in a page that is not visible");
        return false;
    }
#endif

    if (element->isFullscreen()) {
        INFO_LOG(LOGIDENTIFIER, "returning TRUE: is fullscreen");
        return true;
    }

    if (element->muted()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE: muted");
        return false;
    }

    if (element->document().isMediaDocument() && (element->document().frame() && element->document().frame()->isMainFrame())) {
        INFO_LOG(LOGIDENTIFIER, "returning TRUE: is media document");
        return true;
    }

    RefPtr manager = sessionManager();
    bool registeredAsNowPlayingApplication = manager && manager->registeredAsNowPlayingApplication();
    if (client().presentationType() == MediaType::Audio && purpose == PlaybackControlsPurpose::NowPlaying) {
        if (!element->hasSource()
            || element->error()
            || (!isLongEnoughForMainContent() && !registeredAsNowPlayingApplication)) {
            INFO_LOG(LOGIDENTIFIER, "returning FALSE: audio too short for NowPlaying");
            return false;
        }
    }

    if (client().presentationType() == MediaType::Audio && (purpose == PlaybackControlsPurpose::ControlsManager || purpose == PlaybackControlsPurpose::MediaSession)) {
        if (!hasBehaviorRestriction(RequireUserGestureToControlControlsManager) || element->document().processingUserGestureForMedia()) {
            INFO_LOG(LOGIDENTIFIER, "returning TRUE: audio element with user gesture");
            return true;
        }

        if (element->isPlaying() && allowsPlaybackControlsForAutoplayingAudio()) {
            INFO_LOG(LOGIDENTIFIER, "returning TRUE: user has played media before");
            return true;
        }

        INFO_LOG(LOGIDENTIFIER, "returning FALSE: audio element is not suitable");
        return false;
    }

    if (purpose == PlaybackControlsPurpose::ControlsManager && !isElementRectMostlyInMainFrame(*element)) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE: not in main frame");
        return false;
    }

    if (!element->hasAudio() && !element->hasEverHadAudio()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE: no audio");
        return false;
    }

    if (!playbackStateChangePermitted(MediaPlaybackState::Playing)) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE: playback not permitted");
        return false;
    }

    if (!hasBehaviorRestriction(RequireUserGestureToControlControlsManager) || element->document().processingUserGestureForMedia()) {
        INFO_LOG(LOGIDENTIFIER, "returning TRUE: no user gesture required");
        return true;
    }

    if (purpose == PlaybackControlsPurpose::ControlsManager && hasBehaviorRestriction(RequirePlaybackToControlControlsManager) && !element->isPlaying()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE: needs to be playing");
        return false;
    }

    if (purpose != PlaybackControlsPurpose::MediaSession && !element->hasEverNotifiedAboutPlaying()) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE: hasn't fired playing notification");
        return false;
    }

#if ENABLE(FULLSCREEN_API)
    // Elements which are not descendants of the current fullscreen element cannot be main content.
    if (RefPtr documentFullscreen = element->document().fullscreenIfExists()) {
        RefPtr fullscreenElement = documentFullscreen->fullscreenElement();
        if (fullscreenElement && !element->isDescendantOf(*fullscreenElement)) {
            INFO_LOG(LOGIDENTIFIER, "returning FALSE: outside of full screen");
            return false;
        }
    }
#endif

    // Only allow the main content heuristic to forbid videos from showing up if our purpose is the controls manager.
    if (purpose == PlaybackControlsPurpose::ControlsManager && element->isVideo()) {
        if (!element->renderer()) {
            INFO_LOG(LOGIDENTIFIER, "returning FALSE: no renderer");
            return false;
        }

        if (!element->hasVideo() && !element->hasEverHadVideo()) {
            INFO_LOG(LOGIDENTIFIER, "returning FALSE: no video");
            return false;
        }

        if (isLargeEnoughForMainContent(MediaSessionMainContentPurpose::MediaControls)) {
            INFO_LOG(LOGIDENTIFIER, "returning TRUE: is main content");
            return true;
        }
    }

    if (purpose == PlaybackControlsPurpose::NowPlaying || purpose == PlaybackControlsPurpose::MediaSession) {
        INFO_LOG(LOGIDENTIFIER, "returning TRUE: potentially plays audio");
        return true;
    }

    INFO_LOG(LOGIDENTIFIER, "returning FALSE: no user gesture");
    return false;
}

bool MediaElementSession::isLargeEnoughForMainContent(MediaSessionMainContentPurpose purpose) const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    return isElementLargeEnoughForMainContent(*element, purpose);
}

bool MediaElementSession::isLongEnoughForMainContent() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    return isElementLongEnoughForMainContent(*element);
}

bool MediaElementSession::isMainContentForPurposesOfAutoplayEvents() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    return isElementMainContentForPurposesOfAutoplay(*element, false);
}

Markable<MonotonicTime> MediaElementSession::mostRecentUserInteractionTime() const
{
    return m_mostRecentUserInteractionTime;
}

bool MediaElementSession::wantsToObserveViewportVisibilityForMediaControls() const
{
    return isLargeEnoughForMainContent(MediaSessionMainContentPurpose::MediaControls);
}

bool MediaElementSession::wantsToObserveViewportVisibilityForAutoplay() const
{
    if (RefPtr element = m_element.get())
        return element->isVideo();
    return false;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaElementSession::showPlaybackTargetPicker()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    RefPtr element = m_element.get();
    if (!element)
        return;

    Ref document = element->document();
    if (m_restrictions & RequireUserGestureToShowPlaybackTargetPicker && !document->processingUserGestureForMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning early because of permissions");
        return;
    }

    if (!document->page()) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning early because page is NULL");
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    if (element->readyState() < HTMLMediaElementEnums::HAVE_METADATA) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning early because element is not playable");
        return;
    }
#endif

    auto& audioSession = AudioSession::singleton();
    document->showPlaybackTargetPicker(*this, is<HTMLVideoElement>(m_element), audioSession.routeSharingPolicy(), audioSession.routingContextUID());
}

bool MediaElementSession::hasWirelessPlaybackTargets() const
{
    INFO_LOG(LOGIDENTIFIER, "returning ", m_hasPlaybackTargets);

    return m_hasPlaybackTargets;
}

bool MediaElementSession::wirelessVideoPlaybackDisabled() const
{
    RefPtr element = m_element.get();
    if (!element)
        return true;

    if (!element->document().settings().allowsAirPlayForMediaPlayback()) {
        INFO_LOG(LOGIDENTIFIER, "returning TRUE because of settings");
        return true;
    }

    if (element->hasAttributeWithoutSynchronization(HTMLNames::webkitwirelessvideoplaybackdisabledAttr)) {
        INFO_LOG(LOGIDENTIFIER, "returning TRUE because of attribute");
        return true;
    }

#if PLATFORM(IOS_FAMILY)
    auto& legacyAirplayAttributeValue = element->attributeWithoutSynchronization(HTMLNames::webkitairplayAttr);
    if (equalLettersIgnoringASCIICase(legacyAirplayAttributeValue, "deny"_s)) {
        INFO_LOG(LOGIDENTIFIER, "returning TRUE because of legacy attribute");
        return true;
    }
    if (equalLettersIgnoringASCIICase(legacyAirplayAttributeValue, "allow"_s)) {
        INFO_LOG(LOGIDENTIFIER, "returning FALSE because of legacy attribute");
        return false;
    }
#endif

    if (element->document().settings().remotePlaybackEnabled() && element->hasAttributeWithoutSynchronization(HTMLNames::disableremoteplaybackAttr)) {
        LOG(Media, "MediaElementSession::wirelessVideoPlaybackDisabled - returning TRUE because of RemotePlayback attribute");
        return true;
    }

    RefPtr player = element->player();
    if (!player)
        return true;

    bool disabled = player->wirelessVideoPlaybackDisabled();
    INFO_LOG(LOGIDENTIFIER, "returning ", disabled, " because media engine says so");

    return disabled;
}

void MediaElementSession::setWirelessVideoPlaybackDisabled(bool disabled)
{
    if (disabled)
        addBehaviorRestriction(WirelessVideoPlaybackDisabled);
    else
        removeBehaviorRestriction(WirelessVideoPlaybackDisabled);

    RefPtr element = m_element.get();
    if (!element)
        return;

    RefPtr player = element->player();
    if (!player)
        return;

    INFO_LOG(LOGIDENTIFIER, disabled);
    player->setWirelessVideoPlaybackDisabled(disabled);
}

void MediaElementSession::setHasPlaybackTargetAvailabilityListeners(bool hasListeners)
{
    INFO_LOG(LOGIDENTIFIER, hasListeners);

#if PLATFORM(IOS_FAMILY)
    m_hasPlaybackTargetAvailabilityListeners = hasListeners;
    if (RefPtr manager = sessionManager())
        manager->configureWirelessTargetMonitoring();
#else
    UNUSED_PARAM(hasListeners);
    if (RefPtr element = m_element.get())
        element->document().playbackTargetPickerClientStateDidChange(*this, element->mediaState());
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

    INFO_LOG(LOGIDENTIFIER, hasTargets);

    m_hasPlaybackTargets = hasTargets;
    m_targetAvailabilityChangedTimer.startOneShot(0_s);
}

bool MediaElementSession::isPlayingToWirelessPlaybackTarget() const
{
#if !PLATFORM(IOS_FAMILY)
    if (!m_playbackTarget || !m_playbackTarget->hasActiveRoute())
        return false;
#endif

    return client().isPlayingToWirelessPlaybackTarget();
}

void MediaElementSession::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    INFO_LOG(LOGIDENTIFIER, shouldPlay);
    m_shouldPlayToPlaybackTarget = shouldPlay;
    updateClientDataBuffering();
    client().setShouldPlayToPlaybackTarget(shouldPlay);
}

void MediaElementSession::playbackTargetPickerWasDismissed()
{
    INFO_LOG(LOGIDENTIFIER);
    client().playbackTargetPickerWasDismissed();
}

void MediaElementSession::audioSessionCategoryChanged(AudioSessionCategory category, AudioSessionMode mode, RouteSharingPolicy policy)
{
    if (RefPtr element = m_element.get())
        element->audioSessionCategoryChanged(category, mode, policy);
}

void MediaElementSession::mediaStateDidChange(MediaProducerMediaStateFlags state)
{
    if (RefPtr element = m_element.get())
        element->document().playbackTargetPickerClientStateDidChange(*this, state);
}
#endif

MediaPlayer::Preload MediaElementSession::effectivePreloadForElement() const
{
    MediaPlayer::Preload preload = [&] {
        RefPtr element = m_element.get();
        if (!element)
            return MediaPlayer::Preload::None;

        MediaPlayer::Preload preload = element->effectivePreloadValue();

        if (pageExplicitlyAllowsElementToAutoplayInline(*element))
            return preload;

        if (m_restrictions & AutoPreloadingNotPermitted) {
            if (preload > MediaPlayer::Preload::MetaData)
                return MediaPlayer::Preload::MetaData;
        }

        return preload;
    }();

    ALWAYS_LOG(LOGIDENTIFIER, preload);

    return preload;
}

bool MediaElementSession::requiresFullscreenForVideoPlayback() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    if (pageExplicitlyAllowsElementToAutoplayInline(*element))
        return false;

    if (is<HTMLAudioElement>(*element))
        return false;

    if (element->document().isMediaDocument()) {
        const HTMLVideoElement& videoElement = downcast<const HTMLVideoElement>(*element);
        if (element->readyState() < HTMLVideoElement::HAVE_METADATA || !videoElement.hasEverHadVideo())
            return false;
    }

    if (element->isTemporarilyAllowingInlinePlaybackAfterFullscreen())
        return false;

    if (!element->document().settings().allowsInlineMediaPlayback())
        return true;

    if (!element->document().settings().inlineMediaPlaybackRequiresPlaysInlineAttribute())
        return false;

#if ENABLE(MEDIA_STREAM)
    if (element->hasMediaStreamSrcObject())
        return false;
#endif

    if (element->document().quirks().shouldIgnorePlaysInlineRequirementQuirk())
        return false;

#if PLATFORM(IOS_FAMILY)
    if (WTF::CocoaApplication::isAppleBooks())
        return !element->hasAttributeWithoutSynchronization(HTMLNames::webkit_playsinlineAttr) && !element->hasAttributeWithoutSynchronization(HTMLNames::playsinlineAttr);
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::UnprefixedPlaysInlineAttribute))
        return !element->hasAttributeWithoutSynchronization(HTMLNames::webkit_playsinlineAttr);
#endif

    if (element->document().isMediaDocument() && element->document().ownerElement())
        return false;

    return !element->hasAttributeWithoutSynchronization(HTMLNames::playsinlineAttr);
}

bool MediaElementSession::allowsAutomaticMediaDataLoading() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    if (pageExplicitlyAllowsElementToAutoplayInline(*element))
        return true;

    if (element->document().settings().mediaDataLoadsAutomatically())
        return true;

    return false;
}

void MediaElementSession::mediaEngineUpdated()
{
    INFO_LOG(LOGIDENTIFIER);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (m_restrictions & WirelessVideoPlaybackDisabled)
        setWirelessVideoPlaybackDisabled(true);
    if (m_playbackTarget)
        client().setWirelessPlaybackTarget(*m_playbackTarget.copyRef());
    if (m_shouldPlayToPlaybackTarget)
        client().setShouldPlayToPlaybackTarget(true);
#endif

}

void MediaElementSession::resetPlaybackSessionState()
{
    m_mostRecentUserInteractionTime.reset();
    addBehaviorRestriction(RequireUserGestureToControlControlsManager | RequirePlaybackToControlControlsManager);
}

void MediaElementSession::suspendBuffering()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    updateClientDataBuffering();
}

void MediaElementSession::resumeBuffering()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    updateClientDataBuffering();
}

bool MediaElementSession::bufferingSuspended() const
{
    RefPtr element = m_element.get();
    if (!element)
        return true;

    if (RefPtr page = element->document().page())
        return page->mediaBufferingIsSuspended();
    return true;
}

bool MediaElementSession::allowsPictureInPicture() const
{
    RefPtr element = m_element.get();
    if (element)
        return element->document().settings().allowsPictureInPictureMediaPlayback();
    return false;
}

#if PLATFORM(IOS_FAMILY)
bool MediaElementSession::requiresPlaybackTargetRouteMonitoring() const
{
    RefPtr element = m_element.get();
    if (element)
        return m_hasPlaybackTargetAvailabilityListeners && !element->elementIsHidden();
    return false;
}
#endif

static bool isElementMainContentForPurposesOfAutoplay(const HTMLMediaElement& element, bool shouldHitTestMainFrame)
{
    Ref document = element.document();
    if (!document->hasLivingRenderTree() || document->activeDOMObjectsAreStopped() || element.isSuspended() || !element.hasAudio() || !element.hasVideo())
        return false;

    // Elements which have not yet been laid out, or which are not yet in the DOM, cannot be main content.
    {
        CheckedPtr renderer = element.renderer();
        if (!renderer)
            return false;

        if (!isElementLargeEnoughForMainContent(element, MediaSessionMainContentPurpose::Autoplay))
            return false;

        // Elements which are hidden by style, or have been scrolled out of view, cannot be main content.
        // But elements which have audio & video and are already playing should not stop playing because
        // they are scrolled off the page.
        if (renderer->style().visibility() != Visibility::Visible)
            return false;
        if (renderer->visibleInViewportState() != VisibleInViewportState::Yes && !element.isPlaying())
            return false;
    }

    // Main content elements must be in the main frame.
    if (!document->frame() || !document->frame()->isMainFrame())
        return false;

    RefPtr localMainFrame = document->localMainFrame();
    if (!localMainFrame)
        return false;

    if (!localMainFrame->view() || !localMainFrame->view()->renderView())
        return false;

    if (!shouldHitTestMainFrame)
        return true;

    if (!localMainFrame->document())
        return false;

    // Hit test the area of the main frame where the element appears, to determine if the element is being obscured.
    // Elements which are obscured by other elements cannot be main content.
    IntRect rectRelativeToView = element.boundingBoxInRootViewCoordinates();
    ScrollPosition scrollPosition = localMainFrame->view()->documentScrollPositionRelativeToViewOrigin();
    IntRect rectRelativeToTopDocument(rectRelativeToView.location() + scrollPosition, rectRelativeToView.size());
    OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent, HitTestRequest::Type::IgnoreClipping, HitTestRequest::Type::DisallowUserAgentShadowContent };
    HitTestResult result(rectRelativeToTopDocument.center());

    localMainFrame->protectedDocument()->hitTest(hitType, result);
    result.setToNonUserAgentShadowAncestor();
    return result.targetElement() == &element;
}

static bool isElementRectMostlyInMainFrame(const HTMLMediaElement& element)
{
    if (!element.renderer())
        return false;

    RefPtr documentFrame = element.document().frame();
    if (!documentFrame)
        return false;

    RefPtr mainFrameView = documentFrame->mainFrame().virtualView();
    if (!mainFrameView)
        return false;

    IntRect mainFrameRectAdjustedForScrollPosition = IntRect(-mainFrameView->documentScrollPositionRelativeToViewOrigin(), mainFrameView->contentsSize());
    IntRect elementRectInMainFrame = element.boundingBoxInRootViewCoordinates();
    auto totalElementArea = elementRectInMainFrame.area<RecordOverflow>();
    if (totalElementArea.hasOverflowed())
        return false;

    elementRectInMainFrame.intersect(mainFrameRectAdjustedForScrollPosition);

    return elementRectInMainFrame.area() > totalElementArea / 2;
}

static bool isElementLargeRelativeToMainFrame(const HTMLMediaElement& element)
{
    static const double minimumPercentageOfMainFrameAreaForMainContent = 0.9;
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return false;

    RefPtr documentFrame = element.document().frame();
    if (!documentFrame)
        return false;

    RefPtr mainFrameView = documentFrame->mainFrame().virtualView();
    if (!mainFrameView)
        return false;

    auto maxVisibleClientWidth = std::min(renderer->clientWidth().toInt(), mainFrameView->visibleWidth());
    auto maxVisibleClientHeight = std::min(renderer->clientHeight().toInt(), mainFrameView->visibleHeight());

    return maxVisibleClientWidth * maxVisibleClientHeight > minimumPercentageOfMainFrameAreaForMainContent * mainFrameView->visibleWidth() * mainFrameView->visibleHeight();
}

static bool isElementLargeEnoughForMainContent(const HTMLMediaElement& element, MediaSessionMainContentPurpose purpose)
{
    static const double elementMainContentAreaMinimum = 400 * 300;
    static const double maximumAspectRatio = purpose == MediaSessionMainContentPurpose::MediaControls ? 3 : 1.8;
    static const double minimumAspectRatio = .5; // Slightly smaller than 9:16.

    // Elements which have not yet been laid out, or which are not yet in the DOM, cannot be main content.
    CheckedPtr renderer = element.renderer();
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

static bool isElementLongEnoughForMainContent(const HTMLMediaElement& element)
{
    // Derived from the duration of the "You've got mail!" AOL sound:
    static constexpr MediaTime YouveGotMailDuration = MediaTime(95, 100);

    if (element.readyState() < HTMLMediaElementEnums::ReadyState::HAVE_METADATA)
        return false;

    return element.durationMediaTime() > YouveGotMailDuration;
}

void MediaElementSession::mainContentCheckTimerFired()
{
    if (!hasBehaviorRestriction(OverrideUserGestureRequirementForMainContent))
        return;

    updateIsMainContent();
}

bool MediaElementSession::updateIsMainContent() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    if (element->isSuspended())
        return false;

    bool wasMainContent = m_isMainContent;
    m_isMainContent = isElementMainContentForPurposesOfAutoplay(*element, true);

    if (m_isMainContent != wasMainContent)
        element->updateShouldPlay();

    return m_isMainContent;
}

bool MediaElementSession::allowsPlaybackControlsForAutoplayingAudio() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

    auto page = element->document().page();
    return page && page->allowsPlaybackControlsForAutoplayingAudio();
}

#if ENABLE(MEDIA_SESSION)
#if ENABLE(MEDIA_STREAM)
static bool isDocumentPlayingSeveralMediaStreamsAndCapturing(Document& document)
{
    // We restrict to capturing document for now, until we have a good way to state to the UIProcess application that audio rendering is muted from here.
    RefPtr page = document.page();
    return document.activeMediaElementsWithMediaStreamCount() > 1 && page && MediaProducer::isCapturing(page->mediaState());
}

static bool processRemoteControlCommandIfPlayingMediaStreams(Document& document, PlatformMediaSession::RemoteControlCommandType commandType)
{
    RefPtr page = document.page();
    if (!page)
        return false;

    if (!isDocumentPlayingSeveralMediaStreamsAndCapturing(document))
        return false;

    WebCore::MediaProducerMutedStateFlags mutedState;
    mutedState.add(WebCore::MediaProducerMutedState::AudioIsMuted);
    mutedState.add(WebCore::MediaProducer::AudioAndVideoCaptureIsMuted);
    mutedState.add(WebCore::MediaProducerMutedState::ScreenCaptureIsMuted);

    switch (commandType) {
    case PlatformMediaSession::RemoteControlCommandType::PlayCommand:
        page->setMuted({ });
        return true;
    case PlatformMediaSession::RemoteControlCommandType::StopCommand:
    case PlatformMediaSession::RemoteControlCommandType::PauseCommand:
        page->setMuted(mutedState);
        return true;
    case PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand:
        if (page->mutedState().containsAny(mutedState))
            page->setMuted({ });
        else
            page->setMuted(mutedState);
        return true;
    default:
        break;
    }
    return false;
}
#endif

void MediaElementSession::didReceiveRemoteControlCommand(RemoteControlCommandType commandType, const RemoteCommandArgument& argument)
{
    RefPtr element = m_element.get();
    if (!element)
        return;

    RefPtr session = mediaSession();
    if (!session || !session->hasActiveActionHandlers()) {
#if ENABLE(MEDIA_STREAM)
        if (processRemoteControlCommandIfPlayingMediaStreams(element->document(), commandType))
            return;
#endif
        PlatformMediaSession::didReceiveRemoteControlCommand(commandType, argument);
        return;
    }

    MediaSessionActionDetails actionDetails;
    switch (commandType) {
    case RemoteControlCommandType::NoCommand:
        return;
    case RemoteControlCommandType::PlayCommand:
        actionDetails.action = MediaSessionAction::Play;
        break;
    case RemoteControlCommandType::PauseCommand:
        actionDetails.action = MediaSessionAction::Pause;
        break;
    case RemoteControlCommandType::StopCommand:
        actionDetails.action = MediaSessionAction::Stop;
        break;
    case RemoteControlCommandType::TogglePlayPauseCommand:
        actionDetails.action = element->paused() ? MediaSessionAction::Play : MediaSessionAction::Pause;
        break;
    case RemoteControlCommandType::BeginScrubbingCommand:
        m_isScrubbing = true;
        return;
    case RemoteControlCommandType::EndScrubbingCommand:
        m_isScrubbing = false;
        return;
    case RemoteControlCommandType::SeekToPlaybackPositionCommand:
        ASSERT(argument.time);
        if (!argument.time)
            return;
        actionDetails.action = MediaSessionAction::Seekto;
        actionDetails.seekTime = argument.time.value();
        actionDetails.fastSeek = m_isScrubbing;
        break;
    case RemoteControlCommandType::SkipForwardCommand:
        if (argument.time)
            actionDetails.seekOffset = argument.time.value();
        actionDetails.action = MediaSessionAction::Seekforward;
        break;
    case RemoteControlCommandType::SkipBackwardCommand:
        if (argument.time)
            actionDetails.seekOffset = argument.time.value();
        actionDetails.action = MediaSessionAction::Seekbackward;
        break;
    case RemoteControlCommandType::NextTrackCommand:
        actionDetails.action = MediaSessionAction::Nexttrack;
        break;
    case RemoteControlCommandType::PreviousTrackCommand:
        actionDetails.action = MediaSessionAction::Previoustrack;
        break;
    case RemoteControlCommandType::BeginSeekingBackwardCommand:
    case RemoteControlCommandType::EndSeekingBackwardCommand:
    case RemoteControlCommandType::BeginSeekingForwardCommand:
    case RemoteControlCommandType::EndSeekingForwardCommand:
        ASSERT_NOT_REACHED();
        return;
    }

    session->callActionHandler(actionDetails);
}
#endif

bool MediaElementSession::hasNowPlayingInfo() const
{
    RefPtr element = m_element.get();
    if (!element)
        return false;

#if ENABLE(MEDIA_SESSION)
    if (!canShowControlsManager(MediaElementSession::PlaybackControlsPurpose::NowPlaying))
        return false;

#if ENABLE(MEDIA_STREAM)
    RefPtr session = mediaSession();
    if (element->hasMediaStreamSrcObject() && (!session || (!session->hasActiveActionHandlers() && !session->metadata())))
        return false;
#endif // ENABLE(MEDIA_STREAM)
#endif // ENABLE(MEDIA_SESSION)

    return true;
}

std::optional<NowPlayingInfo> MediaElementSession::computeNowPlayingInfo() const
{
    if (!hasNowPlayingInfo())
        return { };

    RefPtr element = m_element.get();
    if (!element)
        return { };

    RefPtr page = element->document().page();
    if (!page)
        return { };

    bool allowsNowPlayingControlsVisibility = !page->isVisibleAndActive();
    bool isPlaying = state() == PlatformMediaSession::State::Playing;

    bool supportsSeeking = element->supportsSeeking();
    double rate = 1.0;
    double duration = supportsSeeking ? element->duration() : MediaPlayer::invalidTime();
    double currentTime = element->currentTime();
    if (!std::isfinite(currentTime) || !supportsSeeking)
        currentTime = MediaPlayer::invalidTime();
    auto sourceApplicationIdentifier = element->sourceApplicationIdentifier();
#if PLATFORM(COCOA)
    // FIXME: Eventually, this should be moved into HTMLMediaElement, so all clients
    // will use the same bundle identifier (the presentingApplication, rather than the
    // sourceApplication).
    if (!page->presentingApplicationBundleIdentifier().isNull())
        sourceApplicationIdentifier = page->presentingApplicationBundleIdentifier();
#endif

    NowPlayingInfo info {
        {
            element->mediaSessionTitle(),
            emptyString(),
            emptyString(),
            sourceApplicationIdentifier,
            { }
        },
        duration,
        currentTime,
        rate,
        supportsSeeking,
        element->mediaUniqueIdentifier(),
        isPlaying,
        allowsNowPlayingControlsVisibility,
        element->isVideo()
    };

    if (page->usesEphemeralSession() && !element->document().settings().allowPrivacySensitiveOperationsInNonPersistentDataStores()) {
        info.metadata = { };
        return info;
    }

#if ENABLE(MEDIA_SESSION)
    if (RefPtr session = mediaSession())
        session->updateNowPlayingInfo(info);
#endif

    return info;
}

void MediaElementSession::updateMediaUsageIfChanged()
{
    RefPtr element = m_element.get();
    if (!element)
        return;

    Ref document = element->document();
    RefPtr page = document->page();
    if (!page || page->sessionID().isEphemeral())
        return;

    // Bail out early if the element currently has no source (currentSrc or
    // srcObject) and neither did the previous state, to avoid doing a large
    // amount of unnecessary work below.
    if (!element->hasSource() && (!m_mediaUsageInfo || !m_mediaUsageInfo->hasSource))
        return;

    bool isOutsideOfFullscreen = false;
#if ENABLE(FULLSCREEN_API)
    if (RefPtr documentFullscreen = document->fullscreenIfExists()) {
        if (RefPtr fullscreenElement = document->fullscreen().fullscreenElement())
            isOutsideOfFullscreen = element->isDescendantOf(*fullscreenElement);
    }
#endif
    bool isAudio = client().presentationType() == MediaType::Audio;
    bool isVideo = client().presentationType() == MediaType::Video;
    bool processingUserGesture = document->processingUserGestureForMedia();
    bool isPlaying = element->isPlaying();

    MediaUsageInfo usage = {
        element->currentSrc(),
        element->hasSource(),
        state() == PlatformMediaSession::State::Playing,
        canShowControlsManager(PlaybackControlsPurpose::ControlsManager),
        !page->isVisibleAndActive(),
        element->isSuspended(),
        element->inActiveDocument(),
        element->isFullscreen(),
        element->muted(),
        document->isMediaDocument() && (document->frame() && document->frame()->isMainFrame()),
        isVideo,
        isAudio,
        element->hasVideo(),
        element->hasAudio(),
        element->hasRenderer(),
        isAudio && hasBehaviorRestriction(RequireUserGestureToControlControlsManager) && !processingUserGesture,
        element->hasAudio() && isPlaying && allowsPlaybackControlsForAutoplayingAudio(), // userHasPlayedAudioBefore
        isElementRectMostlyInMainFrame(*element),
        !!playbackStateChangePermitted(MediaPlaybackState::Playing),
        page->mediaPlaybackIsSuspended(),
        document->isMediaDocument() && !document->ownerElement(),
        pageExplicitlyAllowsElementToAutoplayInline(*element),
        requiresFullscreenForVideoPlayback() && !fullscreenPermitted(),
        isVideo && hasBehaviorRestriction(RequireUserGestureForVideoRateChange) && !processingUserGesture,
        isAudio && hasBehaviorRestriction(RequireUserGestureForAudioRateChange) && !processingUserGesture && !element->muted() && element->volume(),
        isVideo && hasBehaviorRestriction(RequireUserGestureForVideoDueToLowPowerMode) && !processingUserGesture,
        isVideo && hasBehaviorRestriction(RequireUserGestureForVideoDueToAggressiveThermalMitigation) && !processingUserGesture,
        !hasBehaviorRestriction(RequireUserGestureToControlControlsManager) || processingUserGesture,
        hasBehaviorRestriction(RequirePlaybackToControlControlsManager) && !isPlaying,
        element->hasEverNotifiedAboutPlaying(),
        isOutsideOfFullscreen,
        isLargeEnoughForMainContent(MediaSessionMainContentPurpose::MediaControls),
#if PLATFORM(COCOA) && !HAVE(CGS_FIX_FOR_RADAR_97530095)
        element->isVisibleInViewport()
#endif
    };

    if (m_mediaUsageInfo && *m_mediaUsageInfo == usage)
        return;

    m_mediaUsageInfo = WTFMove(usage);

#if ENABLE(MEDIA_USAGE)
    addMediaUsageManagerSessionIfNecessary();
    page->chrome().client().updateMediaUsageManagerSessionState(mediaSessionIdentifier(), *m_mediaUsageInfo);
#endif
}

String convertEnumerationToString(const MediaPlaybackDenialReason enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 4> values {
        MAKE_STATIC_STRING_IMPL("UserGestureRequired"),
        MAKE_STATIC_STRING_IMPL("FullscreenRequired"),
        MAKE_STATIC_STRING_IMPL("PageConsentRequired"),
        MAKE_STATIC_STRING_IMPL("InvalidState"),
    };
    static_assert(static_cast<size_t>(MediaPlaybackDenialReason::UserGestureRequired) == 0, "MediaPlaybackDenialReason::UserGestureRequired is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlaybackDenialReason::FullscreenRequired) == 1, "MediaPlaybackDenialReason::FullscreenRequired is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlaybackDenialReason::PageConsentRequired) == 2, "MediaPlaybackDenialReason::PageConsentRequired is not 2 as expected");
    static_assert(static_cast<size_t>(MediaPlaybackDenialReason::InvalidState) == 3, "MediaPlaybackDenialReason::InvalidState is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

MediaSession* MediaElementSession::mediaSession() const
{
#if ENABLE(MEDIA_SESSION)
    RefPtr element = m_element.get();
    if (!element)
        return nullptr;

    auto* window = element->document().window();
    if (!window)
        return nullptr;
    return &NavigatorMediaSession::mediaSession(window->protectedNavigator());
#else
    return nullptr;
#endif
}

void MediaElementSession::ensureIsObservingMediaSession()
{
#if ENABLE(MEDIA_SESSION)
    auto* session = mediaSession();
    if (!session || m_observer)
        return;
    m_observer = MediaElementSessionObserver::create(*this, *session);
#endif
}

void MediaElementSession::metadataChanged(const RefPtr<MediaMetadata>&)
{
    clientCharacteristicsChanged(false);
}

void MediaElementSession::positionStateChanged(const std::optional<MediaPositionState>&)
{
    clientCharacteristicsChanged(false);
}

void MediaElementSession::playbackStateChanged(MediaSessionPlaybackState)
{
}

void MediaElementSession::actionHandlersChanged()
{
    clientCharacteristicsChanged(false);
}

void MediaElementSession::clientCharacteristicsChanged(bool positionChanged)
{
#if ENABLE(MEDIA_SESSION)
    RefPtr element = m_element.get();
    auto* session = mediaSession();
    if (element && positionChanged && session) {
        auto positionState = session->positionState();
        if (positionState)
            session->setPositionState(MediaPositionState { positionState->duration, positionState->playbackRate, element->currentTime() });
    }
#endif
    PlatformMediaSession::clientCharacteristicsChanged(positionChanged);
}

#if !RELEASE_LOG_DISABLED
String MediaElementSession::descriptionForTrack(const AudioTrack& track)
{
    if (track.configuration().isProtected())
        return makeString(track.configuration().codec(), " protected"_s);
    return track.configuration().codec();
}

String MediaElementSession::descriptionForTrack(const VideoTrack& track)
{
    StringBuilder builder;

    builder.append(track.configuration().width(), 'x', track.configuration().height());
    if (!track.configuration().codec().isEmpty())
        builder.append(' ', track.configuration().codec());
    if (track.configuration().isProtected())
        builder.append(" protected"_s);
    if (track.configuration().spatialVideoMetadata())
        builder.append(" spatial"_s);
    if (auto metadata = track.configuration().videoProjectionMetadata())
        builder.append(' ', convertEnumerationToString(metadata->kind));

    return builder.toString();
}

String MediaElementSession::description() const
{
    RefPtr element = m_element.get();
    if (!element)
        return "null"_s;

    StringBuilder builder;
    builder.append(PlatformMediaSession::description());

    builder.append(", "_s, element->localizedSourceType());

    if (RefPtr videoTracks = element->videoTracks()) {
        if (RefPtr selectedVideoTrack = videoTracks->selectedItem())
            builder.append(", "_s, descriptionForTrack(*selectedVideoTrack));
    }

    if (RefPtr audioTracks = element->audioTracks()) {
        if (RefPtr enabledAudioTrack = audioTracks->firstEnabled())
            builder.append(", "_s, descriptionForTrack(*enabledAudioTrack));
    }

    if (RefPtr textTracks = element->textTracks()) {
        for (unsigned i = 0, length = textTracks->length(); i < length; ++i) {
            RefPtr textTrack = textTracks->item(i);
            if (textTrack->mode() != TextTrack::Mode::Showing)
                continue;
            builder.append(", "_s, textTrack->kind(), ' ', textTrack->language());
        }
    }

    return builder.toString();
}
#endif

}

#endif // ENABLE(VIDEO)
