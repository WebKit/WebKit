/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLMediaElement.h"

#if ENABLE(VIDEO)

#include "ApplicationCacheHost.h"
#include "ApplicationCacheResource.h"
#include "Attribute.h"
#include "Blob.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "ChromeClient.h"
#include "CommonVM.h"
#include "ContentSecurityPolicy.h"
#include "ContentType.h"
#include "CookieJar.h"
#include "DeprecatedGlobalSettings.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ElementChildIterator.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "HTMLParserIdioms.h"
#include "HTMLSourceElement.h"
#include "HTMLVideoElement.h"
#include "InspectorInstrumentation.h"
#include "JSDOMException.h"
#include "JSDOMPromiseDeferred.h"
#include "JSHTMLMediaElement.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaController.h"
#include "MediaControls.h"
#include "MediaDocument.h"
#include "MediaError.h"
#include "MediaFragmentURIParser.h"
#include "MediaList.h"
#include "MediaPlayer.h"
#include "MediaQueryEvaluator.h"
#include "MediaResourceLoader.h"
#include "NetworkingContext.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformMediaSessionManager.h"
#include "ProgressTracker.h"
#include "PublicSuffix.h"
#include "Quirks.h"
#include "RenderLayerCompositor.h"
#include "RenderTheme.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "ResourceLoadInfo.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ScriptSourceCode.h"
#include "SecurityOriginData.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TimeRanges.h"
#include "UserContentController.h"
#include "UserGestureIndicator.h"
#include "VideoPlaybackQuality.h"
#include <JavaScriptCore/Uint8Array.h>
#include <limits>
#include <pal/SessionID.h>
#include <pal/system/SleepDisabler.h>
#include <wtf/Algorithms.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Language.h>
#include <wtf/MathExtras.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/Ref.h>
#include <wtf/text/CString.h>

#if ENABLE(VIDEO_TRACK)
#include "AudioTrackList.h"
#include "HTMLTrackElement.h"
#include "InbandGenericTextTrack.h"
#include "InbandTextTrackPrivate.h"
#include "InbandWebVTTTextTrack.h"
#include "RuntimeEnabledFeatures.h"
#include "TextTrackCueList.h"
#include "TextTrackList.h"
#include "VideoTrackList.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioSourceProvider.h"
#include "MediaElementAudioSourceNode.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "RuntimeApplicationChecks.h"
#include "VideoFullscreenInterfaceAVKit.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "WebKitPlaybackTargetAvailabilityEvent.h"
#endif

#if ENABLE(MEDIA_SESSION)
#include "MediaSession.h"
#endif

#if ENABLE(MEDIA_SOURCE)
#include "DOMWindow.h"
#include "MediaSource.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "DOMURL.h"
#include "MediaStream.h"
#include "MediaStreamRegistry.h"
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include "WebKitMediaKeyNeededEvent.h"
#include "WebKitMediaKeys.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "MediaEncryptedEvent.h"
#include "MediaKeys.h"
#endif

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
#include "JSMediaControlsHost.h"
#include "MediaControlsHost.h"
#include <JavaScriptCore/ScriptObject.h>
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "NotImplemented.h"
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "VideoFullscreenModel.h"
#endif

namespace WTF {
template <>
struct LogArgument<URL> {
    static String toString(const URL& url)
    {
#if !LOG_DISABLED
        static const unsigned maximumURLLengthForLogging = 512;

        if (url.string().length() < maximumURLLengthForLogging)
            return url.string();
        return url.string().substring(0, maximumURLLengthForLogging) + "...";
#else
        UNUSED_PARAM(url);
        return "[url]";
#endif
    }
};
}


namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLMediaElement);

using namespace PAL;

static const Seconds SeekRepeatDelay { 100_ms };
static const double SeekTime = 0.2;
static const Seconds ScanRepeatDelay { 1.5_s };
static const double ScanMaximumRate = 8;
static const double AutoplayInterferenceTimeThreshold = 10;
static const Seconds hideMediaControlsAfterEndedDelay { 6_s };

#ifndef LOG_CACHED_TIME_WARNINGS
// Default to not logging warnings about excessive drift in the cached media time because it adds a
// fair amount of overhead and logging.
#define LOG_CACHED_TIME_WARNINGS 0
#endif

#if ENABLE(MEDIA_SOURCE)
// URL protocol used to signal that the media source API is being used.
static const char* mediaSourceBlobProtocol = "blob";
#endif

#if ENABLE(MEDIA_STREAM)
// URL protocol used to signal that the media stream API is being used.
static const char* mediaStreamBlobProtocol = "blob";
#endif

using namespace HTMLNames;

String convertEnumerationToString(HTMLMediaElement::ReadyState enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("HAVE_NOTHING"),
        MAKE_STATIC_STRING_IMPL("HAVE_METADATA"),
        MAKE_STATIC_STRING_IMPL("HAVE_CURRENT_DATA"),
        MAKE_STATIC_STRING_IMPL("HAVE_FUTURE_DATA"),
        MAKE_STATIC_STRING_IMPL("HAVE_ENOUGH_DATA"),
    };
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_NOTHING) == 0, "HTMLMediaElement::HAVE_NOTHING is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_METADATA) == 1, "HTMLMediaElement::HAVE_METADATA is not 1 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_CURRENT_DATA) == 2, "HTMLMediaElement::HAVE_CURRENT_DATA is not 2 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_FUTURE_DATA) == 3, "HTMLMediaElement::HAVE_FUTURE_DATA is not 3 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_ENOUGH_DATA) == 4, "HTMLMediaElement::HAVE_ENOUGH_DATA is not 4 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(HTMLMediaElement::NetworkState enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("NETWORK_EMPTY"),
        MAKE_STATIC_STRING_IMPL("NETWORK_IDLE"),
        MAKE_STATIC_STRING_IMPL("NETWORK_LOADING"),
        MAKE_STATIC_STRING_IMPL("NETWORK_NO_SOURCE"),
    };
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::NETWORK_EMPTY) == 0, "HTMLMediaElementEnums::NETWORK_EMPTY is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::NETWORK_IDLE) == 1, "HTMLMediaElementEnums::NETWORK_IDLE is not 1 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::NETWORK_LOADING) == 2, "HTMLMediaElementEnums::NETWORK_LOADING is not 2 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::NETWORK_NO_SOURCE) == 3, "HTMLMediaElementEnums::NETWORK_NO_SOURCE is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(HTMLMediaElement::AutoplayEventPlaybackState enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("PreventedAutoplay"),
        MAKE_STATIC_STRING_IMPL("StartedWithUserGesture"),
        MAKE_STATIC_STRING_IMPL("StartedWithoutUserGesture"),
    };
    static_assert(static_cast<size_t>(HTMLMediaElement::AutoplayEventPlaybackState::None) == 0, "AutoplayEventPlaybackState::None is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::AutoplayEventPlaybackState::PreventedAutoplay) == 1, "AutoplayEventPlaybackState::PreventedAutoplay is not 1 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::AutoplayEventPlaybackState::StartedWithUserGesture) == 2, "AutoplayEventPlaybackState::StartedWithUserGesture is not 2 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::AutoplayEventPlaybackState::StartedWithoutUserGesture) == 3, "AutoplayEventPlaybackState::StartedWithoutUserGesture is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

typedef HashMap<Document*, HashSet<HTMLMediaElement*>> DocumentElementSetMap;
static DocumentElementSetMap& documentToElementSetMap()
{
    static NeverDestroyed<DocumentElementSetMap> map;
    return map;
}

static void addElementToDocumentMap(HTMLMediaElement& element, Document& document)
{
    DocumentElementSetMap& map = documentToElementSetMap();
    HashSet<HTMLMediaElement*> set = map.take(&document);
    set.add(&element);
    map.add(&document, set);
}

static void removeElementFromDocumentMap(HTMLMediaElement& element, Document& document)
{
    DocumentElementSetMap& map = documentToElementSetMap();
    HashSet<HTMLMediaElement*> set = map.take(&document);
    set.remove(&element);
    if (!set.isEmpty())
        map.add(&document, set);
}

#if ENABLE(VIDEO_TRACK)

class TrackDisplayUpdateScope {
public:
    TrackDisplayUpdateScope(HTMLMediaElement& element)
        : m_element(element)
    {
        m_element.beginIgnoringTrackDisplayUpdateRequests();
    }
    ~TrackDisplayUpdateScope()
    {
        m_element.endIgnoringTrackDisplayUpdateRequests();
    }

private:
    HTMLMediaElement& m_element;
};

#endif

struct HTMLMediaElement::TrackGroup {
    enum GroupKind { CaptionsAndSubtitles, Description, Chapter, Metadata, Other };

    TrackGroup(GroupKind kind)
        : kind(kind)
    {
    }

    Vector<RefPtr<TextTrack>> tracks;
    RefPtr<TextTrack> visibleTrack;
    RefPtr<TextTrack> defaultTrack;
    GroupKind kind;
    bool hasSrcLang { false };
};

HashSet<HTMLMediaElement*>& HTMLMediaElement::allMediaElements()
{
    static NeverDestroyed<HashSet<HTMLMediaElement*>> elements;
    return elements;
}

#if ENABLE(MEDIA_SESSION)
typedef HashMap<uint64_t, HTMLMediaElement*> IDToElementMap;

static IDToElementMap& elementIDsToElements()
{
    static NeverDestroyed<IDToElementMap> map;
    return map;
}

HTMLMediaElement* HTMLMediaElement::elementWithID(uint64_t id)
{
    if (id == HTMLMediaElementInvalidID)
        return nullptr;

    return elementIDsToElements().get(id);
}

static uint64_t nextElementID()
{
    static uint64_t elementID = 0;
    return ++elementID;
}
#endif

struct MediaElementSessionInfo {
    const MediaElementSession* session;
    MediaElementSession::PlaybackControlsPurpose purpose;

    MonotonicTime timeOfLastUserInteraction;
    bool canShowControlsManager : 1;
    bool isVisibleInViewportOrFullscreen : 1;
    bool isLargeEnoughForMainContent : 1;
    bool isPlayingAudio : 1;
};

static MediaElementSessionInfo mediaElementSessionInfoForSession(const MediaElementSession& session, MediaElementSession::PlaybackControlsPurpose purpose)
{
    const HTMLMediaElement& element = session.element();
    return {
        &session,
        purpose,
        session.mostRecentUserInteractionTime(),
        session.canShowControlsManager(purpose),
        element.isFullscreen() || element.isVisibleInViewport(),
        session.isLargeEnoughForMainContent(MediaSessionMainContentPurpose::MediaControls),
        element.isPlaying() && element.hasAudio() && !element.muted()
    };
}

static bool preferMediaControlsForCandidateSessionOverOtherCandidateSession(const MediaElementSessionInfo& session, const MediaElementSessionInfo& otherSession)
{
    MediaElementSession::PlaybackControlsPurpose purpose = session.purpose;
    ASSERT(purpose == otherSession.purpose);

    // For the controls manager, prioritize visible media over offscreen media.
    if (purpose == MediaElementSession::PlaybackControlsPurpose::ControlsManager && session.isVisibleInViewportOrFullscreen != otherSession.isVisibleInViewportOrFullscreen)
        return session.isVisibleInViewportOrFullscreen;

    // For Now Playing, prioritize elements that would normally satisfy main content.
    if (purpose == MediaElementSession::PlaybackControlsPurpose::NowPlaying && session.isLargeEnoughForMainContent != otherSession.isLargeEnoughForMainContent)
        return session.isLargeEnoughForMainContent;

    // As a tiebreaker, prioritize elements that the user recently interacted with.
    return session.timeOfLastUserInteraction > otherSession.timeOfLastUserInteraction;
}

static bool mediaSessionMayBeConfusedWithMainContent(const MediaElementSessionInfo& session, MediaElementSession::PlaybackControlsPurpose purpose)
{
    if (purpose == MediaElementSession::PlaybackControlsPurpose::NowPlaying)
        return session.isPlayingAudio;

    if (!session.isVisibleInViewportOrFullscreen)
        return false;

    if (!session.isLargeEnoughForMainContent)
        return false;

    // Even if this video is not a candidate, if it is visible to the user and large enough
    // to be main content, it poses a risk for being confused with main content.
    return true;
}

#if !RELEASE_LOG_DISABLED
static uint64_t nextLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber();
    return ++logIdentifier;
}
#endif

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLElement(tagName, document)
    , ActiveDOMObject(&document)
    , m_progressEventTimer(*this, &HTMLMediaElement::progressEventTimerFired)
    , m_playbackProgressTimer(*this, &HTMLMediaElement::playbackProgressTimerFired)
    , m_scanTimer(*this, &HTMLMediaElement::scanTimerFired)
    , m_playbackControlsManagerBehaviorRestrictionsTimer(*this, &HTMLMediaElement::playbackControlsManagerBehaviorRestrictionsTimerFired)
    , m_seekToPlaybackPositionEndedTimer(*this, &HTMLMediaElement::seekToPlaybackPositionEndedTimerFired)
    , m_asyncEventQueue(*this)
    , m_lastTimeUpdateEventMovieTime(MediaTime::positiveInfiniteTime())
    , m_firstTimePlaying(true)
    , m_playing(false)
    , m_isWaitingUntilMediaCanStart(false)
    , m_shouldDelayLoadEvent(false)
    , m_haveFiredLoadedData(false)
    , m_inActiveDocument(true)
    , m_autoplaying(true)
    , m_muted(false)
    , m_explicitlyMuted(false)
    , m_initiallyMuted(false)
    , m_paused(true)
    , m_seeking(false)
    , m_seekRequested(false)
    , m_sentStalledEvent(false)
    , m_sentEndEvent(false)
    , m_pausedInternal(false)
    , m_closedCaptionsVisible(false)
    , m_webkitLegacyClosedCaptionOverride(false)
    , m_completelyLoaded(false)
    , m_havePreparedToPlay(false)
    , m_parsingInProgress(createdByParser)
    , m_shouldBufferData(true)
    , m_elementIsHidden(document.hidden())
    , m_creatingControls(false)
    , m_receivedLayoutSizeChanged(false)
    , m_hasEverNotifiedAboutPlaying(false)
    , m_hasEverHadAudio(false)
    , m_hasEverHadVideo(false)
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    , m_mediaControlsDependOnPageScaleFactor(false)
    , m_haveSetUpCaptionContainer(false)
#endif
    , m_isScrubbingRemotely(false)
#if ENABLE(VIDEO_TRACK)
    , m_tracksAreReady(true)
    , m_haveVisibleTextTrack(false)
    , m_processingPreferenceChange(false)
#endif
#if !RELEASE_LOG_DISABLED
    , m_logger(&document.logger())
    , m_logIdentifier(nextLogIdentifier())
#endif
{
    allMediaElements().add(this);

    ALWAYS_LOG(LOGIDENTIFIER);

    setHasCustomStyleResolveCallbacks();

    InspectorInstrumentation::addEventListenersToNode(*this);
}

void HTMLMediaElement::finishInitialization()
{
    m_mediaSession = std::make_unique<MediaElementSession>(*this);

    m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForFullscreen);
    m_mediaSession->addBehaviorRestriction(MediaElementSession::RequirePageConsentToLoadMedia);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureToAutoplayToExternalDevice);
#endif
    m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager);
    m_mediaSession->addBehaviorRestriction(MediaElementSession::RequirePlaybackToControlControlsManager);

    auto& document = this->document();
    auto* page = document.page();

    if (document.settings().invisibleAutoplayNotPermitted())
        m_mediaSession->addBehaviorRestriction(MediaElementSession::InvisibleAutoplayNotPermitted);

    if (document.ownerElement() || !document.isMediaDocument()) {
        const auto& topDocument = document.topDocument();
        const bool isProcessingUserGesture = processingUserGestureForMedia();
        const bool shouldAudioPlaybackRequireUserGesture = topDocument.audioPlaybackRequiresUserGesture() && !isProcessingUserGesture;
        const bool shouldVideoPlaybackRequireUserGesture = topDocument.videoPlaybackRequiresUserGesture() && !isProcessingUserGesture;

        if (shouldVideoPlaybackRequireUserGesture) {
            m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoRateChange);
            if (document.settings().requiresUserGestureToLoadVideo())
                m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForLoad);
        }

        if (page && page->isLowPowerModeEnabled())
            m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoDueToLowPowerMode);

        if (shouldAudioPlaybackRequireUserGesture)
            m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForAudioRateChange);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (shouldVideoPlaybackRequireUserGesture || shouldAudioPlaybackRequireUserGesture)
            m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureToShowPlaybackTargetPicker);
#endif

        if (!document.settings().mediaDataLoadsAutomatically())
            m_mediaSession->addBehaviorRestriction(MediaElementSession::AutoPreloadingNotPermitted);

        if (document.settings().mainContentUserGestureOverrideEnabled())
            m_mediaSession->addBehaviorRestriction(MediaElementSession::OverrideUserGestureRequirementForMainContent);
    }

#if PLATFORM(IOS_FAMILY)
    if (!document.settings().videoPlaybackRequiresUserGesture() && !document.settings().audioPlaybackRequiresUserGesture()) {
        // Relax RequireUserGestureForFullscreen when videoPlaybackRequiresUserGesture and audioPlaybackRequiresUserGesture is not set:
        m_mediaSession->removeBehaviorRestriction(MediaElementSession::RequireUserGestureForFullscreen);
    }
#endif

#if ENABLE(MEDIA_SESSION)
    m_elementID = nextElementID();
    elementIDsToElements().add(m_elementID, this);

    setSessionInternal(document.defaultMediaSession());
#endif

    registerWithDocument(document);

#if USE(AUDIO_SESSION) && PLATFORM(MAC)
    AudioSession::sharedSession().addMutedStateObserver(this);
#endif

    mediaSession().clientWillBeginAutoplaying();
}

// FIXME: Remove this code once https://webkit.org/b/185284 is fixed.
static unsigned s_destructorCount = 0;

bool HTMLMediaElement::isRunningDestructor()
{
    return !!s_destructorCount;
}

class HTMLMediaElementDestructorScope {
public:
    HTMLMediaElementDestructorScope() { ++s_destructorCount; }
    ~HTMLMediaElementDestructorScope() { --s_destructorCount; }
};

HTMLMediaElement::~HTMLMediaElement()
{
    HTMLMediaElementDestructorScope destructorScope;
    ALWAYS_LOG(LOGIDENTIFIER);

    beginIgnoringTrackDisplayUpdateRequests();
    allMediaElements().remove(this);

    m_asyncEventQueue.close();

    setShouldDelayLoadEvent(false);
    unregisterWithDocument(document());

#if USE(AUDIO_SESSION) && PLATFORM(MAC)
    AudioSession::sharedSession().removeMutedStateObserver(this);
#endif

#if ENABLE(VIDEO_TRACK)
    if (m_audioTracks)
        m_audioTracks->clearElement();
    if (m_textTracks)
        m_textTracks->clearElement();
    if (m_videoTracks)
        m_videoTracks->clearElement();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (hasEventListeners(eventNames().webkitplaybacktargetavailabilitychangedEvent)) {
        m_hasPlaybackTargetAvailabilityListeners = false;
        m_mediaSession->setHasPlaybackTargetAvailabilityListeners(false);
        updateMediaState();
    }
#endif

    if (m_mediaController) {
        m_mediaController->removeMediaElement(*this);
        m_mediaController = nullptr;
    }

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    webkitSetMediaKeys(nullptr);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    if (m_mediaKeys) {
        m_mediaKeys->detachCDMClient(*this);
        if (m_player)
            m_player->cdmInstanceDetached(m_mediaKeys->cdmInstance());
    }
#endif

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (m_isolatedWorld)
        m_isolatedWorld->clearWrappers();
#endif

#if ENABLE(MEDIA_SESSION)
    if (m_session) {
        m_session->removeMediaElement(*this);
        m_session = nullptr;
    }

    elementIDsToElements().remove(m_elementID);
#endif

    m_seekTaskQueue.close();
    m_resumeTaskQueue.close();
    m_promiseTaskQueue.close();
    m_pauseAfterDetachedTaskQueue.close();
    m_playbackControlsManagerBehaviorRestrictionsQueue.close();
    m_resourceSelectionTaskQueue.close();
    m_visibilityChangeTaskQueue.close();
#if ENABLE(ENCRYPTED_MEDIA)
    m_encryptedMediaQueue.close();
#endif

    m_completelyLoaded = true;

    if (m_player) {
        m_player->invalidate();
        m_player = nullptr;
    }

    m_mediaSession = nullptr;
    schedulePlaybackControlsManagerUpdate();
}

static bool needsAutoplayPlayPauseEventsQuirk(const Document& document)
{
    auto* page = document.page();
    if (!page || !page->settings().needsSiteSpecificQuirks())
        return false;

    auto loader = makeRefPtr(document.loader());
    return loader && loader->allowedAutoplayQuirks().contains(AutoplayQuirk::SynthesizedPauseEvents);
}

RefPtr<HTMLMediaElement> HTMLMediaElement::bestMediaElementForShowingPlaybackControlsManager(MediaElementSession::PlaybackControlsPurpose purpose)
{
    auto allSessions = PlatformMediaSessionManager::sharedManager().currentSessionsMatching([] (const PlatformMediaSession& session) {
        return is<MediaElementSession>(session);
    });

    Vector<MediaElementSessionInfo> candidateSessions;
    bool atLeastOneNonCandidateMayBeConfusedForMainContent = false;
    for (auto& session : allSessions) {
        auto mediaElementSessionInfo = mediaElementSessionInfoForSession(downcast<MediaElementSession>(*session), purpose);
        if (mediaElementSessionInfo.canShowControlsManager)
            candidateSessions.append(mediaElementSessionInfo);
        else if (mediaSessionMayBeConfusedWithMainContent(mediaElementSessionInfo, purpose))
            atLeastOneNonCandidateMayBeConfusedForMainContent = true;
    }

    if (!candidateSessions.size())
        return nullptr;

    std::sort(candidateSessions.begin(), candidateSessions.end(), preferMediaControlsForCandidateSessionOverOtherCandidateSession);
    auto strongestSessionCandidate = candidateSessions.first();
    if (!strongestSessionCandidate.isVisibleInViewportOrFullscreen && !strongestSessionCandidate.isPlayingAudio && atLeastOneNonCandidateMayBeConfusedForMainContent)
        return nullptr;

    return &strongestSessionCandidate.session->element();
}

void HTMLMediaElement::registerWithDocument(Document& document)
{
    m_mediaSession->registerWithDocument(document);

    if (m_isWaitingUntilMediaCanStart)
        document.addMediaCanStartListener(this);

#if !PLATFORM(IOS_FAMILY)
    document.registerForMediaVolumeCallbacks(this);
    document.registerForPrivateBrowsingStateChangedCallbacks(this);
#endif

    document.registerForVisibilityStateChangedCallbacks(this);

#if ENABLE(VIDEO_TRACK)
    if (m_requireCaptionPreferencesChangedCallbacks)
        document.registerForCaptionPreferencesChangedCallbacks(this);
#endif

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (m_mediaControlsDependOnPageScaleFactor)
        document.registerForPageScaleFactorChangedCallbacks(this);
    document.registerForUserInterfaceLayoutDirectionChangedCallbacks(*this);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    document.registerForDocumentSuspensionCallbacks(this);
#endif

    document.registerForAllowsMediaDocumentInlinePlaybackChangedCallbacks(*this);

    document.addAudioProducer(this);
    addElementToDocumentMap(*this, document);

#if ENABLE(MEDIA_STREAM)
    document.registerForMediaStreamStateChangeCallbacks(*this);
#endif

    document.addApplicationStateChangeListener(*this);
}

void HTMLMediaElement::unregisterWithDocument(Document& document)
{
    m_mediaSession->unregisterWithDocument(document);

    if (m_isWaitingUntilMediaCanStart)
        document.removeMediaCanStartListener(this);

#if !PLATFORM(IOS_FAMILY)
    document.unregisterForMediaVolumeCallbacks(this);
    document.unregisterForPrivateBrowsingStateChangedCallbacks(this);
#endif

    document.unregisterForVisibilityStateChangedCallbacks(this);

#if ENABLE(VIDEO_TRACK)
    if (m_requireCaptionPreferencesChangedCallbacks)
        document.unregisterForCaptionPreferencesChangedCallbacks(this);
#endif

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (m_mediaControlsDependOnPageScaleFactor)
        document.unregisterForPageScaleFactorChangedCallbacks(this);
    document.unregisterForUserInterfaceLayoutDirectionChangedCallbacks(*this);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    document.unregisterForDocumentSuspensionCallbacks(this);
#endif

    document.unregisterForAllowsMediaDocumentInlinePlaybackChangedCallbacks(*this);

    document.removeAudioProducer(this);
    removeElementFromDocumentMap(*this, document);

#if ENABLE(MEDIA_STREAM)
    document.unregisterForMediaStreamStateChangeCallbacks(*this);
#endif

    document.removeApplicationStateChangeListener(*this);
}

void HTMLMediaElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);
    if (m_shouldDelayLoadEvent) {
        oldDocument.decrementLoadEventDelayCount();
        newDocument.incrementLoadEventDelayCount();
    }

    unregisterWithDocument(oldDocument);
    registerWithDocument(newDocument);

    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    updateShouldAutoplay();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void HTMLMediaElement::prepareForDocumentSuspension()
{
    m_mediaSession->unregisterWithDocument(document());
}

void HTMLMediaElement::resumeFromDocumentSuspension()
{
    m_mediaSession->registerWithDocument(document());
    updateShouldAutoplay();
}
#endif

bool HTMLMediaElement::supportsFocus() const
{
    if (document().isMediaDocument())
        return false;

    // If no controls specified, we should still be able to focus the element if it has tabIndex.
    return controls() ||  HTMLElement::supportsFocus();
}

bool HTMLMediaElement::isMouseFocusable() const
{
    return false;
}

void HTMLMediaElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == srcAttr) {
        // https://html.spec.whatwg.org/multipage/embedded-content.html#location-of-the-media-resource
        // Location of the Media Resource
        // 12 February 2017

        // If a src attribute of a media element is set or changed, the user
        // agent must invoke the media element's media element load algorithm.
        if (!value.isNull())
            prepareForLoad();
    } else if (name == controlsAttr)
        configureMediaControls();
    else if (name == loopAttr)
        updateSleepDisabling();
    else if (name == preloadAttr) {
        if (equalLettersIgnoringASCIICase(value, "none"))
            m_preload = MediaPlayer::None;
        else if (equalLettersIgnoringASCIICase(value, "metadata"))
            m_preload = MediaPlayer::MetaData;
        else {
            // The spec does not define an "invalid value default" but "auto" is suggested as the
            // "missing value default", so use it for everything except "none" and "metadata"
            m_preload = MediaPlayer::Auto;
        }

        // The attribute must be ignored if the autoplay attribute is present
        if (!autoplay() && !m_havePreparedToPlay && m_player)
            m_player->setPreload(m_mediaSession->effectivePreloadForElement());

    } else if (name == mediagroupAttr)
        setMediaGroup(value);
    else if (name == autoplayAttr) {
        if (processingUserGestureForMedia())
            removeBehaviorsRestrictionsAfterFirstUserGesture();
    } else if (name == titleAttr) {
        if (m_mediaSession)
            m_mediaSession->clientCharacteristicsChanged();
    }
    else
        HTMLElement::parseAttribute(name, value);
}

void HTMLMediaElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    m_parsingInProgress = false;

#if ENABLE(VIDEO_TRACK)
    if (childrenOfType<HTMLTrackElement>(*this).first())
        scheduleConfigureTextTracks();
#endif
}

bool HTMLMediaElement::rendererIsNeeded(const RenderStyle& style)
{
    return controls() && HTMLElement::rendererIsNeeded(style);
}

RenderPtr<RenderElement> HTMLMediaElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderMedia>(*this, WTFMove(style));
}

bool HTMLMediaElement::childShouldCreateRenderer(const Node& child) const
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    return hasShadowRootParent(child) && HTMLElement::childShouldCreateRenderer(child);
#else
    if (!hasMediaControls())
        return false;
    // <media> doesn't allow its content, including shadow subtree, to
    // be rendered. So this should return false for most of the children.
    // One exception is a shadow tree built for rendering controls which should be visible.
    // So we let them go here by comparing its subtree root with one of the controls.
    return &mediaControls()->treeScope() == &child.treeScope()
        && hasShadowRootParent(child)
        && HTMLElement::childShouldCreateRenderer(child);
#endif
}

Node::InsertedIntoAncestorResult HTMLMediaElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    INFO_LOG(LOGIDENTIFIER);

    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        setInActiveDocument(true);

    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLMediaElement::didFinishInsertingNode()
{
    Ref<HTMLMediaElement> protectedThis(*this); // prepareForLoad may result in a 'beforeload' event, which can make arbitrary DOM mutations.

    INFO_LOG(LOGIDENTIFIER);

    if (m_inActiveDocument && m_networkState == NETWORK_EMPTY && !attributeWithoutSynchronization(srcAttr).isEmpty())
        prepareForLoad();

    if (!m_explicitlyMuted) {
        m_explicitlyMuted = true;
        m_muted = hasAttributeWithoutSynchronization(mutedAttr);
        m_mediaSession->canProduceAudioChanged();
    }

    configureMediaControls();
}

void HTMLMediaElement::pauseAfterDetachedTask()
{
    // If we were re-inserted into an active document, no need to pause.
    if (m_inActiveDocument)
        return;

    if (hasMediaControls())
        mediaControls()->hide();
    if (m_networkState > NETWORK_EMPTY)
        pause();
    if (m_videoFullscreenMode != VideoFullscreenModeNone)
        exitFullscreen();

    if (!m_player)
        return;

    size_t extraMemoryCost = m_player->extraMemoryCost();
    if (extraMemoryCost > m_reportedExtraMemoryCost) {
        JSC::VM& vm = commonVM();
        JSC::JSLockHolder lock(vm);

        size_t extraMemoryCostDelta = extraMemoryCost - m_reportedExtraMemoryCost;
        m_reportedExtraMemoryCost = extraMemoryCost;
        // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
        // https://bugs.webkit.org/show_bug.cgi?id=142595
        vm.heap.deprecatedReportExtraMemory(extraMemoryCostDelta);
    }
}

void HTMLMediaElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    INFO_LOG(LOGIDENTIFIER);

    setInActiveDocument(false);
    if (removalType.disconnectedFromDocument) {
        // Pause asynchronously to let the operation that removed us finish, in case we get inserted back into a document.
        m_pauseAfterDetachedTaskQueue.enqueueTask(std::bind(&HTMLMediaElement::pauseAfterDetachedTask, this));
    }

    if (m_mediaSession)
        m_mediaSession->clientCharacteristicsChanged();

    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

void HTMLMediaElement::willAttachRenderers()
{
    ASSERT(!renderer());
}

inline void HTMLMediaElement::updateRenderer()
{
    if (auto* renderer = this->renderer())
        renderer->updateFromElement();
}

void HTMLMediaElement::didAttachRenderers()
{
    if (auto* renderer = this->renderer()) {
        renderer->updateFromElement();
        if (m_mediaSession && m_mediaSession->wantsToObserveViewportVisibilityForAutoplay())
            renderer->registerForVisibleInViewportCallback();
    }
    updateShouldAutoplay();
}

void HTMLMediaElement::willDetachRenderers()
{
    if (auto* renderer = this->renderer())
        renderer->unregisterForVisibleInViewportCallback();
}

void HTMLMediaElement::didDetachRenderers()
{
    updateShouldAutoplay();
}

void HTMLMediaElement::didRecalcStyle(Style::Change)
{
    updateRenderer();
}

void HTMLMediaElement::scheduleNextSourceChild()
{
    // Schedule the timer to try the next <source> element WITHOUT resetting state ala prepareForLoad.
    m_resourceSelectionTaskQueue.enqueueTask([this] {
        loadNextSourceChild();
    });
}

void HTMLMediaElement::mediaPlayerActiveSourceBuffersChanged(const MediaPlayer*)
{
    m_hasEverHadAudio |= hasAudio();
    m_hasEverHadVideo |= hasVideo();
}

void HTMLMediaElement::scheduleEvent(const AtomicString& eventName)
{
    auto event = Event::create(eventName, Event::CanBubble::No, Event::IsCancelable::Yes);

    // Don't set the event target, the event queue will set it in GenericEventQueue::timerFired and setting it here
    // will trigger an ASSERT if this element has been marked for deletion.

    m_asyncEventQueue.enqueueEvent(WTFMove(event));
}

void HTMLMediaElement::scheduleResolvePendingPlayPromises()
{
    m_promiseTaskQueue.enqueueTask([this, pendingPlayPromises = WTFMove(m_pendingPlayPromises)] () mutable {
        resolvePendingPlayPromises(WTFMove(pendingPlayPromises));
    });
}

void HTMLMediaElement::scheduleRejectPendingPlayPromises(Ref<DOMException>&& error)
{
    m_promiseTaskQueue.enqueueTask([this, error = WTFMove(error), pendingPlayPromises = WTFMove(m_pendingPlayPromises)] () mutable {
        rejectPendingPlayPromises(WTFMove(pendingPlayPromises), WTFMove(error));
    });
}

void HTMLMediaElement::rejectPendingPlayPromises(PlayPromiseVector&& pendingPlayPromises, Ref<DOMException>&& error)
{
    for (auto& promise : pendingPlayPromises)
        promise.rejectType<IDLInterface<DOMException>>(error);
}

void HTMLMediaElement::resolvePendingPlayPromises(PlayPromiseVector&& pendingPlayPromises)
{
    for (auto& promise : pendingPlayPromises)
        promise.resolve();
}

void HTMLMediaElement::scheduleNotifyAboutPlaying()
{
    m_promiseTaskQueue.enqueueTask([this, pendingPlayPromises = WTFMove(m_pendingPlayPromises)] () mutable {
        notifyAboutPlaying(WTFMove(pendingPlayPromises));
    });
}

void HTMLMediaElement::notifyAboutPlaying(PlayPromiseVector&& pendingPlayPromises)
{
    Ref<HTMLMediaElement> protectedThis(*this); // The 'playing' event can make arbitrary DOM mutations.
    m_playbackStartedTime = currentMediaTime().toDouble();
    m_hasEverNotifiedAboutPlaying = true;
    dispatchEvent(Event::create(eventNames().playingEvent, Event::CanBubble::No, Event::IsCancelable::Yes));
    resolvePendingPlayPromises(WTFMove(pendingPlayPromises));

    schedulePlaybackControlsManagerUpdate();
}

bool HTMLMediaElement::hasEverNotifiedAboutPlaying() const
{
    return m_hasEverNotifiedAboutPlaying;
}

void HTMLMediaElement::scheduleCheckPlaybackTargetCompatability()
{
    if (m_checkPlaybackTargetCompatablityTask.hasPendingTask())
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier, "task scheduled");
    m_checkPlaybackTargetCompatablityTask.scheduleTask([this, logSiteIdentifier] {
        UNUSED_PARAM(logSiteIdentifier);
        ALWAYS_LOG(logSiteIdentifier, "- lambda(), task fired");
        checkPlaybackTargetCompatablity();
    });
}

void HTMLMediaElement::checkPlaybackTargetCompatablity()
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (m_isPlayingToWirelessTarget && !m_player->canPlayToWirelessPlaybackTarget()) {
        INFO_LOG(LOGIDENTIFIER, "calling setShouldPlayToPlaybackTarget(false)");
        m_failedToPlayToWirelessTarget = true;
        m_player->setShouldPlayToPlaybackTarget(false);
    }
#endif
}

MediaError* HTMLMediaElement::error() const
{
    return m_error.get();
}

void HTMLMediaElement::setSrcObject(MediaProvider&& mediaProvider)
{
    // FIXME: Setting the srcObject attribute may cause other changes to the media element's internal state:
    // Specifically, if srcObject is specified, the UA must use it as the source of media, even if the src
    // attribute is also set or children are present. If the value of srcObject is replaced or set to null
    // the UA must re-run the media element load algorithm.
    //
    // https://bugs.webkit.org/show_bug.cgi?id=124896


    // https://www.w3.org/TR/html51/semantics-embedded-content.html#dom-htmlmediaelement-srcobject
    // 4.7.14.2. Location of the media resource
    // srcObject: On setting, it must set the element’s assigned media provider object to the new
    // value, and then invoke the element’s media element load algorithm.
    INFO_LOG(LOGIDENTIFIER);
    m_mediaProvider = WTFMove(mediaProvider);
    prepareForLoad();
}

void HTMLMediaElement::setCrossOrigin(const AtomicString& value)
{
    setAttributeWithoutSynchronization(crossoriginAttr, value);
}

String HTMLMediaElement::crossOrigin() const
{
    return parseCORSSettingsAttribute(attributeWithoutSynchronization(crossoriginAttr));
}

HTMLMediaElement::NetworkState HTMLMediaElement::networkState() const
{
    return m_networkState;
}

String HTMLMediaElement::canPlayType(const String& mimeType) const
{
    MediaEngineSupportParameters parameters;
    ContentType contentType(mimeType);
    parameters.type = contentType;
    parameters.contentTypesRequiringHardwareSupport = mediaContentTypesRequiringHardwareSupport();
    MediaPlayer::SupportsType support = MediaPlayer::supportsType(parameters);
    String canPlay;

    // 4.8.10.3
    switch (support)
    {
        case MediaPlayer::IsNotSupported:
            canPlay = emptyString();
            break;
        case MediaPlayer::MayBeSupported:
            canPlay = "maybe"_s;
            break;
        case MediaPlayer::IsSupported:
            canPlay = "probably"_s;
            break;
    }

    DEBUG_LOG(LOGIDENTIFIER, "[", mimeType, "] -> ", canPlay);

    return canPlay;
}

double HTMLMediaElement::getStartDate() const
{
    if (!m_player)
        return std::numeric_limits<double>::quiet_NaN();
    return m_player->getStartDate().toDouble();
}

void HTMLMediaElement::load()
{
    Ref<HTMLMediaElement> protectedThis(*this); // prepareForLoad may result in a 'beforeload' event, which can make arbitrary DOM mutations.

    INFO_LOG(LOGIDENTIFIER);

    if (processingUserGestureForMedia())
        removeBehaviorsRestrictionsAfterFirstUserGesture();

    prepareForLoad();
    m_resourceSelectionTaskQueue.enqueueTask([this] {
        prepareToPlay();
    });
}

void HTMLMediaElement::prepareForLoad()
{
    // https://html.spec.whatwg.org/multipage/embedded-content.html#media-element-load-algorithm
    // The Media Element Load Algorithm
    // 12 February 2017

    INFO_LOG(LOGIDENTIFIER);

    // 1 - Abort any already-running instance of the resource selection algorithm for this element.
    // Perform the cleanup required for the resource load algorithm to run.
    stopPeriodicTimers();
    m_resourceSelectionTaskQueue.cancelAllTasks();
    // FIXME: Figure out appropriate place to reset LoadTextTrackResource if necessary and set m_pendingActionFlags to 0 here.
    m_sentEndEvent = false;
    m_sentStalledEvent = false;
    m_haveFiredLoadedData = false;
    m_completelyLoaded = false;
    m_havePreparedToPlay = false;
    m_displayMode = Unknown;
    m_currentSrc = URL();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    m_failedToPlayToWirelessTarget = false;
#endif

    m_loadState = WaitingForSource;
    m_currentSourceNode = nullptr;

    if (!document().hasBrowsingContext())
        return;

    createMediaPlayer();

    // 2 - Let pending tasks be a list of all tasks from the media element's media element event task source in one of the task queues.
    // 3 - For each task in pending tasks that would resolve pending play promises or reject pending play promises, immediately resolve or reject those promises in the order the corresponding tasks were queued.
    // 4 - Remove each task in pending tasks from its task queue
    cancelPendingEventsAndCallbacks();

    // 5 - If the media element's networkState is set to NETWORK_LOADING or NETWORK_IDLE, queue
    // a task to fire a simple event named abort at the media element.
    if (m_networkState == NETWORK_LOADING || m_networkState == NETWORK_IDLE)
        scheduleEvent(eventNames().abortEvent);

    // 6 - If the media element's networkState is not set to NETWORK_EMPTY, then run these substeps
    if (m_networkState != NETWORK_EMPTY) {
        // 6.1 - Queue a task to fire a simple event named emptied at the media element.
        scheduleEvent(eventNames().emptiedEvent);

        // 6.2 - If a fetching process is in progress for the media element, the user agent should stop it.
        m_networkState = NETWORK_EMPTY;

        // 6.3 - If the media element’s assigned media provider object is a MediaSource object, then detach it.
#if ENABLE(MEDIA_SOURCE)
        detachMediaSource();
#endif

        // 6.4 - Forget the media element's media-resource-specific tracks.
        forgetResourceSpecificTracks();

        // 6.5 - If readyState is not set to HAVE_NOTHING, then set it to that state.
        m_readyState = HAVE_NOTHING;
        m_readyStateMaximum = HAVE_NOTHING;

        // 6.6 - If the paused attribute is false, then set it to true.
        m_paused = true;

        // 6.7 - If seeking is true, set it to false.
        clearSeeking();

        // 6.8 - Set the current playback position to 0.
        //       Set the official playback position to 0.
        //       If this changed the official playback position, then queue a task to fire a simple event named timeupdate at the media element.
        m_lastSeekTime = MediaTime::zeroTime();
        m_playedTimeRanges = TimeRanges::create();
        // FIXME: Add support for firing this event. e.g., scheduleEvent(eventNames().timeUpdateEvent);

        // 4.9 - Set the initial playback position to 0.
        // FIXME: Make this less subtle. The position only becomes 0 because of the createMediaPlayer() call
        // above.
        refreshCachedTime();

        invalidateCachedTime();

        // 4.10 - Set the timeline offset to Not-a-Number (NaN).
        // 4.11 - Update the duration attribute to Not-a-Number (NaN).

        updateMediaController();
#if ENABLE(VIDEO_TRACK)
        updateActiveTextTrackCues(MediaTime::zeroTime());
#endif
    }

    // 7 - Set the playbackRate attribute to the value of the defaultPlaybackRate attribute.
    setPlaybackRate(defaultPlaybackRate());

    // 8 - Set the error attribute to null and the autoplaying flag to true.
    m_error = nullptr;
    m_autoplaying = true;
    mediaSession().clientWillBeginAutoplaying();

    if (!MediaPlayer::isAvailable())
        noneSupported();
    else {
        // 9 - Invoke the media element's resource selection algorithm.
        // Note, unless the restriction on requiring user action has been removed,
        // do not begin downloading data.
        if (m_mediaSession->dataLoadingPermitted())
            selectMediaResource();
    }

    // 10 - Note: Playback of any previously playing media resource for this element stops.

    configureMediaControls();
}

void HTMLMediaElement::selectMediaResource()
{
    // https://www.w3.org/TR/2016/REC-html51-20161101/semantics-embedded-content.html#resource-selection-algorithm
    // The Resource Selection Algorithm

    // 1. Set the element’s networkState attribute to the NETWORK_NO_SOURCE value.
    m_networkState = NETWORK_NO_SOURCE;

    // 2. Set the element’s show poster flag to true.
    setDisplayMode(Poster);

    // 3. Set the media element’s delaying-the-load-event flag to true (this delays the load event).
    setShouldDelayLoadEvent(true);

    // 4. in parallel await a stable state, allowing the task that invoked this algorithm to continue.
    if (m_resourceSelectionTaskQueue.hasPendingTasks())
        return;

    if (!m_mediaSession->pageAllowsDataLoading()) {
        ALWAYS_LOG(LOGIDENTIFIER, "not allowed to load in background, waiting");
        setShouldDelayLoadEvent(false);
        if (m_isWaitingUntilMediaCanStart)
            return;
        m_isWaitingUntilMediaCanStart = true;
        document().addMediaCanStartListener(this);
        return;
    }

    // Once the page has allowed an element to load media, it is free to load at will. This allows a
    // playlist that starts in a foreground tab to continue automatically if the tab is subsequently
    // put into the background.
    m_mediaSession->removeBehaviorRestriction(MediaElementSession::RequirePageConsentToLoadMedia);

    m_resourceSelectionTaskQueue.enqueueTask([this]  {
        // 5. If the media element’s blocked-on-parser flag is false, then populate the list of pending text tracks.
#if ENABLE(VIDEO_TRACK)
        if (hasMediaControls())
            mediaControls()->changedClosedCaptionsVisibility();

        // HTMLMediaElement::textTracksAreReady will need "... the text tracks whose mode was not in the
        // disabled state when the element's resource selection algorithm last started".
        // FIXME: Update this to match "populate the list of pending text tracks" step.
        m_textTracksWhenResourceSelectionBegan.clear();
        if (m_textTracks) {
            for (unsigned i = 0; i < m_textTracks->length(); ++i) {
                RefPtr<TextTrack> track = m_textTracks->item(i);
                if (track->mode() != TextTrack::Mode::Disabled)
                    m_textTracksWhenResourceSelectionBegan.append(track);
            }
        }
#endif

        enum Mode { None, Object, Attribute, Children };
        Mode mode = None;

        if (m_mediaProvider) {
            // 6. If the media element has an assigned media provider object, then let mode be object.
            mode = Object;
        } else if (hasAttributeWithoutSynchronization(srcAttr)) {
            //    Otherwise, if the media element has no assigned media provider object but has a src attribute, then let mode be attribute.
            mode = Attribute;
            ASSERT(m_player);
            if (!m_player) {
                ERROR_LOG(LOGIDENTIFIER, " - has srcAttr but m_player is not created");
                return;
            }
        } else if (auto firstSource = childrenOfType<HTMLSourceElement>(*this).first()) {
            //    Otherwise, if the media element does not have an assigned media provider object and does not have a src attribute,
            //    but does have a source element child, then let mode be children and let candidate be the first such source element
            //    child in tree order.
            mode = Children;
            m_nextChildNodeToConsider = firstSource;
            m_currentSourceNode = nullptr;
        } else {
            //  Otherwise the media element has no assigned media provider object and has neither a src attribute nor a source
            //  element child: set the networkState to NETWORK_EMPTY, and abort these steps; the synchronous section ends.
            m_loadState = WaitingForSource;
            setShouldDelayLoadEvent(false);
            m_networkState = NETWORK_EMPTY;

            ALWAYS_LOG(LOGIDENTIFIER, "nothing to load");
            return;
        }

        // 7. Set the media element’s networkState to NETWORK_LOADING.
        m_networkState = NETWORK_LOADING;

        // 8. Queue a task to fire a simple event named loadstart at the media element.
        scheduleEvent(eventNames().loadstartEvent);

        // 9. Run the appropriate steps from the following list:
        // ↳ If mode is object
        if (mode == Object) {
            // 1. Set the currentSrc attribute to the empty string.
            m_currentSrc = URL();

            // 2. End the synchronous section, continuing the remaining steps in parallel.
            // 3. Run the resource fetch algorithm with the assigned media provider object.
            switchOn(m_mediaProvider.value(),
#if ENABLE(MEDIA_STREAM)
                [this](RefPtr<MediaStream> stream) { m_mediaStreamSrcObject = stream; },
#endif
#if ENABLE(MEDIA_SOURCE)
                [this](RefPtr<MediaSource> source) { m_mediaSource = source; },
#endif
                [this](RefPtr<Blob> blob) { m_blob = blob; }
            );

            ContentType contentType;
            loadResource(URL(), contentType, String());
            ALWAYS_LOG(LOGIDENTIFIER, "using 'srcObject' property");

            //    If that algorithm returns without aborting this one, then the load failed.
            // 4. Failed with media provider: Reaching this step indicates that the media resource
            //    failed to load. Queue a task to run the dedicated media source failure steps.
            // 5. Wait for the task queued by the previous step to have executed.
            // 6. Abort these steps. The element won’t attempt to load another resource until this
            //    algorithm is triggered again.
            return;
        }

        // ↳ If mode is attribute
        if (mode == Attribute) {
            m_loadState = LoadingFromSrcAttr;

            // 1. If the src attribute’s value is the empty string, then end the synchronous section,
            //    and jump down to the failed with attribute step below.
            // 2. Let absolute URL be the absolute URL that would have resulted from parsing the URL
            //    specified by the src attribute’s value relative to the media element when the src
            //    attribute was last changed.
            URL absoluteURL = getNonEmptyURLAttribute(srcAttr);
            if (absoluteURL.isEmpty()) {
                mediaLoadingFailed(MediaPlayer::FormatError);
                ALWAYS_LOG(LOGIDENTIFIER, "empty 'src'");
                return;
            }

            if (!isSafeToLoadURL(absoluteURL, Complain) || !dispatchBeforeLoadEvent(absoluteURL.string())) {
                mediaLoadingFailed(MediaPlayer::FormatError);
                return;
            }

            // 3. If absolute URL was obtained successfully, set the currentSrc attribute to absolute URL.
            m_currentSrc = absoluteURL;

            // 4. End the synchronous section, continuing the remaining steps in parallel.
            // 5. If absolute URL was obtained successfully, run the resource fetch algorithm with absolute
            //    URL. If that algorithm returns without aborting this one, then the load failed.

            // No type or key system information is available when the url comes
            // from the 'src' attribute so MediaPlayer
            // will have to pick a media engine based on the file extension.
            ContentType contentType;
            loadResource(absoluteURL, contentType, String());
            ALWAYS_LOG(LOGIDENTIFIER, "using 'src' attribute url");

            // 6. Failed with attribute: Reaching this step indicates that the media resource failed to load
            //    or that the given URL could not be resolved. Queue a task to run the dedicated media source failure steps.
            // 7. Wait for the task queued by the previous step to have executed.
            // 8. Abort these steps. The element won’t attempt to load another resource until this algorithm is triggered again.
            return;
        }

        // ↳ Otherwise (mode is children)
        // (Ctd. in loadNextSourceChild())
        loadNextSourceChild();
    });
}

void HTMLMediaElement::loadNextSourceChild()
{
    ContentType contentType;
    String keySystem;
    URL mediaURL = selectNextSourceChild(&contentType, &keySystem, Complain);
    if (!mediaURL.isValid()) {
        waitForSourceChange();
        return;
    }

    // Recreate the media player for the new url
    createMediaPlayer();

    m_loadState = LoadingFromSourceElement;
    loadResource(mediaURL, contentType, keySystem);
}

void HTMLMediaElement::loadResource(const URL& initialURL, ContentType& contentType, const String& keySystem)
{
    ASSERT(initialURL.isEmpty() || isSafeToLoadURL(initialURL, Complain));

    INFO_LOG(LOGIDENTIFIER, initialURL, contentType.raw(), keySystem);

    RefPtr<Frame> frame = document().frame();
    if (!frame) {
        mediaLoadingFailed(MediaPlayer::FormatError);
        return;
    }

    Page* page = frame->page();
    if (!page) {
        mediaLoadingFailed(MediaPlayer::FormatError);
        return;
    }

    URL url = initialURL;
    if (!url.isEmpty() && !frame->loader().willLoadMediaElementURL(url, *this)) {
        mediaLoadingFailed(MediaPlayer::FormatError);
        return;
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (auto documentLoader = makeRefPtr(frame->loader().documentLoader())) {
        if (page->userContentProvider().processContentExtensionRulesForLoad(url, ResourceType::Media, *documentLoader).blockedLoad) {
            mediaLoadingFailed(MediaPlayer::FormatError);
            return;
        }
    }
#endif

    // The resource fetch algorithm
    m_networkState = NETWORK_LOADING;

    // If the URL should be loaded from the application cache, pass the URL of the cached file to the media engine.
    ApplicationCacheResource* resource = nullptr;
    if (!url.isEmpty() && frame->loader().documentLoader()->applicationCacheHost().shouldLoadResourceFromApplicationCache(ResourceRequest(url), resource)) {
        // Resources that are not present in the manifest will always fail to load (at least, after the
        // cache has been primed the first time), making the testing of offline applications simpler.
        if (!resource || resource->path().isEmpty()) {
            mediaLoadingFailed(MediaPlayer::NetworkError);
            return;
        }
    }

    // Log that we started loading a media element.
    page->diagnosticLoggingClient().logDiagnosticMessage(isVideo() ? DiagnosticLoggingKeys::videoKey() : DiagnosticLoggingKeys::audioKey(), DiagnosticLoggingKeys::loadingKey(), ShouldSample::No);

    m_firstTimePlaying = true;

    // Set m_currentSrc *before* changing to the cache URL, the fact that we are loading from the app
    // cache is an internal detail not exposed through the media element API.
    m_currentSrc = url;

    if (resource) {
        url = ApplicationCacheHost::createFileURL(resource->path());
        INFO_LOG(LOGIDENTIFIER, "will load ", url, " from app cache");
    }

    INFO_LOG(LOGIDENTIFIER, "m_currentSrc is ", m_currentSrc);

    startProgressEventTimer();

    bool privateMode = document().page() && document().page()->usesEphemeralSession();
    m_player->setPrivateBrowsingMode(privateMode);

    // Reset display mode to force a recalculation of what to show because we are resetting the player.
    setDisplayMode(Unknown);

    if (!autoplay() && !m_havePreparedToPlay)
        m_player->setPreload(m_mediaSession->effectivePreloadForElement());
    m_player->setPreservesPitch(m_webkitPreservesPitch);

    if (!m_explicitlyMuted) {
        m_explicitlyMuted = true;
        m_muted = hasAttributeWithoutSynchronization(mutedAttr);
        m_mediaSession->canProduceAudioChanged();
    }

    updateVolume();

    bool loadAttempted = false;
#if ENABLE(MEDIA_SOURCE)
    if (!m_mediaSource && url.protocolIs(mediaSourceBlobProtocol))
        m_mediaSource = MediaSource::lookup(url.string());

    if (m_mediaSource) {
        loadAttempted = true;
        if (!m_mediaSource->attachToElement(*this) || !m_player->load(url, contentType, m_mediaSource.get())) {
            // Forget our reference to the MediaSource, so we leave it alone
            // while processing remainder of load failure.
            m_mediaSource = nullptr;
            mediaLoadingFailed(MediaPlayer::FormatError);
        }
    }
#endif

#if ENABLE(MEDIA_STREAM)
    if (!loadAttempted) {
        if (!m_mediaStreamSrcObject && url.protocolIs(mediaStreamBlobProtocol))
            m_mediaStreamSrcObject = MediaStreamRegistry::shared().lookUp(url);

        if (m_mediaStreamSrcObject) {
            loadAttempted = true;
            if (!m_player->load(m_mediaStreamSrcObject->privateStream()))
                mediaLoadingFailed(MediaPlayer::FormatError);
        }
    }
#endif

    if (!loadAttempted && m_blob) {
        loadAttempted = true;
        if (!m_player->load(m_blob->url(), contentType, keySystem))
            mediaLoadingFailed(MediaPlayer::FormatError);
    }

    if (!loadAttempted && !m_player->load(url, contentType, keySystem))
        mediaLoadingFailed(MediaPlayer::FormatError);

    // If there is no poster to display, allow the media engine to render video frames as soon as
    // they are available.
    updateDisplayState();

    updateRenderer();
}

#if ENABLE(VIDEO_TRACK)

static bool trackIndexCompare(TextTrack* a, TextTrack* b)
{
    return a->trackIndex() - b->trackIndex() < 0;
}

static bool eventTimeCueCompare(const std::pair<MediaTime, TextTrackCue*>& a, const std::pair<MediaTime, TextTrackCue*>& b)
{
    // 12 - Sort the tasks in events in ascending time order (tasks with earlier
    // times first).
    if (a.first != b.first)
        return a.first - b.first < MediaTime::zeroTime();

    // If the cues belong to different text tracks, it doesn't make sense to
    // compare the two tracks by the relative cue order, so return the relative
    // track order.
    if (a.second->track() != b.second->track())
        return trackIndexCompare(a.second->track(), b.second->track());

    // 12 - Further sort tasks in events that have the same time by the
    // relative text track cue order of the text track cues associated
    // with these tasks.
    return a.second->isOrderedBefore(b.second);
}

static bool compareCueInterval(const CueInterval& one, const CueInterval& two)
{
    return one.data()->isOrderedBefore(two.data());
}

static bool compareCueIntervalEndTime(const CueInterval& one, const CueInterval& two)
{
    return one.data()->endMediaTime() > two.data()->endMediaTime();
}

void HTMLMediaElement::updateActiveTextTrackCues(const MediaTime& movieTime)
{
    // 4.8.10.8 Playing the media resource

    //  If the current playback position changes while the steps are running,
    //  then the user agent must wait for the steps to complete, and then must
    //  immediately rerun the steps.
    if (ignoreTrackDisplayUpdateRequests())
        return;

    // 1 - Let current cues be a list of cues, initialized to contain all the
    // cues of all the hidden, showing, or showing by default text tracks of the
    // media element (not the disabled ones) whose start times are less than or
    // equal to the current playback position and whose end times are greater
    // than the current playback position.
    CueList currentCues;

    // The user agent must synchronously unset [the text track cue active] flag
    // whenever ... the media element's readyState is changed back to HAVE_NOTHING.
    auto movieTimeInterval = m_cueTree.createInterval(movieTime, movieTime);
    if (m_readyState != HAVE_NOTHING && m_player) {
        currentCues = m_cueTree.allOverlaps(movieTimeInterval);
        if (currentCues.size() > 1)
            std::sort(currentCues.begin(), currentCues.end(), &compareCueInterval);
    }

    CueList previousCues;
    CueList missedCues;

    // 2 - Let other cues be a list of cues, initialized to contain all the cues
    // of hidden, showing, and showing by default text tracks of the media
    // element that are not present in current cues.
    previousCues = m_currentlyActiveCues;

    // 3 - Let last time be the current playback position at the time this
    // algorithm was last run for this media element, if this is not the first
    // time it has run.
    MediaTime lastTime = m_lastTextTrackUpdateTime;

    // 4 - If the current playback position has, since the last time this
    // algorithm was run, only changed through its usual monotonic increase
    // during normal playback, then let missed cues be the list of cues in other
    // cues whose start times are greater than or equal to last time and whose
    // end times are less than or equal to the current playback position.
    // Otherwise, let missed cues be an empty list.
    if (lastTime >= MediaTime::zeroTime() && m_lastSeekTime < movieTime) {
        for (auto& cue : m_cueTree.allOverlaps(m_cueTree.createInterval(lastTime, movieTime))) {
            // Consider cues that may have been missed since the last seek time.
            if (cue.low() > std::max(m_lastSeekTime, lastTime) && cue.high() < movieTime)
                missedCues.append(cue);
        }
    }

    m_lastTextTrackUpdateTime = movieTime;

    // 5 - If the time was reached through the usual monotonic increase of the
    // current playback position during normal playback, and if the user agent
    // has not fired a timeupdate event at the element in the past 15 to 250ms
    // and is not still running event handlers for such an event, then the user
    // agent must queue a task to fire a simple event named timeupdate at the
    // element. (In the other cases, such as explicit seeks, relevant events get
    // fired as part of the overall process of changing the current playback
    // position.)
    if (!m_paused && m_lastSeekTime <= lastTime)
        scheduleTimeupdateEvent(false);

    // Explicitly cache vector sizes, as their content is constant from here.
    size_t currentCuesSize = currentCues.size();
    size_t missedCuesSize = missedCues.size();
    size_t previousCuesSize = previousCues.size();

    // 6 - If all of the cues in current cues have their text track cue active
    // flag set, none of the cues in other cues have their text track cue active
    // flag set, and missed cues is empty, then abort these steps.
    bool activeSetChanged = missedCuesSize;

    for (size_t i = 0; !activeSetChanged && i < previousCuesSize; ++i)
        if (!currentCues.contains(previousCues[i]) && previousCues[i].data()->isActive())
            activeSetChanged = true;

    for (size_t i = 0; i < currentCuesSize; ++i) {
        RefPtr<TextTrackCue> cue = currentCues[i].data();

        if (cue->isRenderable())
            toVTTCue(cue.get())->updateDisplayTree(movieTime);

        if (!cue->isActive())
            activeSetChanged = true;
    }

    MediaTime nextInterestingTime = MediaTime::invalidTime();
    if (auto nearestEndingCue = std::min_element(currentCues.begin(), currentCues.end(), compareCueIntervalEndTime))
        nextInterestingTime = nearestEndingCue->data()->endMediaTime();

    Optional<CueInterval> nextCue = m_cueTree.nextIntervalAfter(movieTimeInterval);
    if (nextCue)
        nextInterestingTime = std::min(nextInterestingTime, nextCue->low());

    INFO_LOG(LOGIDENTIFIER, "nextInterestingTime:", nextInterestingTime);

    if (nextInterestingTime.isValid() && m_player) {
        m_player->performTaskAtMediaTime([this, weakThis = makeWeakPtr(this), nextInterestingTime] {
            if (!weakThis)
                return;

            auto currentMediaTime = weakThis->currentMediaTime();
            INFO_LOG(LOGIDENTIFIER, " - lambda, currentMediaTime:", currentMediaTime);
            weakThis->updateActiveTextTrackCues(currentMediaTime);
        }, nextInterestingTime);
    }

    if (!activeSetChanged)
        return;

    // 7 - If the time was reached through the usual monotonic increase of the
    // current playback position during normal playback, and there are cues in
    // other cues that have their text track cue pause-on-exi flag set and that
    // either have their text track cue active flag set or are also in missed
    // cues, then immediately pause the media element.
    for (size_t i = 0; !m_paused && i < previousCuesSize; ++i) {
        if (previousCues[i].data()->pauseOnExit()
            && previousCues[i].data()->isActive()
            && !currentCues.contains(previousCues[i]))
            pause();
    }

    for (size_t i = 0; !m_paused && i < missedCuesSize; ++i) {
        if (missedCues[i].data()->pauseOnExit())
            pause();
    }

    // 8 - Let events be a list of tasks, initially empty. Each task in this
    // list will be associated with a text track, a text track cue, and a time,
    // which are used to sort the list before the tasks are queued.
    Vector<std::pair<MediaTime, TextTrackCue*>> eventTasks;

    // 8 - Let affected tracks be a list of text tracks, initially empty.
    Vector<TextTrack*> affectedTracks;

    for (size_t i = 0; i < missedCuesSize; ++i) {
        // 9 - For each text track cue in missed cues, prepare an event named enter
        // for the TextTrackCue object with the text track cue start time.
        eventTasks.append({ missedCues[i].data()->startMediaTime(), missedCues[i].data() });

        // 10 - For each text track [...] in missed cues, prepare an event
        // named exit for the TextTrackCue object with the  with the later of
        // the text track cue end time and the text track cue start time.

        // Note: An explicit task is added only if the cue is NOT a zero or
        // negative length cue. Otherwise, the need for an exit event is
        // checked when these tasks are actually queued below. This doesn't
        // affect sorting events before dispatch either, because the exit
        // event has the same time as the enter event.
        if (missedCues[i].data()->startMediaTime() < missedCues[i].data()->endMediaTime())
            eventTasks.append({ missedCues[i].data()->endMediaTime(), missedCues[i].data() });
    }

    for (size_t i = 0; i < previousCuesSize; ++i) {
        // 10 - For each text track cue in other cues that has its text
        // track cue active flag set prepare an event named exit for the
        // TextTrackCue object with the text track cue end time.
        if (!currentCues.contains(previousCues[i]))
            eventTasks.append({ previousCues[i].data()->endMediaTime(), previousCues[i].data() });
    }

    for (size_t i = 0; i < currentCuesSize; ++i) {
        // 11 - For each text track cue in current cues that does not have its
        // text track cue active flag set, prepare an event named enter for the
        // TextTrackCue object with the text track cue start time.
        if (!previousCues.contains(currentCues[i]))
            eventTasks.append({ currentCues[i].data()->startMediaTime(), currentCues[i].data() });
    }

    // 12 - Sort the tasks in events in ascending time order (tasks with earlier
    // times first).
    std::sort(eventTasks.begin(), eventTasks.end(), eventTimeCueCompare);

    for (auto& eventTask : eventTasks) {
        if (!affectedTracks.contains(eventTask.second->track()))
            affectedTracks.append(eventTask.second->track());

        // 13 - Queue each task in events, in list order.

        // Each event in eventTasks may be either an enterEvent or an exitEvent,
        // depending on the time that is associated with the event. This
        // correctly identifies the type of the event, if the startTime is
        // less than the endTime in the cue.
        if (eventTask.second->startTime() >= eventTask.second->endTime()) {
            auto enterEvent = Event::create(eventNames().enterEvent, Event::CanBubble::No, Event::IsCancelable::No);
            enterEvent->setTarget(eventTask.second);
            m_asyncEventQueue.enqueueEvent(WTFMove(enterEvent));

            auto exitEvent = Event::create(eventNames().exitEvent, Event::CanBubble::No, Event::IsCancelable::No);
            exitEvent->setTarget(eventTask.second);
            m_asyncEventQueue.enqueueEvent(WTFMove(exitEvent));
        } else {
            RefPtr<Event> event;
            if (eventTask.first == eventTask.second->startMediaTime())
                event = Event::create(eventNames().enterEvent, Event::CanBubble::No, Event::IsCancelable::No);
            else
                event = Event::create(eventNames().exitEvent, Event::CanBubble::No, Event::IsCancelable::No);
            event->setTarget(eventTask.second);
            m_asyncEventQueue.enqueueEvent(WTFMove(event));
        }
    }

    // 14 - Sort affected tracks in the same order as the text tracks appear in
    // the media element's list of text tracks, and remove duplicates.
    std::sort(affectedTracks.begin(), affectedTracks.end(), trackIndexCompare);

    // 15 - For each text track in affected tracks, in the list order, queue a
    // task to fire a simple event named cuechange at the TextTrack object, and, ...
    for (auto& affectedTrack : affectedTracks) {
        auto event = Event::create(eventNames().cuechangeEvent, Event::CanBubble::No, Event::IsCancelable::No);
        event->setTarget(affectedTrack);
        m_asyncEventQueue.enqueueEvent(WTFMove(event));

        // ... if the text track has a corresponding track element, to then fire a
        // simple event named cuechange at the track element as well.
        if (is<LoadableTextTrack>(*affectedTrack)) {
            auto event = Event::create(eventNames().cuechangeEvent, Event::CanBubble::No, Event::IsCancelable::No);
            auto trackElement = makeRefPtr(downcast<LoadableTextTrack>(*affectedTrack).trackElement());
            ASSERT(trackElement);
            event->setTarget(trackElement);
            m_asyncEventQueue.enqueueEvent(WTFMove(event));
        }
    }

    // 16 - Set the text track cue active flag of all the cues in the current
    // cues, and unset the text track cue active flag of all the cues in the
    // other cues.
    for (size_t i = 0; i < currentCuesSize; ++i)
        currentCues[i].data()->setIsActive(true);

    for (size_t i = 0; i < previousCuesSize; ++i)
        if (!currentCues.contains(previousCues[i]))
            previousCues[i].data()->setIsActive(false);

    // Update the current active cues.
    m_currentlyActiveCues = currentCues;

    if (activeSetChanged)
        updateTextTrackDisplay();
}

bool HTMLMediaElement::textTracksAreReady() const
{
    // 4.8.10.12.1 Text track model
    // ...
    // The text tracks of a media element are ready if all the text tracks whose mode was not
    // in the disabled state when the element's resource selection algorithm last started now
    // have a text track readiness state of loaded or failed to load.
    for (unsigned i = 0; i < m_textTracksWhenResourceSelectionBegan.size(); ++i) {
        if (m_textTracksWhenResourceSelectionBegan[i]->readinessState() == TextTrack::Loading
            || m_textTracksWhenResourceSelectionBegan[i]->readinessState() == TextTrack::NotLoaded)
            return false;
    }

    return true;
}

void HTMLMediaElement::textTrackReadyStateChanged(TextTrack* track)
{
    if (track->readinessState() != TextTrack::Loading
        && track->mode() != TextTrack::Mode::Disabled) {
        // The display trees exist as long as the track is active, in this case,
        // and if the same track is loaded again (for example if the src attribute was changed),
        // cues can be accumulated with the old ones, that's why they needs to be flushed
        if (hasMediaControls())
            mediaControls()->clearTextDisplayContainer();
        updateTextTrackDisplay();
    }
    if (m_player && m_textTracksWhenResourceSelectionBegan.contains(track)) {
        if (track->readinessState() != TextTrack::Loading)
            setReadyState(m_player->readyState());
    } else {
        // The track readiness state might have changed as a result of the user
        // clicking the captions button. In this case, a check whether all the
        // resources have failed loading should be done in order to hide the CC button.
        if (hasMediaControls() && track->readinessState() == TextTrack::FailedToLoad)
            mediaControls()->refreshClosedCaptionsButtonVisibility();
    }
}

void HTMLMediaElement::audioTrackEnabledChanged(AudioTrack& track)
{
    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();
    if (processingUserGestureForMedia())
        removeBehaviorsRestrictionsAfterFirstUserGesture(MediaElementSession::AllRestrictions & ~MediaElementSession::RequireUserGestureToControlControlsManager);
}

void HTMLMediaElement::textTrackModeChanged(TextTrack& track)
{
    bool trackIsLoaded = true;
    if (track.trackType() == TextTrack::TrackElement) {
        trackIsLoaded = false;
        for (auto& trackElement : childrenOfType<HTMLTrackElement>(*this)) {
            if (&trackElement.track() == &track) {
                if (trackElement.readyState() == HTMLTrackElement::LOADING || trackElement.readyState() == HTMLTrackElement::LOADED)
                    trackIsLoaded = true;
                break;
            }
        }
    }

    // If this is the first added track, create the list of text tracks.
    if (!m_textTracks)
        m_textTracks = TextTrackList::create(this, ActiveDOMObject::scriptExecutionContext());

    // Mark this track as "configured" so configureTextTracks won't change the mode again.
    track.setHasBeenConfigured(true);

    if (track.mode() != TextTrack::Mode::Disabled && trackIsLoaded)
        textTrackAddCues(track, *track.cues());

    configureTextTrackDisplay(AssumeTextTrackVisibilityChanged);

    if (m_textTracks && m_textTracks->contains(track))
        m_textTracks->scheduleChangeEvent();

#if ENABLE(AVF_CAPTIONS)
    if (track.trackType() == TextTrack::TrackElement && m_player)
        m_player->notifyTrackModeChanged();
#endif
}

void HTMLMediaElement::videoTrackSelectedChanged(VideoTrack& track)
{
    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();
}

void HTMLMediaElement::textTrackKindChanged(TextTrack& track)
{
    if (track.kind() != TextTrack::Kind::Captions && track.kind() != TextTrack::Kind::Subtitles && track.mode() == TextTrack::Mode::Showing)
        track.setMode(TextTrack::Mode::Hidden);
}

void HTMLMediaElement::beginIgnoringTrackDisplayUpdateRequests()
{
    ++m_ignoreTrackDisplayUpdate;
}

void HTMLMediaElement::endIgnoringTrackDisplayUpdateRequests()
{
    ASSERT(m_ignoreTrackDisplayUpdate);
    --m_ignoreTrackDisplayUpdate;
    if (!m_ignoreTrackDisplayUpdate && m_inActiveDocument)
        updateActiveTextTrackCues(currentMediaTime());
}

void HTMLMediaElement::textTrackAddCues(TextTrack& track, const TextTrackCueList& cues)
{
    if (track.mode() == TextTrack::Mode::Disabled)
        return;

    TrackDisplayUpdateScope scope { *this };
    for (unsigned i = 0; i < cues.length(); ++i)
        textTrackAddCue(track, *cues.item(i));
}

void HTMLMediaElement::textTrackRemoveCues(TextTrack&, const TextTrackCueList& cues)
{
    TrackDisplayUpdateScope scope { *this };
    for (unsigned i = 0; i < cues.length(); ++i) {
        auto& cue = *cues.item(i);
        textTrackRemoveCue(*cue.track(), cue);
    }
}

void HTMLMediaElement::textTrackAddCue(TextTrack& track, TextTrackCue& cue)
{
    if (track.mode() == TextTrack::Mode::Disabled)
        return;

    // Negative duration cues need be treated in the interval tree as
    // zero-length cues.
    MediaTime endTime = std::max(cue.startMediaTime(), cue.endMediaTime());

    CueInterval interval = m_cueTree.createInterval(cue.startMediaTime(), endTime, &cue);
    if (!m_cueTree.contains(interval))
        m_cueTree.add(interval);
    updateActiveTextTrackCues(currentMediaTime());
}

void HTMLMediaElement::textTrackRemoveCue(TextTrack&, TextTrackCue& cue)
{
    // Negative duration cues need to be treated in the interval tree as
    // zero-length cues.
    MediaTime endTime = std::max(cue.startMediaTime(), cue.endMediaTime());

    CueInterval interval = m_cueTree.createInterval(cue.startMediaTime(), endTime, &cue);
    m_cueTree.remove(interval);

    // Since the cue will be removed from the media element and likely the
    // TextTrack might also be destructed, notifying the region of the cue
    // removal shouldn't be done.
    if (cue.isRenderable())
        toVTTCue(&cue)->notifyRegionWhenRemovingDisplayTree(false);

    size_t index = m_currentlyActiveCues.find(interval);
    if (index != notFound) {
        cue.setIsActive(false);
        m_currentlyActiveCues.remove(index);
    }

    if (cue.isRenderable())
        toVTTCue(&cue)->removeDisplayTree();
    updateActiveTextTrackCues(currentMediaTime());

    if (cue.isRenderable())
        toVTTCue(&cue)->notifyRegionWhenRemovingDisplayTree(true);
}

#endif

static inline bool isAllowedToLoadMediaURL(HTMLMediaElement& element, const URL& url, bool isInUserAgentShadowTree)
{
    // Elements in user agent show tree should load whatever the embedding document policy is.
    if (isInUserAgentShadowTree)
        return true;

    ASSERT(element.document().contentSecurityPolicy());
    return element.document().contentSecurityPolicy()->allowMediaFromSource(url);
}

bool HTMLMediaElement::isSafeToLoadURL(const URL& url, InvalidURLAction actionIfInvalid)
{
    if (!url.isValid()) {
        ERROR_LOG(LOGIDENTIFIER, url, " is invalid");
        return false;
    }

    RefPtr<Frame> frame = document().frame();
    if (!frame || !document().securityOrigin().canDisplay(url)) {
        if (actionIfInvalid == Complain)
            FrameLoader::reportLocalLoadFailed(frame.get(), url.stringCenterEllipsizedToLength());
            ERROR_LOG(LOGIDENTIFIER, url , " was rejected by SecurityOrigin");
        return false;
    }

    if (!isAllowedToLoadMediaURL(*this, url, isInUserAgentShadowTree())) {
        ERROR_LOG(LOGIDENTIFIER, url, " was rejected by Content Security Policy");
        return false;
    }

    return true;
}

void HTMLMediaElement::startProgressEventTimer()
{
    if (m_progressEventTimer.isActive())
        return;

    m_previousProgressTime = MonotonicTime::now();
    // 350ms is not magic, it is in the spec!
    m_progressEventTimer.startRepeating(350_ms);
}

void HTMLMediaElement::waitForSourceChange()
{
    INFO_LOG(LOGIDENTIFIER);

    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 6.17 - Waiting: Set the element's networkState attribute to the NETWORK_NO_SOURCE value
    m_networkState = NETWORK_NO_SOURCE;

    // 6.18 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    updateDisplayState();
    updateRenderer();
}

void HTMLMediaElement::noneSupported()
{
    if (m_error)
        return;

    INFO_LOG(LOGIDENTIFIER);

    stopPeriodicTimers();
    m_loadState = WaitingForSource;
    m_currentSourceNode = nullptr;

    // 4.8.10.5
    // 6 - Reaching this step indicates that the media resource failed to load or that the given
    // URL could not be resolved. In one atomic operation, run the following steps:

    // 6.1 - Set the error attribute to a new MediaError object whose code attribute is set to
    // MEDIA_ERR_SRC_NOT_SUPPORTED.
    m_error = MediaError::create(MediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);

    // 6.2 - Forget the media element's media-resource-specific text tracks.
    forgetResourceSpecificTracks();

    // 6.3 - Set the element's networkState attribute to the NETWORK_NO_SOURCE value.
    m_networkState = NETWORK_NO_SOURCE;

    // 7 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(eventNames().errorEvent);

    rejectPendingPlayPromises(WTFMove(m_pendingPlayPromises), DOMException::create(NotSupportedError));

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    // 8 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 9 - Abort these steps. Until the load() method is invoked or the src attribute is changed,
    // the element won't attempt to load another resource.

    updateDisplayState();
    updateRenderer();
}

void HTMLMediaElement::mediaLoadingFailedFatally(MediaPlayer::NetworkState error)
{
    // 1 - The user agent should cancel the fetching process.
    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 2 - Set the error attribute to a new MediaError object whose code attribute is
    // set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
    if (error == MediaPlayer::NetworkError)
        m_error = MediaError::create(MediaError::MEDIA_ERR_NETWORK);
    else if (error == MediaPlayer::DecodeError)
        m_error = MediaError::create(MediaError::MEDIA_ERR_DECODE);
    else
        ASSERT_NOT_REACHED();

    // 3 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(eventNames().errorEvent);

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    // 4 - Set the element's networkState attribute to the NETWORK_EMPTY value and queue a
    // task to fire a simple event called emptied at the element.
    m_networkState = NETWORK_EMPTY;
    scheduleEvent(eventNames().emptiedEvent);

    // 5 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 6 - Abort the overall resource selection algorithm.
    m_currentSourceNode = nullptr;

#if PLATFORM(COCOA)
    if (is<MediaDocument>(document()))
        downcast<MediaDocument>(document()).mediaElementSawUnsupportedTracks();
#endif
}

void HTMLMediaElement::cancelPendingEventsAndCallbacks()
{
    INFO_LOG(LOGIDENTIFIER);
    m_asyncEventQueue.cancelAllEvents();

    for (auto& source : childrenOfType<HTMLSourceElement>(*this))
        source.cancelPendingErrorEvent();

    rejectPendingPlayPromises(WTFMove(m_pendingPlayPromises), DOMException::create(AbortError));
}

void HTMLMediaElement::mediaPlayerNetworkStateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    setNetworkState(m_player->networkState());
    endProcessingMediaPlayerCallback();
}

static void logMediaLoadRequest(Page* page, const String& mediaEngine, const String& errorMessage, bool succeeded)
{
    if (!page)
        return;

    DiagnosticLoggingClient& diagnosticLoggingClient = page->diagnosticLoggingClient();
    if (!succeeded) {
        diagnosticLoggingClient.logDiagnosticMessageWithResult(DiagnosticLoggingKeys::mediaLoadingFailedKey(), errorMessage, DiagnosticLoggingResultFail, ShouldSample::No);
        return;
    }

    diagnosticLoggingClient.logDiagnosticMessage(DiagnosticLoggingKeys::mediaLoadedKey(), mediaEngine, ShouldSample::No);

    if (!page->hasSeenAnyMediaEngine())
        diagnosticLoggingClient.logDiagnosticMessage(DiagnosticLoggingKeys::pageContainsAtLeastOneMediaEngineKey(), emptyString(), ShouldSample::No);

    if (!page->hasSeenMediaEngine(mediaEngine))
        diagnosticLoggingClient.logDiagnosticMessage(DiagnosticLoggingKeys::pageContainsMediaEngineKey(), mediaEngine, ShouldSample::No);

    page->sawMediaEngine(mediaEngine);
}

static String stringForNetworkState(MediaPlayer::NetworkState state)
{
    switch (state) {
    case MediaPlayer::Empty: return "Empty"_s;
    case MediaPlayer::Idle: return "Idle"_s;
    case MediaPlayer::Loading: return "Loading"_s;
    case MediaPlayer::Loaded: return "Loaded"_s;
    case MediaPlayer::FormatError: return "FormatError"_s;
    case MediaPlayer::NetworkError: return "NetworkError"_s;
    case MediaPlayer::DecodeError: return "DecodeError"_s;
    default: return emptyString();
    }
}

void HTMLMediaElement::mediaLoadingFailed(MediaPlayer::NetworkState error)
{
    stopPeriodicTimers();

    // If we failed while trying to load a <source> element, the movie was never parsed, and there are more
    // <source> children, schedule the next one
    if (m_readyState < HAVE_METADATA && m_loadState == LoadingFromSourceElement) {

        // resource selection algorithm
        // Step 9.Otherwise.9 - Failed with elements: Queue a task, using the DOM manipulation task source, to fire a simple event named error at the candidate element.
        if (m_currentSourceNode)
            m_currentSourceNode->scheduleErrorEvent();
        else
            INFO_LOG(LOGIDENTIFIER, "error event not sent, <source> was removed");

        // 9.Otherwise.10 - Asynchronously await a stable state. The synchronous section consists of all the remaining steps of this algorithm until the algorithm says the synchronous section has ended.

        // 9.Otherwise.11 - Forget the media element's media-resource-specific tracks.
        forgetResourceSpecificTracks();

        if (havePotentialSourceChild()) {
            INFO_LOG(LOGIDENTIFIER, "scheduling next <source>");
            scheduleNextSourceChild();
        } else {
            INFO_LOG(LOGIDENTIFIER, "no more <source> elements, waiting");
            waitForSourceChange();
        }

        return;
    }

    if ((error == MediaPlayer::NetworkError && m_readyState >= HAVE_METADATA) || error == MediaPlayer::DecodeError)
        mediaLoadingFailedFatally(error);
    else if ((error == MediaPlayer::FormatError || error == MediaPlayer::NetworkError) && m_loadState == LoadingFromSrcAttr)
        noneSupported();

    updateDisplayState();
    if (hasMediaControls()) {
        mediaControls()->reset();
        mediaControls()->reportedError();
    }

    ERROR_LOG(LOGIDENTIFIER, "error = ", static_cast<int>(error));

    logMediaLoadRequest(document().page(), String(), stringForNetworkState(error), false);

    m_mediaSession->clientCharacteristicsChanged();
}

void HTMLMediaElement::setNetworkState(MediaPlayer::NetworkState state)
{
    if (static_cast<int>(state) != static_cast<int>(m_networkState))
        ALWAYS_LOG(LOGIDENTIFIER, "new state = ", state, ", current state = ", m_networkState);

    if (state == MediaPlayer::Empty) {
        // Just update the cached state and leave, we can't do anything.
        m_networkState = NETWORK_EMPTY;
        return;
    }

    if (state == MediaPlayer::FormatError || state == MediaPlayer::NetworkError || state == MediaPlayer::DecodeError) {
        mediaLoadingFailed(state);
        return;
    }

    if (state == MediaPlayer::Idle) {
        if (m_networkState > NETWORK_IDLE) {
            changeNetworkStateFromLoadingToIdle();
            setShouldDelayLoadEvent(false);
        } else {
            m_networkState = NETWORK_IDLE;
        }
    }

    if (state == MediaPlayer::Loading) {
        if (m_networkState < NETWORK_LOADING || m_networkState == NETWORK_NO_SOURCE)
            startProgressEventTimer();
        m_networkState = NETWORK_LOADING;
    }

    if (state == MediaPlayer::Loaded) {
        if (m_networkState != NETWORK_IDLE)
            changeNetworkStateFromLoadingToIdle();
        m_completelyLoaded = true;
    }

    if (hasMediaControls())
        mediaControls()->updateStatusDisplay();
}

void HTMLMediaElement::changeNetworkStateFromLoadingToIdle()
{
    m_progressEventTimer.stop();
    if (hasMediaControls() && m_player->didLoadingProgress())
        mediaControls()->bufferingProgressed();

    // Schedule one last progress event so we guarantee that at least one is fired
    // for files that load very quickly.
    scheduleEvent(eventNames().progressEvent);
    scheduleEvent(eventNames().suspendEvent);
    m_networkState = NETWORK_IDLE;
}

void HTMLMediaElement::mediaPlayerReadyStateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();

    setReadyState(m_player->readyState());

    endProcessingMediaPlayerCallback();
}

SuccessOr<MediaPlaybackDenialReason> HTMLMediaElement::canTransitionFromAutoplayToPlay() const
{
    if (isAutoplaying()
        && mediaSession().autoplayPermitted()
        && paused()
        && autoplay()
        && !pausedForUserInteraction()
        && !document().isSandboxed(SandboxAutomaticFeatures)
        && m_readyState == HAVE_ENOUGH_DATA)
        return mediaSession().playbackPermitted();

    ALWAYS_LOG(LOGIDENTIFIER, "page consent required");
    return MediaPlaybackDenialReason::PageConsentRequired;
}

void HTMLMediaElement::dispatchPlayPauseEventsIfNeedsQuirks()
{
    auto& document = this->document();
    if (!needsAutoplayPlayPauseEventsQuirk(document) && !needsAutoplayPlayPauseEventsQuirk(document.topDocument()))
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    scheduleEvent(eventNames().playingEvent);
    scheduleEvent(eventNames().pauseEvent);
}

void HTMLMediaElement::setReadyState(MediaPlayer::ReadyState state)
{
    // Set "wasPotentiallyPlaying" BEFORE updating m_readyState, potentiallyPlaying() uses it
    bool wasPotentiallyPlaying = potentiallyPlaying();

    ReadyState oldState = m_readyState;
    ReadyState newState = static_cast<ReadyState>(state);

#if ENABLE(VIDEO_TRACK)
    bool tracksAreReady = textTracksAreReady();

    if (newState == oldState && m_tracksAreReady == tracksAreReady)
        return;

    m_tracksAreReady = tracksAreReady;
#else
    if (newState == oldState)
        return;
    bool tracksAreReady = true;
#endif

    ALWAYS_LOG(LOGIDENTIFIER, "new state = ", state, ", current state = ", m_readyState);

    if (tracksAreReady)
        m_readyState = newState;
    else {
        // If a media file has text tracks the readyState may not progress beyond HAVE_FUTURE_DATA until
        // the text tracks are ready, regardless of the state of the media file.
        if (newState <= HAVE_METADATA)
            m_readyState = newState;
        else
            m_readyState = HAVE_CURRENT_DATA;
    }

    if (oldState > m_readyStateMaximum)
        m_readyStateMaximum = oldState;

    if (m_networkState == NETWORK_EMPTY)
        return;

    if (m_seeking) {
        // 4.8.10.9, step 11
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA)
            scheduleEvent(eventNames().waitingEvent);

        // 4.8.10.10 step 14 & 15.
        if (m_seekRequested && !m_player->seeking() && m_readyState >= HAVE_CURRENT_DATA)
            finishSeek();
    } else {
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA) {
            // 4.8.10.8
            invalidateCachedTime();
            scheduleTimeupdateEvent(false);
            scheduleEvent(eventNames().waitingEvent);
        }
    }

    if (m_readyState >= HAVE_METADATA && oldState < HAVE_METADATA) {
        prepareMediaFragmentURI();
        scheduleEvent(eventNames().durationchangeEvent);
        scheduleResizeEvent();
        scheduleEvent(eventNames().loadedmetadataEvent);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (hasEventListeners(eventNames().webkitplaybacktargetavailabilitychangedEvent))
            enqueuePlaybackTargetAvailabilityChangedEvent();
#endif
        m_initiallyMuted = m_volume < 0.05 || muted();

        if (hasMediaControls())
            mediaControls()->loadedMetadata();
        updateRenderer();

        if (is<MediaDocument>(document()))
            downcast<MediaDocument>(document()).mediaElementNaturalSizeChanged(expandedIntSize(m_player->naturalSize()));

        logMediaLoadRequest(document().page(), m_player->engineDescription(), String(), true);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        scheduleUpdateMediaState();
#endif

        m_mediaSession->clientCharacteristicsChanged();
    }

    bool shouldUpdateDisplayState = false;

    if (m_readyState >= HAVE_CURRENT_DATA && oldState < HAVE_CURRENT_DATA) {
        if (!m_haveFiredLoadedData) {
            m_haveFiredLoadedData = true;
            scheduleEvent(eventNames().loadeddataEvent);
            // FIXME: It's not clear that it's correct to skip these two operations just
            // because m_haveFiredLoadedData is already true. At one time we were skipping
            // the call to setShouldDelayLoadEvent, which was definitely incorrect.
            shouldUpdateDisplayState = true;
            applyMediaFragmentURI();
        }
        setShouldDelayLoadEvent(false);
    }

    if (m_readyState == HAVE_FUTURE_DATA && oldState <= HAVE_CURRENT_DATA && tracksAreReady) {
        scheduleEvent(eventNames().canplayEvent);
        shouldUpdateDisplayState = true;
    }

    if (m_readyState == HAVE_ENOUGH_DATA && oldState < HAVE_ENOUGH_DATA && tracksAreReady) {
        if (oldState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().canplayEvent);

        scheduleEvent(eventNames().canplaythroughEvent);

        auto success = canTransitionFromAutoplayToPlay();
        if (success) {
            m_paused = false;
            invalidateCachedTime();
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::StartedWithoutUserGesture);
            m_playbackStartedTime = currentMediaTime().toDouble();
            scheduleEvent(eventNames().playEvent);
        } else if (success.value() == MediaPlaybackDenialReason::UserGestureRequired) {
            ALWAYS_LOG(LOGIDENTIFIER, "Autoplay blocked, user gesture required");
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
        }

        shouldUpdateDisplayState = true;
    }

    // If we transition to the Future Data state and we're about to begin playing, ensure playback is actually permitted first,
    // honoring any playback denial reasons such as the requirement of a user gesture.
    if (m_readyState == HAVE_FUTURE_DATA && oldState < HAVE_FUTURE_DATA && potentiallyPlaying() && !m_mediaSession->playbackPermitted()) {
        auto canTransition = canTransitionFromAutoplayToPlay();
        if (canTransition && canTransition.value() == MediaPlaybackDenialReason::UserGestureRequired)
            ALWAYS_LOG(LOGIDENTIFIER, "Autoplay blocked, user gesture required");

        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    }

    if (shouldUpdateDisplayState) {
        updateDisplayState();
        if (hasMediaControls()) {
            mediaControls()->refreshClosedCaptionsButtonVisibility();
            mediaControls()->updateStatusDisplay();
        }
    }

    updatePlayState();
    updateMediaController();
#if ENABLE(VIDEO_TRACK)
    updateActiveTextTrackCues(currentMediaTime());
#endif
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RefPtr<ArrayBuffer> HTMLMediaElement::mediaPlayerCachedKeyForKeyId(const String& keyId) const
{
    return m_webKitMediaKeys ? m_webKitMediaKeys->cachedKeyForKeyId(keyId) : nullptr;
}

bool HTMLMediaElement::mediaPlayerKeyNeeded(MediaPlayer*, Uint8Array* initData)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().legacyEncryptedMediaAPIEnabled())
        return false;

    if (!hasEventListeners("webkitneedkey")
#if ENABLE(ENCRYPTED_MEDIA)
        // Only fire an error if ENCRYPTED_MEDIA is not enabled, to give clients of the 
        // "encrypted" event a chance to handle it without resulting in a synthetic error.
        && (!RuntimeEnabledFeatures::sharedFeatures().encryptedMediaAPIEnabled() || document().quirks().hasBrokenEncryptedMediaAPISupportQuirk())
#endif
        ) {
        m_error = MediaError::create(MediaError::MEDIA_ERR_ENCRYPTED);
        scheduleEvent(eventNames().errorEvent);
        return false;
    }

    auto event = WebKitMediaKeyNeededEvent::create(eventNames().webkitneedkeyEvent, initData);
    event->setTarget(this);
    m_asyncEventQueue.enqueueEvent(WTFMove(event));

    return true;
}

String HTMLMediaElement::mediaPlayerMediaKeysStorageDirectory() const
{
    auto* page = document().page();
    if (!page || page->usesEphemeralSession())
        return emptyString();

    String storageDirectory = document().settings().mediaKeysStorageDirectory();
    if (storageDirectory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(storageDirectory, document().securityOrigin().data().databaseIdentifier());
}

void HTMLMediaElement::webkitSetMediaKeys(WebKitMediaKeys* mediaKeys)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().legacyEncryptedMediaAPIEnabled())
        return;

    if (m_webKitMediaKeys == mediaKeys)
        return;

    if (m_webKitMediaKeys)
        m_webKitMediaKeys->setMediaElement(nullptr);
    m_webKitMediaKeys = mediaKeys;
    if (m_webKitMediaKeys)
        m_webKitMediaKeys->setMediaElement(this);
}

void HTMLMediaElement::keyAdded()
{
    if (!RuntimeEnabledFeatures::sharedFeatures().legacyEncryptedMediaAPIEnabled())
        return;

    if (m_player)
        m_player->keyAdded();
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)

MediaKeys* HTMLMediaElement::mediaKeys() const
{
    return m_mediaKeys.get();
}

void HTMLMediaElement::setMediaKeys(MediaKeys* mediaKeys, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-htmlmediaelement-setmediakeys
    // W3C Editor's Draft 23 June 2017

    // 1. If this object's attaching media keys value is true, return a promise rejected with an InvalidStateError.
    if (m_attachingMediaKeys) {
        promise->reject(InvalidStateError);
        return;
    }

    // 2. If mediaKeys and the mediaKeys attribute are the same object, return a resolved promise.
    if (mediaKeys == m_mediaKeys) {
        promise->resolve();
        return;
    }

    // 3. Let this object's attaching media keys value be true.
    m_attachingMediaKeys = true;

    // 4. Let promise be a new promise.
    // 5. Run the following steps in parallel:
    m_encryptedMediaQueue.enqueueTask([this, mediaKeys = RefPtr<MediaKeys>(mediaKeys), promise = WTFMove(promise)]() mutable {
        // 5.1. If all the following conditions hold:
        //      - mediaKeys is not null,
        //      - the CDM instance represented by mediaKeys is already in use by another media element
        //      - the user agent is unable to use it with this element
        //      then let this object's attaching media keys value be false and reject promise with a QuotaExceededError.
        // FIXME: ^

        // 5.2. If the mediaKeys attribute is not null, run the following steps:
        if (m_mediaKeys) {
            // 5.2.1. If the user agent or CDM do not support removing the association, let this object's attaching media keys value be false and reject promise with a NotSupportedError.
            // 5.2.2. If the association cannot currently be removed, let this object's attaching media keys value be false and reject promise with an InvalidStateError.
            // 5.2.3. Stop using the CDM instance represented by the mediaKeys attribute to decrypt media data and remove the association with the media element.
            // 5.2.4. If the preceding step failed, let this object's attaching media keys value be false and reject promise with the appropriate error name.
            // FIXME: ^

            m_mediaKeys->detachCDMClient(*this);
            if (m_player)
                m_player->cdmInstanceDetached(m_mediaKeys->cdmInstance());
        }

        // 5.3. If mediaKeys is not null, run the following steps:
        if (mediaKeys) {
            // 5.3.1. Associate the CDM instance represented by mediaKeys with the media element for decrypting media data.
            mediaKeys->attachCDMClient(*this);
            if (m_player)
                m_player->cdmInstanceAttached(mediaKeys->cdmInstance());

            // 5.3.2. If the preceding step failed, run the following steps:
            //   5.3.2.1. Set the mediaKeys attribute to null.
            //   5.3.2.2. Let this object's attaching media keys value be false.
            //   5.3.2.3. Reject promise with a new DOMException whose name is the appropriate error name.
            // FIXME: ^

            // 5.3.3. Queue a task to run the Attempt to Resume Playback If Necessary algorithm on the media element.
            m_encryptedMediaQueue.enqueueTask([this] {
                attemptToResumePlaybackIfNecessary();
            });
        }

        // 5.4. Set the mediaKeys attribute to mediaKeys.
        // 5.5. Let this object's attaching media keys value be false.
        // 5.6. Resolve promise.
        m_mediaKeys = WTFMove(mediaKeys);
        m_attachingMediaKeys = false;
        promise->resolve();
    });

    // 6. Return promise.
}

void HTMLMediaElement::mediaPlayerInitializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().encryptedMediaAPIEnabled() || document().quirks().hasBrokenEncryptedMediaAPISupportQuirk())
        return;

    // https://w3c.github.io/encrypted-media/#initdata-encountered
    // W3C Editor's Draft 23 June 2017

    // 1. Let the media element be the specified HTMLMediaElement object.
    // 2. Let initDataType be the empty string.
    // 3. Let initData be null.
    // 4. If the media data is CORS-same-origin and not mixed content, run the following steps:
    //   4.1. Let initDataType be the string representing the Initialization Data Type of the Initialization Data.
    //   4.2. Let initData be the Initialization Data.
    // FIXME: ^

    // 5. Queue a task to create an event named encrypted that does not bubble and is not cancellable using the
    //    MediaEncryptedEvent interface with its type attribute set to encrypted and its isTrusted attribute
    //    initialized to true, and dispatch it at the media element.
    //    The event interface MediaEncryptedEvent has:
    //      initDataType = initDataType
    //      initData = initData
    MediaEncryptedEventInit initializer { initDataType, WTFMove(initData) };
    m_asyncEventQueue.enqueueEvent(MediaEncryptedEvent::create(eventNames().encryptedEvent, initializer, Event::IsTrusted::Yes));
}

void HTMLMediaElement::mediaPlayerWaitingForKeyChanged()
{
    if (!m_player)
        return;

    if (!m_player->waitingForKey() && m_playbackBlockedWaitingForKey) {
        // https://w3c.github.io/encrypted-media/#resume-playback
        // W3C Editor's Draft 23 June 2017

        // NOTE: continued from HTMLMediaElement::attemptToDecrypt().
        // 4. If the user agent can advance the current playback position in the direction of playback:
        //   4.1. Set the media element's decryption blocked waiting for key value to false.
        // FIXME: ^
        //   4.2. Set the media element's playback blocked waiting for key value to false.
        m_playbackBlockedWaitingForKey = false;

        //   4.3. Set the media element's readyState value to HAVE_CURRENT_DATA, HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA as appropriate.
        setReadyState(m_player->readyState());

        return;
    }

    // https://www.w3.org/TR/encrypted-media/#wait-for-key
    // W3C Recommendation 18 September 2017

    // The Wait for Key algorithm queues a waitingforkey event and
    // updates readyState. It should only be called when the
    // HTMLMediaElement object is potentially playing and its
    // readyState is equal to HAVE_FUTURE_DATA or greater. Requests to
    // run this algorithm include a target HTMLMediaElement object.

    // The following steps are run:

    // 1. Let the media element be the specified HTMLMediaElement
    // object.
    // 2. If the media element's playback blocked waiting for key
    // value is true, abort these steps.
    if (m_playbackBlockedWaitingForKey)
        return;

    // 3. Set the media element's playback blocked waiting for key
    // value to true.
    m_playbackBlockedWaitingForKey = true;

    // NOTE
    // As a result of the above step, the media element will become a
    // blocked media element if it wasn't already. In that case, the
    // media element will stop playback.

    // 4. Follow the steps for the first matching condition from the
    // following list:

    // If data for the immediate current playback position is
    // available
    // Set the readyState of media element to HAVE_CURRENT_DATA.
    // Otherwise
    // Set the readyState of media element to HAVE_METADATA.
    ReadyState nextReadyState = buffered()->contain(currentTime()) ? HAVE_CURRENT_DATA : HAVE_METADATA;
    if (nextReadyState < m_readyState)
        setReadyState(static_cast<MediaPlayer::ReadyState>(nextReadyState));

    // NOTE
    // In other words, if the video frame and audio data for the
    // current playback position have been decoded because they were
    // unencrypted and/or successfully decrypted, set readyState to
    // HAVE_CURRENT_DATA. Otherwise, including if this was previously
    // the case but the data is no longer available, set readyState to
    // HAVE_METADATA.

    // 5. Queue a task to fire a simple event named waitingforkey at the
    // media element.
    scheduleEvent(eventNames().waitingforkeyEvent);

    // 6. Suspend playback.
    // GStreamer handles this without suspending explicitly.
}

void HTMLMediaElement::attemptToDecrypt()
{
    // https://w3c.github.io/encrypted-media/#attempt-to-decrypt
    // W3C Editor's Draft 23 June 2017

    // 1. Let the media element be the specified HTMLMediaElement object.
    // 2. If the media element's encrypted block queue is empty, abort these steps.
    // FIXME: ^

    // 3. If the media element's mediaKeys attribute is not null, run the following steps:
    if (m_mediaKeys) {
        // 3.1. Let media keys be the MediaKeys object referenced by that attribute.
        // 3.2. Let cdm be the CDM instance represented by media keys's cdm instance value.
        auto& cdmInstance = m_mediaKeys->cdmInstance();

        // 3.3. If cdm is no longer usable for any reason, run the following steps:
        //   3.3.1. Run the media data is corrupted steps of the resource fetch algorithm.
        //   3.3.2. Run the CDM Unavailable algorithm on media keys.
        //   3.3.3. Abort these steps.
        // FIXME: ^

        // 3.4. If there is at least one MediaKeySession created by the media keys that is not closed, run the following steps:
        if (m_mediaKeys->hasOpenSessions()) {
            // Continued in MediaPlayer::attemptToDecryptWithInstance().
            if (m_player)
                m_player->attemptToDecryptWithInstance(cdmInstance);
        }
    }

    // 4. Set the media element's decryption blocked waiting for key value to true.
    // FIXME: ^
}

void HTMLMediaElement::attemptToResumePlaybackIfNecessary()
{
    // https://w3c.github.io/encrypted-media/#resume-playback
    // W3C Editor's Draft 23 June 2017

    // 1. Let the media element be the specified HTMLMediaElement object.
    // 2. If the media element's playback blocked waiting for key is false, abort these steps.
    if (!m_playbackBlockedWaitingForKey)
        return;

    // 3. Run the Attempt to Decrypt algorithm on the media element.
    attemptToDecrypt();

    // NOTE: continued in HTMLMediaElement::waitingForKeyChanged()
}

void HTMLMediaElement::cdmClientAttemptToResumePlaybackIfNecessary()
{
    attemptToResumePlaybackIfNecessary();
}

#endif // ENABLE(ENCRYPTED_MEDIA)

void HTMLMediaElement::progressEventTimerFired()
{
    ASSERT(m_player);
    if (m_networkState != NETWORK_LOADING)
        return;

    MonotonicTime time = MonotonicTime::now();
    Seconds timedelta = time - m_previousProgressTime;

    if (m_player->didLoadingProgress()) {
        scheduleEvent(eventNames().progressEvent);
        m_previousProgressTime = time;
        m_sentStalledEvent = false;
        updateRenderer();
        if (hasMediaControls())
            mediaControls()->bufferingProgressed();
    } else if (timedelta > 3_s && !m_sentStalledEvent) {
        scheduleEvent(eventNames().stalledEvent);
        m_sentStalledEvent = true;
        setShouldDelayLoadEvent(false);
    }
}

void HTMLMediaElement::rewind(double timeDelta)
{
    setCurrentTime(std::max(currentMediaTime() - MediaTime::createWithDouble(timeDelta), minTimeSeekable()));
}

void HTMLMediaElement::returnToRealtime()
{
    setCurrentTime(maxTimeSeekable());
}

void HTMLMediaElement::addPlayedRange(const MediaTime& start, const MediaTime& end)
{
    DEBUG_LOG(LOGIDENTIFIER, MediaTimeRange { start, end });
    if (!m_playedTimeRanges)
        m_playedTimeRanges = TimeRanges::create();
    m_playedTimeRanges->ranges().add(start, end);
}

bool HTMLMediaElement::supportsScanning() const
{
    return m_player ? m_player->supportsScanning() : false;
}

void HTMLMediaElement::prepareToPlay()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    INFO_LOG(LOGIDENTIFIER);
    if (m_havePreparedToPlay || !document().hasBrowsingContext())
        return;
    m_havePreparedToPlay = true;
    if (m_player)
        m_player->prepareToPlay();
}

void HTMLMediaElement::fastSeek(double time)
{
    fastSeek(MediaTime::createWithDouble(time));
}

void HTMLMediaElement::fastSeek(const MediaTime& time)
{
    INFO_LOG(LOGIDENTIFIER, time);
    // 4.7.10.9 Seeking
    // 9. If the approximate-for-speed flag is set, adjust the new playback position to a value that will
    // allow for playback to resume promptly. If new playback position before this step is before current
    // playback position, then the adjusted new playback position must also be before the current playback
    // position. Similarly, if the new playback position before this step is after current playback position,
    // then the adjusted new playback position must also be after the current playback position.
    refreshCachedTime();

    MediaTime delta = time - currentMediaTime();
    MediaTime negativeTolerance = delta < MediaTime::zeroTime() ? MediaTime::positiveInfiniteTime() : delta;
    seekWithTolerance(time, negativeTolerance, MediaTime::zeroTime(), true);
}

void HTMLMediaElement::seek(const MediaTime& time)
{
    INFO_LOG(LOGIDENTIFIER, time);
    seekWithTolerance(time, MediaTime::zeroTime(), MediaTime::zeroTime(), true);
}

void HTMLMediaElement::seekInternal(const MediaTime& time)
{
    INFO_LOG(LOGIDENTIFIER, time);
    seekWithTolerance(time, MediaTime::zeroTime(), MediaTime::zeroTime(), false);
}

void HTMLMediaElement::seekWithTolerance(const MediaTime& inTime, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance, bool fromDOM)
{
    // 4.8.10.9 Seeking
    MediaTime time = inTime;

    // 1 - Set the media element's show poster flag to false.
    setDisplayMode(Video);

    // 2 - If the media element's readyState is HAVE_NOTHING, abort these steps.
    if (m_readyState == HAVE_NOTHING || !m_player)
        return;

    // If the media engine has been told to postpone loading data, let it go ahead now.
    if (m_preload < MediaPlayer::Auto && m_readyState < HAVE_FUTURE_DATA)
        prepareToPlay();

    // Get the current time before setting m_seeking, m_lastSeekTime is returned once it is set.
    refreshCachedTime();
    MediaTime now = currentMediaTime();

    // 3 - If the element's seeking IDL attribute is true, then another instance of this algorithm is
    // already running. Abort that other instance of the algorithm without waiting for the step that
    // it is running to complete.
    if (m_seekTaskQueue.hasPendingTask()) {
        INFO_LOG(LOGIDENTIFIER, "cancelling pending seeks");
        m_seekTaskQueue.cancelTask();
        if (m_pendingSeek) {
            now = m_pendingSeek->now;
            m_pendingSeek = nullptr;
        }
        m_pendingSeekType = NoSeek;
    }

    // 4 - Set the seeking IDL attribute to true.
    // The flag will be cleared when the engine tells us the time has actually changed.
    m_seeking = true;
    if (m_playing) {
        if (m_lastSeekTime < now)
            addPlayedRange(m_lastSeekTime, now);
    }
    m_lastSeekTime = time;

    // 5 - If the seek was in response to a DOM method call or setting of an IDL attribute, then continue
    // the script. The remainder of these steps must be run asynchronously.
    m_pendingSeek = std::make_unique<PendingSeek>(now, time, negativeTolerance, positiveTolerance);
    if (fromDOM) {
        INFO_LOG(LOGIDENTIFIER, "enqueuing seek from ", now, " to ", time);
        m_seekTaskQueue.scheduleTask(std::bind(&HTMLMediaElement::seekTask, this));
    } else
        seekTask();

    if (processingUserGestureForMedia())
        m_mediaSession->removeBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager);
}

void HTMLMediaElement::seekTask()
{
    INFO_LOG(LOGIDENTIFIER);

    if (!m_player) {
        clearSeeking();
        return;
    }

    ASSERT(m_pendingSeek);
    MediaTime now = m_pendingSeek->now;
    MediaTime time = m_pendingSeek->targetTime;
    MediaTime negativeTolerance = m_pendingSeek->negativeTolerance;
    MediaTime positiveTolerance = m_pendingSeek->positiveTolerance;
    m_pendingSeek = nullptr;

    ASSERT(negativeTolerance >= MediaTime::zeroTime());

    // 6 - If the new playback position is later than the end of the media resource, then let it be the end
    // of the media resource instead.
    time = std::min(time, durationMediaTime());

    // 7 - If the new playback position is less than the earliest possible position, let it be that position instead.
    MediaTime earliestTime = m_player->startTime();
    time = std::max(time, earliestTime);

    // Ask the media engine for the time value in the movie's time scale before comparing with current time. This
    // is necessary because if the seek time is not equal to currentTime but the delta is less than the movie's
    // time scale, we will ask the media engine to "seek" to the current movie time, which may be a noop and
    // not generate a timechanged callback. This means m_seeking will never be cleared and we will never
    // fire a 'seeked' event.
    if (willLog(WTFLogLevelDebug)) {
        MediaTime mediaTime = m_player->mediaTimeForTimeValue(time);
        if (time != mediaTime)
            DEBUG_LOG(LOGIDENTIFIER, time, " media timeline equivalent is ", mediaTime);
    }

    time = m_player->mediaTimeForTimeValue(time);

    // 8 - If the (possibly now changed) new playback position is not in one of the ranges given in the
    // seekable attribute, then let it be the position in one of the ranges given in the seekable attribute
    // that is the nearest to the new playback position. ... If there are no ranges given in the seekable
    // attribute then set the seeking IDL attribute to false and abort these steps.
    RefPtr<TimeRanges> seekableRanges = seekable();
    bool noSeekRequired = !seekableRanges->length();

    // Short circuit seeking to the current time by just firing the events if no seek is required.
    // Don't skip calling the media engine if 1) we are in poster mode (because a seek should always cancel
    // poster display), or 2) if there is a pending fast seek, or 3) if this seek is not an exact seek
    SeekType thisSeekType = (negativeTolerance == MediaTime::zeroTime() && positiveTolerance == MediaTime::zeroTime()) ? Precise : Fast;
    if (!noSeekRequired && time == now && thisSeekType == Precise && m_pendingSeekType != Fast && displayMode() != Poster)
        noSeekRequired = true;

#if ENABLE(MEDIA_SOURCE)
    // Always notify the media engine of a seek if the source is not closed. This ensures that the source is
    // always in a flushed state when the 'seeking' event fires.
    if (m_mediaSource && !m_mediaSource->isClosed())
        noSeekRequired = false;
#endif

    if (noSeekRequired) {
        INFO_LOG(LOGIDENTIFIER, "seek to ", time, " ignored");
        if (time == now) {
            scheduleEvent(eventNames().seekingEvent);
            scheduleTimeupdateEvent(false);
            scheduleEvent(eventNames().seekedEvent);
        }
        clearSeeking();
        return;
    }
    time = seekableRanges->ranges().nearest(time);

    m_sentEndEvent = false;
    m_lastSeekTime = time;
    m_pendingSeekType = thisSeekType;
    m_seeking = true;

    // 10 - Queue a task to fire a simple event named seeking at the element.
    scheduleEvent(eventNames().seekingEvent);

    // 11 - Set the current playback position to the given new playback position
    m_seekRequested = true;
    m_player->seekWithTolerance(time, negativeTolerance, positiveTolerance);

    // 12 - Wait until the user agent has established whether or not the media data for the new playback
    // position is available, and, if it is, until it has decoded enough data to play back that position.
    // 13 - Await a stable state. The synchronous section consists of all the remaining steps of this algorithm.
}

void HTMLMediaElement::clearSeeking()
{
    m_seeking = false;
    m_seekRequested = false;
    m_pendingSeekType = NoSeek;
    invalidateCachedTime();
}

void HTMLMediaElement::finishSeek()
{
    // 4.8.10.9 Seeking
    // 14 - Set the seeking IDL attribute to false.
    clearSeeking();

    INFO_LOG(LOGIDENTIFIER, "current time = ", currentMediaTime());

    // 15 - Run the time maches on steps.
    // Handled by mediaPlayerTimeChanged().

    // 16 - Queue a task to fire a simple event named timeupdate at the element.
    scheduleEvent(eventNames().timeupdateEvent);

    // 17 - Queue a task to fire a simple event named seeked at the element.
    scheduleEvent(eventNames().seekedEvent);

    if (m_mediaSession)
        m_mediaSession->clientCharacteristicsChanged();

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource)
        m_mediaSource->monitorSourceBuffers();
#endif
}

HTMLMediaElement::ReadyState HTMLMediaElement::readyState() const
{
    return m_readyState;
}

MediaPlayer::MovieLoadType HTMLMediaElement::movieLoadType() const
{
    return m_player ? m_player->movieLoadType() : MediaPlayer::Unknown;
}

bool HTMLMediaElement::hasAudio() const
{
    return m_player ? m_player->hasAudio() : false;
}

bool HTMLMediaElement::seeking() const
{
    return m_seeking;
}

void HTMLMediaElement::refreshCachedTime() const
{
    if (!m_player)
        return;

    m_cachedTime = m_player->currentTime();
    if (!m_cachedTime) {
        // Do not use m_cachedTime until the media engine returns a non-zero value because we can't
        // estimate current time until playback actually begins.
        invalidateCachedTime();
        return;
    }

    m_clockTimeAtLastCachedTimeUpdate = MonotonicTime::now();
}

void HTMLMediaElement::invalidateCachedTime() const
{
    m_cachedTime = MediaTime::invalidTime();
    if (!m_player || !m_player->maximumDurationToCacheMediaTime())
        return;

    // Don't try to cache movie time when playback first starts as the time reported by the engine
    // sometimes fluctuates for a short amount of time, so the cached time will be off if we take it
    // too early.
    static const Seconds minimumTimePlayingBeforeCacheSnapshot = 500_ms;

    m_minimumClockTimeToUpdateCachedTime = MonotonicTime::now() + minimumTimePlayingBeforeCacheSnapshot;
}

// playback state
double HTMLMediaElement::currentTime() const
{
    return currentMediaTime().toDouble();
}

MediaTime HTMLMediaElement::currentMediaTime() const
{
#if LOG_CACHED_TIME_WARNINGS
    static const MediaTime minCachedDeltaForWarning = MediaTime::create(1, 100);
#endif

    if (!m_player)
        return MediaTime::zeroTime();

    if (m_seeking) {
        INFO_LOG(LOGIDENTIFIER, "seeking, returning", m_lastSeekTime);
        return m_lastSeekTime;
    }

    if (m_cachedTime.isValid() && m_paused) {
#if LOG_CACHED_TIME_WARNINGS
        MediaTime delta = m_cachedTime - m_player->currentTime();
        if (delta > minCachedDeltaForWarning)
            WARNING_LOG(LOGIDENTIFIER, "cached time is ", delta, " seconds off of media time when paused");
#endif
        return m_cachedTime;
    }

    // Is it too soon use a cached time?
    MonotonicTime now = MonotonicTime::now();
    double maximumDurationToCacheMediaTime = m_player->maximumDurationToCacheMediaTime();

    if (maximumDurationToCacheMediaTime && m_cachedTime.isValid() && !m_paused && now > m_minimumClockTimeToUpdateCachedTime) {
        Seconds clockDelta = now - m_clockTimeAtLastCachedTimeUpdate;

        // Not too soon, use the cached time only if it hasn't expired.
        if (clockDelta.seconds() < maximumDurationToCacheMediaTime) {
            MediaTime adjustedCacheTime = m_cachedTime + MediaTime::createWithDouble(effectivePlaybackRate() * clockDelta.seconds());

#if LOG_CACHED_TIME_WARNINGS
            MediaTime delta = adjustedCacheTime - m_player->currentTime();
            if (delta > minCachedDeltaForWarning)
                WARNING_LOG(LOGIDENTIFIER, "cached time is ", delta, " seconds off of media time when playing");
#endif
            return adjustedCacheTime;
        }
    }

#if LOG_CACHED_TIME_WARNINGS
    if (maximumDurationToCacheMediaTime && now > m_minimumClockTimeToUpdateCachedTime && m_cachedTime != MediaPlayer::invalidTime()) {
        Seconds clockDelta = now - m_clockTimeAtLastCachedTimeUpdate;
        MediaTime delta = m_cachedTime + MediaTime::createWithDouble(effectivePlaybackRate() * clockDelta.seconds()) - m_player->currentTime();
        WARNING_LOG(LOGIDENTIFIER, "cached time was ", delta, " seconds off of media time when it expired");
    }
#endif

    refreshCachedTime();

    if (m_cachedTime.isInvalid())
        return MediaTime::zeroTime();

    return m_cachedTime;
}

void HTMLMediaElement::setCurrentTime(double time)
{
    setCurrentTime(MediaTime::createWithDouble(time));
}

void HTMLMediaElement::setCurrentTimeWithTolerance(double time, double toleranceBefore, double toleranceAfter)
{
    seekWithTolerance(MediaTime::createWithDouble(time), MediaTime::createWithDouble(toleranceBefore), MediaTime::createWithDouble(toleranceAfter), true);
}

void HTMLMediaElement::setCurrentTime(const MediaTime& time)
{
    if (m_mediaController)
        return;

    seekInternal(time);
}

ExceptionOr<void> HTMLMediaElement::setCurrentTimeForBindings(double time)
{
    if (m_mediaController)
        return Exception { InvalidStateError };
    seek(MediaTime::createWithDouble(time));
    return { };
}

double HTMLMediaElement::duration() const
{
    return durationMediaTime().toDouble();
}

MediaTime HTMLMediaElement::durationMediaTime() const
{
    if (m_player && m_readyState >= HAVE_METADATA)
        return m_player->duration();

    return MediaTime::invalidTime();
}

bool HTMLMediaElement::paused() const
{
    // As of this writing, JavaScript garbage collection calls this function directly. In the past
    // we had problems where this was called on an object after a bad cast. The assertion below
    // made our regression test detect the problem, so we should keep it because of that. But note
    // that the value of the assertion relies on the compiler not being smart enough to know that
    // isHTMLUnknownElement is guaranteed to return false for an HTMLMediaElement.
    ASSERT(!isHTMLUnknownElement());

    return m_paused;
}

double HTMLMediaElement::defaultPlaybackRate() const
{
#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // "defaultPlaybackRate" - On setting: ignored. On getting: return 1.0
    // A MediaStream is not seekable. Therefore, this attribute must always have the
    // value 1.0 and any attempt to alter it must be ignored. Note that this also means
    // that the ratechange event will not fire.
    if (m_mediaStreamSrcObject)
        return 1;
#endif

    return m_defaultPlaybackRate;
}

void HTMLMediaElement::setDefaultPlaybackRate(double rate)
{
#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // "defaultPlaybackRate" - On setting: ignored. On getting: return 1.0
    // A MediaStream is not seekable. Therefore, this attribute must always have the
    // value 1.0 and any attempt to alter it must be ignored. Note that this also means
    // that the ratechange event will not fire.
    if (m_mediaStreamSrcObject)
        return;
#endif

    if (m_defaultPlaybackRate == rate)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, rate);
    m_defaultPlaybackRate = rate;
    scheduleEvent(eventNames().ratechangeEvent);
}

double HTMLMediaElement::effectivePlaybackRate() const
{
    return m_mediaController ? m_mediaController->playbackRate() : m_reportedPlaybackRate;
}

double HTMLMediaElement::requestedPlaybackRate() const
{
    return m_mediaController ? m_mediaController->playbackRate() : m_requestedPlaybackRate;
}

double HTMLMediaElement::playbackRate() const
{
#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // "playbackRate" - A MediaStream is not seekable. Therefore, this attribute must always
    // have the value 1.0 and any attempt to alter it must be ignored. Note that this also
    // means that the ratechange event will not fire.
    if (m_mediaStreamSrcObject)
        return 1;
#endif

    return m_requestedPlaybackRate;
}

void HTMLMediaElement::setPlaybackRate(double rate)
{
    ALWAYS_LOG(LOGIDENTIFIER, rate);

#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // "playbackRate" - A MediaStream is not seekable. Therefore, this attribute must always
    // have the value 1.0 and any attempt to alter it must be ignored. Note that this also
    // means that the ratechange event will not fire.
    if (m_mediaStreamSrcObject)
        return;
#endif

    if (m_player && potentiallyPlaying() && m_player->rate() != rate && !m_mediaController)
        m_player->setRate(rate);

    if (m_requestedPlaybackRate != rate) {
        m_reportedPlaybackRate = m_requestedPlaybackRate = rate;
        invalidateCachedTime();
        scheduleEvent(eventNames().ratechangeEvent);
    }
}

void HTMLMediaElement::updatePlaybackRate()
{
    double requestedRate = requestedPlaybackRate();
    if (m_player && potentiallyPlaying() && m_player->rate() != requestedRate)
        m_player->setRate(requestedRate);
}

bool HTMLMediaElement::webkitPreservesPitch() const
{
    return m_webkitPreservesPitch;
}

void HTMLMediaElement::setWebkitPreservesPitch(bool preservesPitch)
{
    INFO_LOG(LOGIDENTIFIER, preservesPitch);

    m_webkitPreservesPitch = preservesPitch;

    if (!m_player)
        return;

    m_player->setPreservesPitch(preservesPitch);
}

bool HTMLMediaElement::ended() const
{
#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // When the MediaStream state moves from the active to the inactive state, the User Agent
    // must raise an ended event on the HTMLMediaElement and set its ended attribute to true.
    if (m_mediaStreamSrcObject && m_player && m_player->ended())
        return true;
#endif

    // 4.8.10.8 Playing the media resource
    // The ended attribute must return true if the media element has ended
    // playback and the direction of playback is forwards, and false otherwise.
    return endedPlayback() && requestedPlaybackRate() > 0;
}

bool HTMLMediaElement::autoplay() const
{
    return hasAttributeWithoutSynchronization(autoplayAttr);
}

String HTMLMediaElement::preload() const
{
#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // "preload" - On getting: none. On setting: ignored.
    if (m_mediaStreamSrcObject)
        return "none"_s;
#endif

    switch (m_preload) {
    case MediaPlayer::None:
        return "none"_s;
    case MediaPlayer::MetaData:
        return "metadata"_s;
    case MediaPlayer::Auto:
        return "auto"_s;
    }

    ASSERT_NOT_REACHED();
    return String();
}

void HTMLMediaElement::setPreload(const String& preload)
{
    INFO_LOG(LOGIDENTIFIER, preload);
#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // "preload" - On getting: none. On setting: ignored.
    if (m_mediaStreamSrcObject)
        return;
#endif

    setAttributeWithoutSynchronization(preloadAttr, preload);
}

void HTMLMediaElement::play(DOMPromiseDeferred<void>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    auto success = m_mediaSession->playbackPermitted();
    if (!success) {
        if (success.value() == MediaPlaybackDenialReason::UserGestureRequired)
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
        promise.reject(NotAllowedError);
        return;
    }

    if (m_error && m_error->code() == MediaError::MEDIA_ERR_SRC_NOT_SUPPORTED) {
        promise.reject(NotSupportedError, "The operation is not supported.");
        return;
    }

    if (processingUserGestureForMedia())
        removeBehaviorsRestrictionsAfterFirstUserGesture();

    m_pendingPlayPromises.append(WTFMove(promise));
    playInternal();
}

void HTMLMediaElement::play()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    auto success = m_mediaSession->playbackPermitted();
    if (!success) {
        if (success.value() == MediaPlaybackDenialReason::UserGestureRequired)
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
        return;
    }
    if (processingUserGestureForMedia())
        removeBehaviorsRestrictionsAfterFirstUserGesture();

    playInternal();
}

void HTMLMediaElement::playInternal()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (isSuspended()) {
        ALWAYS_LOG(LOGIDENTIFIER, "  returning because context is suspended");
        return;
    }

    if (!document().hasBrowsingContext()) {
        INFO_LOG(LOGIDENTIFIER, "  returning because there is no browsing context");
        return;
    }

    if (!m_mediaSession->clientWillBeginPlayback()) {
        ALWAYS_LOG(LOGIDENTIFIER, "  returning because of interruption");
        return;
    }
    
    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY)
        selectMediaResource();

    if (endedPlayback())
        seekInternal(MediaTime::zeroTime());

    if (m_mediaController)
        m_mediaController->bringElementUpToSpeed(*this);

    if (m_paused) {
        m_paused = false;
        invalidateCachedTime();
        m_playbackStartedTime = currentMediaTime().toDouble();
        scheduleEvent(eventNames().playEvent);

#if ENABLE(MEDIA_SESSION)
        // 6.3 Activating a media session from a media element
        // When the play() method is invoked, the paused attribute is true, and the readyState attribute has the value
        // HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA, then
        // 1. Let media session be the value of the current media session.
        // 2. If we are not currently in media session's list of active participating media elements then append
        //    ourselves to this list.
        // 3. Let activated be the result of running the media session invocation algorithm for media session.
        // 4. If activated is failure, pause ourselves.
        if (m_readyState == HAVE_ENOUGH_DATA || m_readyState == HAVE_FUTURE_DATA) {
            if (m_session) {
                m_session->addActiveMediaElement(*this);

                if (m_session->kind() == MediaSessionKind::Content) {
                    if (Page* page = document().page())
                        page->chrome().client().focusedContentMediaElementDidChange(m_elementID);
                }

                if (!m_session->invoke()) {
                    pause();
                    return;
                }
            }
        }
#endif
        if (m_readyState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().waitingEvent);
    } else if (m_readyState >= HAVE_FUTURE_DATA)
        scheduleResolvePendingPlayPromises();

    if (processingUserGestureForMedia()) {
        if (m_autoplayEventPlaybackState == AutoplayEventPlaybackState::PreventedAutoplay) {
            handleAutoplayEvent(AutoplayEvent::DidPlayMediaWithUserGesture);
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);
        } else
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::StartedWithUserGesture);
    } else
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::StartedWithoutUserGesture);

    m_autoplaying = false;
    updatePlayState();
}

void HTMLMediaElement::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_temporarilyAllowingInlinePlaybackAfterFullscreen = false;

    if (m_waitingToEnterFullscreen)
        m_waitingToEnterFullscreen = false;

    if (!m_mediaSession->playbackPermitted())
        return;

    if (processingUserGestureForMedia())
        removeBehaviorsRestrictionsAfterFirstUserGesture(MediaElementSession::RequireUserGestureToControlControlsManager);

    pauseInternal();
}


void HTMLMediaElement::pauseInternal()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (isSuspended()) {
        ALWAYS_LOG(LOGIDENTIFIER, "  returning because context is suspended");
        return;
    }

    if (!document().hasBrowsingContext()) {
        INFO_LOG(LOGIDENTIFIER, "  returning because there is no browsing context");
        return;
    }

    if (!m_mediaSession->clientWillPausePlayback()) {
        ALWAYS_LOG(LOGIDENTIFIER, "  returning because of interruption");
        return;
    }

    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY) {
        // Unless the restriction on media requiring user action has been lifted
        // don't trigger loading if a script calls pause().
        if (!m_mediaSession->playbackPermitted())
            return;
        selectMediaResource();
    }

    m_autoplaying = false;

    if (processingUserGestureForMedia())
        userDidInterfereWithAutoplay();

    setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);

    if (!m_paused) {
        m_paused = true;
        scheduleTimeupdateEvent(false);
        scheduleEvent(eventNames().pauseEvent);
        scheduleRejectPendingPlayPromises(DOMException::create(AbortError));
        if (MemoryPressureHandler::singleton().isUnderMemoryPressure())
            purgeBufferedDataIfPossible();
    }

    updatePlayState();
}

#if ENABLE(MEDIA_SOURCE)

void HTMLMediaElement::detachMediaSource()
{
    if (!m_mediaSource)
        return;

    m_mediaSource->detachFromElement(*this);
    m_mediaSource = nullptr;
}

#endif

bool HTMLMediaElement::loop() const
{
    return hasAttributeWithoutSynchronization(loopAttr);
}

void HTMLMediaElement::setLoop(bool b)
{
    INFO_LOG(LOGIDENTIFIER, b);
    setBooleanAttribute(loopAttr, b);
}

bool HTMLMediaElement::controls() const
{
    RefPtr<Frame> frame = document().frame();

    // always show controls when scripting is disabled
    if (frame && !frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return true;

    return hasAttributeWithoutSynchronization(controlsAttr);
}

void HTMLMediaElement::setControls(bool b)
{
    INFO_LOG(LOGIDENTIFIER, b);
    setBooleanAttribute(controlsAttr, b);
}

double HTMLMediaElement::volume() const
{
    return m_volume;
}

ExceptionOr<void> HTMLMediaElement::setVolume(double volume)
{
    INFO_LOG(LOGIDENTIFIER, volume);

    if (!(volume >= 0 && volume <= 1))
        return Exception { IndexSizeError };

#if !PLATFORM(IOS_FAMILY)
    if (m_volume == volume)
        return { };

    if (volume && processingUserGestureForMedia())
        removeBehaviorsRestrictionsAfterFirstUserGesture(MediaElementSession::AllRestrictions & ~MediaElementSession::RequireUserGestureToControlControlsManager);

    m_volume = volume;
    m_volumeInitialized = true;
    updateVolume();
    scheduleEvent(eventNames().volumechangeEvent);

    if (isPlaying() && !m_mediaSession->playbackPermitted()) {
        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    }
#endif
    return { };
}

bool HTMLMediaElement::muted() const
{
    return m_explicitlyMuted ? m_muted : hasAttributeWithoutSynchronization(mutedAttr);
}

void HTMLMediaElement::setMuted(bool muted)
{
    INFO_LOG(LOGIDENTIFIER, muted);

    bool mutedStateChanged = m_muted != muted;
    if (mutedStateChanged || !m_explicitlyMuted) {
        if (processingUserGestureForMedia()) {
            removeBehaviorsRestrictionsAfterFirstUserGesture(MediaElementSession::AllRestrictions & ~MediaElementSession::RequireUserGestureToControlControlsManager);

            if (hasAudio() && muted)
                userDidInterfereWithAutoplay();
        }

        m_muted = muted;
        m_explicitlyMuted = true;

        // Avoid recursion when the player reports volume changes.
        if (!processingMediaPlayerCallback()) {
            if (m_player) {
                m_player->setMuted(effectiveMuted());
                if (hasMediaControls())
                    mediaControls()->changedMute();
            }
        }

        if (mutedStateChanged)
            scheduleEvent(eventNames().volumechangeEvent);

        updateShouldPlay();

#if ENABLE(MEDIA_SESSION)
        document().updateIsPlayingMedia(m_elementID);
#else
        document().updateIsPlayingMedia();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        scheduleUpdateMediaState();
#endif
        m_mediaSession->canProduceAudioChanged();
    }

    schedulePlaybackControlsManagerUpdate();
}

#if USE(AUDIO_SESSION) && PLATFORM(MAC)
void HTMLMediaElement::hardwareMutedStateDidChange(AudioSession* session)
{
    if (!session->isMuted())
        return;

    if (!hasAudio())
        return;

    if (effectiveMuted() || !volume())
        return;

    INFO_LOG(LOGIDENTIFIER);
    userDidInterfereWithAutoplay();
}
#endif

void HTMLMediaElement::togglePlayState()
{
    INFO_LOG(LOGIDENTIFIER, "canPlay() is ", canPlay());

    // We can safely call the internal play/pause methods, which don't check restrictions, because
    // this method is only called from the built-in media controller
    if (canPlay()) {
        updatePlaybackRate();
        playInternal();
    } else
        pauseInternal();
}

void HTMLMediaElement::beginScrubbing()
{
    INFO_LOG(LOGIDENTIFIER, "paused() is ", paused());

    if (!paused()) {
        if (ended()) {
            // Because a media element stays in non-paused state when it reaches end, playback resumes
            // when the slider is dragged from the end to another position unless we pause first. Do
            // a "hard pause" so an event is generated, since we want to stay paused after scrubbing finishes.
            pause();
        } else {
            // Not at the end but we still want to pause playback so the media engine doesn't try to
            // continue playing during scrubbing. Pause without generating an event as we will
            // unpause after scrubbing finishes.
            setPausedInternal(true);
        }
    }

    m_mediaSession->removeBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager);
}

void HTMLMediaElement::endScrubbing()
{
    INFO_LOG(LOGIDENTIFIER, "m_pausedInternal is", m_pausedInternal);

    if (m_pausedInternal)
        setPausedInternal(false);
}

void HTMLMediaElement::beginScanning(ScanDirection direction)
{
    m_scanType = supportsScanning() ? Scan : Seek;
    m_scanDirection = direction;

    if (m_scanType == Seek) {
        // Scanning by seeking requires the video to be paused during scanning.
        m_actionAfterScan = paused() ? Nothing : Play;
        pause();
    } else {
        // Scanning by scanning requires the video to be playing during scanninging.
        m_actionAfterScan = paused() ? Pause : Nothing;
        play();
        setPlaybackRate(nextScanRate());
    }

    m_scanTimer.start(0_s, m_scanType == Seek ? SeekRepeatDelay : ScanRepeatDelay);
}

void HTMLMediaElement::endScanning()
{
    if (m_scanType == Scan)
        setPlaybackRate(defaultPlaybackRate());

    if (m_actionAfterScan == Play)
        play();
    else if (m_actionAfterScan == Pause)
        pause();

    if (m_scanTimer.isActive())
        m_scanTimer.stop();
}

double HTMLMediaElement::nextScanRate()
{
    double rate = std::min(ScanMaximumRate, fabs(playbackRate() * 2));
    if (m_scanDirection == Backward)
        rate *= -1;
#if PLATFORM(IOS_FAMILY)
    rate = std::min(std::max(rate, minFastReverseRate()), maxFastForwardRate());
#endif
    return rate;
}

void HTMLMediaElement::scanTimerFired()
{
    if (m_scanType == Seek) {
        double seekTime = m_scanDirection == Forward ? SeekTime : -SeekTime;
        setCurrentTime(currentTime() + seekTime);
    } else
        setPlaybackRate(nextScanRate());
}

// The spec says to fire periodic timeupdate events (those sent while playing) every
// "15 to 250ms", we choose the slowest frequency
static const Seconds maxTimeupdateEventFrequency { 250_ms };

void HTMLMediaElement::startPlaybackProgressTimer()
{
    if (m_playbackProgressTimer.isActive())
        return;

    m_previousProgressTime = MonotonicTime::now();
    m_playbackProgressTimer.startRepeating(maxTimeupdateEventFrequency);
}

void HTMLMediaElement::playbackProgressTimerFired()
{
    ASSERT(m_player);

    if (m_fragmentEndTime.isValid() && currentMediaTime() >= m_fragmentEndTime && requestedPlaybackRate() > 0) {
        m_fragmentEndTime = MediaTime::invalidTime();
        if (!m_mediaController && !m_paused) {
            // changes paused to true and fires a simple event named pause at the media element.
            pauseInternal();
        }
    }

    scheduleTimeupdateEvent(true);

    if (!requestedPlaybackRate())
        return;

    if (!m_paused && hasMediaControls())
        mediaControls()->playbackProgressed();

#if ENABLE(VIDEO_TRACK)
    updateActiveTextTrackCues(currentMediaTime());
#endif

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource)
        m_mediaSource->monitorSourceBuffers();
#endif

    bool playbackStarted = m_autoplayEventPlaybackState == AutoplayEventPlaybackState::StartedWithUserGesture || m_autoplayEventPlaybackState == AutoplayEventPlaybackState::StartedWithoutUserGesture;
    if (!seeking() && playbackStarted && currentTime() - playbackStartedTime() > AutoplayInterferenceTimeThreshold) {
        handleAutoplayEvent(m_autoplayEventPlaybackState == AutoplayEventPlaybackState::StartedWithoutUserGesture ? AutoplayEvent::DidAutoplayMediaPastThresholdWithoutUserInterference : AutoplayEvent::DidPlayMediaWithUserGesture);
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);
    }
}

void HTMLMediaElement::scheduleTimeupdateEvent(bool periodicEvent)
{
    MonotonicTime now = MonotonicTime::now();
    Seconds timedelta = now - m_clockTimeAtLastUpdateEvent;

    // throttle the periodic events
    if (periodicEvent && timedelta < maxTimeupdateEventFrequency)
        return;

    // Some media engines make multiple "time changed" callbacks at the same time, but we only want one
    // event at a given time so filter here
    MediaTime movieTime = currentMediaTime();
    if (movieTime != m_lastTimeUpdateEventMovieTime) {
        scheduleEvent(eventNames().timeupdateEvent);
        m_clockTimeAtLastUpdateEvent = now;
        m_lastTimeUpdateEventMovieTime = movieTime;
    }
}

bool HTMLMediaElement::canPlay() const
{
    return paused() || ended() || m_readyState < HAVE_METADATA;
}

double HTMLMediaElement::percentLoaded() const
{
    if (!m_player)
        return 0;
    MediaTime duration = m_player->duration();

    if (!duration || duration.isPositiveInfinite() || duration.isNegativeInfinite())
        return 0;

    MediaTime buffered = MediaTime::zeroTime();
    bool ignored;
    std::unique_ptr<PlatformTimeRanges> timeRanges = m_player->buffered();
    for (unsigned i = 0; i < timeRanges->length(); ++i) {
        MediaTime start = timeRanges->start(i, ignored);
        MediaTime end = timeRanges->end(i, ignored);
        buffered += end - start;
    }
    return buffered.toDouble() / duration.toDouble();
}

#if ENABLE(VIDEO_TRACK)

void HTMLMediaElement::mediaPlayerDidAddAudioTrack(AudioTrackPrivate& track)
{
    if (isPlaying() && !m_mediaSession->playbackPermitted()) {
        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    }

    addAudioTrack(AudioTrack::create(*this, track));
}

void HTMLMediaElement::mediaPlayerDidAddTextTrack(InbandTextTrackPrivate& track)
{
    // 4.8.10.12.2 Sourcing in-band text tracks
    // 1. Associate the relevant data with a new text track and its corresponding new TextTrack object.
    auto textTrack = InbandTextTrack::create(*ActiveDOMObject::scriptExecutionContext(), *this, track);
    textTrack->setMediaElement(this);

    // 2. Set the new text track's kind, label, and language based on the semantics of the relevant data,
    // as defined by the relevant specification. If there is no label in that data, then the label must
    // be set to the empty string.
    // 3. Associate the text track list of cues with the rules for updating the text track rendering appropriate
    // for the format in question.
    // 4. If the new text track's kind is metadata, then set the text track in-band metadata track dispatch type
    // as follows, based on the type of the media resource:
    // 5. Populate the new text track's list of cues with the cues parsed so far, folllowing the guidelines for exposing
    // cues, and begin updating it dynamically as necessary.
    //   - Thess are all done by the media engine.

    // 6. Set the new text track's readiness state to loaded.
    textTrack->setReadinessState(TextTrack::Loaded);

    // 7. Set the new text track's mode to the mode consistent with the user's preferences and the requirements of
    // the relevant specification for the data.
    //  - This will happen in configureTextTracks()
    scheduleConfigureTextTracks();

    // 8. Add the new text track to the media element's list of text tracks.
    // 9. Fire an event with the name addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent
    // interface, with the track attribute initialized to the text track's TextTrack object, at the media element's
    // textTracks attribute's TextTrackList object.
    addTextTrack(WTFMove(textTrack));
}

void HTMLMediaElement::mediaPlayerDidAddVideoTrack(VideoTrackPrivate& track)
{
    addVideoTrack(VideoTrack::create(*this, track));
}

void HTMLMediaElement::mediaPlayerDidRemoveAudioTrack(AudioTrackPrivate& track)
{
    track.willBeRemoved();
}

void HTMLMediaElement::mediaPlayerDidRemoveTextTrack(InbandTextTrackPrivate& track)
{
    track.willBeRemoved();
}

void HTMLMediaElement::mediaPlayerDidRemoveVideoTrack(VideoTrackPrivate& track)
{
    track.willBeRemoved();
}

void HTMLMediaElement::closeCaptionTracksChanged()
{
    if (hasMediaControls())
        mediaControls()->closedCaptionTracksChanged();
}

void HTMLMediaElement::addAudioTrack(Ref<AudioTrack>&& track)
{
    ensureAudioTracks().append(WTFMove(track));
}

void HTMLMediaElement::addTextTrack(Ref<TextTrack>&& track)
{
    if (!m_requireCaptionPreferencesChangedCallbacks) {
        m_requireCaptionPreferencesChangedCallbacks = true;
        Document& document = this->document();
        document.registerForCaptionPreferencesChangedCallbacks(this);
        if (Page* page = document.page())
            m_captionDisplayMode = page->group().captionPreferences().captionDisplayMode();
    }

    ensureTextTracks().append(WTFMove(track));

    closeCaptionTracksChanged();
}

void HTMLMediaElement::addVideoTrack(Ref<VideoTrack>&& track)
{
    ensureVideoTracks().append(WTFMove(track));
}

void HTMLMediaElement::removeAudioTrack(Ref<AudioTrack>&& track)
{
    track->clearClient();
    m_audioTracks->remove(track.get());
}

void HTMLMediaElement::removeTextTrack(Ref<TextTrack>&& track, bool scheduleEvent)
{
    TrackDisplayUpdateScope scope { *this };
    if (auto cues = makeRefPtr(track->cues()))
        textTrackRemoveCues(track, *cues);
    track->clearClient();
    if (m_textTracks)
        m_textTracks->remove(track, scheduleEvent);

    closeCaptionTracksChanged();
}

void HTMLMediaElement::removeVideoTrack(Ref<VideoTrack>&& track)
{
    track->clearClient();
    m_videoTracks->remove(track);
}

void HTMLMediaElement::forgetResourceSpecificTracks()
{
    while (m_audioTracks &&  m_audioTracks->length())
        removeAudioTrack(*m_audioTracks->lastItem());

    if (m_textTracks) {
        TrackDisplayUpdateScope scope { *this };
        for (int i = m_textTracks->length() - 1; i >= 0; --i) {
            auto track = makeRef(*m_textTracks->item(i));
            if (track->trackType() == TextTrack::InBand)
                removeTextTrack(WTFMove(track), false);
        }
    }

    while (m_videoTracks &&  m_videoTracks->length())
        removeVideoTrack(*m_videoTracks->lastItem());
}

ExceptionOr<TextTrack&> HTMLMediaElement::addTextTrack(const String& kind, const String& label, const String& language)
{
    // 4.8.10.12.4 Text track API
    // The addTextTrack(kind, label, language) method of media elements, when invoked, must run the following steps:

    // 1. If kind is not one of the following strings, then throw a SyntaxError exception and abort these steps
    if (!TextTrack::isValidKindKeyword(kind))
        return Exception { TypeError };

    // 2. If the label argument was omitted, let label be the empty string.
    // 3. If the language argument was omitted, let language be the empty string.
    // 4. Create a new TextTrack object.

    // 5. Create a new text track corresponding to the new object, and set its text track kind to kind, its text
    // track label to label, its text track language to language...
    auto track = TextTrack::create(ActiveDOMObject::scriptExecutionContext(), this, kind, emptyString(), label, language);
    auto& trackReference = track.get();

    // Note, due to side effects when changing track parameters, we have to
    // first append the track to the text track list.

    // 6. Add the new text track to the media element's list of text tracks.
    addTextTrack(WTFMove(track));

    // ... its text track readiness state to the text track loaded state ...
    trackReference.setReadinessState(TextTrack::Loaded);

    // ... its text track mode to the text track hidden mode, and its text track list of cues to an empty list ...
    trackReference.setMode(TextTrack::Mode::Hidden);

    return trackReference;
}

AudioTrackList& HTMLMediaElement::ensureAudioTracks()
{
    if (!m_audioTracks)
        m_audioTracks = AudioTrackList::create(this, ActiveDOMObject::scriptExecutionContext());

    return *m_audioTracks;
}

TextTrackList& HTMLMediaElement::ensureTextTracks()
{
    if (!m_textTracks)
        m_textTracks = TextTrackList::create(this, ActiveDOMObject::scriptExecutionContext());

    return *m_textTracks;
}

VideoTrackList& HTMLMediaElement::ensureVideoTracks()
{
    if (!m_videoTracks)
        m_videoTracks = VideoTrackList::create(this, ActiveDOMObject::scriptExecutionContext());

    return *m_videoTracks;
}

void HTMLMediaElement::didAddTextTrack(HTMLTrackElement& trackElement)
{
    ASSERT(trackElement.hasTagName(trackTag));

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the new parent is a media element,
    // then the user agent must add the track element's corresponding text track to the
    // media element's list of text tracks ... [continues in TextTrackList::append]
    addTextTrack(trackElement.track());

    // Do not schedule the track loading until parsing finishes so we don't start before all tracks
    // in the markup have been added.
    if (!m_parsingInProgress)
        scheduleConfigureTextTracks();

    if (hasMediaControls())
        mediaControls()->closedCaptionTracksChanged();
}

void HTMLMediaElement::didRemoveTextTrack(HTMLTrackElement& trackElement)
{
    ASSERT(trackElement.hasTagName(trackTag));

    auto& textTrack = trackElement.track();

    textTrack.setHasBeenConfigured(false);

    if (!m_textTracks)
        return;

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the old parent was a media element,
    // then the user agent must remove the track element's corresponding text track from the
    // media element's list of text tracks.
    removeTextTrack(textTrack);

    m_textTracksWhenResourceSelectionBegan.removeFirst(&textTrack);
}

void HTMLMediaElement::configureTextTrackGroup(const TrackGroup& group)
{
    ASSERT(group.tracks.size());

    Page* page = document().page();
    CaptionUserPreferences* captionPreferences = page ? &page->group().captionPreferences() : 0;
    CaptionUserPreferences::CaptionDisplayMode displayMode = captionPreferences ? captionPreferences->captionDisplayMode() : CaptionUserPreferences::Automatic;

    // First, find the track in the group that should be enabled (if any).
    Vector<RefPtr<TextTrack>> currentlyEnabledTracks;
    RefPtr<TextTrack> trackToEnable;
    RefPtr<TextTrack> defaultTrack;
    RefPtr<TextTrack> fallbackTrack;
    RefPtr<TextTrack> forcedSubitleTrack;
    int highestTrackScore = 0;
    int highestForcedScore = 0;

    // If there is a visible track, it has already been configured so it won't be considered in the loop below. We don't want to choose another
    // track if it is less suitable, and we do want to disable it if another track is more suitable.
    int alreadyVisibleTrackScore = 0;
    if (group.visibleTrack && captionPreferences) {
        alreadyVisibleTrackScore = captionPreferences->textTrackSelectionScore(group.visibleTrack.get(), this);
        currentlyEnabledTracks.append(group.visibleTrack);
    }

    for (size_t i = 0; i < group.tracks.size(); ++i) {
        RefPtr<TextTrack> textTrack = group.tracks[i];

        if (m_processingPreferenceChange && textTrack->mode() == TextTrack::Mode::Showing)
            currentlyEnabledTracks.append(textTrack);

        int trackScore = captionPreferences ? captionPreferences->textTrackSelectionScore(textTrack.get(), this) : 0;
        INFO_LOG(LOGIDENTIFIER, "'", textTrack->kindKeyword(), "' track with language '", textTrack->language(), "' and BCP 47 language '", textTrack->validBCP47Language(), "' has score ", trackScore);

        if (trackScore) {

            // * If the text track kind is { [subtitles or captions] [descriptions] } and the user has indicated an interest in having a
            // track with this text track kind, text track language, and text track label enabled, and there is no
            // other text track in the media element's list of text tracks with a text track kind of either subtitles
            // or captions whose text track mode is showing
            // ...
            // * If the text track kind is chapters and the text track language is one that the user agent has reason
            // to believe is appropriate for the user, and there is no other text track in the media element's list of
            // text tracks with a text track kind of chapters whose text track mode is showing
            //    Let the text track mode be showing.
            if (trackScore > highestTrackScore && trackScore > alreadyVisibleTrackScore) {
                highestTrackScore = trackScore;
                trackToEnable = textTrack;
            }

            if (!defaultTrack && textTrack->isDefault())
                defaultTrack = textTrack;
            if (!defaultTrack && !fallbackTrack)
                fallbackTrack = textTrack;
            if (textTrack->containsOnlyForcedSubtitles() && trackScore > highestForcedScore) {
                forcedSubitleTrack = textTrack;
                highestForcedScore = trackScore;
            }
        } else if (!group.visibleTrack && !defaultTrack && textTrack->isDefault()) {
            // * If the track element has a default attribute specified, and there is no other text track in the media
            // element's list of text tracks whose text track mode is showing or showing by default
            //    Let the text track mode be showing by default.
            if (group.kind != TrackGroup::CaptionsAndSubtitles || displayMode != CaptionUserPreferences::ForcedOnly)
                defaultTrack = textTrack;
        }
    }

    if (displayMode != CaptionUserPreferences::Manual) {
        if (!trackToEnable && defaultTrack)
            trackToEnable = defaultTrack;

        // If no track matches the user's preferred language, none was marked as 'default', and there is a forced subtitle track
        // in the same language as the language of the primary audio track, enable it.
        if (!trackToEnable && forcedSubitleTrack)
            trackToEnable = forcedSubitleTrack;

        // If no track matches, don't disable an already visible track unless preferences say they all should be off.
        if (group.kind != TrackGroup::CaptionsAndSubtitles || displayMode != CaptionUserPreferences::ForcedOnly) {
            if (!trackToEnable && !defaultTrack && group.visibleTrack)
                trackToEnable = group.visibleTrack;
        }

        // If no track matches the user's preferred language and non was marked 'default', enable the first track
        // because the user has explicitly stated a preference for this kind of track.
        if (!trackToEnable && fallbackTrack)
            trackToEnable = fallbackTrack;

        if (trackToEnable)
            m_subtitleTrackLanguage = trackToEnable->language();
        else
            m_subtitleTrackLanguage = emptyString();
    }

    if (currentlyEnabledTracks.size()) {
        for (size_t i = 0; i < currentlyEnabledTracks.size(); ++i) {
            RefPtr<TextTrack> textTrack = currentlyEnabledTracks[i];
            if (textTrack != trackToEnable)
                textTrack->setMode(TextTrack::Mode::Disabled);
        }
    }

    if (trackToEnable) {
        trackToEnable->setMode(TextTrack::Mode::Showing);

        // If user preferences indicate we should always display captions, make sure we reflect the
        // proper status via the webkitClosedCaptionsVisible API call:
        if (!webkitClosedCaptionsVisible() && closedCaptionsVisible() && displayMode == CaptionUserPreferences::AlwaysOn)
            m_webkitLegacyClosedCaptionOverride = true;
    }

    m_processingPreferenceChange = false;
}

static JSC::JSValue controllerJSValue(JSC::ExecState& exec, JSDOMGlobalObject& globalObject, HTMLMediaElement& media)
{
    JSC::VM& vm = globalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto mediaJSWrapper = toJS(&exec, &globalObject, media);

    // Retrieve the controller through the JS object graph
    JSC::JSObject* mediaJSWrapperObject = JSC::jsDynamicCast<JSC::JSObject*>(vm, mediaJSWrapper);
    if (!mediaJSWrapperObject)
        return JSC::jsNull();

    JSC::Identifier controlsHost = JSC::Identifier::fromString(&vm, "controlsHost");
    JSC::JSValue controlsHostJSWrapper = mediaJSWrapperObject->get(&exec, controlsHost);
    RETURN_IF_EXCEPTION(scope, JSC::jsNull());

    JSC::JSObject* controlsHostJSWrapperObject = JSC::jsDynamicCast<JSC::JSObject*>(vm, controlsHostJSWrapper);
    if (!controlsHostJSWrapperObject)
        return JSC::jsNull();

    JSC::Identifier controllerID = JSC::Identifier::fromString(&vm, "controller");
    JSC::JSValue controllerJSWrapper = controlsHostJSWrapperObject->get(&exec, controllerID);
    RETURN_IF_EXCEPTION(scope, JSC::jsNull());

    return controllerJSWrapper;
}

void HTMLMediaElement::ensureMediaControlsShadowRoot()
{
    ASSERT(!m_creatingControls);
    m_creatingControls = true;
    ensureUserAgentShadowRoot();
    m_creatingControls = false;
}

bool HTMLMediaElement::setupAndCallJS(const JSSetupFunction& task)
{
    Page* page = document().page();
    if (!page)
        return false;

    auto pendingActivity = makePendingActivity(*this);
    auto& world = ensureIsolatedWorld();
    auto& scriptController = document().frame()->script();
    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(scriptController.globalObject(world));
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* exec = globalObject->globalExec();

    RETURN_IF_EXCEPTION(scope, false);

    return task(*globalObject, *exec, scriptController, world);
}

void HTMLMediaElement::updateCaptionContainer()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (m_haveSetUpCaptionContainer)
        return;

    if (!ensureMediaControlsInjectedScript())
        return;

    ensureMediaControlsShadowRoot();

    if (!m_mediaControlsHost)
        m_mediaControlsHost = MediaControlsHost::create(this);

    setupAndCallJS([this](JSDOMGlobalObject& globalObject, JSC::ExecState& exec, ScriptController&, DOMWrapperWorld&) {
        auto& vm = globalObject.vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);
        auto controllerValue = controllerJSValue(exec, globalObject, *this);
        auto* controllerObject = JSC::jsDynamicCast<JSC::JSObject*>(vm, controllerValue);
        if (!controllerObject)
            return false;

        // The media controls script must provide a method on the Controller object with the following details.
        // Name: updateCaptionContainer
        // Parameters:
        //     None
        // Return value:
        //     None
        auto methodValue = controllerObject->get(&exec, JSC::Identifier::fromString(&exec, "updateCaptionContainer"));
        auto* methodObject = JSC::jsDynamicCast<JSC::JSObject*>(vm, methodValue);
        if (!methodObject)
            return false;

        JSC::CallData callData;
        auto callType = methodObject->methodTable(vm)->getCallData(methodObject, callData);
        if (callType == JSC::CallType::None)
            return false;

        JSC::MarkedArgumentBuffer noArguments;
        ASSERT(!noArguments.hasOverflowed());
        JSC::call(&exec, methodObject, callType, callData, controllerObject, noArguments);
        scope.clearException();

        m_haveSetUpCaptionContainer = true;

        return true;
    });

#endif
}

void HTMLMediaElement::layoutSizeChanged()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (auto frameView = makeRefPtr(document().view())) {
        auto task = [this, protectedThis = makeRef(*this)] {
            if (auto root = userAgentShadowRoot())
                root->dispatchEvent(Event::create("resize", Event::CanBubble::No, Event::IsCancelable::No));
        };
        frameView->queuePostLayoutCallback(WTFMove(task));
    }
#endif

    if (!m_receivedLayoutSizeChanged) {
        m_receivedLayoutSizeChanged = true;
        schedulePlaybackControlsManagerUpdate();
    }

    // If the video is a candidate for main content, we should register it for viewport visibility callbacks
    // if it hasn't already been registered.
    if (renderer() && m_mediaSession && !m_mediaSession->wantsToObserveViewportVisibilityForAutoplay() && m_mediaSession->wantsToObserveViewportVisibilityForMediaControls())
        renderer()->registerForVisibleInViewportCallback();
}

void HTMLMediaElement::visibilityDidChange()
{
    updateShouldAutoplay();
}

void HTMLMediaElement::setSelectedTextTrack(TextTrack* trackToSelect)
{
    auto* trackList = textTracks();
    if (!trackList || !trackList->length())
        return;

    if (trackToSelect == TextTrack::captionMenuAutomaticItem()) {
        if (captionDisplayMode() != CaptionUserPreferences::Automatic)
            m_textTracks->scheduleChangeEvent();
    } else if (trackToSelect == TextTrack::captionMenuOffItem()) {
        for (int i = 0, length = trackList->length(); i < length; ++i)
            trackList->item(i)->setMode(TextTrack::Mode::Disabled);

        if (captionDisplayMode() != CaptionUserPreferences::ForcedOnly && !trackList->isChangeEventScheduled())
            m_textTracks->scheduleChangeEvent();
    } else {
        if (!trackToSelect || !trackList->contains(*trackToSelect))
            return;

        for (int i = 0, length = trackList->length(); i < length; ++i) {
            auto& track = *trackList->item(i);
            if (&track != trackToSelect)
                track.setMode(TextTrack::Mode::Disabled);
            else
                track.setMode(TextTrack::Mode::Showing);
        }
    }

    if (!document().page())
        return;

    auto& captionPreferences = document().page()->group().captionPreferences();
    CaptionUserPreferences::CaptionDisplayMode displayMode;
    if (trackToSelect == TextTrack::captionMenuOffItem())
        displayMode = CaptionUserPreferences::ForcedOnly;
    else if (trackToSelect == TextTrack::captionMenuAutomaticItem())
        displayMode = CaptionUserPreferences::Automatic;
    else {
        displayMode = CaptionUserPreferences::AlwaysOn;
        if (trackToSelect->validBCP47Language().length())
            captionPreferences.setPreferredLanguage(trackToSelect->validBCP47Language());
    }

    captionPreferences.setCaptionDisplayMode(displayMode);
}

void HTMLMediaElement::scheduleConfigureTextTracks()
{
    if (m_configureTextTracksTask.hasPendingTask())
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier, "task scheduled");
    m_configureTextTracksTask.scheduleTask([this, logSiteIdentifier] {
        UNUSED_PARAM(logSiteIdentifier);
        ALWAYS_LOG(logSiteIdentifier, "- lambda(), task fired");
        Ref<HTMLMediaElement> protectedThis(*this); // configureTextTracks calls methods that can trigger arbitrary DOM mutations.
        configureTextTracks();
    });
}

void HTMLMediaElement::configureTextTracks()
{
    TrackGroup captionAndSubtitleTracks(TrackGroup::CaptionsAndSubtitles);
    TrackGroup descriptionTracks(TrackGroup::Description);
    TrackGroup chapterTracks(TrackGroup::Chapter);
    TrackGroup metadataTracks(TrackGroup::Metadata);
    TrackGroup otherTracks(TrackGroup::Other);

    if (!m_textTracks)
        return;

    for (size_t i = 0; i < m_textTracks->length(); ++i) {
        RefPtr<TextTrack> textTrack = m_textTracks->item(i);
        if (!textTrack)
            continue;

        auto kind = textTrack->kind();
        TrackGroup* currentGroup;
        if (kind == TextTrack::Kind::Subtitles || kind == TextTrack::Kind::Captions || kind == TextTrack::Kind::Forced)
            currentGroup = &captionAndSubtitleTracks;
        else if (kind == TextTrack::Kind::Descriptions)
            currentGroup = &descriptionTracks;
        else if (kind == TextTrack::Kind::Chapters)
            currentGroup = &chapterTracks;
        else if (kind == TextTrack::Kind::Metadata)
            currentGroup = &metadataTracks;
        else
            currentGroup = &otherTracks;

        if (!currentGroup->visibleTrack && textTrack->mode() == TextTrack::Mode::Showing)
            currentGroup->visibleTrack = textTrack;
        if (!currentGroup->defaultTrack && textTrack->isDefault())
            currentGroup->defaultTrack = textTrack;

        // Do not add this track to the group if it has already been automatically configured
        // as we only want to call configureTextTrack once per track so that adding another
        // track after the initial configuration doesn't reconfigure every track - only those
        // that should be changed by the new addition. For example all metadata tracks are
        // disabled by default, and we don't want a track that has been enabled by script
        // to be disabled automatically when a new metadata track is added later.
        if (textTrack->hasBeenConfigured())
            continue;

        if (textTrack->language().length())
            currentGroup->hasSrcLang = true;
        currentGroup->tracks.append(textTrack);
    }

    if (captionAndSubtitleTracks.tracks.size())
        configureTextTrackGroup(captionAndSubtitleTracks);
    if (descriptionTracks.tracks.size())
        configureTextTrackGroup(descriptionTracks);
    if (chapterTracks.tracks.size())
        configureTextTrackGroup(chapterTracks);
    if (metadataTracks.tracks.size())
        configureTextTrackGroup(metadataTracks);
    if (otherTracks.tracks.size())
        configureTextTrackGroup(otherTracks);

    updateCaptionContainer();
    configureTextTrackDisplay();
    if (hasMediaControls())
        mediaControls()->closedCaptionTracksChanged();
}
#endif

bool HTMLMediaElement::havePotentialSourceChild()
{
    // Stash the current <source> node and next nodes so we can restore them after checking
    // to see there is another potential.
    RefPtr<HTMLSourceElement> currentSourceNode = m_currentSourceNode;
    RefPtr<HTMLSourceElement> nextNode = m_nextChildNodeToConsider;

    URL nextURL = selectNextSourceChild(0, 0, DoNothing);

    m_currentSourceNode = currentSourceNode;
    m_nextChildNodeToConsider = nextNode;

    return nextURL.isValid();
}

URL HTMLMediaElement::selectNextSourceChild(ContentType* contentType, String* keySystem, InvalidURLAction actionIfInvalid)
{
    UNUSED_PARAM(keySystem);

    // Don't log if this was just called to find out if there are any valid <source> elements.
    bool shouldLog = willLog(WTFLogLevelDebug) && actionIfInvalid != DoNothing;
    if (shouldLog)
        INFO_LOG(LOGIDENTIFIER);

    if (!m_nextChildNodeToConsider) {
        if (shouldLog)
            INFO_LOG(LOGIDENTIFIER, "end of list, stopping");
        return URL();
    }

    // Because the DOM may be mutated in the course of the following algorithm,
    // keep strong references to each of the child source nodes, and verify that
    // each still is a child of this media element before using.
    Vector<Ref<HTMLSourceElement>> potentialSourceNodes;
    auto sources = childrenOfType<HTMLSourceElement>(*this);
    for (auto next = m_nextChildNodeToConsider ? sources.beginAt(*m_nextChildNodeToConsider) : sources.begin(), end = sources.end(); next != end; ++next)
        potentialSourceNodes.append(*next);

    for (auto& source : potentialSourceNodes) {
        if (source->parentNode() != this)
            continue;

        // If candidate does not have a src attribute, or if its src attribute's value is the empty string ... jump down to the failed step below
        auto mediaURL = source->getNonEmptyURLAttribute(srcAttr);
        String type;
        if (shouldLog)
            INFO_LOG(LOGIDENTIFIER, "'src' is ", mediaURL);
        if (mediaURL.isEmpty())
            goto CheckAgain;

        if (auto* media = source->parsedMediaAttribute(document())) {
            if (shouldLog)
                INFO_LOG(LOGIDENTIFIER, "'media' is ", source->attributeWithoutSynchronization(mediaAttr));
            auto* renderer = this->renderer();
            LOG(MediaQueries, "HTMLMediaElement %p selectNextSourceChild evaluating media queries", this);
            if (!MediaQueryEvaluator { "screen", document(), renderer ? &renderer->style() : nullptr }.evaluate(*media))
                goto CheckAgain;
        }

        type = source->attributeWithoutSynchronization(typeAttr);
        if (type.isEmpty() && mediaURL.protocolIsData())
            type = mimeTypeFromDataURL(mediaURL);
        if (!type.isEmpty()) {
            if (shouldLog)
                INFO_LOG(LOGIDENTIFIER, "'type' is ", type);
            MediaEngineSupportParameters parameters;
            parameters.type = ContentType(type);
            parameters.url = mediaURL;
#if ENABLE(MEDIA_SOURCE)
            parameters.isMediaSource = mediaURL.protocolIs(mediaSourceBlobProtocol);
#endif
#if ENABLE(MEDIA_STREAM)
            parameters.isMediaStream = mediaURL.protocolIs(mediaStreamBlobProtocol);
#endif
            if (!document().settings().allowMediaContentTypesRequiringHardwareSupportAsFallback() || Traversal<HTMLSourceElement>::nextSkippingChildren(source))
                parameters.contentTypesRequiringHardwareSupport = mediaContentTypesRequiringHardwareSupport();

            if (!MediaPlayer::supportsType(parameters))
                goto CheckAgain;
        }

        // Is it safe to load this url?
        if (!isSafeToLoadURL(mediaURL, actionIfInvalid) || !dispatchBeforeLoadEvent(mediaURL.string()))
            goto CheckAgain;

        // A 'beforeload' event handler can mutate the DOM, so check to see if the source element is still a child node.
        if (source->parentNode() != this) {
            INFO_LOG(LOGIDENTIFIER, "'beforeload' removed current element");
            continue;
        }

        // Making it this far means the <source> looks reasonable.
        if (contentType)
            *contentType = ContentType(type);
        m_nextChildNodeToConsider = Traversal<HTMLSourceElement>::nextSkippingChildren(source);
        m_currentSourceNode = WTFMove(source);

        if (shouldLog)
            INFO_LOG(LOGIDENTIFIER, " = ", mediaURL);

        return mediaURL;

CheckAgain:
        if (actionIfInvalid == Complain)
            source->scheduleErrorEvent();
    }

    m_currentSourceNode = nullptr;
    m_nextChildNodeToConsider = nullptr;

#if !LOG_DISABLED
    if (shouldLog)
        INFO_LOG(LOGIDENTIFIER, "failed");
#endif
    return URL();
}

void HTMLMediaElement::sourceWasAdded(HTMLSourceElement& source)
{
    if (willLog(WTFLogLevelInfo) && source.hasTagName(sourceTag)) {
        URL url = source.getNonEmptyURLAttribute(srcAttr);
        INFO_LOG(LOGIDENTIFIER, "'src' is ", url);
    }

    if (!document().hasBrowsingContext()) {
        INFO_LOG(LOGIDENTIFIER, "<source> inserted inside a document without a browsing context is not loaded");
        return;
    }

    // We should only consider a <source> element when there is not src attribute at all.
    if (hasAttributeWithoutSynchronization(srcAttr))
        return;

    // 4.8.8 - If a source element is inserted as a child of a media element that has no src
    // attribute and whose networkState has the value NETWORK_EMPTY, the user agent must invoke
    // the media element's resource selection algorithm.
    if (m_networkState == NETWORK_EMPTY) {
        m_nextChildNodeToConsider = &source;
#if PLATFORM(IOS_FAMILY)
        if (m_mediaSession->dataLoadingPermitted())
#endif
            selectMediaResource();
        return;
    }

    if (m_currentSourceNode && &source == Traversal<HTMLSourceElement>::nextSibling(*m_currentSourceNode)) {
        INFO_LOG(LOGIDENTIFIER, "<source> inserted immediately after current source");
        m_nextChildNodeToConsider = &source;
        return;
    }

    if (m_nextChildNodeToConsider)
        return;

    // 4.8.9.5, resource selection algorithm, source elements section:
    // 21. Wait until the node after pointer is a node other than the end of the list. (This step might wait forever.)
    // 22. Asynchronously await a stable state...
    // 23. Set the element's delaying-the-load-event flag back to true (this delays the load event again, in case
    // it hasn't been fired yet).
    setShouldDelayLoadEvent(true);

    // 24. Set the networkState back to NETWORK_LOADING.
    m_networkState = NETWORK_LOADING;

    // 25. Jump back to the find next candidate step above.
    m_nextChildNodeToConsider = &source;
    scheduleNextSourceChild();
}

void HTMLMediaElement::sourceWasRemoved(HTMLSourceElement& source)
{
    if (willLog(WTFLogLevelInfo) && source.hasTagName(sourceTag)) {
        URL url = source.getNonEmptyURLAttribute(srcAttr);
        INFO_LOG(LOGIDENTIFIER, "'src' is ", url);
    }

    if (&source != m_currentSourceNode && &source != m_nextChildNodeToConsider)
        return;

    if (&source == m_nextChildNodeToConsider) {
        m_nextChildNodeToConsider = m_currentSourceNode ? Traversal<HTMLSourceElement>::nextSibling(*m_currentSourceNode) : nullptr;
        INFO_LOG(LOGIDENTIFIER);
    } else if (&source == m_currentSourceNode) {
        // Clear the current source node pointer, but don't change the movie as the spec says:
        // 4.8.8 - Dynamically modifying a source element and its attribute when the element is already
        // inserted in a video or audio element will have no effect.
        m_currentSourceNode = nullptr;
        INFO_LOG(LOGIDENTIFIER, "m_currentSourceNode set to 0");
    }
}

void HTMLMediaElement::mediaPlayerTimeChanged(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

#if ENABLE(VIDEO_TRACK)
    updateActiveTextTrackCues(currentMediaTime());
#endif

    beginProcessingMediaPlayerCallback();

    invalidateCachedTime();
    bool wasSeeking = seeking();

    // 4.8.10.9 step 14 & 15.  Needed if no ReadyState change is associated with the seek.
    if (m_seekRequested && m_readyState >= HAVE_CURRENT_DATA && !m_player->seeking())
        finishSeek();

    // Always call scheduleTimeupdateEvent when the media engine reports a time discontinuity,
    // it will only queue a 'timeupdate' event if we haven't already posted one at the current
    // movie time.
    else
        scheduleTimeupdateEvent(false);

    MediaTime now = currentMediaTime();
    MediaTime dur = durationMediaTime();
    double playbackRate = requestedPlaybackRate();

    // When the current playback position reaches the end of the media resource then the user agent must follow these steps:
    if (dur && dur.isValid() && !dur.isPositiveInfinite() && !dur.isNegativeInfinite()) {
        // If the media element has a loop attribute specified and does not have a current media controller,
        if (loop() && !m_mediaController && playbackRate > 0) {
            m_sentEndEvent = false;
            // then seek to the earliest possible position of the media resource and abort these steps when the direction of
            // playback is forwards,
            if (now >= dur)
                seekInternal(MediaTime::zeroTime());
        } else if ((now <= MediaTime::zeroTime() && playbackRate < 0) || (now >= dur && playbackRate > 0)) {
            // If the media element does not have a current media controller, and the media element
            // has still ended playback and paused is false,
            if (!m_mediaController && !m_paused) {
                // changes paused to true and fires a simple event named pause at the media element.
                m_paused = true;
                scheduleEvent(eventNames().pauseEvent);
                m_mediaSession->clientWillPausePlayback();
            }
            // Queue a task to fire a simple event named ended at the media element.
            if (!m_sentEndEvent) {
                m_sentEndEvent = true;
                scheduleEvent(eventNames().endedEvent);
                if (!wasSeeking)
                    addBehaviorRestrictionsOnEndIfNecessary();
                setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);
            }
            setPlaying(false);
            // If the media element has a current media controller, then report the controller state
            // for the media element's current media controller.
            updateMediaController();
        } else
            m_sentEndEvent = false;
    } else {
#if ENABLE(MEDIA_STREAM)
        if (m_mediaStreamSrcObject) {
            // http://w3c.github.io/mediacapture-main/#event-mediastream-inactive
            // 6. MediaStreams in Media Elements
            // When the MediaStream state moves from the active to the inactive state, the User Agent
            // must raise an ended event on the HTMLMediaElement and set its ended attribute to true.
            // Note that once ended equals true the HTMLMediaElement will not play media even if new
            // MediaStreamTrack's are added to the MediaStream (causing it to return to the active
            // state) unless autoplay is true or the web application restarts the element, e.g.,
            // by calling play()
            if (!m_sentEndEvent && m_player && m_player->ended()) {
                m_sentEndEvent = true;
                scheduleEvent(eventNames().endedEvent);
                if (!wasSeeking)
                    addBehaviorRestrictionsOnEndIfNecessary();
                m_paused = true;
                setPlaying(false);
            }
        } else
#endif
        m_sentEndEvent = false;
    }

    scheduleUpdatePlayState();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::addBehaviorRestrictionsOnEndIfNecessary()
{
    if (isFullscreen())
        return;

    m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager);
    m_playbackControlsManagerBehaviorRestrictionsTimer.stop();
    m_playbackControlsManagerBehaviorRestrictionsTimer.startOneShot(hideMediaControlsAfterEndedDelay);
}

void HTMLMediaElement::handleSeekToPlaybackPosition(double position)
{
#if PLATFORM(MAC)
    // FIXME: This should ideally use faskSeek, but this causes MediaRemote's playhead to flicker upon release.
    // Please see <rdar://problem/28457219> for more details.
    seek(MediaTime::createWithDouble(position));
    m_seekToPlaybackPositionEndedTimer.stop();
    m_seekToPlaybackPositionEndedTimer.startOneShot(500_ms);

    if (!m_isScrubbingRemotely) {
        m_isScrubbingRemotely = true;
        if (!paused())
            pauseInternal();
    }
#else
    fastSeek(position);
#endif
}

void HTMLMediaElement::seekToPlaybackPositionEndedTimerFired()
{
#if PLATFORM(MAC)
    if (!m_isScrubbingRemotely)
        return;

    PlatformMediaSessionManager::sharedManager().sessionDidEndRemoteScrubbing(*m_mediaSession);
    m_isScrubbingRemotely = false;
    m_seekToPlaybackPositionEndedTimer.stop();
#endif
}

void HTMLMediaElement::mediaPlayerVolumeChanged(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

    beginProcessingMediaPlayerCallback();
    if (m_player) {
        double vol = m_player->volume();
        if (vol != m_volume) {
            m_volume = vol;
            updateVolume();
            scheduleEvent(eventNames().volumechangeEvent);
        }
    }
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerMuteChanged(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

    beginProcessingMediaPlayerCallback();
    if (m_player)
        setMuted(m_player->muted());
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerDurationChanged(MediaPlayer* player)
{
    INFO_LOG(LOGIDENTIFIER);

    beginProcessingMediaPlayerCallback();

    scheduleEvent(eventNames().durationchangeEvent);
    mediaPlayerCharacteristicChanged(player);

    MediaTime now = currentMediaTime();
    MediaTime dur = durationMediaTime();
    if (now > dur)
        seekInternal(dur);

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerRateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();

    // Stash the rate in case the one we tried to set isn't what the engine is
    // using (eg. it can't handle the rate we set)
    m_reportedPlaybackRate = m_player->rate();

    INFO_LOG(LOGIDENTIFIER, "rate: ", m_reportedPlaybackRate);

    if (m_playing)
        invalidateCachedTime();

    updateSleepDisabling();

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerPlaybackStateChanged(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

    if (!m_player || m_pausedInternal)
        return;

    beginProcessingMediaPlayerCallback();
    if (m_player->paused())
        pauseInternal();
    else
        playInternal();

    updateSleepDisabling();

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerSawUnsupportedTracks(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

    // The MediaPlayer came across content it cannot completely handle.
    // This is normally acceptable except when we are in a standalone
    // MediaDocument. If so, tell the document what has happened.
    if (is<MediaDocument>(document()))
        downcast<MediaDocument>(document()).mediaElementSawUnsupportedTracks();
}

void HTMLMediaElement::mediaPlayerResourceNotSupported(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

    // The MediaPlayer came across content which no installed engine supports.
    mediaLoadingFailed(MediaPlayer::FormatError);
}

// MediaPlayerPresentation methods
void HTMLMediaElement::mediaPlayerRepaint(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    updateDisplayState();
    if (auto* renderer = this->renderer())
        renderer->repaint();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerSizeChanged(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

    if (is<MediaDocument>(document()) && m_player)
        downcast<MediaDocument>(document()).mediaElementNaturalSizeChanged(expandedIntSize(m_player->naturalSize()));

    beginProcessingMediaPlayerCallback();
    if (m_readyState > HAVE_NOTHING)
        scheduleResizeEventIfSizeChanged();
    updateRenderer();
    endProcessingMediaPlayerCallback();
}

bool HTMLMediaElement::mediaPlayerRenderingCanBeAccelerated(MediaPlayer*)
{
    auto* renderer = this->renderer();
    return is<RenderVideo>(renderer)
        && downcast<RenderVideo>(*renderer).view().compositor().canAccelerateVideoRendering(downcast<RenderVideo>(*renderer));
}

void HTMLMediaElement::mediaPlayerRenderingModeChanged(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

    // Kick off a fake recalcStyle that will update the compositing tree.
    invalidateStyleAndLayerComposition();
}

bool HTMLMediaElement::mediaPlayerAcceleratedCompositingEnabled()
{
    return document().settings().acceleratedCompositingEnabled();
}

#if PLATFORM(WIN) && USE(AVFOUNDATION)

GraphicsDeviceAdapter* HTMLMediaElement::mediaPlayerGraphicsDeviceAdapter(const MediaPlayer*) const
{
    auto* page = document().page();
    if (!page)
        return nullptr;
    return page->chrome().client().graphicsDeviceAdapter();
}

#endif

void HTMLMediaElement::scheduleMediaEngineWasUpdated()
{
    if (m_mediaEngineUpdatedTask.hasPendingTask())
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier, "task scheduled");
    m_mediaEngineUpdatedTask.scheduleTask([this, logSiteIdentifier] {
        UNUSED_PARAM(logSiteIdentifier);
        ALWAYS_LOG(logSiteIdentifier, "- lambda(), task fired");
        Ref<HTMLMediaElement> protectedThis(*this); // mediaEngineWasUpdated calls methods that can trigger arbitrary DOM mutations.
        mediaEngineWasUpdated();
    });
}

void HTMLMediaElement::mediaEngineWasUpdated()
{
    INFO_LOG(LOGIDENTIFIER);
    beginProcessingMediaPlayerCallback();
    updateRenderer();
    endProcessingMediaPlayerCallback();

    m_mediaSession->mediaEngineUpdated();

#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode && audioSourceProvider()) {
        m_audioSourceNode->lock();
        audioSourceProvider()->setClient(m_audioSourceNode);
        m_audioSourceNode->unlock();
    }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    if (m_player && m_mediaKeys)
        m_player->cdmInstanceAttached(m_mediaKeys->cdmInstance());
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (!m_player)
        return;
    m_player->setVideoFullscreenFrame(m_videoFullscreenFrame);
    m_player->setVideoFullscreenGravity(m_videoFullscreenGravity);
    m_player->setVideoFullscreenLayer(m_videoFullscreenLayer.get());
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    scheduleUpdateMediaState();
#endif
}

void HTMLMediaElement::mediaPlayerEngineUpdated(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

#if ENABLE(MEDIA_SOURCE)
    m_droppedVideoFrames = 0;
#endif

    m_havePreparedToPlay = false;

    scheduleMediaEngineWasUpdated();
}

void HTMLMediaElement::mediaPlayerFirstVideoFrameAvailable(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER, "current display mode = ", (int)displayMode());

    beginProcessingMediaPlayerCallback();
    if (displayMode() == PosterWaitingForVideo) {
        setDisplayMode(Video);
        mediaPlayerRenderingModeChanged(m_player.get());
    }
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerCharacteristicChanged(MediaPlayer*)
{
    INFO_LOG(LOGIDENTIFIER);

    beginProcessingMediaPlayerCallback();

#if ENABLE(VIDEO_TRACK)
    if (captionDisplayMode() == CaptionUserPreferences::Automatic && m_subtitleTrackLanguage != m_player->languageOfPrimaryAudioTrack())
        markCaptionAndSubtitleTracksAsUnconfigured(AfterDelay);
#endif

    if (potentiallyPlaying() && displayMode() == PosterWaitingForVideo) {
        setDisplayMode(Video);
        mediaPlayerRenderingModeChanged(m_player.get());
    }

    if (hasMediaControls())
        mediaControls()->reset();
    updateRenderer();

    if (!paused() && !m_mediaSession->playbackPermitted()) {
        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    }

#if ENABLE(MEDIA_SESSION)
    document().updateIsPlayingMedia(m_elementID);
#else
    document().updateIsPlayingMedia();
#endif

    m_hasEverHadAudio |= hasAudio();
    m_hasEverHadVideo |= hasVideo();

    m_mediaSession->canProduceAudioChanged();

    updateSleepDisabling();

    endProcessingMediaPlayerCallback();
}

Ref<TimeRanges> HTMLMediaElement::buffered() const
{
    if (!m_player)
        return TimeRanges::create();

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource)
        return TimeRanges::create(*m_mediaSource->buffered());
#endif

    return TimeRanges::create(*m_player->buffered());
}

double HTMLMediaElement::maxBufferedTime() const
{
    auto bufferedRanges = buffered();
    unsigned numRanges = bufferedRanges->length();
    if (!numRanges)
        return 0;
    return bufferedRanges.get().ranges().end(numRanges - 1).toDouble();
}

Ref<TimeRanges> HTMLMediaElement::played()
{
    if (m_playing) {
        MediaTime time = currentMediaTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);
    }

    if (!m_playedTimeRanges)
        m_playedTimeRanges = TimeRanges::create();

    return m_playedTimeRanges->copy();
}

Ref<TimeRanges> HTMLMediaElement::seekable() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource)
        return m_mediaSource->seekable();
#endif

    if (m_player)
        return TimeRanges::create(*m_player->seekable());

    return TimeRanges::create();
}

double HTMLMediaElement::seekableTimeRangesLastModifiedTime() const
{
    return m_player ? m_player->seekableTimeRangesLastModifiedTime() : 0;
}

double HTMLMediaElement::liveUpdateInterval() const
{
    return m_player ? m_player->liveUpdateInterval() : 0;
}

bool HTMLMediaElement::potentiallyPlaying() const
{
    if (isBlockedOnMediaController())
        return false;

    if (!couldPlayIfEnoughData())
        return false;

    if (m_readyState >= HAVE_FUTURE_DATA)
        return true;

    return m_readyStateMaximum >= HAVE_FUTURE_DATA && m_readyState < HAVE_FUTURE_DATA;
}

bool HTMLMediaElement::couldPlayIfEnoughData() const
{
    if (paused())
        return false;

    if (endedPlayback())
        return false;

    if (stoppedDueToErrors())
        return false;

    if (pausedForUserInteraction())
        return false;

    return true;
}

bool HTMLMediaElement::endedPlayback() const
{
    MediaTime dur = durationMediaTime();
    if (!m_player || !dur.isValid())
        return false;

    // 4.8.10.8 Playing the media resource

    // A media element is said to have ended playback when the element's
    // readyState attribute is HAVE_METADATA or greater,
    if (m_readyState < HAVE_METADATA)
        return false;

    // and the current playback position is the end of the media resource and the direction
    // of playback is forwards, Either the media element does not have a loop attribute specified,
    // or the media element has a current media controller.
    MediaTime now = currentMediaTime();
    if (requestedPlaybackRate() > 0)
        return dur > MediaTime::zeroTime() && now >= dur && (!loop() || m_mediaController);

    // or the current playback position is the earliest possible position and the direction
    // of playback is backwards
    if (requestedPlaybackRate() < 0)
        return now <= MediaTime::zeroTime();

    return false;
}

bool HTMLMediaElement::stoppedDueToErrors() const
{
    if (m_readyState >= HAVE_METADATA && m_error) {
        RefPtr<TimeRanges> seekableRanges = seekable();
        if (!seekableRanges->contain(currentTime()))
            return true;
    }

    return false;
}

bool HTMLMediaElement::pausedForUserInteraction() const
{
    if (m_mediaSession->state() == PlatformMediaSession::Interrupted)
        return true;

    return false;
}

MediaTime HTMLMediaElement::minTimeSeekable() const
{
    return m_player ? m_player->minTimeSeekable() : MediaTime::zeroTime();
}

MediaTime HTMLMediaElement::maxTimeSeekable() const
{
    return m_player ? m_player->maxTimeSeekable() : MediaTime::zeroTime();
}

void HTMLMediaElement::updateVolume()
{
    if (!m_player)
        return;
#if PLATFORM(IOS_FAMILY)
    // Only the user can change audio volume so update the cached volume and post the changed event.
    float volume = m_player->volume();
    if (m_volume != volume) {
        m_volume = volume;
        scheduleEvent(eventNames().volumechangeEvent);
    }
#else
    // Avoid recursion when the player reports volume changes.
    if (!processingMediaPlayerCallback()) {
        Page* page = document().page();
        double volumeMultiplier = page ? page->mediaVolume() : 1;
        bool shouldMute = effectiveMuted();

        if (m_mediaController) {
            volumeMultiplier *= m_mediaController->volume();
            shouldMute = m_mediaController->muted() || (page && page->isAudioMuted());
        }

#if ENABLE(MEDIA_SESSION)
        if (m_shouldDuck)
            volumeMultiplier *= 0.25;
#endif

        m_player->setMuted(shouldMute);
        m_player->setVolume(m_volume * volumeMultiplier);
    }

#if ENABLE(MEDIA_SESSION)
    document().updateIsPlayingMedia(m_elementID);
#else
    document().updateIsPlayingMedia();
#endif

    if (hasMediaControls())
        mediaControls()->changedVolume();
#endif
}

void HTMLMediaElement::scheduleUpdatePlayState()
{
    if (m_updatePlayStateTask.hasPendingTask())
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier, "task scheduled");
    m_updatePlayStateTask.scheduleTask([this, logSiteIdentifier] {
        UNUSED_PARAM(logSiteIdentifier);
        ALWAYS_LOG(logSiteIdentifier, "- lambda(), task fired");
        Ref<HTMLMediaElement> protectedThis(*this); // updatePlayState calls methods that can trigger arbitrary DOM mutations.
        updatePlayState();
    });
}

void HTMLMediaElement::updatePlayState()
{
    if (!m_player)
        return;

    if (m_pausedInternal) {
        if (!m_player->paused())
            m_player->pause();
        refreshCachedTime();
        m_playbackProgressTimer.stop();
        if (hasMediaControls())
            mediaControls()->playbackStopped();
        return;
    }

    bool shouldBePlaying = potentiallyPlaying();
    bool playerPaused = m_player->paused();

    INFO_LOG(LOGIDENTIFIER, "shouldBePlaying = ", shouldBePlaying, ", playerPaused = ", playerPaused);

    if (shouldBePlaying && playerPaused && m_mediaSession->requiresFullscreenForVideoPlayback() && (m_waitingToEnterFullscreen || !isFullscreen())) {
        if (!m_waitingToEnterFullscreen)
            enterFullscreen();

#if PLATFORM(WATCHOS)
        // FIXME: Investigate doing this for all builds.
        return;
#endif
    }

    if (shouldBePlaying) {
        schedulePlaybackControlsManagerUpdate();

        setDisplayMode(Video);
        invalidateCachedTime();

        if (playerPaused) {
            m_mediaSession->clientWillBeginPlayback();

            // Set rate, muted before calling play in case they were set before the media engine was setup.
            // The media engine should just stash the rate and muted values since it isn't already playing.
            m_player->setRate(requestedPlaybackRate());
            m_player->setMuted(effectiveMuted());

            if (m_firstTimePlaying) {
                // Log that a media element was played.
                if (auto* page = document().page())
                    page->diagnosticLoggingClient().logDiagnosticMessage(isVideo() ? DiagnosticLoggingKeys::videoKey() : DiagnosticLoggingKeys::audioKey(), DiagnosticLoggingKeys::playedKey(), ShouldSample::No);
                m_firstTimePlaying = false;
            }

            m_player->play();
        }

        if (hasMediaControls())
            mediaControls()->playbackStarted();

        startPlaybackProgressTimer();
        setPlaying(true);
    } else {
        schedulePlaybackControlsManagerUpdate();

        if (!playerPaused)
            m_player->pause();
        refreshCachedTime();

        m_playbackProgressTimer.stop();
        setPlaying(false);
        MediaTime time = currentMediaTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);

        if (couldPlayIfEnoughData())
            prepareToPlay();

        if (hasMediaControls())
            mediaControls()->playbackStopped();
    }

    updateMediaController();
    updateRenderer();

    m_hasEverHadAudio |= hasAudio();
    m_hasEverHadVideo |= hasVideo();
}

void HTMLMediaElement::setPlaying(bool playing)
{
    if (playing && m_mediaSession)
        m_mediaSession->removeBehaviorRestriction(MediaElementSession::RequirePlaybackToControlControlsManager);

    if (m_playing == playing)
        return;

    m_playing = playing;

    if (m_playing)
        scheduleNotifyAboutPlaying();

#if ENABLE(MEDIA_SESSION)
    document().updateIsPlayingMedia(m_elementID);
#else
    document().updateIsPlayingMedia();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    scheduleUpdateMediaState();
#endif
}

void HTMLMediaElement::setPausedInternal(bool b)
{
    m_pausedInternal = b;
    scheduleUpdatePlayState();
}

void HTMLMediaElement::stopPeriodicTimers()
{
    m_progressEventTimer.stop();
    m_playbackProgressTimer.stop();
}

void HTMLMediaElement::cancelPendingTasks()
{
    m_configureTextTracksTask.cancelTask();
    m_checkPlaybackTargetCompatablityTask.cancelTask();
    m_updateMediaStateTask.cancelTask();
    m_mediaEngineUpdatedTask.cancelTask();
    m_updatePlayStateTask.cancelTask();
}

void HTMLMediaElement::userCancelledLoad()
{
    INFO_LOG(LOGIDENTIFIER);

    // FIXME: We should look to reconcile the iOS and non-iOS code (below).
#if PLATFORM(IOS_FAMILY)
    if (m_networkState == NETWORK_EMPTY || m_readyState >= HAVE_METADATA)
        return;
#else
    if (m_networkState == NETWORK_EMPTY || m_completelyLoaded)
        return;
#endif

    // If the media data fetching process is aborted by the user:

    // 1 - The user agent should cancel the fetching process.
    clearMediaPlayer();

    // 2 - Set the error attribute to a new MediaError object whose code attribute is set to MEDIA_ERR_ABORTED.
    m_error = MediaError::create(MediaError::MEDIA_ERR_ABORTED);

    // 3 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(eventNames().abortEvent);

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    // 4 - If the media element's readyState attribute has a value equal to HAVE_NOTHING, set the
    // element's networkState attribute to the NETWORK_EMPTY value and queue a task to fire a
    // simple event named emptied at the element. Otherwise, set the element's networkState
    // attribute to the NETWORK_IDLE value.
    if (m_readyState == HAVE_NOTHING) {
        m_networkState = NETWORK_EMPTY;
        scheduleEvent(eventNames().emptiedEvent);
    }
    else
        m_networkState = NETWORK_IDLE;

    // 5 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 6 - Abort the overall resource selection algorithm.
    m_currentSourceNode = nullptr;

    // Reset m_readyState since m_player is gone.
    m_readyState = HAVE_NOTHING;
    updateMediaController();

#if ENABLE(VIDEO_TRACK)
    auto* context = scriptExecutionContext();
    if (!context || context->activeDOMObjectsAreStopped())
        return; // Document is about to be destructed. Avoid updating layout in updateActiveTextTrackCues.

    updateActiveTextTrackCues(MediaTime::zeroTime());
#endif
}

void HTMLMediaElement::clearMediaPlayer()
{
#if ENABLE(MEDIA_STREAM)
    if (!m_settingMediaStreamSrcObject)
        m_mediaStreamSrcObject = nullptr;
#endif

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    m_blob = nullptr;

#if ENABLE(VIDEO_TRACK)
    forgetResourceSpecificTracks();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (hasEventListeners(eventNames().webkitplaybacktargetavailabilitychangedEvent)) {
        m_hasPlaybackTargetAvailabilityListeners = false;
        m_mediaSession->setHasPlaybackTargetAvailabilityListeners(false);

        // Send an availability event in case scripts want to hide the picker when the element
        // doesn't support playback to a target.
        enqueuePlaybackTargetAvailabilityChangedEvent();
    }

    if (m_isPlayingToWirelessTarget) {
        m_isPlayingToWirelessTarget = false;
        scheduleEvent(eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent);
    }
#endif

    if (m_isWaitingUntilMediaCanStart) {
        m_isWaitingUntilMediaCanStart = false;
        document().removeMediaCanStartListener(this);
    }

    if (m_player) {
        m_player->invalidate();
        m_player = nullptr;
    }
    schedulePlaybackControlsManagerUpdate();

    stopPeriodicTimers();
    cancelPendingTasks();

    m_loadState = WaitingForSource;

#if ENABLE(VIDEO_TRACK)
    if (m_textTracks)
        configureTextTrackDisplay();
#endif

    m_mediaSession->clientCharacteristicsChanged();
    m_mediaSession->canProduceAudioChanged();

    m_resourceSelectionTaskQueue.cancelAllTasks();

    updateSleepDisabling();
}

bool HTMLMediaElement::canSuspendForDocumentSuspension() const
{
    return true;
}

const char* HTMLMediaElement::activeDOMObjectName() const
{
    return "HTMLMediaElement";
}

void HTMLMediaElement::stopWithoutDestroyingMediaPlayer()
{
    INFO_LOG(LOGIDENTIFIER);

    if (m_videoFullscreenMode != VideoFullscreenModeNone)
        exitFullscreen();

    setPreparedToReturnVideoLayerToInline(true);

    schedulePlaybackControlsManagerUpdate();
    setInActiveDocument(false);

    // Stop the playback without generating events
    setPlaying(false);
    setPausedInternal(true);
    m_mediaSession->stopSession();

    setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);

    userCancelledLoad();

    updateRenderer();

    stopPeriodicTimers();

    updateSleepDisabling();
}

void HTMLMediaElement::closeTaskQueues()
{
    m_configureTextTracksTask.close();
    m_checkPlaybackTargetCompatablityTask.close();
    m_updateMediaStateTask.close();
    m_mediaEngineUpdatedTask.close();
    m_updatePlayStateTask.close();
    m_resumeTaskQueue.close();
    m_seekTaskQueue.close();
    m_playbackControlsManagerBehaviorRestrictionsQueue.close();
    m_seekTaskQueue.close();
    m_resumeTaskQueue.close();
    m_promiseTaskQueue.close();
    m_pauseAfterDetachedTaskQueue.close();
    m_resourceSelectionTaskQueue.close();
    m_visibilityChangeTaskQueue.close();
#if ENABLE(ENCRYPTED_MEDIA)
    m_encryptedMediaQueue.close();
#endif
    m_asyncEventQueue.close();
}

void HTMLMediaElement::contextDestroyed()
{
    closeTaskQueues();
    m_pendingPlayPromises.clear();

    ActiveDOMObject::contextDestroyed();
}

void HTMLMediaElement::stop()
{
    INFO_LOG(LOGIDENTIFIER);

    Ref<HTMLMediaElement> protectedThis(*this);
    stopWithoutDestroyingMediaPlayer();
    closeTaskQueues();

    // Once an active DOM object has been stopped it can not be restarted, so we can deallocate
    // the media player now. Note that userCancelledLoad will already called clearMediaPlayer
    // if the media was not fully loaded, but we need the same cleanup if the file was completely
    // loaded and calling it again won't cause any problems.
    clearMediaPlayer();

    m_mediaSession->stopSession();
}

void HTMLMediaElement::suspend(ReasonForSuspension reason)
{
    INFO_LOG(LOGIDENTIFIER);
    Ref<HTMLMediaElement> protectedThis(*this);

    m_resumeTaskQueue.cancelTask();

    switch (reason) {
    case ReasonForSuspension::PageCache:
        stopWithoutDestroyingMediaPlayer();
        m_asyncEventQueue.suspend();
        setShouldBufferData(false);
        m_mediaSession->addBehaviorRestriction(MediaElementSession::RequirePageConsentToResumeMedia);
        break;
    case ReasonForSuspension::PageWillBeSuspended:
    case ReasonForSuspension::JavaScriptDebuggerPaused:
    case ReasonForSuspension::WillDeferLoading:
        // Do nothing, we don't pause media playback in these cases.
        break;
    }
}

void HTMLMediaElement::resume()
{
    INFO_LOG(LOGIDENTIFIER);

    setInActiveDocument(true);

    m_asyncEventQueue.resume();

    setShouldBufferData(true);

    if (!m_mediaSession->pageAllowsPlaybackAfterResuming())
        document().addMediaCanStartListener(this);
    else
        setPausedInternal(false);

    m_mediaSession->removeBehaviorRestriction(MediaElementSession::RequirePageConsentToResumeMedia);

    if (m_error && m_error->code() == MediaError::MEDIA_ERR_ABORTED && !m_resumeTaskQueue.hasPendingTask()) {
        // Restart the load if it was aborted in the middle by moving the document to the page cache.
        // m_error is only left at MEDIA_ERR_ABORTED when the document becomes inactive (it is set to
        //  MEDIA_ERR_ABORTED while the abortEvent is being sent, but cleared immediately afterwards).
        // This behavior is not specified but it seems like a sensible thing to do.
        // As it is not safe to immedately start loading now, let's schedule a load.
        m_resumeTaskQueue.scheduleTask(std::bind(&HTMLMediaElement::prepareForLoad, this));
    }

    updateRenderer();
}

bool HTMLMediaElement::hasPendingActivity() const
{
    return (hasAudio() && isPlaying()) || m_asyncEventQueue.hasPendingEvents() || m_creatingControls;
}

void HTMLMediaElement::mediaVolumeDidChange()
{
    INFO_LOG(LOGIDENTIFIER);
    updateVolume();
}

void HTMLMediaElement::visibilityStateChanged()
{
    bool elementIsHidden = document().hidden() && m_videoFullscreenMode != VideoFullscreenModePictureInPicture;
    if (elementIsHidden == m_elementIsHidden)
        return;

    m_elementIsHidden = elementIsHidden;
    INFO_LOG(LOGIDENTIFIER, "visible = ", !m_elementIsHidden);

    updateSleepDisabling();
    m_mediaSession->visibilityChanged();
    if (m_player)
        m_player->setVisible(!m_elementIsHidden);

    bool isPlayingAudio = isPlaying() && hasAudio() && !muted() && volume();
    if (!isPlayingAudio) {
        if (m_elementIsHidden) {
            ALWAYS_LOG(LOGIDENTIFIER, "Suspending playback after going to the background");
            m_mediaSession->beginInterruption(PlatformMediaSession::EnteringBackground);
        } else {
            ALWAYS_LOG(LOGIDENTIFIER, "Resuming playback after entering foreground");
            m_mediaSession->endInterruption(PlatformMediaSession::MayResumePlaying);
        }
    }
}

#if ENABLE(VIDEO_TRACK)
bool HTMLMediaElement::requiresTextTrackRepresentation() const
{
    return (m_videoFullscreenMode != VideoFullscreenModeNone) && m_player ? m_player->requiresTextTrackRepresentation() : false;
}

void HTMLMediaElement::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    if (m_player)
        m_player->setTextTrackRepresentation(representation);
}

void HTMLMediaElement::syncTextTrackBounds()
{
    if (m_player)
        m_player->syncTextTrackBounds();
}
#endif // ENABLE(VIDEO_TRACK)

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void HTMLMediaElement::webkitShowPlaybackTargetPicker()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (processingUserGestureForMedia())
        removeBehaviorsRestrictionsAfterFirstUserGesture();
    m_mediaSession->showPlaybackTargetPicker();
}

void HTMLMediaElement::wirelessRoutesAvailableDidChange()
{
    enqueuePlaybackTargetAvailabilityChangedEvent();
}

void HTMLMediaElement::mediaPlayerCurrentPlaybackTargetIsWirelessChanged(MediaPlayer*)
{
    m_isPlayingToWirelessTarget = m_player && m_player->isCurrentPlaybackTargetWireless();

    ALWAYS_LOG(LOGIDENTIFIER, m_isPlayingToWirelessTarget);
    ASSERT(m_player);
    configureMediaControls();
    scheduleEvent(eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent);
    m_mediaSession->isPlayingToWirelessPlaybackTargetChanged(m_isPlayingToWirelessTarget);
    m_mediaSession->canProduceAudioChanged();
    scheduleUpdateMediaState();
    updateSleepDisabling();
}

void HTMLMediaElement::dispatchEvent(Event& event)
{
    if (event.type() == eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent) {
        m_failedToPlayToWirelessTarget = false;
        scheduleCheckPlaybackTargetCompatability();
    }

    DEBUG_LOG(LOGIDENTIFIER, "dispatching '", event.type(), "'");

    HTMLElement::dispatchEvent(event);
}

bool HTMLMediaElement::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (eventType != eventNames().webkitplaybacktargetavailabilitychangedEvent)
        return Node::addEventListener(eventType, WTFMove(listener), options);

    bool isFirstAvailabilityChangedListener = !hasEventListeners(eventNames().webkitplaybacktargetavailabilitychangedEvent);
    if (!Node::addEventListener(eventType, WTFMove(listener), options))
        return false;

    if (isFirstAvailabilityChangedListener) {
        m_hasPlaybackTargetAvailabilityListeners = true;
        m_mediaSession->setHasPlaybackTargetAvailabilityListeners(true);
    }

    INFO_LOG(LOGIDENTIFIER, "'webkitplaybacktargetavailabilitychanged'");

    enqueuePlaybackTargetAvailabilityChangedEvent(); // Ensure the event listener gets at least one event.
    return true;
}

bool HTMLMediaElement::removeEventListener(const AtomicString& eventType, EventListener& listener, const ListenerOptions& options)
{
    if (eventType != eventNames().webkitplaybacktargetavailabilitychangedEvent)
        return Node::removeEventListener(eventType, listener, options);

    if (!Node::removeEventListener(eventType, listener, options))
        return false;

    bool didRemoveLastAvailabilityChangedListener = !hasEventListeners(eventNames().webkitplaybacktargetavailabilitychangedEvent);
    INFO_LOG(LOGIDENTIFIER, "removed last listener = ", didRemoveLastAvailabilityChangedListener);
    if (didRemoveLastAvailabilityChangedListener) {
        m_hasPlaybackTargetAvailabilityListeners = false;
        m_mediaSession->setHasPlaybackTargetAvailabilityListeners(false);
        scheduleUpdateMediaState();
    }

    return true;
}

void HTMLMediaElement::enqueuePlaybackTargetAvailabilityChangedEvent()
{
    bool hasTargets = m_mediaSession->hasWirelessPlaybackTargets();
    INFO_LOG(LOGIDENTIFIER, "hasTargets = ", hasTargets);
    auto event = WebKitPlaybackTargetAvailabilityEvent::create(eventNames().webkitplaybacktargetavailabilitychangedEvent, hasTargets);
    event->setTarget(this);
    m_asyncEventQueue.enqueueEvent(WTFMove(event));
    scheduleUpdateMediaState();
}

void HTMLMediaElement::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& device)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_player)
        m_player->setWirelessPlaybackTarget(WTFMove(device));
}

void HTMLMediaElement::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    ALWAYS_LOG(LOGIDENTIFIER, shouldPlay);

    if (m_player)
        m_player->setShouldPlayToPlaybackTarget(shouldPlay);
}

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

bool HTMLMediaElement::webkitCurrentPlaybackTargetIsWireless() const
{
    INFO_LOG(LOGIDENTIFIER, m_isPlayingToWirelessTarget);
    return m_isPlayingToWirelessTarget;
}

void HTMLMediaElement::setPlayingOnSecondScreen(bool value)
{
    if (value == m_playingOnSecondScreen)
        return;

    m_playingOnSecondScreen = value;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    scheduleUpdateMediaState();
#endif
}

double HTMLMediaElement::minFastReverseRate() const
{
    return m_player ? m_player->minFastReverseRate() : 0;
}

double HTMLMediaElement::maxFastForwardRate() const
{
    return m_player ? m_player->maxFastForwardRate() : 0;
}

bool HTMLMediaElement::isFullscreen() const
{
    if (m_videoFullscreenMode != VideoFullscreenModeNone)
        return true;

#if ENABLE(FULLSCREEN_API)
    if (document().webkitIsFullScreen() && document().webkitCurrentFullScreenElement() == this)
        return true;
#endif

    return false;
}

bool HTMLMediaElement::isStandardFullscreen() const
{
#if ENABLE(FULLSCREEN_API)
    if (document().webkitIsFullScreen() && document().webkitCurrentFullScreenElement() == this)
        return true;
#endif

    return m_videoFullscreenMode == VideoFullscreenModeStandard;
}

void HTMLMediaElement::toggleStandardFullscreenState()
{
    if (isStandardFullscreen())
        exitFullscreen();
    else
        enterFullscreen();
}

void HTMLMediaElement::enterFullscreen(VideoFullscreenMode mode)
{
    INFO_LOG(LOGIDENTIFIER);
    ASSERT(mode != VideoFullscreenModeNone);

    if (m_videoFullscreenMode == mode)
        return;

    m_temporarilyAllowingInlinePlaybackAfterFullscreen = false;
    m_waitingToEnterFullscreen = true;

#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    if (document().settings().fullScreenEnabled() && mode == VideoFullscreenModeStandard) {
        document().requestFullScreenForElement(this, Document::ExemptIFrameAllowFullScreenRequirement);
        return;
    }
#endif

    m_fullscreenTaskQueue.enqueueTask([this, mode] {
        if (document().hidden()) {
            ALWAYS_LOG(LOGIDENTIFIER, "  returning because document is hidden");
            return;
        }

        fullscreenModeChanged(mode);
        configureMediaControls();
        if (hasMediaControls())
            mediaControls()->enteredFullscreen();
        if (is<HTMLVideoElement>(*this)) {
            HTMLVideoElement& asVideo = downcast<HTMLVideoElement>(*this);
            if (document().page()->chrome().client().supportsVideoFullscreen(m_videoFullscreenMode)) {
                document().page()->chrome().client().enterVideoFullscreenForVideoElement(asVideo, m_videoFullscreenMode, m_videoFullscreenStandby);
                scheduleEvent(eventNames().webkitbeginfullscreenEvent);
            }
        }
    });
}

void HTMLMediaElement::enterFullscreen()
{
    enterFullscreen(VideoFullscreenModeStandard);
}

void HTMLMediaElement::exitFullscreen()
{
    INFO_LOG(LOGIDENTIFIER);

    m_waitingToEnterFullscreen = false;

#if ENABLE(FULLSCREEN_API)
    if (document().settings().fullScreenEnabled() && document().webkitCurrentFullScreenElement() == this) {
        if (document().webkitIsFullScreen())
            document().webkitCancelFullScreen();

        if (m_videoFullscreenMode == VideoFullscreenModeStandard)
            return;
    }
#endif

    ASSERT(m_videoFullscreenMode != VideoFullscreenModeNone);
    VideoFullscreenMode oldVideoFullscreenMode = m_videoFullscreenMode;
    fullscreenModeChanged(VideoFullscreenModeNone);
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    Ref<HTMLMediaElement> protectedThis(*this); // updateMediaControlsAfterPresentationModeChange calls methods that can trigger arbitrary DOM mutations.
    updateMediaControlsAfterPresentationModeChange();
#endif
    if (hasMediaControls())
        mediaControls()->exitedFullscreen();

    if (!document().page() || !is<HTMLVideoElement>(*this))
        return;

    if (!paused() && m_mediaSession->requiresFullscreenForVideoPlayback()) {
        if (!document().settings().allowsInlineMediaPlaybackAfterFullscreen() || isVideoTooSmallForInlinePlayback())
            pauseInternal();
        else {
            // Allow inline playback, but set a flag so pausing and starting again (e.g. when scrubbing or looping) won't go back to fullscreen.
            // Also set the controls attribute so the user will be able to control playback.
            m_temporarilyAllowingInlinePlaybackAfterFullscreen = true;
            setControls(true);
        }
    }

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    if (document().activeDOMObjectsAreSuspended() || document().activeDOMObjectsAreStopped())
        document().page()->chrome().client().exitVideoFullscreenToModeWithoutAnimation(downcast<HTMLVideoElement>(*this), VideoFullscreenModeNone);
    else
#endif
    if (document().page()->chrome().client().supportsVideoFullscreen(oldVideoFullscreenMode)) {
        if (m_videoFullscreenStandby)
            document().page()->chrome().client().enterVideoFullscreenForVideoElement(downcast<HTMLVideoElement>(*this), m_videoFullscreenMode, m_videoFullscreenStandby);
        else
            document().page()->chrome().client().exitVideoFullscreenForVideoElement(downcast<HTMLVideoElement>(*this));
        scheduleEvent(eventNames().webkitendfullscreenEvent);
        scheduleEvent(eventNames().webkitpresentationmodechangedEvent);
    }
}

WEBCORE_EXPORT void HTMLMediaElement::setVideoFullscreenStandby(bool value)
{
    ASSERT(is<HTMLVideoElement>(*this));
    if (m_videoFullscreenStandby == value)
        return;

    if (!document().page())
        return;

    if (!document().page()->chrome().client().supportsVideoFullscreenStandby())
        return;

    m_videoFullscreenStandby = value;
    
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (m_player)
        m_player->videoFullscreenStandbyChanged();
#endif

    if (m_videoFullscreenStandby || m_videoFullscreenMode != VideoFullscreenModeNone)
        document().page()->chrome().client().enterVideoFullscreenForVideoElement(downcast<HTMLVideoElement>(*this), m_videoFullscreenMode, m_videoFullscreenStandby);
    else
        document().page()->chrome().client().exitVideoFullscreenForVideoElement(downcast<HTMLVideoElement>(*this));
}

void HTMLMediaElement::willBecomeFullscreenElement()
{
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    HTMLMediaElementEnums::VideoFullscreenMode oldVideoFullscreenMode = m_videoFullscreenMode;
#endif

    fullscreenModeChanged(VideoFullscreenModeStandard);

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    switch (oldVideoFullscreenMode) {
    case VideoFullscreenModeNone:
    case VideoFullscreenModeStandard:
        // Don't need to do anything if we are not in any special fullscreen mode or it's already
        // in standard fullscreen mode.
        break;
    case VideoFullscreenModePictureInPicture:
        if (is<HTMLVideoElement>(*this))
            downcast<HTMLVideoElement>(this)->exitToFullscreenModeWithoutAnimationIfPossible(oldVideoFullscreenMode, VideoFullscreenModeStandard);
        break;
    }
#endif

    Element::willBecomeFullscreenElement();
}

void HTMLMediaElement::didBecomeFullscreenElement()
{
    m_waitingToEnterFullscreen = false;
    if (hasMediaControls())
        mediaControls()->enteredFullscreen();
    scheduleUpdatePlayState();
}

void HTMLMediaElement::willStopBeingFullscreenElement()
{
    if (hasMediaControls())
        mediaControls()->exitedFullscreen();

    if (fullscreenMode() == VideoFullscreenModeStandard)
        fullscreenModeChanged(VideoFullscreenModeNone);
}

PlatformLayer* HTMLMediaElement::platformLayer() const
{
    return m_player ? m_player->platformLayer() : nullptr;
}

void HTMLMediaElement::setPreparedToReturnVideoLayerToInline(bool value)
{
    m_preparedForInline = value;
    if (m_preparedForInline && m_preparedForInlineCompletionHandler) {
        m_preparedForInlineCompletionHandler();
        m_preparedForInlineCompletionHandler = nullptr;
    }
}

void HTMLMediaElement::waitForPreparedForInlineThen(WTF::Function<void()>&& completionHandler)
{
    ASSERT(!m_preparedForInlineCompletionHandler);
    if (m_preparedForInline)  {
        completionHandler();
        return;
    }

    m_preparedForInlineCompletionHandler = WTFMove(completionHandler);
}

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

void HTMLMediaElement::willExitFullscreen()
{
    if (m_player)
        m_player->updateVideoFullscreenInlineImage();
}

bool HTMLMediaElement::isVideoLayerInline()
{
    return !m_videoFullscreenLayer;
};

void HTMLMediaElement::setVideoFullscreenLayer(PlatformLayer* platformLayer, WTF::Function<void()>&& completionHandler)
{
    m_videoFullscreenLayer = platformLayer;
    if (!m_player) {
        completionHandler();
        return;
    }

    m_player->setVideoFullscreenLayer(platformLayer, WTFMove(completionHandler));
    invalidateStyleAndLayerComposition();
#if ENABLE(VIDEO_TRACK)
    updateTextTrackDisplay();
#endif
}

void HTMLMediaElement::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoFullscreenFrame = frame;
    if (m_player)
        m_player->setVideoFullscreenFrame(frame);
}

void HTMLMediaElement::setVideoFullscreenGravity(MediaPlayer::VideoGravity gravity)
{
    m_videoFullscreenGravity = gravity;
    if (m_player)
        m_player->setVideoFullscreenGravity(gravity);
}

#else

bool HTMLMediaElement::isVideoLayerInline()
{
    return true;
};

#endif

bool HTMLMediaElement::hasClosedCaptions() const
{
    if (m_player && m_player->hasClosedCaptions())
        return true;

#if ENABLE(VIDEO_TRACK)
    if (!m_textTracks)
        return false;

    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
        auto& track = *m_textTracks->item(i);
        if (track.readinessState() == TextTrack::FailedToLoad)
            continue;
        if (track.kind() == TextTrack::Kind::Captions || track.kind() == TextTrack::Kind::Subtitles)
            return true;
    }
#endif

    return false;
}

bool HTMLMediaElement::closedCaptionsVisible() const
{
    return m_closedCaptionsVisible;
}

#if ENABLE(VIDEO_TRACK)

void HTMLMediaElement::updateTextTrackDisplay()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    ensureMediaControlsShadowRoot();
    if (!m_mediaControlsHost)
        m_mediaControlsHost = MediaControlsHost::create(this);
    m_mediaControlsHost->updateTextTrackContainer();
#else
    if (!hasMediaControls() && !createMediaControls())
        return;

    mediaControls()->updateTextTrackDisplay();
#endif
}

#endif

void HTMLMediaElement::setClosedCaptionsVisible(bool closedCaptionVisible)
{
    INFO_LOG(LOGIDENTIFIER, closedCaptionVisible);

    m_closedCaptionsVisible = false;

    if (!m_player || !hasClosedCaptions())
        return;

    m_closedCaptionsVisible = closedCaptionVisible;
    m_player->setClosedCaptionsVisible(closedCaptionVisible);

#if ENABLE(VIDEO_TRACK)
    markCaptionAndSubtitleTracksAsUnconfigured(Immediately);
    updateTextTrackDisplay();
#else
    if (hasMediaControls())
        mediaControls()->changedClosedCaptionsVisibility();
#endif
}

void HTMLMediaElement::setWebkitClosedCaptionsVisible(bool visible)
{
    m_webkitLegacyClosedCaptionOverride = visible;
    setClosedCaptionsVisible(visible);
}

bool HTMLMediaElement::webkitClosedCaptionsVisible() const
{
    return m_webkitLegacyClosedCaptionOverride && m_closedCaptionsVisible;
}


bool HTMLMediaElement::webkitHasClosedCaptions() const
{
    return hasClosedCaptions();
}

#if ENABLE(MEDIA_STATISTICS)
unsigned HTMLMediaElement::webkitAudioDecodedByteCount() const
{
    if (!m_player)
        return 0;
    return m_player->audioDecodedByteCount();
}

unsigned HTMLMediaElement::webkitVideoDecodedByteCount() const
{
    if (!m_player)
        return 0;
    return m_player->videoDecodedByteCount();
}
#endif

void HTMLMediaElement::mediaCanStart(Document& document)
{
    ASSERT_UNUSED(document, &document == &this->document());
    INFO_LOG(LOGIDENTIFIER, "m_isWaitingUntilMediaCanStart = ", m_isWaitingUntilMediaCanStart, ", m_pausedInternal = ", m_pausedInternal);

    ASSERT(m_isWaitingUntilMediaCanStart || m_pausedInternal);
    if (m_isWaitingUntilMediaCanStart) {
        m_isWaitingUntilMediaCanStart = false;
        selectMediaResource();
    }
    if (m_pausedInternal)
        setPausedInternal(false);
}

bool HTMLMediaElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLMediaElement::setShouldDelayLoadEvent(bool shouldDelay)
{
    if (m_shouldDelayLoadEvent == shouldDelay)
        return;

    INFO_LOG(LOGIDENTIFIER, shouldDelay);

    m_shouldDelayLoadEvent = shouldDelay;
    if (shouldDelay)
        document().incrementLoadEventDelayCount();
    else
        document().decrementLoadEventDelayCount();
}

static String& sharedMediaCacheDirectory()
{
    static NeverDestroyed<String> sharedMediaCacheDirectory;
    return sharedMediaCacheDirectory;
}

void HTMLMediaElement::setMediaCacheDirectory(const String& path)
{
    sharedMediaCacheDirectory() = path;
}

const String& HTMLMediaElement::mediaCacheDirectory()
{
    return sharedMediaCacheDirectory();
}

HashSet<RefPtr<SecurityOrigin>> HTMLMediaElement::originsInMediaCache(const String& path)
{
    return MediaPlayer::originsInMediaCache(path);
}

void HTMLMediaElement::clearMediaCache(const String& path, WallTime modifiedSince)
{
    MediaPlayer::clearMediaCache(path, modifiedSince);
}

void HTMLMediaElement::clearMediaCacheForOrigins(const String& path, const HashSet<RefPtr<SecurityOrigin>>& origins)
{
    MediaPlayer::clearMediaCacheForOrigins(path, origins);
}

void HTMLMediaElement::resetMediaEngines()
{
    MediaPlayer::resetMediaEngines();
}

void HTMLMediaElement::privateBrowsingStateDidChange()
{
    if (!m_player)
        return;

    bool privateMode = document().page() && document().page()->usesEphemeralSession();
    m_player->setPrivateBrowsingMode(privateMode);
}

MediaControls* HTMLMediaElement::mediaControls() const
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    return nullptr;
#else
    auto root = userAgentShadowRoot();
    if (!root)
        return nullptr;

    return childrenOfType<MediaControls>(*root).first();
#endif
}

bool HTMLMediaElement::hasMediaControls() const
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    return false;
#else

    if (auto userAgent = userAgentShadowRoot()) {
        RefPtr<Node> node = childrenOfType<MediaControls>(*root).first();
        ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isMediaControls());
        return node;
    }

    return false;
#endif
}

bool HTMLMediaElement::createMediaControls()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    ensureMediaControlsShadowRoot();
    return false;
#else
    if (hasMediaControls())
        return true;

    auto mediaControls = MediaControls::create(document());
    if (!mediaControls)
        return false;

    mediaControls->setMediaController(m_mediaController ? m_mediaController.get() : static_cast<MediaControllerInterface*>(this));
    mediaControls->reset();
    if (isFullscreen())
        mediaControls->enteredFullscreen();

    ensureUserAgentShadowRoot().appendChild(mediaControls);

    if (!controls() || !isConnected())
        mediaControls->hide();

    return true;
#endif
}

bool HTMLMediaElement::shouldForceControlsDisplay() const
{
    // Always create controls for autoplay video that requires user gesture due to being in low power mode.
    return isVideo() && autoplay() && m_mediaSession->hasBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoDueToLowPowerMode);
}

void HTMLMediaElement::configureMediaControls()
{
    bool requireControls = controls();

    // Always create controls for video when fullscreen playback is required.
    if (isVideo() && m_mediaSession->requiresFullscreenForVideoPlayback())
        requireControls = true;

    if (shouldForceControlsDisplay())
        requireControls = true;

    // Always create controls when in full screen mode.
    if (isFullscreen())
        requireControls = true;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (m_isPlayingToWirelessTarget)
        requireControls = true;
#endif

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (!requireControls || !isConnected() || !inActiveDocument())
        return;

    ensureMediaControlsShadowRoot();
#else
    if (!requireControls || !isConnected() || !inActiveDocument()) {
        if (hasMediaControls())
            mediaControls()->hide();
        return;
    }

    if (!hasMediaControls() && !createMediaControls())
        return;

    mediaControls()->show();
#endif
}

#if ENABLE(VIDEO_TRACK)
void HTMLMediaElement::configureTextTrackDisplay(TextTrackVisibilityCheckType checkType)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    ASSERT(m_textTracks);

    if (m_processingPreferenceChange)
        return;

    if (document().activeDOMObjectsAreStopped())
        return;

    bool haveVisibleTextTrack = false;
    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
        if (m_textTracks->item(i)->mode() == TextTrack::Mode::Showing) {
            haveVisibleTextTrack = true;
            break;
        }
    }

    if (checkType == CheckTextTrackVisibility && m_haveVisibleTextTrack == haveVisibleTextTrack) {
        updateActiveTextTrackCues(currentMediaTime());
        return;
    }

    m_haveVisibleTextTrack = haveVisibleTextTrack;
    m_closedCaptionsVisible = m_haveVisibleTextTrack;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (!m_haveVisibleTextTrack)
        return;

    ensureMediaControlsShadowRoot();
#else
    if (!m_haveVisibleTextTrack && !hasMediaControls())
        return;
    if (!hasMediaControls() && !createMediaControls())
        return;

    mediaControls()->changedClosedCaptionsVisibility();

    updateTextTrackDisplay();
    updateActiveTextTrackCues(currentMediaTime());
#endif
}

void HTMLMediaElement::captionPreferencesChanged()
{
    if (!isVideo())
        return;

    if (hasMediaControls())
        mediaControls()->textTrackPreferencesChanged();

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (m_mediaControlsHost)
        m_mediaControlsHost->updateCaptionDisplaySizes();
#endif

    if (m_player)
        m_player->tracksChanged();

    if (!document().page())
        return;

    CaptionUserPreferences::CaptionDisplayMode displayMode = document().page()->group().captionPreferences().captionDisplayMode();
    if (captionDisplayMode() == displayMode)
        return;

    m_captionDisplayMode = displayMode;
    setWebkitClosedCaptionsVisible(captionDisplayMode() == CaptionUserPreferences::AlwaysOn);
}

CaptionUserPreferences::CaptionDisplayMode HTMLMediaElement::captionDisplayMode()
{
    if (!m_captionDisplayMode.hasValue()) {
        if (document().page())
            m_captionDisplayMode = document().page()->group().captionPreferences().captionDisplayMode();
        else
            m_captionDisplayMode = CaptionUserPreferences::Automatic;
    }

    return m_captionDisplayMode.value();
}

void HTMLMediaElement::markCaptionAndSubtitleTracksAsUnconfigured(ReconfigureMode mode)
{
    if (!m_textTracks)
        return;

    INFO_LOG(LOGIDENTIFIER);

    // Mark all tracks as not "configured" so that configureTextTracks()
    // will reconsider which tracks to display in light of new user preferences
    // (e.g. default tracks should not be displayed if the user has turned off
    // captions and non-default tracks should be displayed based on language
    // preferences if the user has turned captions on).
    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
        auto& track = *m_textTracks->item(i);
        auto kind = track.kind();
        if (kind == TextTrack::Kind::Subtitles || kind == TextTrack::Kind::Captions)
            track.setHasBeenConfigured(false);
    }

    m_processingPreferenceChange = true;
    m_configureTextTracksTask.cancelTask();
    if (mode == Immediately) {
        Ref<HTMLMediaElement> protectedThis(*this); // configureTextTracks calls methods that can trigger arbitrary DOM mutations.
        configureTextTracks();
    }
    else
        scheduleConfigureTextTracks();
}

#endif

void HTMLMediaElement::createMediaPlayer()
{
    INFO_LOG(LOGIDENTIFIER);

#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode)
        m_audioSourceNode->lock();
#endif

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

#if ENABLE(VIDEO_TRACK)
    forgetResourceSpecificTracks();
#endif
    m_player = MediaPlayer::create(*this);
    m_player->setShouldBufferData(m_shouldBufferData);
    schedulePlaybackControlsManagerUpdate();

#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode) {
        // When creating the player, make sure its AudioSourceProvider knows about the MediaElementAudioSourceNode.
        if (audioSourceProvider())
            audioSourceProvider()->setClient(m_audioSourceNode);

        m_audioSourceNode->unlock();
    }
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (hasEventListeners(eventNames().webkitplaybacktargetavailabilitychangedEvent)) {
        m_hasPlaybackTargetAvailabilityListeners = true;
        m_mediaSession->setHasPlaybackTargetAvailabilityListeners(true);
        enqueuePlaybackTargetAvailabilityChangedEvent(); // Ensure the event listener gets at least one event.
    }
#endif

    updateSleepDisabling();
}

#if ENABLE(WEB_AUDIO)
void HTMLMediaElement::setAudioSourceNode(MediaElementAudioSourceNode* sourceNode)
{
    m_audioSourceNode = sourceNode;

    if (audioSourceProvider())
        audioSourceProvider()->setClient(m_audioSourceNode);
}

AudioSourceProvider* HTMLMediaElement::audioSourceProvider()
{
    if (m_player)
        return m_player->audioSourceProvider();

    return 0;
}
#endif

const String& HTMLMediaElement::mediaGroup() const
{
    return m_mediaGroup;
}

void HTMLMediaElement::setMediaGroup(const String& group)
{
    if (m_mediaGroup == group)
        return;
    m_mediaGroup = group;

    // When a media element is created with a mediagroup attribute, and when a media element's mediagroup
    // attribute is set, changed, or removed, the user agent must run the following steps:
    // 1. Let m [this] be the media element in question.
    // 2. Let m have no current media controller, if it currently has one.
    setController(nullptr);

    // 3. If m's mediagroup attribute is being removed, then abort these steps.
    if (group.isEmpty())
        return;

    // 4. If there is another media element whose Document is the same as m's Document (even if one or both
    // of these elements are not actually in the Document),
    HashSet<HTMLMediaElement*> elements = documentToElementSetMap().get(&document());
    for (auto& element : elements) {
        if (element == this)
            continue;

        // and which also has a mediagroup attribute, and whose mediagroup attribute has the same value as
        // the new value of m's mediagroup attribute,
        if (element->mediaGroup() == group) {
            //  then let controller be that media element's current media controller.
            setController(element->controller());
            return;
        }
    }

    // Otherwise, let controller be a newly created MediaController.
    setController(MediaController::create(document()));
}

MediaController* HTMLMediaElement::controller() const
{
    return m_mediaController.get();
}

void HTMLMediaElement::setController(RefPtr<MediaController>&& controller)
{
    if (m_mediaController)
        m_mediaController->removeMediaElement(*this);

    m_mediaController = WTFMove(controller);

    if (m_mediaController)
        m_mediaController->addMediaElement(*this);

    if (hasMediaControls())
        mediaControls()->setMediaController(m_mediaController ? m_mediaController.get() : static_cast<MediaControllerInterface*>(this));
}

void HTMLMediaElement::setControllerForBindings(MediaController* controller)
{
    // 4.8.10.11.2 Media controllers: controller attribute.
    // On setting, it must first remove the element's mediagroup attribute, if any,
    setMediaGroup({ });
    // and then set the current media controller to the given value.
    setController(controller);
}

void HTMLMediaElement::updateMediaController()
{
    if (m_mediaController)
        m_mediaController->reportControllerState();
}

bool HTMLMediaElement::isBlocked() const
{
    // A media element is a blocked media element if its readyState attribute is in the
    // HAVE_NOTHING state, the HAVE_METADATA state, or the HAVE_CURRENT_DATA state,
    if (m_readyState <= HAVE_CURRENT_DATA)
        return true;

    // or if the element has paused for user interaction.
    return pausedForUserInteraction();
}

bool HTMLMediaElement::isBlockedOnMediaController() const
{
    if (!m_mediaController)
        return false;

    // A media element is blocked on its media controller if the MediaController is a blocked
    // media controller,
    if (m_mediaController->isBlocked())
        return true;

    // or if its media controller position is either before the media resource's earliest possible
    // position relative to the MediaController's timeline or after the end of the media resource
    // relative to the MediaController's timeline.
    double mediaControllerPosition = m_mediaController->currentTime();
    if (mediaControllerPosition < 0 || mediaControllerPosition > duration())
        return true;

    return false;
}

void HTMLMediaElement::prepareMediaFragmentURI()
{
    MediaFragmentURIParser fragmentParser(m_currentSrc);
    MediaTime dur = durationMediaTime();

    MediaTime start = fragmentParser.startTime();
    if (start.isValid() && start > MediaTime::zeroTime()) {
        m_fragmentStartTime = start;
        if (m_fragmentStartTime > dur)
            m_fragmentStartTime = dur;
    } else
        m_fragmentStartTime = MediaTime::invalidTime();

    MediaTime end = fragmentParser.endTime();
    if (end.isValid() && end > MediaTime::zeroTime() && (!m_fragmentStartTime.isValid() || end > m_fragmentStartTime)) {
        m_fragmentEndTime = end;
        if (m_fragmentEndTime > dur)
            m_fragmentEndTime = dur;
    } else
        m_fragmentEndTime = MediaTime::invalidTime();

    if (m_fragmentStartTime.isValid() && m_readyState < HAVE_FUTURE_DATA)
        prepareToPlay();
}

void HTMLMediaElement::applyMediaFragmentURI()
{
    if (m_fragmentStartTime.isValid()) {
        m_sentEndEvent = false;
        seek(m_fragmentStartTime);
    }
}

void HTMLMediaElement::updateSleepDisabling()
{
    SleepType shouldDisableSleep = this->shouldDisableSleep();
    if (shouldDisableSleep == SleepType::None && m_sleepDisabler)
        m_sleepDisabler = nullptr;
    else if (shouldDisableSleep != SleepType::None) {
        auto type = shouldDisableSleep == SleepType::Display ? PAL::SleepDisabler::Type::Display : PAL::SleepDisabler::Type::System;
        if (!m_sleepDisabler || m_sleepDisabler->type() != type)
            m_sleepDisabler = PAL::SleepDisabler::create("com.apple.WebCore: HTMLMediaElement playback", type);
    }

    if (m_player)
        m_player->setShouldDisableSleep(shouldDisableSleep == SleepType::Display);
}

#if ENABLE(MEDIA_STREAM)
static inline bool isRemoteMediaStreamVideoTrack(RefPtr<MediaStreamTrack>& item)
{
    auto* track = item.get();
    return track->privateTrack().type() == RealtimeMediaSource::Type::Video && !track->isCaptureTrack() && !track->isCanvas();
}
#endif

HTMLMediaElement::SleepType HTMLMediaElement::shouldDisableSleep() const
{
#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WPE)
    return SleepType::None;
#endif
    if (!m_player || m_player->paused() || loop())
        return SleepType::None;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    // If the media is playing remotely, we can't know definitively whether it has audio or video tracks.
    if (m_isPlayingToWirelessTarget)
        return SleepType::System;
#endif

    bool shouldBeAbleToSleep = !hasVideo() || !hasAudio();
#if ENABLE(MEDIA_STREAM)
    // Remote media stream video tracks may have their corresponding audio tracks being played outside of the media element. Let's ensure to not IDLE the screen in that case.
    // FIXME: We should check that audio is being/to be played. Ideally, we would come up with a media stream agnostic heuristisc.
    shouldBeAbleToSleep = shouldBeAbleToSleep && !(m_mediaStreamSrcObject && WTF::anyOf(m_mediaStreamSrcObject->getTracks(), isRemoteMediaStreamVideoTrack));
#endif

    if (shouldBeAbleToSleep)
        return SleepType::None;

    if (m_elementIsHidden)
        return SleepType::System;

    return SleepType::Display;
}

String HTMLMediaElement::mediaPlayerReferrer() const
{
    RefPtr<Frame> frame = document().frame();
    if (!frame)
        return String();

    return SecurityPolicy::generateReferrerHeader(document().referrerPolicy(), m_currentSrc, frame->loader().outgoingReferrer());
}

String HTMLMediaElement::mediaPlayerUserAgent() const
{
    RefPtr<Frame> frame = document().frame();
    if (!frame)
        return String();

    return frame->loader().userAgent(m_currentSrc);
}

#if ENABLE(AVF_CAPTIONS)

static inline PlatformTextTrack::TrackKind toPlatform(TextTrack::Kind kind)
{
    switch (kind) {
    case TextTrack::Kind::Captions:
        return PlatformTextTrack::Caption;
    case TextTrack::Kind::Chapters:
        return PlatformTextTrack::Chapter;
    case TextTrack::Kind::Descriptions:
        return PlatformTextTrack::Description;
    case TextTrack::Kind::Forced:
        return PlatformTextTrack::Forced;
    case TextTrack::Kind::Metadata:
        return PlatformTextTrack::MetaData;
    case TextTrack::Kind::Subtitles:
        return PlatformTextTrack::Subtitle;
    }
    ASSERT_NOT_REACHED();
    return PlatformTextTrack::Caption;
}

static inline PlatformTextTrack::TrackMode toPlatform(TextTrack::Mode mode)
{
    switch (mode) {
    case TextTrack::Mode::Disabled:
        return PlatformTextTrack::Disabled;
    case TextTrack::Mode::Hidden:
        return PlatformTextTrack::Hidden;
    case TextTrack::Mode::Showing:
        return PlatformTextTrack::Showing;
    }
    ASSERT_NOT_REACHED();
    return PlatformTextTrack::Disabled;
}

Vector<RefPtr<PlatformTextTrack>> HTMLMediaElement::outOfBandTrackSources()
{
    Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources;
    for (auto& trackElement : childrenOfType<HTMLTrackElement>(*this)) {
        URL url = trackElement.getNonEmptyURLAttribute(srcAttr);
        if (url.isEmpty())
            continue;

        if (!isAllowedToLoadMediaURL(*this, url, trackElement.isInUserAgentShadowTree()))
            continue;

        auto& track = trackElement.track();
        auto kind = track.kind();

        // FIXME: The switch statement below preserves existing behavior where we ignore chapters and metadata tracks.
        // If we confirm this behavior is valuable, we should remove this comment. Otherwise, remove both comment and switch.
        switch (kind) {
        case TextTrack::Kind::Captions:
        case TextTrack::Kind::Descriptions:
        case TextTrack::Kind::Forced:
        case TextTrack::Kind::Subtitles:
            break;
        case TextTrack::Kind::Chapters:
        case TextTrack::Kind::Metadata:
            continue;
        }

        outOfBandTrackSources.append(PlatformTextTrack::createOutOfBand(trackElement.label(), trackElement.srclang(), url.string(), toPlatform(track.mode()), toPlatform(kind), track.uniqueId(), trackElement.isDefault()));
    }

    return outOfBandTrackSources;
}

#endif

void HTMLMediaElement::mediaPlayerEnterFullscreen()
{
    enterFullscreen();
}

void HTMLMediaElement::mediaPlayerExitFullscreen()
{
    exitFullscreen();
}

bool HTMLMediaElement::mediaPlayerIsFullscreen() const
{
    return isFullscreen();
}

bool HTMLMediaElement::mediaPlayerIsFullscreenPermitted() const
{
    return m_mediaSession->fullscreenPermitted();
}

bool HTMLMediaElement::mediaPlayerIsVideo() const
{
    return isVideo();
}

LayoutRect HTMLMediaElement::mediaPlayerContentBoxRect() const
{
    auto* renderer = this->renderer();
    if (!renderer)
        return { };
    return renderer->enclosingBox().contentBoxRect();
}

float HTMLMediaElement::mediaPlayerContentsScale() const
{
    if (auto page = document().page())
        return page->pageScaleFactor() * page->deviceScaleFactor();
    return 1;
}

void HTMLMediaElement::mediaPlayerSetSize(const IntSize& size)
{
    setIntegralAttribute(widthAttr, size.width());
    setIntegralAttribute(heightAttr, size.height());
}

void HTMLMediaElement::mediaPlayerPause()
{
    pause();
}

void HTMLMediaElement::mediaPlayerPlay()
{
    play();
}

bool HTMLMediaElement::mediaPlayerPlatformVolumeConfigurationRequired() const
{
    return !m_volumeInitialized;
}

bool HTMLMediaElement::mediaPlayerIsPaused() const
{
    return paused();
}

bool HTMLMediaElement::mediaPlayerIsLooping() const
{
    return loop();
}

CachedResourceLoader* HTMLMediaElement::mediaPlayerCachedResourceLoader()
{
    return &document().cachedResourceLoader();
}

RefPtr<PlatformMediaResourceLoader> HTMLMediaElement::mediaPlayerCreateResourceLoader()
{
    auto mediaResourceLoader = adoptRef(*new MediaResourceLoader(document(), *this, crossOrigin()));

    m_lastMediaResourceLoaderForTesting = makeWeakPtr(mediaResourceLoader.get());

    return WTFMove(mediaResourceLoader);
}

const MediaResourceLoader* HTMLMediaElement::lastMediaResourceLoaderForTesting() const
{
    return m_lastMediaResourceLoaderForTesting.get();
}

bool HTMLMediaElement::mediaPlayerShouldUsePersistentCache() const
{
    if (Page* page = document().page())
        return !page->usesEphemeralSession() && !page->isResourceCachingDisabled();

    return false;
}

const String& HTMLMediaElement::mediaPlayerMediaCacheDirectory() const
{
    return mediaCacheDirectory();
}

String HTMLMediaElement::sourceApplicationIdentifier() const
{
    if (RefPtr<Frame> frame = document().frame()) {
        if (NetworkingContext* networkingContext = frame->loader().networkingContext())
            return networkingContext->sourceApplicationIdentifier();
    }
    return emptyString();
}

Vector<String> HTMLMediaElement::mediaPlayerPreferredAudioCharacteristics() const
{
    if (Page* page = document().page())
        return page->group().captionPreferences().preferredAudioCharacteristics();
    return Vector<String>();
}

#if PLATFORM(IOS_FAMILY)
String HTMLMediaElement::mediaPlayerNetworkInterfaceName() const
{
    return DeprecatedGlobalSettings::networkInterfaceName();
}

bool HTMLMediaElement::mediaPlayerGetRawCookies(const URL& url, Vector<Cookie>& cookies) const
{
    if (auto* page = document().page())
        return page->cookieJar().getRawCookies(document(), url, cookies);
    return false;
}
#endif

bool HTMLMediaElement::mediaPlayerIsInMediaDocument() const
{
    return document().isMediaDocument();
}

void HTMLMediaElement::mediaPlayerEngineFailedToLoad() const
{
    if (!m_player)
        return;

    if (auto* page = document().page())
        page->diagnosticLoggingClient().logDiagnosticMessageWithValue(DiagnosticLoggingKeys::engineFailedToLoadKey(), m_player->engineDescription(), m_player->platformErrorCode(), 4, ShouldSample::No);
}

double HTMLMediaElement::mediaPlayerRequestedPlaybackRate() const
{
    return potentiallyPlaying() ? requestedPlaybackRate() : 0;
}

const Vector<ContentType>& HTMLMediaElement::mediaContentTypesRequiringHardwareSupport() const
{
    return document().settings().mediaContentTypesRequiringHardwareSupport();
}

bool HTMLMediaElement::mediaPlayerShouldCheckHardwareSupport() const
{
    if (!document().settings().allowMediaContentTypesRequiringHardwareSupportAsFallback())
        return true;

    if (m_loadState == LoadingFromSourceElement && m_currentSourceNode && !m_nextChildNodeToConsider)
        return false;

    if (m_loadState == LoadingFromSrcAttr)
        return false;

    return true;
}

#if USE(GSTREAMER)
void HTMLMediaElement::requestInstallMissingPlugins(const String& details, const String& description, MediaPlayerRequestInstallMissingPluginsCallback& callback)
{
    if (!document().page())
        return;

    document().page()->chrome().client().requestInstallMissingMediaPlugins(details, description, callback);
}
#endif

void HTMLMediaElement::removeBehaviorsRestrictionsAfterFirstUserGesture(MediaElementSession::BehaviorRestrictions mask)
{
    MediaElementSession::BehaviorRestrictions restrictionsToRemove = mask &
        (MediaElementSession::RequireUserGestureForLoad
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        | MediaElementSession::RequireUserGestureToShowPlaybackTargetPicker
        | MediaElementSession::RequireUserGestureToAutoplayToExternalDevice
#endif
        | MediaElementSession::RequireUserGestureForVideoRateChange
        | MediaElementSession::RequireUserGestureForAudioRateChange
        | MediaElementSession::RequireUserGestureForFullscreen
        | MediaElementSession::RequireUserGestureForVideoDueToLowPowerMode
        | MediaElementSession::InvisibleAutoplayNotPermitted
        | MediaElementSession::RequireUserGestureToControlControlsManager);

    m_mediaSession->removeBehaviorRestriction(restrictionsToRemove);
    document().topDocument().noteUserInteractionWithMediaElement();
}

void HTMLMediaElement::updateRateChangeRestrictions()
{
    const auto& document = this->document();
    if (!document.ownerElement() && document.isMediaDocument())
        return;

    const auto& topDocument = document.topDocument();
    if (topDocument.videoPlaybackRequiresUserGesture())
        m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoRateChange);
    else
        m_mediaSession->removeBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoRateChange);

    if (topDocument.audioPlaybackRequiresUserGesture())
        m_mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForAudioRateChange);
    else
        m_mediaSession->removeBehaviorRestriction(MediaElementSession::RequireUserGestureForAudioRateChange);
}

RefPtr<VideoPlaybackQuality> HTMLMediaElement::getVideoPlaybackQuality()
{
    RefPtr<DOMWindow> domWindow = document().domWindow();
    double timestamp = domWindow ? 1000 * domWindow->nowTimestamp() : 0;

    auto metrics = m_player ? m_player->videoPlaybackQualityMetrics() : WTF::nullopt;
    if (!metrics)
        return VideoPlaybackQuality::create(timestamp, { });

#if ENABLE(MEDIA_SOURCE)
    metrics.value().totalVideoFrames += m_droppedVideoFrames;
    metrics.value().droppedVideoFrames += m_droppedVideoFrames;
#endif

    return VideoPlaybackQuality::create(timestamp, metrics.value());
}

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
DOMWrapperWorld& HTMLMediaElement::ensureIsolatedWorld()
{
    if (!m_isolatedWorld)
        m_isolatedWorld = DOMWrapperWorld::create(commonVM());
    return *m_isolatedWorld;
}

bool HTMLMediaElement::ensureMediaControlsInjectedScript()
{
    DEBUG_LOG(LOGIDENTIFIER);

    Page* page = document().page();
    if (!page)
        return false;

    String mediaControlsScript = RenderTheme::singleton().mediaControlsScript();
    if (!mediaControlsScript.length())
        return false;

    return setupAndCallJS([mediaControlsScript](JSDOMGlobalObject& globalObject, JSC::ExecState& exec, ScriptController& scriptController, DOMWrapperWorld& world) {
        auto& vm = globalObject.vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);

        auto functionValue = globalObject.get(&exec, JSC::Identifier::fromString(&exec, "createControls"));
        if (functionValue.isFunction(vm))
            return true;

#ifndef NDEBUG
        // Setting a scriptURL allows the source to be debuggable in the inspector.
        URL scriptURL = URL({ }, "mediaControlsScript"_s);
#else
        URL scriptURL;
#endif
        scriptController.evaluateInWorld(ScriptSourceCode(mediaControlsScript, WTFMove(scriptURL)), world);
        if (UNLIKELY(scope.exception())) {
            scope.clearException();
            return false;
        }

        return true;
    });
}

void HTMLMediaElement::updatePageScaleFactorJSProperty()
{
    Page* page = document().page();
    if (!page)
        return;

    setControllerJSProperty("pageScaleFactor", JSC::jsNumber(page->pageScaleFactor()));
}

void HTMLMediaElement::updateUsesLTRUserInterfaceLayoutDirectionJSProperty()
{
    Page* page = document().page();
    if (!page)
        return;

    bool usesLTRUserInterfaceLayoutDirectionProperty = page->userInterfaceLayoutDirection() == UserInterfaceLayoutDirection::LTR;
    setControllerJSProperty("usesLTRUserInterfaceLayoutDirection", JSC::jsBoolean(usesLTRUserInterfaceLayoutDirectionProperty));
}

void HTMLMediaElement::setControllerJSProperty(const char* propertyName, JSC::JSValue propertyValue)
{
    setupAndCallJS([this, propertyName, propertyValue](JSDOMGlobalObject& globalObject, JSC::ExecState& exec, ScriptController&, DOMWrapperWorld&) {
        auto& vm = globalObject.vm();
        auto controllerValue = controllerJSValue(exec, globalObject, *this);
        if (controllerValue.isNull())
            return false;

        JSC::PutPropertySlot propertySlot(controllerValue);
        auto* controllerObject = controllerValue.toObject(&exec);
        if (!controllerObject)
            return false;

        controllerObject->methodTable(vm)->put(controllerObject, &exec, JSC::Identifier::fromString(&exec, propertyName), propertyValue, propertySlot);

        return true;
    });
}

void HTMLMediaElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    DEBUG_LOG(LOGIDENTIFIER);

    if (!ensureMediaControlsInjectedScript())
        return;

    setupAndCallJS([this, &root](JSDOMGlobalObject& globalObject, JSC::ExecState& exec, ScriptController&, DOMWrapperWorld&) {
        auto& vm = globalObject.vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);

        // The media controls script must provide a method with the following details.
        // Name: createControls
        // Parameters:
        //     1. The ShadowRoot element that will hold the controls.
        //     2. This object (and HTMLMediaElement).
        //     3. The MediaControlsHost object.
        // Return value:
        //     A reference to the created media controller instance.

        auto functionValue = globalObject.get(&exec, JSC::Identifier::fromString(&exec, "createControls"));
        if (functionValue.isUndefinedOrNull())
            return false;

        if (!m_mediaControlsHost)
            m_mediaControlsHost = MediaControlsHost::create(this);

        auto mediaJSWrapper = toJS(&exec, &globalObject, *this);
        auto mediaControlsHostJSWrapper = toJS(&exec, &globalObject, *m_mediaControlsHost);

        JSC::MarkedArgumentBuffer argList;
        argList.append(toJS(&exec, &globalObject, root));
        argList.append(mediaJSWrapper);
        argList.append(mediaControlsHostJSWrapper);
        ASSERT(!argList.hasOverflowed());

        auto* function = functionValue.toObject(&exec);
        scope.assertNoException();
        JSC::CallData callData;
        auto callType = function->methodTable(vm)->getCallData(function, callData);
        if (callType == JSC::CallType::None)
            return false;

        auto controllerValue = JSC::call(&exec, function, callType, callData, &globalObject, argList);
        scope.clearException();
        auto* controllerObject = JSC::jsDynamicCast<JSC::JSObject*>(vm, controllerValue);
        if (!controllerObject)
            return false;

        // Connect the Media, MediaControllerHost, and Controller so the GC knows about their relationship
        auto* mediaJSWrapperObject = mediaJSWrapper.toObject(&exec);
        scope.assertNoException();
        auto controlsHost = JSC::Identifier::fromString(&exec.vm(), "controlsHost");

        ASSERT(!mediaJSWrapperObject->hasProperty(&exec, controlsHost));

        mediaJSWrapperObject->putDirect(exec.vm(), controlsHost, mediaControlsHostJSWrapper, JSC::PropertyAttribute::DontDelete | JSC::PropertyAttribute::DontEnum | JSC::PropertyAttribute::ReadOnly);

        auto* mediaControlsHostJSWrapperObject = JSC::jsDynamicCast<JSC::JSObject*>(vm, mediaControlsHostJSWrapper);
        if (!mediaControlsHostJSWrapperObject)
            return false;

        auto controller = JSC::Identifier::fromString(&exec.vm(), "controller");

        ASSERT(!controllerObject->hasProperty(&exec, controller));

        mediaControlsHostJSWrapperObject->putDirect(exec.vm(), controller, controllerValue, JSC::PropertyAttribute::DontDelete | JSC::PropertyAttribute::DontEnum | JSC::PropertyAttribute::ReadOnly);

        updatePageScaleFactorJSProperty();
        updateUsesLTRUserInterfaceLayoutDirectionJSProperty();

        if (UNLIKELY(scope.exception()))
            scope.clearException();

        return true;
    });
}

void HTMLMediaElement::setMediaControlsDependOnPageScaleFactor(bool dependsOnPageScale)
{
    DEBUG_LOG(LOGIDENTIFIER, "MediaElement::setMediaControlsDependPageScaleFactor", dependsOnPageScale);

    if (document().settings().mediaControlsScaleWithPageZoom()) {
        DEBUG_LOG(LOGIDENTIFIER, "MediaElement::setMediaControlsDependPageScaleFactor", "forced to false by Settings value");
        m_mediaControlsDependOnPageScaleFactor = false;
        return;
    }

    if (m_mediaControlsDependOnPageScaleFactor == dependsOnPageScale)
        return;

    m_mediaControlsDependOnPageScaleFactor = dependsOnPageScale;

    if (m_mediaControlsDependOnPageScaleFactor)
        document().registerForPageScaleFactorChangedCallbacks(this);
    else
        document().unregisterForPageScaleFactorChangedCallbacks(this);
}

void HTMLMediaElement::updateMediaControlsAfterPresentationModeChange()
{
    // Don't execute script if the controls script hasn't been injected yet, or we have
    // stopped/suspended the object.
    if (!m_mediaControlsHost || document().activeDOMObjectsAreSuspended() || document().activeDOMObjectsAreStopped())
        return;

    setupAndCallJS([this](JSDOMGlobalObject& globalObject, JSC::ExecState& exec, ScriptController&, DOMWrapperWorld&) {
        auto& vm = globalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto controllerValue = controllerJSValue(exec, globalObject, *this);
        auto* controllerObject = controllerValue.toObject(&exec);

        RETURN_IF_EXCEPTION(scope, false);

        auto functionValue = controllerObject->get(&exec, JSC::Identifier::fromString(&exec, "handlePresentationModeChange"));
        if (UNLIKELY(scope.exception()) || functionValue.isUndefinedOrNull())
            return false;

        auto* function = functionValue.toObject(&exec);
        scope.assertNoException();
        JSC::CallData callData;
        auto callType = function->methodTable(vm)->getCallData(function, callData);
        if (callType == JSC::CallType::None)
            return false;

        JSC::MarkedArgumentBuffer argList;
        ASSERT(!argList.hasOverflowed());
        JSC::call(&exec, function, callType, callData, controllerObject, argList);

        return true;
    });
}

void HTMLMediaElement::pageScaleFactorChanged()
{
    updatePageScaleFactorJSProperty();
}

void HTMLMediaElement::userInterfaceLayoutDirectionChanged()
{
    updateUsesLTRUserInterfaceLayoutDirectionJSProperty();
}

String HTMLMediaElement::getCurrentMediaControlsStatus()
{
    ensureMediaControlsShadowRoot();

    String status;
    setupAndCallJS([this, &status](JSDOMGlobalObject& globalObject, JSC::ExecState& exec, ScriptController&, DOMWrapperWorld&) {
        auto& vm = globalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto controllerValue = controllerJSValue(exec, globalObject, *this);
        auto* controllerObject = controllerValue.toObject(&exec);

        RETURN_IF_EXCEPTION(scope, false);

        auto functionValue = controllerObject->get(&exec, JSC::Identifier::fromString(&exec, "getCurrentControlsStatus"));
        if (UNLIKELY(scope.exception()) || functionValue.isUndefinedOrNull())
            return false;

        auto* function = functionValue.toObject(&exec);
        scope.assertNoException();
        JSC::CallData callData;
        auto callType = function->methodTable(vm)->getCallData(function, callData);
        JSC::MarkedArgumentBuffer argList;
        ASSERT(!argList.hasOverflowed());
        if (callType == JSC::CallType::None)
            return false;

        auto outputValue = JSC::call(&exec, function, callType, callData, controllerObject, argList);

        RETURN_IF_EXCEPTION(scope, false);

        status = outputValue.getString(&exec);
        return true;
    });

    return status;
}
#endif // ENABLE(MEDIA_CONTROLS_SCRIPT)

unsigned long long HTMLMediaElement::fileSize() const
{
    if (m_player)
        return m_player->fileSize();

    return 0;
}

PlatformMediaSession::MediaType HTMLMediaElement::mediaType() const
{
    if (m_player && m_readyState >= HAVE_METADATA) {
        if (hasVideo() && hasAudio() && !muted())
            return PlatformMediaSession::VideoAudio;
        return hasVideo() ? PlatformMediaSession::Video : PlatformMediaSession::Audio;
    }

    return presentationType();
}

PlatformMediaSession::MediaType HTMLMediaElement::presentationType() const
{
    if (hasTagName(HTMLNames::videoTag))
        return muted() ? PlatformMediaSession::Video : PlatformMediaSession::VideoAudio;

    return PlatformMediaSession::Audio;
}

PlatformMediaSession::DisplayType HTMLMediaElement::displayType() const
{
    if (m_videoFullscreenMode == VideoFullscreenModeStandard)
        return PlatformMediaSession::Fullscreen;
    if (m_videoFullscreenMode & VideoFullscreenModePictureInPicture)
        return PlatformMediaSession::Optimized;
    if (m_videoFullscreenMode == VideoFullscreenModeNone)
        return PlatformMediaSession::Normal;

    ASSERT_NOT_REACHED();
    return PlatformMediaSession::Normal;
}

PlatformMediaSession::CharacteristicsFlags HTMLMediaElement::characteristics() const
{
    if (m_readyState < HAVE_METADATA)
        return PlatformMediaSession::HasNothing;

    PlatformMediaSession::CharacteristicsFlags state = PlatformMediaSession::HasNothing;
    if (isVideo() && hasVideo())
        state |= PlatformMediaSession::HasVideo;
    if (this->hasAudio())
        state |= PlatformMediaSession::HasAudio;

    return state;
}

bool HTMLMediaElement::canProduceAudio() const
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    // Because the remote target could unmute playback without notifying us, we must assume
    // that we may be playing audio.
    if (m_isPlayingToWirelessTarget)
        return true;
#endif

    if (muted())
        return false;

    return m_player && m_readyState >= HAVE_METADATA && hasAudio();
}

bool HTMLMediaElement::isSuspended() const
{
    return document().activeDOMObjectsAreSuspended() || document().activeDOMObjectsAreStopped();
}

#if ENABLE(MEDIA_SOURCE)
size_t HTMLMediaElement::maximumSourceBufferSize(const SourceBuffer& buffer) const
{
    return m_mediaSession->maximumMediaSourceBufferSize(buffer);
}
#endif

void HTMLMediaElement::suspendPlayback()
{
    INFO_LOG(LOGIDENTIFIER, "paused = ", paused());
    if (!paused())
        pause();
}

void HTMLMediaElement::resumeAutoplaying()
{
    INFO_LOG(LOGIDENTIFIER, "paused = ", paused());
    m_autoplaying = true;

    if (canTransitionFromAutoplayToPlay())
        play();
}

void HTMLMediaElement::mayResumePlayback(bool shouldResume)
{
    INFO_LOG(LOGIDENTIFIER, "paused = ", paused());
    if (paused() && shouldResume)
        play();
}

String HTMLMediaElement::mediaSessionTitle() const
{
    if (!document().page() || document().page()->usesEphemeralSession())
        return emptyString();

    auto title = String(attributeWithoutSynchronization(titleAttr)).stripWhiteSpace().simplifyWhiteSpace();
    if (!title.isEmpty())
        return title;

    title = document().title().stripWhiteSpace().simplifyWhiteSpace();
    if (!title.isEmpty())
        return title;

    title = m_currentSrc.host().toString();
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    if (!title.isEmpty())
        title = decodeHostName(title);
#endif
#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (!title.isEmpty()) {
        auto domain = topPrivatelyControlledDomain(title);
        if (!domain.isEmpty())
            title = domain;
    }
#endif

    return title;
}

uint64_t HTMLMediaElement::mediaSessionUniqueIdentifier() const
{
    auto& url = m_currentSrc.string();
    return url.impl() ? url.impl()->hash() : 0;
}

void HTMLMediaElement::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument* argument)
{
    INFO_LOG(LOGIDENTIFIER, command);

    UserGestureIndicator remoteControlUserGesture(ProcessingUserGesture, &document());
    switch (command) {
    case PlatformMediaSession::PlayCommand:
        play();
        break;
    case PlatformMediaSession::StopCommand:
    case PlatformMediaSession::PauseCommand:
        pause();
        break;
    case PlatformMediaSession::TogglePlayPauseCommand:
        canPlay() ? play() : pause();
        break;
    case PlatformMediaSession::BeginSeekingBackwardCommand:
        beginScanning(Backward);
        break;
    case PlatformMediaSession::BeginSeekingForwardCommand:
        beginScanning(Forward);
        break;
    case PlatformMediaSession::EndSeekingBackwardCommand:
    case PlatformMediaSession::EndSeekingForwardCommand:
        endScanning();
        break;
    case PlatformMediaSession::SeekToPlaybackPositionCommand:
        ASSERT(argument);
        if (argument)
            handleSeekToPlaybackPosition(argument->asDouble);
        break;
    default:
        { } // Do nothing
    }
}

static bool needsSeekingSupportQuirk(Document& document)
{
    if (!document.settings().needsSiteSpecificQuirks())
        return false;

    auto host = document.topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "netflix.com") || host.endsWithIgnoringASCIICase(".netflix.com");
}

bool HTMLMediaElement::supportsSeeking() const
{
    return !needsSeekingSupportQuirk(document()) && !isLiveStream();
}

bool HTMLMediaElement::shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType type) const
{
    if (type == PlatformMediaSession::EnteringBackground) {
        if (isPlayingToExternalTarget()) {
            INFO_LOG(LOGIDENTIFIER, "returning true because isPlayingToExternalTarget() is true");
            return true;
        }
        if (m_videoFullscreenMode & VideoFullscreenModePictureInPicture)
            return true;
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
        if (((m_videoFullscreenMode == VideoFullscreenModeStandard) || m_videoFullscreenStandby) && supportsPictureInPicture() && isPlaying())
            return true;
#endif
    } else if (type == PlatformMediaSession::SuspendedUnderLock) {
        if (isPlayingToExternalTarget()) {
            INFO_LOG(LOGIDENTIFIER, "returning true because isPlayingToExternalTarget() is true");
            return true;
        }
    }
    return false;
}

bool HTMLMediaElement::processingUserGestureForMedia() const
{
    return document().processingUserGestureForMedia();
}
#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void HTMLMediaElement::scheduleUpdateMediaState()
{
    if (m_updateMediaStateTask.hasPendingTask())
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier, "task scheduled");
    m_updateMediaStateTask.scheduleTask([this, logSiteIdentifier] {
        UNUSED_PARAM(logSiteIdentifier);
        ALWAYS_LOG(logSiteIdentifier, "- lambda(), task fired");
        Ref<HTMLMediaElement> protectedThis(*this); // updateMediaState calls methods that can trigger arbitrary DOM mutations.
        updateMediaState();
    });
}

void HTMLMediaElement::updateMediaState()
{
    MediaProducer::MediaStateFlags state = mediaState();
    if (m_mediaState == state)
        return;

    m_mediaState = state;
    m_mediaSession->mediaStateDidChange(m_mediaState);
#if ENABLE(MEDIA_SESSION)
    document().updateIsPlayingMedia(m_elementID);
#else
    document().updateIsPlayingMedia();
#endif
}
#endif

MediaProducer::MediaStateFlags HTMLMediaElement::mediaState() const
{
    MediaStateFlags state = IsNotPlaying;

    bool hasActiveVideo = isVideo() && hasVideo();
    bool hasAudio = this->hasAudio();
    if (isPlayingToExternalTarget())
        state |= IsPlayingToExternalDevice;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (m_hasPlaybackTargetAvailabilityListeners) {
        state |= HasPlaybackTargetAvailabilityListener;
        if (!m_mediaSession->wirelessVideoPlaybackDisabled())
            state |= RequiresPlaybackTargetMonitoring;
    }

    bool requireUserGesture = m_mediaSession->hasBehaviorRestriction(MediaElementSession::RequireUserGestureToAutoplayToExternalDevice);
    if (m_readyState >= HAVE_METADATA && !requireUserGesture && !m_failedToPlayToWirelessTarget)
        state |= ExternalDeviceAutoPlayCandidate;

    if (hasActiveVideo || hasAudio)
        state |= HasAudioOrVideo;

    if (hasActiveVideo && endedPlayback())
        state |= DidPlayToEnd;
#endif

    if (!isPlaying())
        return state;

    if (hasAudio && !muted() && volume())
        state |= IsPlayingAudio;

    if (hasActiveVideo)
        state |= IsPlayingVideo;

    return state;
}

void HTMLMediaElement::handleAutoplayEvent(AutoplayEvent event)
{
    if (Page* page = document().page()) {
        bool hasAudio = this->hasAudio() && !muted() && volume();
        bool wasPlaybackPrevented = m_autoplayEventPlaybackState == AutoplayEventPlaybackState::PreventedAutoplay;
        bool hasMainContent = m_mediaSession && m_mediaSession->isMainContentForPurposesOfAutoplayEvents();
        ALWAYS_LOG(LOGIDENTIFIER, "hasAudio = ", hasAudio, " wasPlaybackPrevented = ", wasPlaybackPrevented, " hasMainContent = ", hasMainContent);

        OptionSet<AutoplayEventFlags> flags;
        if (hasAudio)
            flags.add(AutoplayEventFlags::HasAudio);
        if (wasPlaybackPrevented)
            flags.add(AutoplayEventFlags::PlaybackWasPrevented);
        if (hasMainContent)
            flags.add(AutoplayEventFlags::MediaIsMainContent);

        page->chrome().client().handleAutoplayEvent(event, flags);
    }
}

void HTMLMediaElement::userDidInterfereWithAutoplay()
{
    if (m_autoplayEventPlaybackState != AutoplayEventPlaybackState::StartedWithoutUserGesture)
        return;

    // Only consider interference in the first 10 seconds of automatic playback.
    if (currentTime() - playbackStartedTime() > AutoplayInterferenceTimeThreshold)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    handleAutoplayEvent(AutoplayEvent::UserDidInterfereWithPlayback);
    setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);
}

void HTMLMediaElement::setAutoplayEventPlaybackState(AutoplayEventPlaybackState reason)
{
    ALWAYS_LOG(LOGIDENTIFIER, reason);

    m_autoplayEventPlaybackState = reason;

    if (reason == AutoplayEventPlaybackState::PreventedAutoplay) {
        dispatchPlayPauseEventsIfNeedsQuirks();
        handleAutoplayEvent(AutoplayEvent::DidPreventMediaFromPlaying);
    }
}

void HTMLMediaElement::pageMutedStateDidChange()
{
    updateVolume();

    if (Page* page = document().page()) {
        if (hasAudio() && !muted() && page->isAudioMuted())
            userDidInterfereWithAutoplay();
    }
}

bool HTMLMediaElement::effectiveMuted() const
{
    return muted() || (document().page() && document().page()->isAudioMuted());
}

bool HTMLMediaElement::doesHaveAttribute(const AtomicString& attribute, AtomicString* value) const
{
    QualifiedName attributeName(nullAtom(), attribute, nullAtom());

    auto& elementValue = attributeWithoutSynchronization(attributeName);
    if (elementValue.isNull())
        return false;

    if (attributeName == HTMLNames::x_itunes_inherit_uri_query_componentAttr && !document().settings().enableInheritURIQueryComponent())
        return false;

    if (value)
        *value = elementValue;

    return true;
}

void HTMLMediaElement::setShouldBufferData(bool shouldBuffer)
{
    if (shouldBuffer == m_shouldBufferData)
        return;

    m_shouldBufferData = shouldBuffer;
    if (m_player)
        m_player->setShouldBufferData(shouldBuffer);
}

void HTMLMediaElement::purgeBufferedDataIfPossible()
{
#if PLATFORM(IOS_FAMILY)
    if (!MemoryPressureHandler::singleton().isUnderMemoryPressure() && m_mediaSession->dataBufferingPermitted())
        return;

    if (isPlayingToExternalTarget()) {
        INFO_LOG(LOGIDENTIFIER, "early return because playing to wireless target");
        return;
    }

    // This is called to relieve memory pressure. Turning off buffering causes the media playback
    // daemon to release memory associated with queued-up video frames.
    // We turn it back on right away, but new frames won't get loaded unless playback is resumed.
    INFO_LOG(LOGIDENTIFIER, "toggling data buffering");
    setShouldBufferData(false);
    setShouldBufferData(true);
#endif
}

bool HTMLMediaElement::canSaveMediaData() const
{
    if (m_player)
        return m_player->canSaveMediaData();

    return false;
}

#if ENABLE(MEDIA_SESSION)
double HTMLMediaElement::playerVolume() const
{
    return m_player ? m_player->volume() : 0;
}

MediaSession* HTMLMediaElement::session() const
{
    RefPtr<MediaSession> session = m_session.get();
    if (session && session == &document().defaultMediaSession())
        return nullptr;

    return session.get();
}

void HTMLMediaElement::setSession(MediaSession* session)
{
    // 6.1. Extensions to the HTMLMediaElement interface
    // 1. Let m be the media element in question.
    // 2. Let old media session be m’s current media session, if it has one, and null otherwise.
    // 3. Let m’s current media session be the new value or the top-level browsing context’s media session if the new value is null.
    // 4. Let new media session be m’s current media session.

    // 5. Update media sessions: If old media session and new media session are the same (whether both null or both the same media session), then terminate these steps.
    if (m_session.get() == session)
        return;

    if (m_session) {
        // 6. If m is an audio-producing participant of old media session, then pause m and remove m from old media session’s list of audio-producing participants.
        if (m_session->isMediaElementActive(*this))
            pause();

        m_session->removeMediaElement(*this);

        // 7. If old media session is not null and no longer has one or more audio-producing participants, then run the media session deactivation algorithm for old media session.
        if (!m_session->hasActiveMediaElements())
            m_session->deactivate();
    }

    if (session)
        setSessionInternal(*session);
    else
        setSessionInternal(document().defaultMediaSession());
}

void HTMLMediaElement::setSessionInternal(MediaSession& session)
{
    m_session = &session;
    session.addMediaElement(*this);
    m_kind = session.kind();
}

void HTMLMediaElement::setShouldDuck(bool duck)
{
    if (m_shouldDuck == duck)
        return;

    m_shouldDuck = duck;
    updateVolume();
}

#endif

void HTMLMediaElement::allowsMediaDocumentInlinePlaybackChanged()
{
    if (potentiallyPlaying() && m_mediaSession->requiresFullscreenForVideoPlayback() && !isFullscreen())
        enterFullscreen();
}

bool HTMLMediaElement::isVideoTooSmallForInlinePlayback()
{
    auto* renderer = this->renderer();

    if (!renderer || !is<RenderVideo>(*renderer))
        return true;

    IntRect videoBox = downcast<RenderVideo>(*renderer).videoBox();
    return (videoBox.width() <= 1 || videoBox.height() <= 1);
}

void HTMLMediaElement::isVisibleInViewportChanged()
{
    m_visibilityChangeTaskQueue.enqueueTask([this] {
        m_mediaSession->isVisibleInViewportChanged();
        updateShouldAutoplay();
        schedulePlaybackControlsManagerUpdate();
    });
}

void HTMLMediaElement::updateShouldAutoplay()
{
    if (!autoplay())
        return;

    if (!m_mediaSession->hasBehaviorRestriction(MediaElementSession::InvisibleAutoplayNotPermitted))
        return;

    bool canAutoplay = mediaSession().autoplayPermitted();
    if (canAutoplay
        && m_mediaSession->state() == PlatformMediaSession::Interrupted
        && m_mediaSession->interruptionType() == PlatformMediaSession::InvisibleAutoplay)
        m_mediaSession->endInterruption(PlatformMediaSession::MayResumePlaying);
    else if (!canAutoplay
        && m_mediaSession->state() != PlatformMediaSession::Interrupted)
        m_mediaSession->beginInterruption(PlatformMediaSession::InvisibleAutoplay);
}

void HTMLMediaElement::updateShouldPlay()
{
    if (!paused() && !m_mediaSession->playbackPermitted()) {
        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    } else if (canTransitionFromAutoplayToPlay())
        play();
}

void HTMLMediaElement::resetPlaybackSessionState()
{
    if (m_mediaSession)
        m_mediaSession->resetPlaybackSessionState();
}

bool HTMLMediaElement::isVisibleInViewport() const
{
    auto renderer = this->renderer();
    return renderer && renderer->visibleInViewportState() == VisibleInViewportState::Yes;
}

void HTMLMediaElement::schedulePlaybackControlsManagerUpdate()
{
    Page* page = document().page();
    if (!page)
        return;
    page->schedulePlaybackControlsManagerUpdate();
}

void HTMLMediaElement::playbackControlsManagerBehaviorRestrictionsTimerFired()
{
    if (m_playbackControlsManagerBehaviorRestrictionsQueue.hasPendingTask())
        return;

    if (!m_mediaSession->hasBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager))
        return;

    RefPtr<HTMLMediaElement> protectedThis(this);
    m_playbackControlsManagerBehaviorRestrictionsQueue.scheduleTask([protectedThis] () {
        MediaElementSession* mediaElementSession = protectedThis->m_mediaSession.get();
        if (protectedThis->isPlaying() || mediaElementSession->state() == PlatformMediaSession::Autoplaying || mediaElementSession->state() == PlatformMediaSession::Playing)
            return;

        mediaElementSession->addBehaviorRestriction(MediaElementSession::RequirePlaybackToControlControlsManager);
        protectedThis->schedulePlaybackControlsManagerUpdate();
    });
}

bool HTMLMediaElement::shouldOverrideBackgroundLoadingRestriction() const
{
    if (isPlayingToExternalTarget())
        return true;

    return m_videoFullscreenMode == VideoFullscreenModePictureInPicture;
}

void HTMLMediaElement::fullscreenModeChanged(VideoFullscreenMode mode)
{
    if (m_videoFullscreenMode == mode)
        return;

    m_videoFullscreenMode = mode;
    visibilityStateChanged();
    schedulePlaybackControlsManagerUpdate();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& HTMLMediaElement::logChannel() const
{
    return LogMedia;
}
#endif

bool HTMLMediaElement::willLog(WTFLogLevel level) const
{
#if !RELEASE_LOG_DISABLED
    return m_logger->willLog(logChannel(), level);
#else
    UNUSED_PARAM(level);
    return false;
#endif
}

void HTMLMediaElement::applicationWillResignActive()
{
    if (m_player)
        m_player->applicationWillResignActive();
}

void HTMLMediaElement::applicationDidBecomeActive()
{
    if (m_player)
        m_player->applicationDidBecomeActive();
}

void HTMLMediaElement::setInActiveDocument(bool inActiveDocument)
{
    if (inActiveDocument == m_inActiveDocument)
        return;

    m_inActiveDocument = inActiveDocument;
    m_mediaSession->inActiveDocumentChanged();
}

}

#endif
