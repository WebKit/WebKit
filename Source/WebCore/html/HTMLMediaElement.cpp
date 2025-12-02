/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2016 Google Inc. All rights reserved.
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

#include "Attribute.h"
#include "AudioTrackConfiguration.h"
#include "AudioTrackList.h"
#include "AudioTrackPrivate.h"
#include "Blob.h"
#include "BlobURL.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CodecUtilities.h"
#include "CommonAtomStrings.h"
#include "CommonVM.h"
#include "ContainerNodeInlines.h"
#include "ContentRuleListResults.h"
#include "ContentSecurityPolicy.h"
#include "ContentType.h"
#include "ContextDestructionObserverInlines.h"
#include "CookieJar.h"
#include "DNS.h"
#include "DeprecatedGlobalSettings.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "DiagnosticLoggingResultType.h"
#include "DocumentEventLoop.h"
#include "DocumentFullscreen.h"
#include "DocumentLoader.h"
#include "DocumentMediaElement.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "DocumentResourceLoader.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentView.h"
#include "ElementChildIteratorInlines.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "FourCC.h"
#include "FrameLoader.h"
#include "FrameMemoryMonitor.h"
#include "HTMLAudioElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLSourceElement.h"
#include "HTMLTrackElement.h"
#include "HTMLVideoElement.h"
#include "HostingContext.h"
#include "ImageOverlay.h"
#include "InbandGenericTextTrack.h"
#include "InbandTextTrackPrivate.h"
#include "InbandWebVTTTextTrack.h"
#include "InspectorInstrumentation.h"
#include "JSDOMException.h"
#include "JSDOMPromiseDeferred.h"
#include "JSHTMLMediaElement.h"
#include "JSMediaControlsHost.h"
#include "LoadableTextTrack.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaController.h"
#include "MediaControlsHost.h"
#include "MediaDevices.h"
#include "MediaDocument.h"
#include "MediaError.h"
#include "MediaFragmentURIParser.h"
#include "MediaList.h"
#include "MediaPlayer.h"
#include "MediaQueryEvaluator.h"
#include "MediaResourceLoader.h"
#include "MediaResourceSniffer.h"
#include "MessageClientForTesting.h"
#include "NavigatorMediaDevices.h"
#include "NetworkingContext.h"
#include "NodeInlines.h"
#include "NodeName.h"
#include "NowPlayingInfo.h"
#include "OriginAccessPatterns.h"
#include "PODIntervalTree.h"
#include "PageGroup.h"
#include "PageInlines.h"
#include "PictureInPictureSupport.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformTextTrack.h"
#include "ProgressTracker.h"
#include "PseudoClassChangeInvalidation.h"
#include "RegistrableDomain.h"
#include "RenderBoxInlines.h"
#include "RenderMediaInlines.h"
#include "RenderTheme.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "ResourceLoadInfo.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ScriptExecutionContextInlines.h"
#include "ScriptSourceCode.h"
#include "SecurityOriginData.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SleepDisabler.h"
#include "SpatialVideoMetadata.h"
#include "SpeechSynthesis.h"
#include "TextTrackCueList.h"
#include "TextTrackList.h"
#include "TextTrackRepresentation.h"
#include "ThreadableBlobRegistry.h"
#include "TimeRanges.h"
#include "UserContentController.h"
#include "UserGestureIndicator.h"
#include "VideoPlaybackQuality.h"
#include "VideoTrack.h"
#include "VideoTrackConfiguration.h"
#include "VideoTrackList.h"
#include "VideoTrackPrivate.h"
#include "VisibilityAdjustment.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/Uint8Array.h>
#include <limits>
#include <pal/SessionID.h>
#include <ranges>
#include <wtf/JSONValues.h>
#include <wtf/Language.h>
#include <wtf/MathExtras.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NativePromise.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

#if USE(AUDIO_SESSION)
#include "AudioSession.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioSourceProvider.h"
#include "MediaElementAudioSourceNode.h"
#endif

#if PLATFORM(IOS)
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "VideoPresentationInterfaceIOS.h"
#include <wtf/RuntimeApplicationChecks.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "RemotePlayback.h"
#include "WebKitPlaybackTargetAvailabilityEvent.h"
#endif

#if ENABLE(MEDIA_SOURCE)
#include "MediaSource.h"
#include "MediaSourceInterfaceMainThread.h"
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
#include "MediaSourceHandle.h"
#include "MediaSourceInterfaceWorker.h"
#endif
#include "LocalDOMWindow.h"
#include "SourceBufferList.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "MediaStream.h"
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include "WebKitMediaKeyNeededEvent.h"
#include "WebKitMediaKeys.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "MediaEncryptedEvent.h"
#include "MediaKeys.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "NotImplemented.h"
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
#include "VideoPresentationModel.h"
#endif

#if ENABLE(MEDIA_SESSION)
#include "MediaSession.h"
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#include "MediaSessionCoordinator.h"
#endif

#if RELEASE_LOG_DISABLED
#define HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(thisPtr, formatString, ...)
#else
#define HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(thisPtr, formatString, ...) \
do { \
    if ((thisPtr)->willLog(WTFLogLevel::Always)) { \
        RELEASE_LOG_FORWARDABLE(Media, HTMLMEDIAELEMENT_##formatString, (thisPtr)->logIdentifier(), ##__VA_ARGS__); \
        if ((thisPtr)->logger().hasEnabledInspector()) { \
            char buffer[1024] = { 0 }; \
            SAFE_SPRINTF(std::span { buffer }, MESSAGE_HTMLMEDIAELEMENT_##formatString, (thisPtr)->logIdentifier(), ##__VA_ARGS__); \
            (thisPtr)->logger().toObservers((thisPtr)->logChannel(), WTFLogLevel::Always, String::fromUTF8(buffer)); \
        } \
    } \
} while (0)
#endif

#define HTMLMEDIAELEMENT_RELEASE_LOG(formatString, ...) HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(this, formatString, ##__VA_ARGS__)

namespace WTF {
template <>
struct LogArgument<URL> {
    static String toString(const URL& url)
    {
#if !LOG_DISABLED
        static const unsigned maximumURLLengthForLogging = 512;

        if (url.string().length() < maximumURLLengthForLogging)
            return url.string();
        return makeString(StringView(url.string()).left(maximumURLLengthForLogging), "..."_s);
#else
        UNUSED_PARAM(url);
        return "[url]"_s;
#endif
    }
};
}


namespace WebCore {

typedef PODIntervalTree<MediaTime, TextTrackCue*> TextTrackCueIntervalTree;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLMediaElement);

static const Seconds SeekRepeatDelay { 100_ms };
static const double SeekTime = 0.2;
static const Seconds ScanRepeatDelay { 1.5_s };
static const double ScanMaximumRate = 8;
static const double AutoplayInterferenceTimeThreshold = 10;
static const Seconds hideMediaControlsAfterEndedDelay { 6_s };
static const Seconds WatchtimeTimerInterval { 5_min };

#if ENABLE(MEDIA_SOURCE)
// URL protocol used to signal that the media source API is being used.
static constexpr auto mediaSourceBlobProtocol = "blob"_s;
#endif

using namespace HTMLNames;

String convertEnumerationToString(HTMLMediaElement::ReadyState enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 5> values {
        MAKE_STATIC_STRING_IMPL("HAVE_NOTHING"),
        MAKE_STATIC_STRING_IMPL("HAVE_METADATA"),
        MAKE_STATIC_STRING_IMPL("HAVE_CURRENT_DATA"),
        MAKE_STATIC_STRING_IMPL("HAVE_FUTURE_DATA"),
        MAKE_STATIC_STRING_IMPL("HAVE_ENOUGH_DATA"),
    };
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_NOTHING) == 0, "HTMLMediaElementEnums::HAVE_NOTHING is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_METADATA) == 1, "HTMLMediaElementEnums::HAVE_METADATA is not 1 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_CURRENT_DATA) == 2, "HTMLMediaElementEnums::HAVE_CURRENT_DATA is not 2 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_FUTURE_DATA) == 3, "HTMLMediaElementEnums::HAVE_FUTURE_DATA is not 3 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::HAVE_ENOUGH_DATA) == 4, "HTMLMediaElementEnums::HAVE_ENOUGH_DATA is not 4 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(HTMLMediaElement::NetworkState enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 4> values {
        MAKE_STATIC_STRING_IMPL("NETWORK_EMPTY"),
        MAKE_STATIC_STRING_IMPL("NETWORK_IDLE"),
        MAKE_STATIC_STRING_IMPL("NETWORK_LOADING"),
        MAKE_STATIC_STRING_IMPL("NETWORK_NO_SOURCE"),
    };
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::NETWORK_EMPTY) == 0, "HTMLMediaElementEnums::NETWORK_EMPTY is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::NETWORK_IDLE) == 1, "HTMLMediaElementEnums::NETWORK_IDLE is not 1 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::NETWORK_LOADING) == 2, "HTMLMediaElementEnums::NETWORK_LOADING is not 2 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElementEnums::NETWORK_NO_SOURCE) == 3, "HTMLMediaElementEnums::NETWORK_NO_SOURCE is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(HTMLMediaElement::AutoplayEventPlaybackState enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 4> values {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("PreventedAutoplay"),
        MAKE_STATIC_STRING_IMPL("StartedWithUserGesture"),
        MAKE_STATIC_STRING_IMPL("StartedWithoutUserGesture"),
    };
    static_assert(static_cast<size_t>(HTMLMediaElement::AutoplayEventPlaybackState::None) == 0, "AutoplayEventPlaybackState::None is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::AutoplayEventPlaybackState::PreventedAutoplay) == 1, "AutoplayEventPlaybackState::PreventedAutoplay is not 1 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::AutoplayEventPlaybackState::StartedWithUserGesture) == 2, "AutoplayEventPlaybackState::StartedWithUserGesture is not 2 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::AutoplayEventPlaybackState::StartedWithoutUserGesture) == 3, "AutoplayEventPlaybackState::StartedWithoutUserGesture is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(HTMLMediaElement::TextTrackVisibilityCheckType enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 2> values {
        MAKE_STATIC_STRING_IMPL("CheckTextTrackVisibility"),
        MAKE_STATIC_STRING_IMPL("AssumeTextTrackVisibilityChanged"),
    };
    static_assert(static_cast<size_t>(HTMLMediaElement::TextTrackVisibilityCheckType::CheckTextTrackVisibility) == 0, "TextTrackVisibilityCheckType::CheckTextTrackVisibility is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::TextTrackVisibilityCheckType::AssumeTextTrackVisibilityChanged) == 1, "TextTrackVisibilityCheckType::AssumeTextTrackVisibilityChanged is not 1 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(HTMLMediaElement::SpeechSynthesisState enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 4> values {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("Speaking"),
        MAKE_STATIC_STRING_IMPL("CompletingExtendedDescription"),
        MAKE_STATIC_STRING_IMPL("Paused"),
    };
    static_assert(static_cast<size_t>(HTMLMediaElement::SpeechSynthesisState::None) == 0, "SpeechSynthesisState::None is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::SpeechSynthesisState::Speaking) == 1, "SpeechSynthesisState::Speaking is not 1 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::SpeechSynthesisState::CompletingExtendedDescription) == 2, "SpeechSynthesisState::CompletingExtendedDescription is not 2 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::SpeechSynthesisState::Paused) == 3, "SpeechSynthesisState::Paused is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(HTMLMediaElement::ControlsState enumerationValue)
{
    // None, Initializing, Ready, PartiallyDeinitialized
    static const std::array<NeverDestroyed<String>, 4> values {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("Initializing"),
        MAKE_STATIC_STRING_IMPL("Ready"),
        MAKE_STATIC_STRING_IMPL("PartiallyDeinitialized"),
    };
    static_assert(!static_cast<size_t>(HTMLMediaElement::ControlsState::None), "HTMLMediaElement::None is not 0 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::ControlsState::Initializing) == 1, "HTMLMediaElement::Initializing is not 1 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::ControlsState::Ready) == 2, "HTMLMediaElement::Ready is not 2 as expected");
    static_assert(static_cast<size_t>(HTMLMediaElement::ControlsState::PartiallyDeinitialized) == 3, "HTMLMediaElement::PartiallyDeinitialized is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

class TrackDisplayUpdateScope {
public:
    TrackDisplayUpdateScope(HTMLMediaElement& element)
        : m_element(element)
    {
        element.beginIgnoringTrackDisplayUpdateRequests();
    }
    ~TrackDisplayUpdateScope()
    {
        if (RefPtr element = m_element.ptr())
            element->endIgnoringTrackDisplayUpdateRequests();
    }

private:
    WeakRef<HTMLMediaElement> m_element;
};

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

HashSet<WeakRef<HTMLMediaElement>>& HTMLMediaElement::allMediaElements()
{
    static NeverDestroyed<HashSet<WeakRef<HTMLMediaElement>>> elements;
    return elements;
}

struct MediaElementSessionInfo {
    WeakPtr<const MediaElementSession> session;
    MediaElementSession::PlaybackControlsPurpose purpose;

    Markable<MonotonicTime> timeOfLastUserInteraction;
    bool canShowControlsManager : 1;
    bool isVisibleInViewportOrFullscreen : 1;
    bool isLargeEnoughForMainContent : 1;
    bool isLongEnoughForMainContent : 1;
    bool isPlayingAudio : 1;
    bool hasEverNotifiedAboutPlaying : 1;
};

static MediaElementSessionInfo mediaElementSessionInfoForSession(const MediaElementSession& session, MediaElementSession::PlaybackControlsPurpose purpose)
{
    if (RefPtr element = session.element().get()) {
        return {
            &session,
            purpose,
            session.mostRecentUserInteractionTime(),
            session.canShowControlsManager(purpose),
            element->isFullscreen() || element->isVisibleInViewport(),
            session.isLargeEnoughForMainContent(MediaSessionMainContentPurpose::MediaControls),
            session.isLongEnoughForMainContent(),
            element->isPlaying() && element->hasAudio() && !element->muted(),
            element->hasEverNotifiedAboutPlaying()
        };
    }
    return { };
}

static bool preferMediaControlsForCandidateSessionOverOtherCandidateSession(const MediaElementSessionInfo& session, const MediaElementSessionInfo& otherSession)
{
    MediaElementSession::PlaybackControlsPurpose purpose = session.purpose;
    ASSERT(purpose == otherSession.purpose);

    // For the controls manager and MediaSession, prioritize visible media over offscreen media.
    if ((purpose == MediaElementSession::PlaybackControlsPurpose::ControlsManager || purpose == MediaElementSession::PlaybackControlsPurpose::MediaSession)
        && session.isVisibleInViewportOrFullscreen != otherSession.isVisibleInViewportOrFullscreen)
        return session.isVisibleInViewportOrFullscreen;

    // For Now Playing and MediaSession, prioritize elements that would normally satisfy main content.
    if ((purpose == MediaElementSession::PlaybackControlsPurpose::NowPlaying || purpose == MediaElementSession::PlaybackControlsPurpose::MediaSession)
        && session.isLargeEnoughForMainContent != otherSession.isLargeEnoughForMainContent)
        return session.isLargeEnoughForMainContent;

    // For MediaSession, prioritize elements that have been played before.
    if (purpose == MediaElementSession::PlaybackControlsPurpose::MediaSession
        && session.hasEverNotifiedAboutPlaying != otherSession.hasEverNotifiedAboutPlaying)
        return session.hasEverNotifiedAboutPlaying;

    // As a tiebreaker, prioritize elements that the user recently interacted with.
    return session.timeOfLastUserInteraction.value_or(MonotonicTime { }) > otherSession.timeOfLastUserInteraction.value_or(MonotonicTime { });
}

static bool mediaSessionMayBeConfusedWithMainContent(const MediaElementSessionInfo& session, MediaElementSession::PlaybackControlsPurpose purpose)
{
    if (purpose == MediaElementSession::PlaybackControlsPurpose::MediaSession)
        return false;

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

static bool defaultVolumeLocked()
{
#if PLATFORM(IOS)
    return PAL::currentUserInterfaceIdiomIsSmallScreen();
#else
    return false;
#endif
}

static bool isInWindowOrStandardFullscreen(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    return mode == HTMLMediaElementEnums::VideoFullscreenModeStandard || mode == HTMLMediaElementEnums::VideoFullscreenModeInWindow;
}

struct HTMLMediaElement::CueData {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(HTMLMediaElement);
    TextTrackCueIntervalTree cueTree;
    CueList currentlyActiveCues;
};

class PausableIntervalTimer final : public TimerBase {
    WTF_MAKE_TZONE_ALLOCATED(PausableIntervalTimer);
public:
    PausableIntervalTimer(Seconds interval, Function<void()>&& function)
        : m_interval { interval }
        , m_function { WTFMove(function) }
        , m_remainingInterval { interval }
    {
    }

    void start()
    {
        m_startTime = MonotonicTime::now();
        TimerBase::start(m_remainingInterval, m_interval);
    }

    void stop()
    {
        m_remainingInterval = m_interval;
        TimerBase::stop();
    }

    void pause()
    {
        if (!isActive())
            return;

        auto partialInterval = MonotonicTime::now() - m_startTime;
        m_remainingInterval -= partialInterval;
        if (m_remainingInterval <= 0_s)
            m_remainingInterval = m_interval;
        TimerBase::stop();
    }

    Seconds secondsRemaining() const
    {
        if (!isActive())
            return m_remainingInterval;

        auto partialInterval = MonotonicTime::now() - m_startTime;
        return std::max(0_s, m_remainingInterval - partialInterval);
    }

    Seconds secondsCompleted() const
    {
        return m_interval - secondsRemaining();
    }

    void fireAndRestart()
    {
        bool wasActive = isActive();

        if (wasActive)
            pause();
        m_function();
        m_remainingInterval = m_interval;
        if (wasActive)
            start();
    }

private:
    void start(Seconds, Seconds) = delete;
    void startRepeating(Seconds) = delete;
    void startOneShot(Seconds) = delete;

    void fired() final
    {
        m_remainingInterval = 0_s;
        m_function();
        m_remainingInterval = m_interval;
    }

    Seconds m_interval;
    Function<void()> m_function;

    Seconds m_remainingInterval;
    MonotonicTime m_startTime;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(PausableIntervalTimer);

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLElement(tagName, document, { TypeFlag::HasCustomStyleResolveCallbacks, TypeFlag::HasDidMoveToNewDocument })
    , ActiveDOMObject(document)
    , m_progressEventTimer(*this, &HTMLMediaElement::progressEventTimerFired)
    , m_playbackProgressTimer(*this, &HTMLMediaElement::playbackProgressTimerFired)
    , m_scanTimer(*this, &HTMLMediaElement::scanTimerFired)
    , m_playbackControlsManagerBehaviorRestrictionsTimer(*this, &HTMLMediaElement::playbackControlsManagerBehaviorRestrictionsTimerFired)
    , m_seekToPlaybackPositionEndedTimer(*this, &HTMLMediaElement::seekToPlaybackPositionEndedTimerFired)
    , m_checkPlaybackTargetCompatibilityTimer(*this, &HTMLMediaElement::checkPlaybackTargetCompatibility)
    , m_currentIdentifier(MediaUniqueIdentifier::generate())
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
    , m_paused(true)
    , m_seeking(false)
    , m_buffering(false)
    , m_stalled(false)
    , m_seekRequested(false)
    , m_wasPlayingBeforeSeeking(false)
    , m_sentStalledEvent(false)
    , m_sentEndEvent(false)
    , m_pausedInternal(false)
    , m_closedCaptionsVisible(false)
    , m_completelyLoaded(false)
    , m_havePreparedToPlay(false)
    , m_parsingInProgress(createdByParser)
    , m_elementIsHidden(document.hidden())
    , m_receivedLayoutSizeChanged(false)
    , m_hasEverNotifiedAboutPlaying(false)
    , m_hasEverHadAudio(false)
    , m_hasEverHadVideo(false)
    , m_mediaControlsDependOnPageScaleFactor(false)
    , m_isScrubbingRemotely(false)
    , m_waitingToEnterFullscreen(false)
    , m_changingVideoFullscreenMode(false)
    , m_showPoster(true)
    , m_tracksAreReady(true)
    , m_haveVisibleTextTrack(false)
    , m_processingPreferenceChange(false)
    , m_volumeLocked(defaultVolumeLocked())
    , m_opaqueRootProvider(WTF::Observer<WebCoreOpaqueRoot()>::create([weakThis = WeakPtr { *this }] {
        // This gets called on the GC thread so we cannot ref `this`.
        return weakThis->opaqueRoot();
    }))
#if USE(AUDIO_SESSION)
    , m_categoryAtMostRecentPlayback(AudioSessionCategory::None)
    , m_modeAtMostRecentPlayback(AudioSessionMode::Default)
#endif
#if HAVE(SPATIAL_TRACKING_LABEL)
    , m_defaultSpatialTrackingLabelChangedObserver(DefaultSpatialTrackingLabelChangedObserver::create([weakThis = WeakPtr { *this }] (const String& defaultSpatialTrackingLabel) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->defaultSpatialTrackingLabelChanged(defaultSpatialTrackingLabel);
    }))
#endif
#if !RELEASE_LOG_DISABLED
    , m_logger(document.logger())
    , m_logIdentifier(uniqueLogIdentifier())
#endif
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    , m_remote(RemotePlayback::create(*this))
#endif
{
    RefPtr page = document.page();
    m_shouldAudioPlaybackRequireUserGesture = page && page->requiresUserGestureForAudioPlayback() && !processingUserGestureForMedia();
    m_shouldVideoPlaybackRequireUserGesture = page && page->requiresUserGestureForVideoPlayback() && !processingUserGestureForMedia();

    allMediaElements().add(*this);

    HTMLMEDIAELEMENT_RELEASE_LOG(CONSTRUCTOR);

    InspectorInstrumentation::addEventListenersToNode(*this);
}

void HTMLMediaElement::invalidateMediaSession()
{
    RefPtr mediaSession = m_mediaSession;
    if (!mediaSession)
        return;

    mediaSession->unregisterWithDocument(protectedDocument());
    mediaSession->invalidateClient();
    m_mediaSession = nullptr;
}

void HTMLMediaElement::initializeMediaSession()
{
    ASSERT(!m_mediaSession);
    Ref mediaSession = MediaElementSession::create(*this);
    m_mediaSession = mediaSession.copyRef();
    mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForFullscreen);
    mediaSession->addBehaviorRestriction(MediaElementSession::RequirePageConsentToLoadMedia);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureToAutoplayToExternalDevice);
#endif
    mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager);
    mediaSession->addBehaviorRestriction(MediaElementSession::RequirePlaybackToControlControlsManager);

    Ref document = this->document();
    RefPtr page = document->page();

    if (document->settings().invisibleAutoplayNotPermitted())
        mediaSession->addBehaviorRestriction(MediaElementSession::InvisibleAutoplayNotPermitted);

    if (document->settings().requiresPageVisibilityToPlayAudio())
        mediaSession->addBehaviorRestriction(MediaElementSession::RequirePageVisibilityToPlayAudio);

    if (document->ownerElement() || !document->isMediaDocument()) {
        if (m_shouldVideoPlaybackRequireUserGesture) {
            mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoRateChange);
            if (document->settings().requiresUserGestureToLoadVideo())
                mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForLoad);
        }

        if (page && page->isLowPowerModeEnabled())
            mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoDueToLowPowerMode);

        if (page && page->isAggressiveThermalMitigationEnabled())
            mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoDueToAggressiveThermalMitigation);

        if (m_shouldAudioPlaybackRequireUserGesture)
            mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForAudioRateChange);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (m_shouldVideoPlaybackRequireUserGesture || m_shouldAudioPlaybackRequireUserGesture)
            mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureToShowPlaybackTargetPicker);
#endif

        if (!document->mediaDataLoadsAutomatically() && !document->quirks().needsPreloadAutoQuirk())
            mediaSession->addBehaviorRestriction(MediaElementSession::AutoPreloadingNotPermitted);

        if (document->settings().mainContentUserGestureOverrideEnabled())
            mediaSession->addBehaviorRestriction(MediaElementSession::OverrideUserGestureRequirementForMainContent);
    }

#if PLATFORM(IOS_FAMILY)
    if (!document->requiresUserGestureForVideoPlayback() && !document->requiresUserGestureForAudioPlayback()) {
        // Relax RequireUserGestureForFullscreen when requiresUserGestureForVideoPlayback and requiresUserGestureForAudioPlayback is not set:
        mediaSession->removeBehaviorRestriction(MediaElementSession::RequireUserGestureForFullscreen);
    }
#endif

#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
    if (document->settings().requiresPageVisibilityForVideoToBeNowPlaying())
        mediaSession->addBehaviorRestriction(MediaElementSession::RequirePageVisibilityForVideoToBeNowPlaying);
#endif

    registerWithDocument(document);

#if USE(AUDIO_SESSION)
    AudioSession::singleton().addConfigurationChangeObserver(*this);
#endif

    mediaSession->clientWillBeginAutoplaying();
}

HTMLMediaElement::~HTMLMediaElement()
{
    HTMLMEDIAELEMENT_RELEASE_LOG(DESTRUCTOR);

    invalidateWatchtimeTimer();
    invalidateBufferingStopwatch();

    beginIgnoringTrackDisplayUpdateRequests();

    if (m_textTracks) {
        for (unsigned i = 0; i < m_textTracks->length(); ++i) {
            RefPtr track = m_textTracks->item(i);
            track->clearClient(*this);
        }
    }

    if (m_audioTracks) {
        for (unsigned i = 0; i < m_audioTracks->length(); ++i) {
            RefPtr track = m_audioTracks->item(i);
            track->clearClient(*this);
        }
    }

    if (m_videoTracks) {
        for (unsigned i = 0; i < m_videoTracks->length(); ++i) {
            RefPtr track = m_videoTracks->item(i);
            track->clearClient(*this);
        }
    }

    allMediaElements().remove(*this);

    setShouldDelayLoadEvent(false);

#if USE(AUDIO_SESSION)
    AudioSession::singleton().removeConfigurationChangeObserver(*this);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (hasTargetAvailabilityListeners()) {
        m_hasPlaybackTargetAvailabilityListeners = false;
        if (RefPtr mediaSession = m_mediaSession)
            mediaSession->setHasPlaybackTargetAvailabilityListeners(false);
        updateMediaState();
    }
#endif

    if (RefPtr mediaController = m_mediaController) {
        mediaController->removeMediaElement(*this);
        m_mediaController = nullptr;
    }

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = std::exchange(m_mediaSource, { }))
        mediaSource->elementIsShuttingDown();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    webkitSetMediaKeys(nullptr);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    if (RefPtr mediaKeys = m_mediaKeys) {
        mediaKeys->detachCDMClient(*this);
        if (RefPtr player = m_player)
            player->cdmInstanceDetached(mediaKeys->cdmInstance());
    }
#endif

    m_completelyLoaded = true;

    cancelSniffer();

    if (RefPtr player = m_player) {
        player->invalidate();
        m_player = nullptr;
    }

    schedulePlaybackControlsManagerUpdate();

    invalidateMediaSession();
    unregisterWithDocument(Ref<Document> { document() });
}

std::optional<MediaPlayerIdentifier> HTMLMediaElement::playerIdentifier() const
{
    RefPtr player = m_player;
    return player ? std::optional { player->identifier() } : std::nullopt;
}

bool HTMLMediaElement::isNowPlayingEligible() const
{
    RefPtr page = document().page();
    if (page && page->mediaPlaybackIsSuspended())
        return false;

    // FIXME: should this use protectedMediaSession()?
    return Ref { *m_mediaSession }->hasNowPlayingInfo();
}

std::optional<NowPlayingInfo> HTMLMediaElement::nowPlayingInfo() const
{
    // FIXME: should this use protectedMediaSession()?
    return Ref { *m_mediaSession }->computeNowPlayingInfo();
}

WeakPtr<PlatformMediaSessionInterface> HTMLMediaElement::selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSessionInterface>>& sessions, PlatformMediaSession::PlaybackControlsPurpose purpose)
{
    if (!sessions.size())
        return nullptr;

    Vector<MediaElementSessionInfo> candidateSessions;
    bool atLeastOneNonCandidateMayBeConfusedForMainContent = false;
    for (auto& session : sessions) {
        auto mediaElementSessionInfo = mediaElementSessionInfoForSession(*downcast<MediaElementSession>(session.get()), purpose);
        if (mediaElementSessionInfo.canShowControlsManager)
            candidateSessions.append(mediaElementSessionInfo);
        else if (mediaSessionMayBeConfusedWithMainContent(mediaElementSessionInfo, purpose))
            atLeastOneNonCandidateMayBeConfusedForMainContent = true;
    }

    if (!candidateSessions.size())
        return nullptr;

    std::ranges::sort(candidateSessions, preferMediaControlsForCandidateSessionOverOtherCandidateSession);
    auto strongestSessionCandidate = candidateSessions.first();
    if (!strongestSessionCandidate.isVisibleInViewportOrFullscreen && !strongestSessionCandidate.isPlayingAudio && atLeastOneNonCandidateMayBeConfusedForMainContent)
        return nullptr;

    return strongestSessionCandidate.session.get();
}

void HTMLMediaElement::registerWithDocument(Document& document)
{
    document.registerMediaElement(*this);

    protectedMediaSession()->registerWithDocument(document);

    if (m_isWaitingUntilMediaCanStart)
        document.addMediaCanStartListener(*this);

    document.registerForVisibilityStateChangedCallbacks(*this);

    if (m_requireCaptionPreferencesChangedCallbacks)
        document.registerForCaptionPreferencesChangedCallbacks(*this);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    document.registerForDocumentSuspensionCallbacks(*this);
#endif

    document.addAudioProducer(*this);
}

void HTMLMediaElement::unregisterWithDocument(Document& document)
{
    document.unregisterMediaElement(*this);

#if ENABLE(SPEECH_SYNTHESIS)
    if (RefPtr speechSynthesis = m_speechSynthesis) {
        speechSynthesis->cancel();
        m_speechSynthesis = nullptr;
    }
#endif

    invalidateMediaSession();

    if (m_isWaitingUntilMediaCanStart)
        document.removeMediaCanStartListener(*this);

    document.unregisterForVisibilityStateChangedCallbacks(*this);

    if (m_requireCaptionPreferencesChangedCallbacks)
        document.unregisterForCaptionPreferencesChangedCallbacks(*this);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    document.unregisterForDocumentSuspensionCallbacks(*this);
#endif

    document.removeAudioProducer(*this);
}

void HTMLMediaElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ActiveDOMObject::didMoveToNewDocument(newDocument);
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);
    if (m_shouldDelayLoadEvent) {
        oldDocument.decrementLoadEventDelayCount();
        newDocument.incrementLoadEventDelayCount();
    }

    if (m_audioTracks)
        m_audioTracks->didMoveToNewDocument(newDocument);
    if (m_textTracks)
        m_textTracks->didMoveToNewDocument(newDocument);
    if (m_videoTracks)
        m_videoTracks->didMoveToNewDocument(newDocument);

    unregisterWithDocument(oldDocument);
    registerWithDocument(newDocument);

    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    updateShouldAutoplay();
    visibilityStateChanged();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void HTMLMediaElement::prepareForDocumentSuspension()
{
    protectedMediaSession()->unregisterWithDocument(protectedDocument());
}

void HTMLMediaElement::resumeFromDocumentSuspension()
{
    protectedMediaSession()->registerWithDocument(protectedDocument());
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

bool HTMLMediaElement::isInteractiveContent() const
{
    return controls();
}

void HTMLMediaElement::removeAllEventListeners()
{
    Element::removeAllEventListeners();

    if (m_audioTracks)
        m_audioTracks->removeAllEventListeners();

    if (m_textTracks)
        m_textTracks->removeAllEventListeners();

    if (m_videoTracks)
        m_videoTracks->removeAllEventListeners();
}

void HTMLMediaElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::idAttr:
        m_id = newValue;
        if (RefPtr player = m_player)
            player->elementIdChanged(m_id);
        break;
    case AttributeNames::srcAttr:
        // https://html.spec.whatwg.org/multipage/embedded-content.html#location-of-the-media-resource
        // Location of the Media Resource
        // 12 February 2017

        // If a src attribute of a media element is set or changed, the user
        // agent must invoke the media element's media element load algorithm.
        if (!newValue.isNull())
            prepareForLoad();
        return;
    case AttributeNames::controlsAttr:
        configureMediaControls();
        return;
    case AttributeNames::loopAttr:
        updateSleepDisabling();
        if (RefPtr player = m_player)
            player->isLoopingChanged();
        return;
    case AttributeNames::preloadAttr:
        if (equalLettersIgnoringASCIICase(newValue, "none"_s))
            m_preload = MediaPlayer::Preload::None;
        else if (equalLettersIgnoringASCIICase(newValue, "metadata"_s))
            m_preload = MediaPlayer::Preload::MetaData;
        else {
            // The spec does not define an "invalid value default" but "auto" is suggested as the
            // "missing value default", so use it for everything except "none" and "metadata"
            m_preload = MediaPlayer::Preload::Auto;
        }
        maybeUpdatePlayerPreload();
        return;
    case AttributeNames::mediagroupAttr:
        setMediaGroup(newValue);
        return;
    case AttributeNames::autoplayAttr:
        if (processingUserGestureForMedia())
            removeBehaviorRestrictionsAfterFirstUserGesture();
        return;
    case AttributeNames::titleAttr:
        if (RefPtr mediaSession = m_mediaSession)
            mediaSession->clientCharacteristicsChanged(false);
        return;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    case AttributeNames::webkitwirelessvideoplaybackdisabledAttr:
        protectedMediaSession()->setWirelessVideoPlaybackDisabled(newValue != nullAtom());
        [[fallthrough]];
    case AttributeNames::disableremoteplaybackAttr:
    case AttributeNames::webkitairplayAttr:
        isWirelessPlaybackTargetDisabledChanged();
#if ENABLE(MEDIA_SOURCE)
        if (RefPtr mediaSource = m_mediaSource; mediaSource && isWirelessPlaybackTargetDisabled())
            mediaSource->openIfDeferredOpen();
#endif
        break;
#endif
    default:
        break;
    }
    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void HTMLMediaElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    m_parsingInProgress = false;

    if (childrenOfType<HTMLTrackElement>(*this).first())
        scheduleConfigureTextTracks();
}

bool HTMLMediaElement::rendererIsNeeded(const RenderStyle& style)
{
    return controls() && HTMLElement::rendererIsNeeded(style);
}

RenderPtr<RenderElement> HTMLMediaElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderMedia>(RenderObject::Type::Media, *this, WTFMove(style));
}

bool HTMLMediaElement::childShouldCreateRenderer(const Node& child) const
{
    return hasShadowRootParent(child) && HTMLElement::childShouldCreateRenderer(child);
}

Node::InsertedIntoAncestorResult HTMLMediaElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLMEDIAELEMENT_RELEASE_LOG(INSERTEDINTOANCESTOR);

    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        setInActiveDocument(true);

    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLMediaElement::didFinishInsertingNode()
{
    Ref protectedThis { *this }; // prepareForLoad may result in a 'beforeload' event, which can make arbitrary DOM mutations.

    HTMLMEDIAELEMENT_RELEASE_LOG(DIDFINISHINSERTINGNODE);

    if (m_inActiveDocument && m_networkState == NETWORK_EMPTY && !attributeWithoutSynchronization(srcAttr).isEmpty())
        prepareForLoad();

    visibilityAdjustmentStateDidChange();

    if (!m_explicitlyMuted) {
        m_explicitlyMuted = true;
        m_muted = hasAttributeWithoutSynchronization(mutedAttr);
        protectedMediaSession()->canProduceAudioChanged();
    }

    configureMediaControls();
}

void HTMLMediaElement::pauseAfterDetachedTask()
{
    // If we were re-inserted into an active document, no need to pause.
    if (m_inActiveDocument)
        return;

    if (m_videoFullscreenMode != VideoFullscreenModePictureInPicture && m_networkState > NETWORK_EMPTY && !m_wasInterruptedForInvisibleAutoplay)
        pause();
    if (m_videoFullscreenMode == VideoFullscreenModeStandard && !protectedDocument()->quirks().needsNowPlayingFullscreenSwapQuirk())
        exitFullscreen();

    if (m_controlsState == ControlsState::Initializing || m_controlsState == ControlsState::Ready) {
        // Call MediaController.deinitialize() to get rid of circular references.
        bool isDeinitialized = DocumentMediaElement::from(protectedDocument()).setupAndCallMediaControlsJS([this](JSDOMGlobalObject& globalObject, JSC::JSGlobalObject& lexicalGlobalObject, ScriptController&, DOMWrapperWorld&) {
            auto& vm = globalObject.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);
            if (!m_mediaControlsHost)
                return false;

            auto controllerValue = m_mediaControlsHost->controllerWrapper().getValue();
            RETURN_IF_EXCEPTION(scope, false);
            auto* controllerObject = controllerValue.toObject(&lexicalGlobalObject);
            RETURN_IF_EXCEPTION(scope, false);

            auto functionValue = controllerObject->get(&lexicalGlobalObject, JSC::Identifier::fromString(vm, "deinitialize"_s));
            if (scope.exception()) [[unlikely]]
                return false;
            if (functionValue.isUndefinedOrNull())
                return false;

            auto* function = functionValue.toObject(&lexicalGlobalObject);
            RETURN_IF_EXCEPTION(scope, false);

            auto callData = JSC::getCallData(function);
            if (callData.type == JSC::CallData::Type::None)
                return false;

            auto resultValue = JSC::call(&lexicalGlobalObject, function, callData, controllerObject, JSC::MarkedArgumentBuffer());
            RETURN_IF_EXCEPTION(scope, false);

            return resultValue.toBoolean(&lexicalGlobalObject);
        });
        m_controlsState = isDeinitialized ? ControlsState::PartiallyDeinitialized : m_controlsState;
    }

    RefPtr player = m_player;
    if (!player)
        return;

    size_t extraMemoryCost = player->extraMemoryCost();
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
    HTMLMEDIAELEMENT_RELEASE_LOG(REMOVEDFROMANCESTOR);

    setInActiveDocument(false);
    if (removalType.disconnectedFromDocument) {
        // Pause asynchronously to let the operation that removed us finish, in case we get inserted back into a document.
        queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& element) {
            if (!element.isContextStopped())
                element.pauseAfterDetachedTask();
        });
    }

    if (RefPtr mediaSession = m_mediaSession)
        mediaSession->clientCharacteristicsChanged(false);

    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    visibilityAdjustmentStateDidChange();
}

void HTMLMediaElement::willAttachRenderers()
{
    ASSERT(!renderer());
    fireAndRestartWatchtimeTimer();
}

inline void HTMLMediaElement::updateRenderer()
{
    if (CheckedPtr renderer = this->renderer())
        renderer->updateFromElement();

    if (RefPtr mediaControlsHost = m_mediaControlsHost.get())
        mediaControlsHost->updateCaptionDisplaySizes();

    if (RefPtr player = m_player)
        player->playerContentBoxRectChanged(mediaPlayerContentBoxRect());
}

void HTMLMediaElement::didAttachRenderers()
{
    if (CheckedPtr renderer = this->renderer()) {
        renderer->updateFromElement();
        if (RefPtr mediaSession = m_mediaSession; mediaSession && mediaSession->wantsToObserveViewportVisibilityForAutoplay())
            renderer->registerForVisibleInViewportCallback();
    }
    scheduleUpdateShouldAutoplay();
}

void HTMLMediaElement::willDetachRenderers()
{
    if (CheckedPtr renderer = this->renderer())
        renderer->unregisterForVisibleInViewportCallback();

    fireAndRestartWatchtimeTimer();
}

void HTMLMediaElement::didDetachRenderers()
{
    scheduleUpdateShouldAutoplay();

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& element) {
        // If we detach a media element from a renderer, we may no longer need the MediaPlayerPrivate
        // to vend a PlatformLayer. However, the renderer may be torn down and re-attached during a
        // single run-loop as a result of layout or due to the element being re-parented.
        element.computeAcceleratedRenderingStateAndUpdateMediaPlayer();
    });
}

void HTMLMediaElement::didRecalcStyle(OptionSet<Style::Change>)
{
    updateRenderer();
}

void HTMLMediaElement::scheduleNextSourceChild()
{
    // Schedule the timer to try the next <source> element WITHOUT resetting state ala prepareForLoad.
    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_resourceSelectionTaskCancellationGroup, [](auto& element) { element.loadNextSourceChild(); });
}

void HTMLMediaElement::mediaPlayerActiveSourceBuffersChanged()
{
    checkForAudioAndVideo();
}

void HTMLMediaElement::scheduleEvent(const AtomString& eventName)
{
    scheduleEvent(Event::create(eventName, Event::CanBubble::No, Event::IsCancelable::Yes));
}

void HTMLMediaElement::scheduleEvent(Ref<Event>&& event)
{
    queueCancellableTaskToDispatchEvent(*this, TaskSource::MediaElement, m_asyncEventsCancellationGroup, WTFMove(event));
}

void HTMLMediaElement::scheduleResolvePendingPlayPromises()
{
    if (m_pendingPlayPromises.isEmpty())
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [pendingPlayPromises = WTFMove(m_pendingPlayPromises)](auto& element) mutable {
        if (!element.isContextStopped())
            element.resolvePendingPlayPromises(WTFMove(pendingPlayPromises));
    });
}

void HTMLMediaElement::scheduleRejectPendingPlayPromises(Ref<DOMException>&& error)
{
    if (m_pendingPlayPromises.isEmpty())
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [error = WTFMove(error), pendingPlayPromises = WTFMove(m_pendingPlayPromises)](auto& element) mutable {
        if (!element.isContextStopped())
            element.rejectPendingPlayPromises(WTFMove(pendingPlayPromises), WTFMove(error));
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
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [pendingPlayPromises = WTFMove(m_pendingPlayPromises)](auto& element) mutable {
        if (!element.isContextStopped())
            element.notifyAboutPlaying(WTFMove(pendingPlayPromises));
    });
}

void HTMLMediaElement::notifyAboutPlaying(PlayPromiseVector&& pendingPlayPromises)
{
    Ref protectedThis { *this }; // The 'playing' event can make arbitrary DOM mutations.
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

void HTMLMediaElement::checkPlaybackTargetCompatibility()
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (!m_isPlayingToWirelessTarget)
        return;

    Ref player = *m_player;
    if (player->canPlayToWirelessPlaybackTarget())
        return;

    auto tryToSwitchEngines = !m_remotePlaybackConfiguration && m_loadState == LoadingFromSourceElement;
    if (tryToSwitchEngines) {
        m_remotePlaybackConfiguration = { currentMediaTime(), playbackRate(), paused() };
        tryToSwitchEngines = havePotentialSourceChild();
    }

    if (!tryToSwitchEngines) {
        ERROR_LOG(LOGIDENTIFIER, "player incompatible, calling setShouldPlayToPlaybackTarget(false)");
        m_failedToPlayToWirelessTarget = true;
        m_remotePlaybackConfiguration = { };
        player->setShouldPlayToPlaybackTarget(false);
        return;
    }

    scheduleNextSourceChild();
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
#if ENABLE(MEDIA_STREAM)
    m_mediaStreamSrcObject = nullptr;
#endif
#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif
    m_blob = nullptr;

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaProvider && std::holds_alternative<RefPtr<MediaSource>>(*m_mediaProvider)) {
        RefPtr mediaSource = std::get<RefPtr<MediaSource>>(*m_mediaProvider);
        mediaSource->setAsSrcObject(true);
    }
#endif

    prepareForLoad();
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
    parameters.allowedMediaContainerTypes = allowedMediaContainerTypes();
    parameters.allowedMediaCodecTypes = allowedMediaCodecTypes();
    parameters.allowedMediaVideoCodecIDs = allowedMediaVideoCodecIDs();
    parameters.allowedMediaAudioCodecIDs = allowedMediaAudioCodecIDs();
    parameters.allowedMediaCaptionFormatTypes = allowedMediaCaptionFormatTypes();
    parameters.supportsLimitedMatroska = limitedMatroskaSupportEnabled();

    MediaPlayer::SupportsType support = MediaPlayer::supportsType(parameters);
    String canPlay;

    // 4.8.10.3
    switch (support)
    {
        case MediaPlayer::SupportsType::IsNotSupported:
            canPlay = emptyString();
            break;
        case MediaPlayer::SupportsType::MayBeSupported:
            canPlay = "maybe"_s;
            break;
        case MediaPlayer::SupportsType::IsSupported:
            canPlay = "probably"_s;
            break;
    }

    HTMLMEDIAELEMENT_RELEASE_LOG(CANPLAYTYPE, mimeType.utf8(), canPlay.utf8());

    return canPlay;
}

WallTime HTMLMediaElement::getStartDate() const
{
    RefPtr player = m_player;
    if (!player)
        return WallTime::nan();

    return WallTime::fromRawSeconds(player->getStartDate().toDouble());
}

void HTMLMediaElement::load()
{
    Ref protectedThis { *this }; // prepareForLoad may result in a 'beforeload' event, which can make arbitrary DOM mutations.

    INFO_LOG(LOGIDENTIFIER);

    if (m_videoFullscreenMode == VideoFullscreenModePictureInPicture && protectedDocument()->quirks().requiresUserGestureToLoadInPictureInPicture() && !protectedDocument()->processingUserGestureForMedia())
        return;

    prepareForLoad();
    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_resourceSelectionTaskCancellationGroup, [](auto& element) { element.prepareToPlay(); });
}

void HTMLMediaElement::prepareForLoad()
{
    // https://html.spec.whatwg.org/multipage/embedded-content.html#media-element-load-algorithm
    // The Media Element Load Algorithm
    // 12 February 2017

    HTMLMEDIAELEMENT_RELEASE_LOG(PREPAREFORLOAD, processingUserGestureForMedia());

    if (processingUserGestureForMedia())
        removeBehaviorRestrictionsAfterFirstUserGesture();

    // 1 - Abort any already-running instance of the resource selection algorithm for this element.
    // Perform the cleanup required for the resource load algorithm to run.
    stopPeriodicTimers();
    m_resourceSelectionTaskCancellationGroup.cancel();
    // FIXME: Figure out appropriate place to reset LoadTextTrackResource if necessary and set m_pendingActionFlags to 0 here.
    m_sentEndEvent = false;
    m_sentStalledEvent = false;
    m_haveFiredLoadedData = false;
    m_completelyLoaded = false;
    m_havePreparedToPlay = false;
    m_currentIdentifier = MediaUniqueIdentifier::generate();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    m_failedToPlayToWirelessTarget = false;
#endif

    m_loadState = WaitingForSource;
    m_currentSourceNode = nullptr;

#if ENABLE(ENCRYPTED_MEDIA)
    m_playbackBlockedWaitingForKey = false;
#endif

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
        setPaused(true);
        setPlaying(false);

        // 6.7 - If seeking is true, set it to false.
        clearSeeking();

        // 6.8 - Set the current playback position to 0.
        //       Set the official playback position to 0.
        //       If this changed the official playback position, then queue a task to fire a simple event named timeupdate at the media element.
        m_lastSeekTime = MediaTime::zeroTime();
        m_playedTimeRanges = TimeRanges::create();
        // FIXME: Add support for firing this event. e.g., scheduleEvent(eventNames().timeUpdateEvent);

        // 4.9 - Set the initial playback position to 0.
        invalidateOfficialPlaybackPosition();
        // 4.10 - Set the timeline offset to Not-a-Number (NaN).
        // 4.11 - Update the duration attribute to Not-a-Number (NaN).

        updateMediaController();
        updateActiveTextTrackCues(MediaTime::zeroTime());
    }

    // 7 - Set the playbackRate attribute to the value of the defaultPlaybackRate attribute.
    setPlaybackRate(defaultPlaybackRate());

    // 8 - Set the error attribute to null and the autoplaying flag to true.
    m_error = nullptr;
    m_autoplaying = true;
    Ref mediaSession = this->mediaSession();
    mediaSession->clientWillBeginAutoplaying();

    if (!MediaPlayer::isAvailable())
        noneSupported();
    else {
        // 9 - Invoke the media element's resource selection algorithm.
        // Note, unless the restriction on requiring user action has been removed,
        // do not begin downloading data.
        if (mediaSession->dataLoadingPermitted())
            selectMediaResource();
    }

    // 10 - Note: Playback of any previously playing media resource for this element stops.

    configureMediaControls();
}

void HTMLMediaElement::mediaPlayerReloadAndResumePlaybackIfNeeded()
{
    auto previousMediaTime = m_player ? RefPtr { m_player }->currentTime() : MediaTime::zeroTime();
    bool wasPaused = paused();

    load();

    if (m_videoFullscreenMode != VideoFullscreenModeNone)
        enterFullscreen(m_videoFullscreenMode);

    if (previousMediaTime) {
        queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_resourceSelectionTaskCancellationGroup, [previousMediaTime](auto& element) {
            if (RefPtr player = element.m_player)
                player->seekWhenPossible(previousMediaTime);
        });
    }

    if (!wasPaused)
        queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_resourceSelectionTaskCancellationGroup, [](auto& element) { element.playInternal(); });
}

void HTMLMediaElement::selectMediaResource()
{
    // https://www.w3.org/TR/2016/REC-html51-20161101/semantics-embedded-content.html#resource-selection-algorithm
    // The Resource Selection Algorithm

    // 1. Set the element’s networkState attribute to the NETWORK_NO_SOURCE value.
    m_networkState = NETWORK_NO_SOURCE;

    // 2. Set the element’s show poster flag to true.
    setShowPosterFlag(true);

    // 3. Set the media element’s delaying-the-load-event flag to true (this delays the load event).
    setShouldDelayLoadEvent(true);

    // 4. in parallel await a stable state, allowing the task that invoked this algorithm to continue.
    if (m_resourceSelectionTaskCancellationGroup.hasPendingTask())
        return;

    Ref mediaSession = this->mediaSession();
    if (!mediaSession->pageAllowsDataLoading()) {
        ALWAYS_LOG(LOGIDENTIFIER, "not allowed to load in background, waiting");
        setShouldDelayLoadEvent(false);
        if (m_isWaitingUntilMediaCanStart)
            return;
        m_isWaitingUntilMediaCanStart = true;
        protectedDocument()->addMediaCanStartListener(*this);
        return;
    }

    // Once the page has allowed an element to load media, it is free to load at will. This allows a
    // playlist that starts in a foreground tab to continue automatically if the tab is subsequently
    // put into the background.
    mediaSession->removeBehaviorRestriction(MediaElementSession::RequirePageConsentToLoadMedia);

    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_resourceSelectionTaskCancellationGroup, [](auto& element) {
        HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(&element, SELECTMEDIARESOURCE_LAMBDA_TASK_FIRED);
        // 5. If the media element’s blocked-on-parser flag is false, then populate the list of pending text tracks.
        // HTMLMediaElement::textTracksAreReady will need "... the text tracks whose mode was not in the
        // disabled state when the element's resource selection algorithm last started".
        // FIXME: Update this to match "populate the list of pending text tracks" step.
        element.m_textTracksWhenResourceSelectionBegan.clear();
        if (RefPtr textTracks = element.m_textTracks) {
            for (unsigned i = 0; i < textTracks->length(); ++i) {
                RefPtr<TextTrack> track = textTracks->item(i);
                if (track->mode() != TextTrack::Mode::Disabled)
                    element.m_textTracksWhenResourceSelectionBegan.append(track);
            }
        }

        enum Mode { None, Object, Attribute, Children };
        Mode mode = None;

        if (element.m_mediaProvider) {
            // 6. If the media element has an assigned media provider object, then let mode be object.
            mode = Object;
        } else if (element.hasAttributeWithoutSynchronization(srcAttr)) {
            //    Otherwise, if the media element has no assigned media provider object but has a src attribute, then let mode be attribute.
            mode = Attribute;
            ASSERT(element.m_player);
            if (!element.m_player) {
                HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(&element, SELECTMEDIARESOURCE_HAS_SRCATTR_PLAYER_NOT_CREATED);
                return;
            }
        } else if (auto firstSource = childrenOfType<HTMLSourceElement>(element).first()) {
            //    Otherwise, if the media element does not have an assigned media provider object and does not have a src attribute,
            //    but does have a source element child, then let mode be children and let candidate be the first such source element
            //    child in tree order.
            mode = Children;
            element.m_nextChildNodeToConsider = firstSource;
            element.m_currentSourceNode = nullptr;
        } else {
            //  Otherwise the media element has no assigned media provider object and has neither a src attribute nor a source
            //  element child: set the networkState to NETWORK_EMPTY, and abort these steps; the synchronous section ends.
            element.m_loadState = WaitingForSource;
            element.setShouldDelayLoadEvent(false);
            element.m_networkState = NETWORK_EMPTY;

            HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(&element, SELECTMEDIARESOURCE_NOTHING_TO_LOAD);

            if (element.m_videoFullscreenMode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
                element.exitFullscreen();

            return;
        }

        // 7. Set the media element’s networkState to NETWORK_LOADING.
        element.m_networkState = NETWORK_LOADING;

        // 8. Queue a task to fire a simple event named loadstart at the media element.
        element.scheduleEvent(eventNames().loadstartEvent);

        // 9. Run the appropriate steps from the following list:
        // ↳ If mode is object
        if (mode == Object) {
            element.m_loadState = LoadingFromSrcAttr;

            // 1. Set the currentSrc attribute to the empty string.
            element.setCurrentSrc(URL());

            // 2. End the synchronous section, continuing the remaining steps in parallel.
            // 3. Run the resource fetch algorithm with the assigned media provider object.
            switchOn(element.m_mediaProvider.value(),
#if ENABLE(MEDIA_STREAM)
                [element = Ref { element }](RefPtr<MediaStream> stream) { element->m_mediaStreamSrcObject = stream; },
#endif
#if ENABLE(MEDIA_SOURCE)
                [element = Ref { element }](RefPtr<MediaSource> source) { element->m_mediaSource = MediaSourceInterfaceMainThread::create(source.releaseNonNull()); },
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
                [element = Ref { element }](RefPtr<MediaSourceHandle> handle) {
                    // If the media provider object is a MediaSourceHandle whose [[Detached]] internal slot is true
                    // Run the "If the media data cannot be fetched at all, due to network errors, causing the user agent to give up trying to fetch the resource" steps of the resource fetch algorithm's media data processing steps list.
                    // If the media provider object is a MediaSourceHandle whose underlying MediaSource's [[has ever been attached]] internal slot is true
                    // Run the "If the media data cannot be fetched at all, due to network errors, causing the user agent to give up trying to fetch the resource" steps of the resource fetch algorithm's media data processing steps list.
                    if (!handle->isDetached() && !handle->hasEverBeenAssignedAsSrcObject())
                        element->m_mediaSource = MediaSourceInterfaceWorker::create(handle.releaseNonNull());
                    else
                        HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(element, SELECTMEDIARESOURCE_ATTEMPTING_USE_OF_UNATTACHED_MEDIASOURCEHANDLE);
                },
#endif
                [element = Ref { element }](RefPtr<Blob> blob) { element->m_blob = blob; }
            );

            ContentType contentType;
            element.loadResource(URL(), contentType);
            HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(&element, SELECTMEDIARESOURCE_USING_SRCOBJECT_PROPERTY);

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
            element.m_loadState = LoadingFromSrcAttr;

            // 1. If the src attribute’s value is the empty string, then end the synchronous section,
            //    and jump down to the failed with attribute step below.
            // 2. Let absolute URL be the absolute URL that would have resulted from parsing the URL
            //    specified by the src attribute’s value relative to the media element when the src
            //    attribute was last changed.
            auto& srcValue = element.attributeWithoutSynchronization(srcAttr);
            if (srcValue.isEmpty()) {
                element.mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
                HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(&element, SELECTMEDIARESOURCE_EMPTY_SRC);
                return;
            }

            auto absoluteURL = element.document().completeURL(srcValue);
            if (!element.isSafeToLoadURL(absoluteURL, InvalidURLAction::Complain)) {
                element.mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
                return;
            }

            // 3. If absolute URL was obtained successfully, set the currentSrc attribute to absolute URL.
            element.setCurrentSrc(absoluteURL);

            // 4. End the synchronous section, continuing the remaining steps in parallel.
            // 5. If absolute URL was obtained successfully, run the resource fetch algorithm with absolute
            //    URL. If that algorithm returns without aborting this one, then the load failed.

            // No type or key system information is available when the url comes
            // from the 'src' attribute so MediaPlayer
            // will have to pick a media engine based on the file extension.
            ContentType contentType;
            element.loadResource(absoluteURL, contentType);
            HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(&element, SELECTMEDIARESOURCE_USING_SRC_ATTRIBUTE_URL);

            // 6. Failed with attribute: Reaching this step indicates that the media resource failed to load
            //    or that the given URL could not be resolved. Queue a task to run the dedicated media source failure steps.
            // 7. Wait for the task queued by the previous step to have executed.
            // 8. Abort these steps. The element won’t attempt to load another resource until this algorithm is triggered again.
            return;
        }

        // ↳ Otherwise (mode is children)
        // (Ctd. in loadNextSourceChild())
        element.loadNextSourceChild();
    });
}

void HTMLMediaElement::loadNextSourceChild()
{
    ContentType contentType;
    auto mediaURL = selectNextSourceChild(&contentType, InvalidURLAction::Complain);
    if (!mediaURL.isValid()) {
        waitForSourceChange();
        return;
    }

    // Recreate the media player for the new url
    createMediaPlayer();

    m_loadState = LoadingFromSourceElement;
    loadResource(mediaURL, contentType);
}

void HTMLMediaElement::maybeUpdatePlayerPreload() const
{
    if (RefPtr player = m_player; player && !m_havePreparedToPlay && !autoplay())
        player->setPreload(protectedMediaSession()->effectivePreloadForElement());
}

MediaPlayer::Preload HTMLMediaElement::effectivePreloadValue() const
{
    if (m_hasEverPreparedToPlay)
        return MediaPlayer::Preload::Auto;

    return m_preload;
}

#if USE(AVFOUNDATION) && ENABLE(MEDIA_SOURCE)
static VideoRendererPreferences videoRendererPreferences(const Settings& settings, bool forceStereo)
{
    VideoRendererPreferences preferences { VideoRendererPreference::PrefersDecompressionSession };
#if USE(MODERN_AVCONTENTKEYSESSION_WITH_VTDECOMPRESSIONSESSION)
    if (settings.videoRendererProtectedFallbackDisabled())
        preferences.add(VideoRendererPreference::ProtectedFallbackDisabled);
    if (settings.videoRendererUseDecompressionSessionForProtected())
        preferences.add(VideoRendererPreference::UseDecompressionSessionForProtectedContent);
#else
    UNUSED_PARAM(settings);
#endif
#if PLATFORM(VISION)
    UNUSED_PARAM(forceStereo);
    preferences.add(VideoRendererPreference::UseStereoDecoding);
#else
    if (forceStereo)
        preferences.add(VideoRendererPreference::UseStereoDecoding);
#endif
    return preferences;
}
#endif

void HTMLMediaElement::loadResource(const URL& initialURL, const ContentType& initialContentType)
{
    ASSERT(initialURL.isEmpty() || isSafeToLoadURL(initialURL, InvalidURLAction::Complain));

    auto logSiteIdentifier = LOGIDENTIFIER;
    INFO_LOG(logSiteIdentifier, initialURL, initialContentType);

    RefPtr frame = document().frame();
    if (!frame) {
        mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
        return;
    }

    RefPtr page = frame->page();
    if (!page) {
        mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
        return;
    }

    if (!m_player) {
        ASSERT_NOT_REACHED("It should not be possible to enter loadResource without a valid m_player object");
        mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
        return;
    }

    URL url = initialURL;
#if PLATFORM(COCOA)
    if (url.protocolIsFile() && !frame->loader().willLoadMediaElementURL(url, *this)) {
        mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
        return;
    }
#elif USE(GSTREAMER)
    if (!url.isEmpty() && !frame->loader().willLoadMediaElementURL(url, *this)) {
        mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
        return;
    }
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    RefPtr documentLoader = frame->loader().documentLoader();
    RefPtr userContentProvider = frame->userContentProvider();
    if (documentLoader && userContentProvider) {
        if (userContentProvider->processContentRuleListsForLoad(*page, url, ContentExtensions::ResourceType::Media, *documentLoader).shouldBlock()) {
            mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
            return;
        }
    }
#endif

    // The resource fetch algorithm
    m_networkState = NETWORK_LOADING;

    // Log that we started loading a media element.
    page->checkedDiagnosticLoggingClient()->logDiagnosticMessage(isVideo() ? DiagnosticLoggingKeys::videoKey() : DiagnosticLoggingKeys::audioKey(), DiagnosticLoggingKeys::loadingKey(), ShouldSample::No);

    m_firstTimePlaying = true;

    // Set m_currentSrc *before* changing to the cache URL, the fact that we are loading from the app
    // cache is an internal detail not exposed through the media element API.
    setCurrentSrc(url);

    INFO_LOG(logSiteIdentifier, "m_currentSrc is ", m_currentSrc);

    startProgressEventTimer();

    bool privateMode = document().page() && document().page()->usesEphemeralSession();
    RefPtr player = m_player;
    player->setPrivateBrowsingMode(privateMode);

    maybeUpdatePlayerPreload();
    player->setPreservesPitch(m_preservesPitch);
    player->setPitchCorrectionAlgorithm(document().settings().pitchCorrectionAlgorithm());

    if (!m_explicitlyMuted) {
        m_explicitlyMuted = true;
        m_muted = hasAttributeWithoutSynchronization(mutedAttr);
        protectedMediaSession()->canProduceAudioChanged();
    }

    updateVolume();

    auto contentType = initialContentType;

    if (m_blob && !m_remotePlaybackConfiguration) {
        ALWAYS_LOG(logSiteIdentifier, "loading generic blob");
        if (!m_blobURLForReading.isEmpty())
            ThreadableBlobRegistry::unregisterBlobURL(m_blobURLForReading);
        m_blobURLForReading = { BlobURL::createPublicURL(protectedDocument()->protectedSecurityOrigin().ptr()), protectedDocument()->topOrigin().data() };
        ThreadableBlobRegistry::registerBlobURL(protectedDocument()->protectedSecurityOrigin().ptr(), protectedDocument()->policyContainer(), m_blobURLForReading, m_blob->url());

        url = m_blobURLForReading;
        if (contentType.isEmpty())
            contentType = ContentType { m_blob->type() };
    }

    auto completionHandler = [url, player = m_player, logSiteIdentifier, weakThis = WeakPtr { *this }](SnifferPromise::Result&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!result) {
            if (result.error() != PlatformMediaError::Cancelled)
                protectedThis->mediaLoadingFailed(MediaPlayer::NetworkState::NetworkError);
            return;
        }

        MediaPlayer::LoadOptions options = {
            .contentType = *result,
            .requiresRemotePlayback = !!protectedThis->m_remotePlaybackConfiguration,
            .supportsLimitedMatroska = protectedThis->limitedMatroskaSupportEnabled(),
        };

#if ENABLE(MEDIA_SOURCE)
#if USE(AVFOUNDATION)
        if (protectedThis->document().settings().mediaSourcePrefersDecompressionSession())
            options.videoRendererPreferences = videoRendererPreferences(protectedThis->document().settings(), protectedThis->m_forceStereoDecoding);
#endif
        if (!protectedThis->m_mediaSource && url.protocolIs(mediaSourceBlobProtocol) && !protectedThis->m_remotePlaybackConfiguration) {
            if (RefPtr mediaSource = MediaSource::lookup(url.string()))
                protectedThis->m_mediaSource = MediaSourceInterfaceMainThread::create(mediaSource.releaseNonNull());
        }

        if (RefPtr mediaSource = protectedThis->m_mediaSource) {
            ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "loading MSE blob");
#if !RELEASE_LOG_DISABLED
            mediaSource->setLogIdentifier(protectedThis->m_logIdentifier);
#endif
            if (url.protocolIs(mediaSourceBlobProtocol) && mediaSource->detachable()) {
                protectedThis->protectedDocument()->addConsoleMessage(MessageSource::MediaSource, MessageLevel::Error, makeString("Unable to attach detachable MediaSource via blob URL, use srcObject attribute"_s));
                return protectedThis->mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
            }

#if PLATFORM(IOS_FAMILY)
            if (protectedThis->canShowWhileLocked())
                options.videoRendererPreferences |= VideoRendererPreference::CanShowWhileLocked;
#endif

            if (!mediaSource->attachToElement(protectedThis.get())) {
                // Forget our reference to the MediaSource, so we leave it alone
                // while processing remainder of load failure.
                protectedThis->m_mediaSource = nullptr;
            } else  if (!mediaSource->client() || !player->load(url, options, *mediaSource->client())) {
                // We have to detach the MediaSource before we forget the reference to it.
                mediaSource->detachFromElement();
                protectedThis->m_mediaSource = nullptr;
            }
            if (!protectedThis->m_mediaSource)
                protectedThis->mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
            else
                protectedThis->mediaPlayerRenderingModeChanged();
            return;
        }
#else
        UNUSED_PARAM(logSiteIdentifier);
#endif

#if ENABLE(MEDIA_STREAM)
        if (RefPtr mediaStreamSrcObject = protectedThis->m_mediaStreamSrcObject; mediaStreamSrcObject && !protectedThis->m_remotePlaybackConfiguration) {
            ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "loading media stream blob ", mediaStreamSrcObject->logIdentifier());
            if (!player->load(mediaStreamSrcObject->protectedPrivateStream()))
                protectedThis->mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
            else
                protectedThis->mediaPlayerRenderingModeChanged();
            return;
        }
#endif
#if PLATFORM(IOS_FAMILY)
        if (protectedThis->canShowWhileLocked())
            options.videoRendererPreferences |= VideoRendererPreference::CanShowWhileLocked;
#endif
        if (!player->load(url, options))
            protectedThis->mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
        else
            protectedThis->mediaPlayerRenderingModeChanged();
    };

    if (needsContentTypeToPlay() && !url.isEmpty()) {
        if (contentType.isEmpty() && url.protocolIsData())
            contentType = ContentType(mimeTypeFromDataURL(url.string()));
        else {
            // If the MIME type is missing or is not meaningful, try to figure it out from the URL.
            AtomString containerType { contentType.containerType() };
            if (containerType.isEmpty() || containerType == applicationOctetStreamAtom() || containerType == textPlainContentTypeAtom())
                contentType = ContentType::fromURL(url);
        }
        m_lastContentTypeUsed = contentType;
    }

    completionHandler(WTFMove(contentType));
}

bool HTMLMediaElement::needsContentTypeToPlay() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource || (currentSrc().protocolIs(mediaSourceBlobProtocol) && MediaSource::lookup(currentSrc().string())))
        return false;
#endif
#if ENABLE(MEDIA_STREAM)
        if (m_mediaStreamSrcObject)
            return false;
#endif
    return !m_remotePlaybackConfiguration;
}

Ref<HTMLMediaElement::SnifferPromise> HTMLMediaElement::sniffForContentType(const URL& url)
{
    ResourceRequest request(URL { url });
    request.setAllowCookies(true);
    // https://mimesniff.spec.whatwg.org/#reading-the-resource-header defines a maximum size of 1445 bytes fetch.
    m_sniffer = MediaResourceSniffer::create(mediaPlayerCreateResourceLoader(), WTFMove(request), 1445);
    return Ref { *m_sniffer }->promise();
}

void HTMLMediaElement::mediaSourceWasDetached()
{
    // The steps on what happen when a MediaSource goes missing are not defined in the current spec.
    // https://github.com/w3c/media-source/issues/348 ; we do what's the most sensible for now.
    userCancelledLoad();
}

static bool trackIndexCompare(const RefPtr<TextTrack>& a, const RefPtr<TextTrack>& b)
{
    return a->trackIndex() - b->trackIndex() < 0;
}

static bool eventTimeCueCompare(const std::pair<MediaTime, RefPtr<TextTrackCue>>& a, const std::pair<MediaTime, RefPtr<TextTrackCue>>& b)
{
    // 12 - Sort the tasks in events in ascending time order (tasks with earlier
    // times first).
    if (a.first != b.first)
        return a.first - b.first < MediaTime::zeroTime();

    // If the cues belong to different text tracks, it doesn't make sense to
    // compare the two tracks by the relative cue order, so return the relative
    // track order.
    if (a.second->track() != b.second->track())
        return trackIndexCompare(a.second->protectedTrack().get(), b.second->protectedTrack().get());

    // 12 - Further sort tasks in events that have the same time by the
    // relative text track cue order of the text track cues associated
    // with these tasks.
    return a.second->isOrderedBefore(b.second.get());
}

static bool compareCueInterval(const CueInterval& one, const CueInterval& two)
{
    return RefPtr { one.data() }->isOrderedBefore(RefPtr { two.data() }.get());
}

static bool compareCueIntervalEndTime(const CueInterval& one, const CueInterval& two)
{
    return one.data()->endMediaTime() > two.data()->endMediaTime();
}

bool HTMLMediaElement::ignoreTrackDisplayUpdateRequests() const
{
    return m_ignoreTrackDisplayUpdate > 0 || !m_textTracks || !m_cueData || m_cueData->cueTree.isEmpty();
}

void HTMLMediaElement::updateActiveTextTrackCues(const MediaTime& movieTime)
{
    if (m_seeking)
        return;

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
    if (m_readyState != HAVE_NOTHING && m_player) {
        for (auto& cue : m_cueData->cueTree.allOverlaps({ movieTime, movieTime })) {
            if (cue.low() <= movieTime && cue.high() > movieTime)
                currentCues.append(cue);
        }
        if (currentCues.size() > 1)
            std::ranges::sort(currentCues, &compareCueInterval);
    }

    CueList previousCues;
    CueList missedCues;

    // 2 - Let other cues be a list of cues, initialized to contain all the cues
    // of hidden, showing, and showing by default text tracks of the media
    // element that are not present in current cues.
    previousCues = m_cueData->currentlyActiveCues;

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
        for (auto& cue : m_cueData->cueTree.allOverlaps({ lastTime, movieTime })) {
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
        scheduleTimeupdateEvent(true);

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
        RefPtr cue = currentCues[i].data();
        cue->updateDisplayTree(movieTime);
        if (!cue->isActive())
            activeSetChanged = true;
    }

    MediaTime nextInterestingTime = MediaTime::invalidTime();
    if (auto nearestEndingCue = std::ranges::min_element(currentCues, compareCueIntervalEndTime))
        nextInterestingTime = nearestEndingCue->data()->endMediaTime();

    std::optional<CueInterval> nextCue = m_cueData->cueTree.nextIntervalAfter(movieTime);
    if (nextCue)
        nextInterestingTime = std::min(nextInterestingTime, nextCue->low());

    auto identifier = LOGIDENTIFIER;
    INFO_LOG(identifier, "nextInterestingTime:", nextInterestingTime);

    if (RefPtr player = m_player; nextInterestingTime.isValid() && player) {
        player->performTaskAtTime([weakThis = WeakPtr { *this }, identifier](auto&) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            auto currentTime = protectedThis->currentMediaTime();
            INFO_LOG_WITH_THIS(protectedThis, identifier, "lambda(), currentMediaTime: ", currentTime);
            protectedThis->updateActiveTextTrackCues(currentTime);
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
    Vector<std::pair<MediaTime, RefPtr<TextTrackCue>>> eventTasks;

    // 8 - Let affected tracks be a list of text tracks, initially empty.
    Vector<RefPtr<TextTrack>> affectedTracks;

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
    std::ranges::sort(eventTasks, eventTimeCueCompare);

    for (auto& eventTask : eventTasks) {
        auto& [eventTime, eventCue] = eventTask;

        if (!affectedTracks.contains(eventCue->track()))
            affectedTracks.append(eventCue->track());

        // 13 - Queue each task in events, in list order.

        // Each event in eventTasks may be either an enterEvent or an exitEvent,
        // depending on the time that is associated with the event. This
        // correctly identifies the type of the event, if the startTime is
        // less than the endTime in the cue.
        if (eventCue->startTime() >= eventCue->endTime()) {
            executeCueEnterOrExitActionForTime(*eventCue, CueAction::Enter);
            executeCueEnterOrExitActionForTime(*eventCue, CueAction::Exit);
        } else {
            CueAction action = eventTime == eventCue->startMediaTime() ? CueAction::Enter : CueAction::Exit;
            executeCueEnterOrExitActionForTime(*eventCue, action);
        }
    }

    // 14 - Sort affected tracks in the same order as the text tracks appear in
    // the media element's list of text tracks, and remove duplicates.
    std::ranges::sort(affectedTracks, trackIndexCompare);

    // 15 - For each text track in affected tracks, in the list order, queue a
    // task to fire a simple event named cuechange at the TextTrack object, and, ...
    for (auto& affectedTrack : affectedTracks) {
        Ref event = Event::create(eventNames().cuechangeEvent, Event::CanBubble::No, Event::IsCancelable::No);
        scheduleEventOn(*affectedTrack, WTFMove(event));

        // ... if the text track has a corresponding track element, to then fire a
        // simple event named cuechange at the track element as well.
        if (RefPtr loadableTextTrack = dynamicDowncast<LoadableTextTrack>(*affectedTrack)) {
            Ref event = Event::create(eventNames().cuechangeEvent, Event::CanBubble::No, Event::IsCancelable::No);
            RefPtr trackElement = loadableTextTrack->trackElement();
            ASSERT(trackElement);
            scheduleEventOn(*trackElement, WTFMove(event));
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
    m_cueData->currentlyActiveCues = currentCues;

    if (activeSetChanged)
        updateTextTrackDisplay();
}

void HTMLMediaElement::setSpeechSynthesisState(SpeechSynthesisState state)
{
#if ENABLE(SPEECH_SYNTHESIS)
    constexpr double volumeMultiplierWhenSpeakingCueText = .4;

    if (m_changingSynthesisState || state == m_speechState)
        return;

    RefPtr cueBeingSpoken = m_cueBeingSpoken;
    if (cueBeingSpoken)
        ALWAYS_LOG(LOGIDENTIFIER, "changing state from ", m_speechState, " to ", state, ", at time ", currentMediaTime(), ", for cue ", cueBeingSpoken->startTime(), "..", cueBeingSpoken->endTime());
    else
        ALWAYS_LOG(LOGIDENTIFIER, "changing state from ", m_speechState, " to ", state, ", at time ", currentMediaTime());

    SetForScope<bool> changingState { m_changingSynthesisState, true };
    auto setSpeechVolumeMultiplier = [this] (double multiplier) {
        m_volumeMultiplierForSpeechSynthesis = multiplier;
        updateVolume();
    };

    auto oldState = m_speechState;
    m_speechState = state;
    switch (state) {
    case SpeechSynthesisState::None:
        setSpeechVolumeMultiplier(1);
        if (oldState == SpeechSynthesisState::CompletingExtendedDescription && m_paused)
            play();

        if (!cueBeingSpoken)
            return;

        cueBeingSpoken->cancelSpeaking();
        m_cueBeingSpoken = nullptr;
        break;

    case SpeechSynthesisState::Speaking:
        ASSERT(cueBeingSpoken);
        setSpeechVolumeMultiplier(volumeMultiplierWhenSpeakingCueText);
        cueBeingSpoken->beginSpeaking();
        break;

    case SpeechSynthesisState::CompletingExtendedDescription:
        if (cueBeingSpoken)
            pauseInternal();
        break;

    case SpeechSynthesisState::Paused:
        if (cueBeingSpoken)
            cueBeingSpoken->pauseSpeaking();
        break;
    }
#else
    UNUSED_PARAM(state);
#endif
}

void HTMLMediaElement::speakCueText(TextTrackCue& cue)
{
#if ENABLE(SPEECH_SYNTHESIS)
    if (RefPtr cueBeingSpoken = m_cueBeingSpoken; cueBeingSpoken && cueBeingSpoken->isEqual(cue, TextTrackCue::MatchAllFields))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, cue);

    if (m_speechState != SpeechSynthesisState::None)
        cancelSpeakingCueText();

    m_cueBeingSpoken = cue;
    RefPtr { m_cueBeingSpoken }->prepareToSpeak(protectedSpeechSynthesis(), m_reportedPlaybackRate ? m_reportedPlaybackRate : m_requestedPlaybackRate, volume(), [weakThis = WeakPtr { *this }](const TextTrackCue&) {
        ASSERT(isMainThread());
        RefPtr<HTMLMediaElement> protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->setSpeechSynthesisState(SpeechSynthesisState::None);
    });

    if (m_pausedInternal || m_paused)
        setSpeechSynthesisState(SpeechSynthesisState::Paused);
    else
        setSpeechSynthesisState(SpeechSynthesisState::Speaking);
#else
    UNUSED_PARAM(cue);
#endif
}

#if ENABLE(SPEECH_SYNTHESIS)
Ref<SpeechSynthesis> HTMLMediaElement::protectedSpeechSynthesis()
{
    return speechSynthesis();
}
#endif

void HTMLMediaElement::pauseSpeakingCueText()
{
#if ENABLE(SPEECH_SYNTHESIS)
    if (m_speechState != SpeechSynthesisState::Speaking && m_speechState != SpeechSynthesisState::CompletingExtendedDescription)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    setSpeechSynthesisState(SpeechSynthesisState::Paused);
#endif
}

void HTMLMediaElement::resumeSpeakingCueText()
{
#if ENABLE(SPEECH_SYNTHESIS)
    if (m_speechState != SpeechSynthesisState::Paused && m_speechState != SpeechSynthesisState::CompletingExtendedDescription)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    setSpeechSynthesisState(SpeechSynthesisState::Speaking);
#endif
}

void HTMLMediaElement::cancelSpeakingCueText()
{
#if ENABLE(SPEECH_SYNTHESIS)
    if (m_speechState == SpeechSynthesisState::None)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    setSpeechSynthesisState(SpeechSynthesisState::None);
#endif
}

void HTMLMediaElement::pausePlaybackForExtendedTextDescription()
{
#if ENABLE(SPEECH_SYNTHESIS)
    if (m_speechState != SpeechSynthesisState::Speaking)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    setSpeechSynthesisState(SpeechSynthesisState::CompletingExtendedDescription);
#endif
}

bool HTMLMediaElement::shouldSpeakCueTextForTime(const MediaTime& time)
{
#if ENABLE(SPEECH_SYNTHESIS)
    RefPtr cueBeingSpoken = m_cueBeingSpoken;
    if (!cueBeingSpoken)
        return false;

    auto result = time.toDouble() >= cueBeingSpoken->startTime() && time.toDouble() < cueBeingSpoken->endTime();
    ALWAYS_LOG(LOGIDENTIFIER, "time = ", time, ", returning ", result);

    return result;
#else
    UNUSED_PARAM(time);
    return false;
#endif
}

#if ENABLE(SPEECH_SYNTHESIS)
SpeechSynthesis& HTMLMediaElement::speechSynthesis()
{
    if (!m_speechSynthesis) {
        m_speechSynthesis = SpeechSynthesis::create(protectedDocument());
        RefPtr { m_speechSynthesis }->removeBehaviorRestriction(SpeechSynthesis::BehaviorRestrictionFlags::RequireUserGestureForSpeechStart);
    }

    return *m_speechSynthesis;
}
#endif

void HTMLMediaElement::executeCueEnterOrExitActionForTime(TextTrackCue& cue, CueAction type)
{
    RefPtr track = cue.track();
    ASSERT(track);
    if (!track)
        return;

    if (m_userPrefersTextDescriptions && track->isSpoken() && cue.startTime() < cue.endTime()) {
        if (type == CueAction::Enter)
            speakCueText(cue);
        else if (m_userPrefersExtendedDescriptions)
            pausePlaybackForExtendedTextDescription();
    }

    Ref event = Event::create(type == CueAction::Enter ? eventNames().enterEvent : eventNames().exitEvent, Event::CanBubble::No, Event::IsCancelable::No);
    scheduleEventOn(cue, WTFMove(event));
}

void HTMLMediaElement::audioTrackEnabledChanged(AudioTrack& track)
{
    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();
    if (processingUserGestureForMedia())
        removeBehaviorRestrictionsAfterFirstUserGesture(MediaElementSession::AllRestrictions & ~MediaElementSession::RequireUserGestureToControlControlsManager);
    checkForAudioAndVideo();
}

void HTMLMediaElement::audioTrackKindChanged(AudioTrack& track)
{
    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();
}

void HTMLMediaElement::audioTrackLabelChanged(AudioTrack& track)
{
    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();
}

void HTMLMediaElement::audioTrackLanguageChanged(AudioTrack& track)
{
    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();
}

void HTMLMediaElement::audioTrackConfigurationChanged(AudioTrack& track)
{
    UNUSED_PARAM(track);
    ALWAYS_LOG(LOGIDENTIFIER, ", "_s, MediaElementSession::descriptionForTrack(track));
}

void HTMLMediaElement::willRemoveAudioTrack(AudioTrack& track)
{
    removeAudioTrack(track);
}

void HTMLMediaElement::textTrackModeChanged(TextTrack& track)
{
    bool trackIsLoaded = true;
    if (track.trackType() == TextTrack::TrackElement) {
        trackIsLoaded = false;
        for (Ref trackElement : childrenOfType<HTMLTrackElement>(*this)) {
            if (&trackElement->track() == &track) {
                if (trackElement->readyState() == HTMLTrackElement::LOADING || trackElement->readyState() == HTMLTrackElement::LOADED)
                    trackIsLoaded = true;
                break;
            }
        }
    }

    // If this is the first added track, create the list of text tracks.
    ensureTextTracks();

    // Mark this track as "configured" so configureTextTracks won't change the mode again.
    track.setHasBeenConfigured(true);

    if (track.mode() != TextTrack::Mode::Disabled && trackIsLoaded)
        textTrackAddCues(track, *track.protectedCues());

    configureTextTrackDisplay(AssumeTextTrackVisibilityChanged);

    if (m_textTracks && m_textTracks->contains(track))
        m_textTracks->scheduleChangeEvent();

    if (track.trackType() == TextTrack::TrackElement && m_player)
        RefPtr { m_player }->notifyTrackModeChanged();
}

void HTMLMediaElement::textTrackKindChanged(TextTrack& track)
{
    if (track.kind() != TextTrack::Kind::Captions && track.kind() != TextTrack::Kind::Subtitles && track.mode() == TextTrack::Mode::Showing)
        track.setMode(TextTrack::Mode::Hidden);

    if (m_textTracks && m_textTracks->contains(track))
        m_textTracks->scheduleChangeEvent();
}

void HTMLMediaElement::textTrackLabelChanged(TextTrack& track)
{
    if (m_textTracks && m_textTracks->contains(track))
        m_textTracks->scheduleChangeEvent();
}

void HTMLMediaElement::textTrackLanguageChanged(TextTrack& track)
{
    if (m_textTracks && m_textTracks->contains(track))
        m_textTracks->scheduleChangeEvent();
}

void HTMLMediaElement::willRemoveTextTrack(TextTrack& track)
{
    if (track.trackType() == TextTrack::InBand)
        removeTextTrack(track);
}

void HTMLMediaElement::videoTrackSelectedChanged(VideoTrack& track)
{
    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();
    checkForAudioAndVideo();
}

void HTMLMediaElement::videoTrackConfigurationChanged(VideoTrack& track)
{
    UNUSED_PARAM(track);
    ALWAYS_LOG(LOGIDENTIFIER, ", "_s, MediaElementSession::descriptionForTrack(track));
}

void HTMLMediaElement::videoTrackKindChanged(VideoTrack& track)
{
    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();
}

void HTMLMediaElement::videoTrackLabelChanged(VideoTrack& track)
{
    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();
}

void HTMLMediaElement::videoTrackLanguageChanged(VideoTrack& track)
{
    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();
}

void HTMLMediaElement::willRemoveVideoTrack(VideoTrack& track)
{
    removeVideoTrack(track);
}

void HTMLMediaElement::beginIgnoringTrackDisplayUpdateRequests()
{
    ++m_ignoreTrackDisplayUpdate;
}

void HTMLMediaElement::endIgnoringTrackDisplayUpdateRequests()
{
    ASSERT(m_ignoreTrackDisplayUpdate);
    --m_ignoreTrackDisplayUpdate;

    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_updateTextTracksTaskCancellationGroup, [](auto& element) {
        if (!element.m_ignoreTrackDisplayUpdate && element.m_inActiveDocument)
            element.updateActiveTextTrackCues(element.currentMediaTime());
    });
}

void HTMLMediaElement::textTrackAddCues(TextTrack& track, const TextTrackCueList& cues)
{
    if (track.mode() == TextTrack::Mode::Disabled)
        return;

    TrackDisplayUpdateScope scope { *this };
    for (unsigned i = 0; i < cues.length(); ++i)
        textTrackAddCue(track, Ref { *cues.item(i) });
}

void HTMLMediaElement::textTrackRemoveCues(TextTrack&, const TextTrackCueList& cues)
{
    TrackDisplayUpdateScope scope { *this };
    for (unsigned i = 0; i < cues.length(); ++i) {
        Ref cue = *cues.item(i);
        textTrackRemoveCue(*cue->protectedTrack(), cue);
    }
}

void HTMLMediaElement::textTrackAddCue(TextTrack& track, TextTrackCue& cue)
{
    if (track.mode() == TextTrack::Mode::Disabled)
        return;

    if (!m_cueData)
        m_cueData = makeUnique<CueData>();

    // Negative duration cues need be treated in the interval tree as
    // zero-length cues.
    MediaTime endTime = std::max(cue.startMediaTime(), cue.endMediaTime());

    CueInterval interval(cue.startMediaTime(), endTime, &cue);
    if (!m_cueData->cueTree.contains(interval))
        m_cueData->cueTree.add(interval);
    updateActiveTextTrackCues(currentMediaTime());
}

void HTMLMediaElement::textTrackRemoveCue(TextTrack&, TextTrackCue& cue)
{
    if (!m_cueData)
        m_cueData = makeUnique<CueData>();

    // Negative duration cues need to be treated in the interval tree as
    // zero-length cues.
    MediaTime endTime = std::max(cue.startMediaTime(), cue.endMediaTime());

    CueInterval interval(cue.startMediaTime(), endTime, &cue);
    m_cueData->cueTree.remove(interval);

    // Since the cue will be removed from the media element and likely the
    // TextTrack might also be destroyed, notifying the region of the cue
    // removal shouldn't be done.
    RefPtr vttCue = dynamicDowncast<VTTCue>(cue);
    if (vttCue)
        vttCue->notifyRegionWhenRemovingDisplayTree(false);

    size_t index = m_cueData->currentlyActiveCues.find(interval);
    if (index != notFound) {
        cue.setIsActive(false);
        m_cueData->currentlyActiveCues.removeAt(index);
    }

    cue.removeDisplayTree();
    updateActiveTextTrackCues(currentMediaTime());

    if (vttCue)
        vttCue->notifyRegionWhenRemovingDisplayTree(true);
}

CueList HTMLMediaElement::currentlyActiveCues() const
{
    if (!m_cueData)
        return { };
    return m_cueData->currentlyActiveCues;
}

static inline bool isAllowedToLoadMediaURL(const HTMLMediaElement& element, const URL& url, bool isInUserAgentShadowTree)
{
    // Elements in user agent show tree should load whatever the embedding document policy is.
    if (isInUserAgentShadowTree)
        return true;

    ASSERT(element.document().contentSecurityPolicy());
    return element.protectedDocument()->checkedContentSecurityPolicy()->allowMediaFromSource(url);
}

bool HTMLMediaElement::isSafeToLoadURL(const URL& url, InvalidURLAction actionIfInvalid, bool shouldLog) const
{
    if (!url.isValid()) {
        if (shouldLog)
            ERROR_LOG(LOGIDENTIFIER, url, " is invalid");
        return false;
    }

    RefPtr frame = protectedDocument()->frame();
    if (!frame || !protectedDocument()->protectedSecurityOrigin()->canDisplay(url, OriginAccessPatternsForWebProcess::singleton())) {
        if (actionIfInvalid == InvalidURLAction::Complain) {
            FrameLoader::reportLocalLoadFailed(frame.get(), url.stringCenterEllipsizedToLength());
            if (shouldLog)
                ERROR_LOG(LOGIDENTIFIER, url , " was rejected by SecurityOrigin");
        }
        return false;
    }

    if (!portAllowed(url) || isIPAddressDisallowed(url)) {
        if (actionIfInvalid == InvalidURLAction::Complain) {
            if (frame)
                FrameLoader::reportBlockedLoadFailed(*frame, url);
            if (shouldLog) {
                if (isIPAddressDisallowed(url))
                    ERROR_LOG(LOGIDENTIFIER, url , " was rejected because the address not allowed");
                else
                    ERROR_LOG(LOGIDENTIFIER, url , " was rejected because the port is not allowed");
            }
        }
        return false;
    }

    if (!isAllowedToLoadMediaURL(*this, url, isInUserAgentShadowTree())) {
        if (shouldLog)
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
    ALWAYS_LOG(LOGIDENTIFIER);

    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 6.17 - Waiting: Set the element's networkState attribute to the NETWORK_NO_SOURCE value
    m_networkState = NETWORK_NO_SOURCE;

    // 6.18 - Set the element's show poster flag to true.
    setShowPosterFlag(true);

    // 6.19 -  Queue a media element task given the media element given the element to set the
    // element's delaying-the-load-event flag to false. This stops delaying the load event.
    // FIXME: this should be done in a task queue
    setShouldDelayLoadEvent(false);

    updateRenderer();
}

void HTMLMediaElement::noneSupported()
{
    if (m_error)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    stopPeriodicTimers();
    m_loadState = WaitingForSource;
    m_currentSourceNode = nullptr;

    // 4.8.10.5
    // 6 - Reaching this step indicates that the media resource failed to load or that the given
    // URL could not be resolved. In one atomic operation, run the following steps:

    // 6.1 - Set the error attribute to a new MediaError object whose code attribute is set to
    // MEDIA_ERR_SRC_NOT_SUPPORTED.
    RefPtr player = m_player;
    m_error = player
        ? MediaError::create(MediaError::MEDIA_ERR_SRC_NOT_SUPPORTED, player->lastErrorMessage())
        : MediaError::create(MediaError::MEDIA_ERR_SRC_NOT_SUPPORTED, "Unsupported source type"_s);

    // 6.2 - Forget the media element's media-resource-specific text tracks.
    forgetResourceSpecificTracks();

    // 6.3 - Set the element's networkState attribute to the NETWORK_NO_SOURCE value.
    m_networkState = NETWORK_NO_SOURCE;

    // 6.4 - Set the element's show poster flag to true.
    setShowPosterFlag(true);

    // 7 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(eventNames().errorEvent);

    rejectPendingPlayPromises(WTFMove(m_pendingPlayPromises), DOMException::create(ExceptionCode::NotSupportedError));

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    // 8 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 9 - Abort these steps. Until the load() method is invoked or the src attribute is changed,
    // the element won't attempt to load another resource.

    updateRenderer();
}

void HTMLMediaElement::mediaLoadingFailedFatally(MediaPlayer::NetworkState error)
{
    // https://html.spec.whatwg.org/#loading-the-media-resource:dom-media-have_nothing-2
    // 17 March 2021

    // 1 - The user agent should cancel the fetching process.
    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    const auto getErrorMessage = [&] (String&& defaultMessage) {
        String message = WTFMove(defaultMessage);
        RefPtr player = m_player;
        if (!player)
            return message;

        auto lastErrorMessage = player->lastErrorMessage();
        if (!lastErrorMessage)
            return message;

        return makeString(message, ": "_s, lastErrorMessage);
    };

    // 2 - Set the error attribute to a new MediaError object whose code attribute is
    // set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
    if (error == MediaPlayer::NetworkState::NetworkError)
        m_error = MediaError::create(MediaError::MEDIA_ERR_NETWORK, getErrorMessage("Media failed to load"_s));
    else if (error == MediaPlayer::NetworkState::DecodeError)
        m_error = MediaError::create(MediaError::MEDIA_ERR_DECODE, getErrorMessage("Media failed to decode"_s));
    else
        ASSERT_NOT_REACHED();

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    // 3 - Set the element's networkState attribute to the NETWORK_IDLE value.
    m_networkState = NETWORK_IDLE;

    // 4 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 5 - Fire an event named error at the media element.
    scheduleEvent(eventNames().errorEvent);

    // 6 - Abort the overall resource selection algorithm.
    m_currentSourceNode = nullptr;
}

void HTMLMediaElement::cancelPendingEventsAndCallbacks()
{
    INFO_LOG(LOGIDENTIFIER);
    m_asyncEventsCancellationGroup.cancel();

    for (Ref source : childrenOfType<HTMLSourceElement>(*this))
        source->cancelPendingErrorEvent();

    rejectPendingPlayPromises(WTFMove(m_pendingPlayPromises), DOMException::create(ExceptionCode::AbortError));
}

void HTMLMediaElement::mediaPlayerNetworkStateChanged()
{
    beginProcessingMediaPlayerCallback();
    setNetworkState(protectedPlayer()->networkState());
    endProcessingMediaPlayerCallback();
}

static void logMediaLoadRequest(Page* page, const String& mediaEngine, const String& errorMessage, bool succeeded)
{
    if (!page)
        return;

    CheckedRef diagnosticLoggingClient = page->diagnosticLoggingClient();
    if (!succeeded) {
        diagnosticLoggingClient->logDiagnosticMessageWithResult(DiagnosticLoggingKeys::mediaLoadingFailedKey(), errorMessage, DiagnosticLoggingResultFail, ShouldSample::No);
        return;
    }

    diagnosticLoggingClient->logDiagnosticMessage(DiagnosticLoggingKeys::mediaLoadedKey(), mediaEngine, ShouldSample::No);

    if (!page->hasSeenAnyMediaEngine())
        diagnosticLoggingClient->logDiagnosticMessage(DiagnosticLoggingKeys::pageContainsAtLeastOneMediaEngineKey(), emptyString(), ShouldSample::No);

    if (!page->hasSeenMediaEngine(mediaEngine))
        diagnosticLoggingClient->logDiagnosticMessage(DiagnosticLoggingKeys::pageContainsMediaEngineKey(), mediaEngine, ShouldSample::No);

    page->sawMediaEngine(mediaEngine);
}

void HTMLMediaElement::mediaLoadingFailed(MediaPlayer::NetworkState error)
{
    stopPeriodicTimers();

    // If we failed while trying to load a <source> element, the movie was never parsed, and there are more
    // <source> children, schedule the next one
    if (m_readyState < HAVE_METADATA && m_loadState == LoadingFromSourceElement) {

        // resource selection algorithm
        // Step 9.Otherwise.9 - Failed with elements: Queue a task, using the DOM manipulation task source, to fire a simple event named error at the candidate element.
        if (RefPtr currentSourceNode = m_currentSourceNode)
            currentSourceNode->scheduleErrorEvent();
        else
            ALWAYS_LOG(LOGIDENTIFIER, "error event not sent, <source> was removed");

        // 9.Otherwise.10 - Asynchronously await a stable state. The synchronous section consists of all the remaining steps of this algorithm until the algorithm says the synchronous section has ended.

        // 9.Otherwise.11 - Forget the media element's media-resource-specific tracks.
        forgetResourceSpecificTracks();

        if (havePotentialSourceChild()) {
            ALWAYS_LOG(LOGIDENTIFIER, "scheduling next <source>");
            scheduleNextSourceChild();
        } else {
            ALWAYS_LOG(LOGIDENTIFIER, "no more <source> elements, waiting");
            waitForSourceChange();
        }

        return;
    }

    ERROR_LOG(LOGIDENTIFIER, "error = ", error);

    if ((error == MediaPlayer::NetworkState::NetworkError && m_readyState >= HAVE_METADATA) || error == MediaPlayer::NetworkState::DecodeError)
        mediaLoadingFailedFatally(error);
    else if ((error == MediaPlayer::NetworkState::FormatError || error == MediaPlayer::NetworkState::NetworkError) && m_loadState == LoadingFromSrcAttr)
        noneSupported();

    logMediaLoadRequest(document().protectedPage().get(), String(), convertEnumerationToString(error), false);

    Ref mediaSession = this->mediaSession();
    mediaSession->clientCharacteristicsChanged(false);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (!m_hasPlaybackTargetAvailabilityListeners)
        mediaSession->setActive(false);
#else
    mediaSession->setActive(false);
#endif
}

void HTMLMediaElement::setNetworkState(MediaPlayer::NetworkState state)
{
    if (static_cast<int>(state) != static_cast<int>(m_networkState))
        HTMLMEDIAELEMENT_RELEASE_LOG(SETNETWORKSTATE, convertEnumerationToString(state).utf8(), convertEnumerationToString(m_networkState).utf8());

    if (state == MediaPlayer::NetworkState::Empty) {
        // Just update the cached state and leave, we can't do anything.
        m_networkState = NETWORK_EMPTY;
        updateBufferingState();
        updateStalledState();
        return;
    }

    if (state == MediaPlayer::NetworkState::FormatError && m_readyState < HAVE_METADATA && m_loadState == LoadingFromSrcAttr && needsContentTypeToPlay() && m_firstTimePlaying && !m_sniffer && !m_networkErrorOccured && m_lastContentTypeUsed) {
        // We couldn't find a suitable MediaPlayer, this could be due to the content-type having been initially set incorrectly.
        auto url = m_blob ? m_blobURLForReading.url() : currentSrc();
        sniffForContentType(url)->whenSettled(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }, url, player = m_player, lastContentType = *m_lastContentTypeUsed](auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (!result) {
                if (result.error() != PlatformMediaError::Cancelled)
                    protectedThis->mediaLoadingFailed(MediaPlayer::NetworkState::NetworkError);
                return;
            }
            player->reset();

            MediaPlayer::LoadOptions options = {
                .contentType = *result,
                .requiresRemotePlayback = !!protectedThis->m_remotePlaybackConfiguration,
                .supportsLimitedMatroska = protectedThis->limitedMatroskaSupportEnabled(),
            };
#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
            if (protectedThis->document().settings().mediaSourcePrefersDecompressionSession())
                options.videoRendererPreferences = videoRendererPreferences(protectedThis->document().settings(), protectedThis->m_forceStereoDecoding);
#endif
#if PLATFORM(IOS_FAMILY)
            if (protectedThis->canShowWhileLocked())
                options.videoRendererPreferences |= VideoRendererPreference::CanShowWhileLocked;
#endif

            if (result->isEmpty() || lastContentType == *result || !player->load(url, options))
                protectedThis->mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
            else
                protectedThis->mediaPlayerRenderingModeChanged();
        });
        return;
    }

    if (state == MediaPlayer::NetworkState::FormatError || state == MediaPlayer::NetworkState::NetworkError || state == MediaPlayer::NetworkState::DecodeError) {
        mediaLoadingFailed(state);
        return;
    }

    if (state == MediaPlayer::NetworkState::Idle) {
        if (m_networkState > NETWORK_IDLE) {
            changeNetworkStateFromLoadingToIdle();
            setShouldDelayLoadEvent(false);
        } else {
            m_networkState = NETWORK_IDLE;
        }
    }

    if (state == MediaPlayer::NetworkState::Loading) {
        if (m_networkState < NETWORK_LOADING || m_networkState == NETWORK_NO_SOURCE)
            startProgressEventTimer();
        m_networkState = NETWORK_LOADING;
    }

    if (state == MediaPlayer::NetworkState::Loaded) {
        if (m_networkState != NETWORK_IDLE)
            changeNetworkStateFromLoadingToIdle();
        m_completelyLoaded = true;
    }

    updateBufferingState();
    updateStalledState();
}

void HTMLMediaElement::changeNetworkStateFromLoadingToIdle()
{
    m_progressEventTimer.stop();

    // Schedule one last progress event so we guarantee that at least one is fired
    // for files that load very quickly.
    scheduleEvent(eventNames().progressEvent);
    scheduleEvent(eventNames().suspendEvent);
    m_networkState = NETWORK_IDLE;
}

void HTMLMediaElement::mediaPlayerReadyStateChanged()
{
    if (isSuspended()) {
        // FIXME: In some situations the MediaSource closing procedure triggerring a readyState
        // update on the player, while the media element is suspended would lead to infinite
        // recursion. The workaround is to attempt a fixed amount of recursions.
        if (!m_isChangingReadyStateWhileSuspended) {
            m_isChangingReadyStateWhileSuspended = true;
            m_remainingReadyStateChangedAttempts.store(128);
        }

        if (m_remainingReadyStateChangedAttempts.exchangeSub(1)) {
            queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& element) {
                element.mediaPlayerReadyStateChanged();
            });
        }
        return;
    }

    beginProcessingMediaPlayerCallback();

    setReadyState(protectedPlayer()->readyState());

    endProcessingMediaPlayerCallback();

    m_isChangingReadyStateWhileSuspended = false;
    m_remainingReadyStateChangedAttempts.store(0);
}

Expected<void, MediaPlaybackDenialReason> HTMLMediaElement::canTransitionFromAutoplayToPlay() const
{
    if (m_readyState != HAVE_ENOUGH_DATA) {
        HTMLMEDIAELEMENT_RELEASE_LOG(CANTRANSITIONFROMAUTOPLAYTOPLAY_NOT_ENOUGH_DATA);
        return makeUnexpected(MediaPlaybackDenialReason::PageConsentRequired);
    }
    if (!isAutoplaying()) {
        HTMLMEDIAELEMENT_RELEASE_LOG(CANTRANSITIONFROMAUTOPLAYTOPLAY_NOT_AUTOPLAYING);
        return makeUnexpected(MediaPlaybackDenialReason::PageConsentRequired);
    }
    Ref mediaSession = this->mediaSession();
    if (!mediaSession->autoplayPermitted()) {
        ALWAYS_LOG(LOGIDENTIFIER, "!mediaSession().autoplayPermitted");
        return makeUnexpected(MediaPlaybackDenialReason::PageConsentRequired);
    }
    if (!paused()) {
        ALWAYS_LOG(LOGIDENTIFIER, "!paused");
        return makeUnexpected(MediaPlaybackDenialReason::PageConsentRequired);
    }
    if (!autoplay()) {
        ALWAYS_LOG(LOGIDENTIFIER, "!autoplay");
        return makeUnexpected(MediaPlaybackDenialReason::PageConsentRequired);
    }
    if (pausedForUserInteraction()) {
        ALWAYS_LOG(LOGIDENTIFIER, "pausedForUserInteraction");
        return makeUnexpected(MediaPlaybackDenialReason::PageConsentRequired);
    }
    if (document().isSandboxed(SandboxFlag::AutomaticFeatures)) {
        ALWAYS_LOG(LOGIDENTIFIER, "isSandboxed");
        return makeUnexpected(MediaPlaybackDenialReason::PageConsentRequired);
    }

    auto permitted = mediaSession->playbackStateChangePermitted(MediaPlaybackState::Playing);
#if !RELEASE_LOG_DISABLED
    if (!permitted)
        ALWAYS_LOG(LOGIDENTIFIER, permitted.error());
    else
        ALWAYS_LOG(LOGIDENTIFIER, "can transition!");
#endif

    return permitted;
}

void HTMLMediaElement::dispatchPlayPauseEventsIfNeedsQuirks()
{
    if (!protectedDocument()->quirks().needsAutoplayPlayPauseEvents())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    scheduleEvent(eventNames().playingEvent);
    scheduleEvent(eventNames().pauseEvent);
}

void HTMLMediaElement::durationChanged()
{
    if (m_textTracks)
        m_textTracks->setDuration(durationMediaTime());
    scheduleEvent(eventNames().durationchangeEvent);
}

void HTMLMediaElement::applyConfiguration(const RemotePlaybackConfiguration& configuration)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (configuration.currentTime)
        setCurrentTime(configuration.currentTime);
    if (configuration.rate != 1)
        setPlaybackRate(configuration.rate);
    if (!configuration.paused)
        resumeAutoplaying();
}

void HTMLMediaElement::setReadyState(MediaPlayer::ReadyState state)
{
    // Set "wasPotentiallyPlaying" BEFORE updating m_readyState, potentiallyPlaying() uses it
    bool wasPotentiallyPlaying = potentiallyPlaying();

    ReadyState oldState = m_readyState;
    ReadyState newState = static_cast<ReadyState>(state);

    bool tracksAreReady = textTracksAreReady();

    if (newState == oldState && m_tracksAreReady == tracksAreReady)
        return;

    m_tracksAreReady = tracksAreReady;

    HTMLMEDIAELEMENT_RELEASE_LOG(SETREADYSTATE, convertEnumerationToString(state).utf8(), convertEnumerationToString(m_readyState).utf8());

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

    RefPtr player = m_player;
    if (m_seeking) {
        // 4.8.10.9, step 11
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA) {
            ALWAYS_LOG(LOGIDENTIFIER, "queuing waiting event, currentTime = ", currentMediaTime());
            scheduleEvent(eventNames().waitingEvent);
        }

        // 4.8.10.10 step 14 & 15.
        if (m_seekRequested && !player->seeking() && m_readyState >= HAVE_CURRENT_DATA)
            finishSeek();
    } else {
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA) {
            // 4.8.10.8
            invalidateOfficialPlaybackPosition();
            scheduleTimeupdateEvent(false);
            scheduleEvent(eventNames().waitingEvent);
        }
    }

    // Apply the first applicable set of substeps from the following list:
    do {
        // FIXME: The specification seems to only say HAVE_METADATA
        // explicitly (rather than or higher) for this state. It's unclear
        // if/how things like loadedmetadataEvent should happen if
        // we go directly from below HAVE_METADATA to higher than
        // HAVE_METADATA.
        if (m_readyState >= HAVE_METADATA && oldState < HAVE_METADATA) {
            prepareMediaFragmentURI();
            durationChanged();
            scheduleResizeEvent(player->naturalSize());
            scheduleEvent(eventNames().loadedmetadataEvent);

            if (m_defaultPlaybackStartPosition > MediaTime::zeroTime()) {
                // We reset it before to cause currentMediaTime() to return the actual current time (not
                // defaultPlaybackPosition) and avoid the seek code to think that the seek was already done.
                MediaTime seekTarget = m_defaultPlaybackStartPosition;
                m_defaultPlaybackStartPosition = MediaTime::zeroTime();
                seekInternal(seekTarget);
            }

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
            if (hasEnabledTargetAvailabilityListeners())
                enqueuePlaybackTargetAvailabilityChangedEvent(EnqueueBehavior::OnlyWhenChanged);
#endif

            updateRenderer();

            if (RefPtr mediaDocument = dynamicDowncast<MediaDocument>(document()))
                mediaDocument->mediaElementNaturalSizeChanged(expandedIntSize(player->naturalSize()));

            logMediaLoadRequest(document().protectedPage().get(), player->engineDescription(), String(), true);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
            scheduleUpdateMediaState();
#endif

            protectedMediaSession()->clientCharacteristicsChanged(false);

            // As the spec only mentiones HAVE_METADATA, run the later
            // steps if we are moving to a higher state.
            if (m_readyState == HAVE_METADATA)
                break;
        }

        if (m_readyState >= HAVE_CURRENT_DATA && oldState < HAVE_CURRENT_DATA) {
            if (!m_haveFiredLoadedData) {
                m_haveFiredLoadedData = true;
                scheduleEvent(eventNames().loadeddataEvent);
                // FIXME: It's not clear that it's correct to skip these this operation just
                // because m_haveFiredLoadedData is already true. At one time we were skipping
                // the call to setShouldDelayLoadEvent, which was definitely incorrect.
                applyMediaFragmentURI();
            }
            setShouldDelayLoadEvent(false);

            // If the new ready state is HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA, then the relevant steps below must then be run also.
            if (m_readyState < HAVE_FUTURE_DATA)
                break;
        }

        if (!tracksAreReady)
            break;

        if (oldState < HAVE_FUTURE_DATA && m_readyState >= HAVE_FUTURE_DATA) {
            if (m_remotePlaybackConfiguration) {
                applyConfiguration(*m_remotePlaybackConfiguration);
                m_remotePlaybackConfiguration = { };
            }
        }

        if (m_readyState == HAVE_FUTURE_DATA && oldState <= HAVE_CURRENT_DATA) {
            scheduleEvent(eventNames().canplayEvent);

            // If the element’s paused attribute is false, the user agent must queue a task to fire a simple event named playing at the element.
            if (!paused())
                scheduleNotifyAboutPlaying();
            break;
        }

        if (m_readyState == HAVE_ENOUGH_DATA && oldState < HAVE_ENOUGH_DATA) {
            // If the previous ready state was HAVE_CURRENT_DATA or less,
            // the user agent must queue a media element task given the media element to fire an event named canplay at the element,
            // and, if the element's paused attribute is false, notify about playing for the element.
            if (oldState <= HAVE_CURRENT_DATA) {
                scheduleEvent(eventNames().canplayEvent);
                if (!paused())
                    scheduleNotifyAboutPlaying();
            }

            // The user agent must queue a media element task given the media element to fire an event named canplaythrough at the element.
            scheduleEvent(eventNames().canplaythroughEvent);

            // If the element is not eligible for autoplay, then the user agent must abort these substeps.
            // The user agent may run the following substeps:
            // Set the paused attribute to false.
            // If the element's show poster flag is true, set it to false and run the time marches on steps.
            // Queue a media element task given the element to fire an event named play at the element.
            // Notify about playing for the element.
            auto canTransition = canTransitionFromAutoplayToPlay();
            if (canTransition) {
                setPaused(false);
                setShowPosterFlag(false);
                invalidateOfficialPlaybackPosition();
                setAutoplayEventPlaybackState(AutoplayEventPlaybackState::StartedWithoutUserGesture);
                m_playbackStartedTime = currentMediaTime().toDouble();
                scheduleEvent(eventNames().playEvent);
                scheduleNotifyAboutPlaying();
            } else if (canTransition.error() == MediaPlaybackDenialReason::UserGestureRequired) {
                ALWAYS_LOG(LOGIDENTIFIER, "Autoplay blocked, user gesture required");
                setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
            }
        }
    } while (false);

    // If we transition to the Future Data state and we're about to begin playing, ensure playback is actually permitted first,
    // honoring any playback denial reasons such as the requirement of a user gesture.
    if (m_readyState == HAVE_FUTURE_DATA && oldState < HAVE_FUTURE_DATA && potentiallyPlaying() && !mediaSession().playbackStateChangePermitted(MediaPlaybackState::Playing)) {
        auto canTransition = canTransitionFromAutoplayToPlay();
        if (!canTransition && canTransition.error() == MediaPlaybackDenialReason::UserGestureRequired)
            ALWAYS_LOG(LOGIDENTIFIER, "Autoplay blocked, user gesture required");

        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    }

    updatePlayState();
    updateMediaController();
    updateActiveTextTrackCues(currentMediaTime());

    updateBufferingState();
    updateStalledState();
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#if ENABLE(ENCRYPTED_MEDIA)
void HTMLMediaElement::updateShouldContinueAfterNeedKey()
{
    RefPtr player = m_player;
    if (!player)
        return;
    bool shouldContinue = hasEventListeners(eventNames().webkitneedkeyEvent) || (protectedDocument()->settings().encryptedMediaAPIEnabled() && !protectedDocument()->quirks().hasBrokenEncryptedMediaAPISupportQuirk());
    player->setShouldContinueAfterKeyNeeded(shouldContinue);
}
#endif

RefPtr<ArrayBuffer> HTMLMediaElement::mediaPlayerCachedKeyForKeyId(const String& keyId) const
{
    RefPtr protectedWebKitMediaKeys = m_webKitMediaKeys;
    return protectedWebKitMediaKeys ? protectedWebKitMediaKeys->cachedKeyForKeyId(keyId) : nullptr;
}

void HTMLMediaElement::mediaPlayerKeyNeeded(const SharedBuffer& initData)
{
    if (!document().settings().legacyEncryptedMediaAPIEnabled())
        return;

    if (!hasEventListeners(eventNames().webkitneedkeyEvent)
#if ENABLE(ENCRYPTED_MEDIA)
        // Only fire an error if ENCRYPTED_MEDIA is not enabled, to give clients of the
        // "encrypted" event a chance to handle it without resulting in a synthetic error.
        && (!protectedDocument()->settings().encryptedMediaAPIEnabled() || protectedDocument()->quirks().hasBrokenEncryptedMediaAPISupportQuirk())
#endif
        ) {
        m_error = MediaError::create(MediaError::MEDIA_ERR_ENCRYPTED, "Media is encrypted"_s);
        scheduleEvent(eventNames().errorEvent);
        return;
    }

    WebKitMediaKeyNeededEvent::Init init;

    if (auto initDataBuffer = initData.tryCreateArrayBuffer())
        init.initData = Uint8Array::create(initDataBuffer.releaseNonNull());

    Ref event = WebKitMediaKeyNeededEvent::create(eventNames().webkitneedkeyEvent, init);
    scheduleEvent(WTFMove(event));
}

String HTMLMediaElement::mediaPlayerMediaKeysStorageDirectory() const
{
    return protectedDocument()->mediaKeysStorageDirectory();
}

void HTMLMediaElement::webkitSetMediaKeys(WebKitMediaKeys* mediaKeys)
{
    if (!document().settings().legacyEncryptedMediaAPIEnabled())
        return;

    if (m_webKitMediaKeys == mediaKeys)
        return;

    if (RefPtr webKitMediaKeys = m_webKitMediaKeys)
        webKitMediaKeys->setMediaElement(nullptr);
    m_webKitMediaKeys = mediaKeys;
    if (RefPtr webKitMediaKeys = m_webKitMediaKeys)
        webKitMediaKeys->setMediaElement(this);
}

void HTMLMediaElement::keyAdded()
{
    if (!document().settings().legacyEncryptedMediaAPIEnabled())
        return;

    if (RefPtr player = m_player)
        player->keyAdded();
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
        promise->reject(ExceptionCode::InvalidStateError);
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
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [mediaKeys = RefPtr { mediaKeys }, promise = WTFMove(promise)](auto& element) mutable {
        if (element.isContextStopped())
            return;

        // 5.1. If all the following conditions hold:
        //      - mediaKeys is not null,
        //      - the CDM instance represented by mediaKeys is already in use by another media element
        //      - the user agent is unable to use it with this element
        //      then let this object's attaching media keys value be false and reject promise with a QuotaExceededError.
        // FIXME: ^

        // 5.2. If the mediaKeys attribute is not null, run the following steps:
        if (element.m_mediaKeys) {
            // 5.2.1. If the user agent or CDM do not support removing the association, let this object's attaching media keys value be false and reject promise with a NotSupportedError.
            // 5.2.2. If the association cannot currently be removed, let this object's attaching media keys value be false and reject promise with an InvalidStateError.
            // 5.2.3. Stop using the CDM instance represented by the mediaKeys attribute to decrypt media data and remove the association with the media element.
            // 5.2.4. If the preceding step failed, let this object's attaching media keys value be false and reject promise with the appropriate error name.
            // FIXME: ^

            element.m_mediaKeys->detachCDMClient(element);
            if (RefPtr player = element.m_player)
                player->cdmInstanceDetached(element.m_mediaKeys->cdmInstance());
        }

        // 5.3. If mediaKeys is not null, run the following steps:
        if (mediaKeys) {
            // 5.3.1. Associate the CDM instance represented by mediaKeys with the media element for decrypting media data.
            mediaKeys->attachCDMClient(element);
            if (RefPtr player = element.m_player)
                player->cdmInstanceAttached(mediaKeys->cdmInstance());

            // 5.3.2. If the preceding step failed, run the following steps:
            //   5.3.2.1. Set the mediaKeys attribute to null.
            //   5.3.2.2. Let this object's attaching media keys value be false.
            //   5.3.2.3. Reject promise with a new DOMException whose name is the appropriate error name.
            // FIXME: ^

            // 5.3.3. Queue a task to run the Attempt to Resume Playback If Necessary algorithm on the media element.
            queueTaskKeepingObjectAlive(element, TaskSource::MediaElement, [](auto& element) {
                if (!element.isContextStopped())
                    element.attemptToResumePlaybackIfNecessary();
            });
        }

        // 5.4. Set the mediaKeys attribute to mediaKeys.
        // 5.5. Let this object's attaching media keys value be false.
        // 5.6. Resolve promise.
        element.m_mediaKeys = WTFMove(mediaKeys);
        element.m_attachingMediaKeys = false;
        promise->resolve();
    });

    // 6. Return promise.
}

void HTMLMediaElement::mediaPlayerInitializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    if (!protectedDocument()->settings().encryptedMediaAPIEnabled() || protectedDocument()->quirks().hasBrokenEncryptedMediaAPISupportQuirk())
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
    scheduleEvent(MediaEncryptedEvent::create(eventNames().encryptedEvent, initializer, Event::IsTrusted::Yes));
}

void HTMLMediaElement::mediaPlayerWaitingForKeyChanged()
{
    RefPtr player = m_player;
    if (!player)
        return;

    if (!player->waitingForKey() && m_playbackBlockedWaitingForKey) {
        // https://w3c.github.io/encrypted-media/#resume-playback
        // W3C Editor's Draft 23 June 2017

        // NOTE: continued from HTMLMediaElement::attemptToDecrypt().
        // 4. If the user agent can advance the current playback position in the direction of playback:
        //   4.1. Set the media element's decryption blocked waiting for key value to false.
        // FIXME: ^
        //   4.2. Set the media element's playback blocked waiting for key value to false.
        m_playbackBlockedWaitingForKey = false;

        //   4.3. Set the media element's readyState value to HAVE_CURRENT_DATA, HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA as appropriate.
        setReadyState(player->readyState());

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
    if (RefPtr mediaKeys = m_mediaKeys) {
        // 3.1. Let media keys be the MediaKeys object referenced by that attribute.
        // 3.2. Let cdm be the CDM instance represented by media keys's cdm instance value.
        Ref cdmInstance = mediaKeys->cdmInstance();

        // 3.3. If cdm is no longer usable for any reason, run the following steps:
        //   3.3.1. Run the media data is corrupted steps of the resource fetch algorithm.
        //   3.3.2. Run the CDM Unavailable algorithm on media keys.
        //   3.3.3. Abort these steps.
        // FIXME: ^

        // 3.4. If there is at least one MediaKeySession created by the media keys that is not closed, run the following steps:
        if (mediaKeys->hasOpenSessions()) {
            // Continued in MediaPlayer::attemptToDecryptWithInstance().
            if (RefPtr player = m_player)
                player->attemptToDecryptWithInstance(cdmInstance);
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

void HTMLMediaElement::cdmClientUnrequestedInitializationDataReceived(const String& initDataType, Ref<SharedBuffer>&& initData)
{
    mediaPlayerInitializationDataEncountered(initDataType, initData->tryCreateArrayBuffer());
}

#endif // ENABLE(ENCRYPTED_MEDIA)

void HTMLMediaElement::progressEventTimerFired()
{
    ASSERT(m_player);
    if (m_networkState != NETWORK_LOADING)
        return;

    updateSleepDisabling();

    Ref player = *m_player;
    if (!player->supportsProgressMonitoring())
        return;

    player->didLoadingProgress([weakThis = WeakPtr { *this }](bool progress) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        MonotonicTime time = MonotonicTime::now();
        Seconds timedelta = time - protectedThis->m_previousProgressTime;
        if (progress) {
            protectedThis->scheduleEvent(eventNames().progressEvent);
            protectedThis->m_previousProgressTime = time;
            if (protectedThis->m_sentStalledEvent) {
                protectedThis->m_sentStalledEvent = false;
                protectedThis->updateStalledState();
            }
            protectedThis->updateRenderer();
        } else if (timedelta > 3_s && !protectedThis->m_sentStalledEvent) {
            protectedThis->scheduleEvent(eventNames().stalledEvent);
            protectedThis->m_sentStalledEvent = true;
            protectedThis->updateStalledState();
            protectedThis->setShouldDelayLoadEvent(false);
        }
    });
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
    RefPtr player = m_player;
    return player ? player->supportsScanning() : false;
}

void HTMLMediaElement::prepareToPlay()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    INFO_LOG(LOGIDENTIFIER);
    if (m_havePreparedToPlay || !document().hasBrowsingContext())
        return;
    m_havePreparedToPlay = true;
    m_hasEverPreparedToPlay = true;
    if (RefPtr player = m_player)
        player->prepareToPlay();
}

void HTMLMediaElement::fastSeek(double time)
{
    fastSeek(MediaTime::createWithDouble(time));
}

void HTMLMediaElement::fastSeek(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);
    // 4.7.10.9 Seeking
    // 9. If the approximate-for-speed flag is set, adjust the new playback position to a value that will
    // allow for playback to resume promptly. If new playback position before this step is before current
    // playback position, then the adjusted new playback position must also be before the current playback
    // position. Similarly, if the new playback position before this step is after current playback position,
    // then the adjusted new playback position must also be after the current playback position.
    invalidateOfficialPlaybackPosition();
    MediaTime delta = time - currentMediaTime();
    MediaTime negativeTolerance = delta < MediaTime::zeroTime() ? MediaTime::positiveInfiniteTime() : delta;
    seekWithTolerance({ time, negativeTolerance, MediaTime::zeroTime() }, true);
}

#if ENABLE(MEDIA_STREAM)
void HTMLMediaElement::setAudioOutputDevice(String&& deviceId, DOMPromiseDeferred<void>&& promise)
{
    RefPtr window = document().window();
    RefPtr mediaDevices = window ? NavigatorMediaDevices::mediaDevices(window->navigator()) : nullptr;
    if (!mediaDevices) {
        promise.reject(Exception { ExceptionCode::NotAllowedError });
        return;
    }

    if (!protectedDocument()->processingUserGestureForMedia() && document().settings().speakerSelectionRequiresUserGesture()) {
        ERROR_LOG(LOGIDENTIFIER, "rejecting promise as a user gesture is required");
        promise.reject(Exception { ExceptionCode::NotAllowedError, "A user gesture is required"_s });
        return;
    }

    if (deviceId.isEmpty())
        deviceId = { };

    if (deviceId == m_audioOutputHashedDeviceId) {
        promise.resolve();
        return;
    }

    String persistentId;
    if (!deviceId.isNull()) {
        persistentId = mediaDevices->deviceIdToPersistentId(deviceId);
        if (persistentId.isNull()) {
            promise.reject(Exception { ExceptionCode::NotFoundError });
            return;
        }
    }

    m_audioOutputPersistentDeviceId = WTFMove(persistentId);
    if (RefPtr player = m_player)
        player->audioOutputDeviceChanged();

    protectedScriptExecutionContext()->checkedEventLoop()->queueTask(TaskSource::MediaElement, [this, protectedThis = Ref { *this }, deviceId = WTFMove(deviceId), promise = WTFMove(promise)]() mutable {
        m_audioOutputHashedDeviceId = WTFMove(deviceId);
        promise.resolve();
    });
}
#endif

void HTMLMediaElement::seek(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);
    seekWithTolerance({ time, MediaTime::zeroTime(), MediaTime::zeroTime() }, true);
}

void HTMLMediaElement::seekInternal(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);
    seekWithTolerance({ time, MediaTime::zeroTime(), MediaTime::zeroTime() }, false);
}

void HTMLMediaElement::seekWithTolerance(const SeekTarget& target, bool fromDOM)
{
    ALWAYS_LOG(LOGIDENTIFIER, "SeekTarget = ", target);
    // 4.8.10.9 Seeking

    // 1 - Set the media element's show poster flag to false.
    setShowPosterFlag(false);

    // 2 - If the media element's readyState is HAVE_NOTHING, abort these steps.
    if (m_readyState == HAVE_NOTHING)
        return;

    RefPtr player = m_player;
    if (!player)
        return;

    // If the media engine has been told to postpone loading data, let it go ahead now.
    if (m_preload < MediaPlayer::Preload::Auto && m_readyState < HAVE_FUTURE_DATA)
        prepareToPlay();

    // Get the current time before setting m_seeking, m_lastSeekTime is returned once it is set.
    invalidateOfficialPlaybackPosition();
    MediaTime now = currentMediaTime();

    // 3 - If the element's seeking IDL attribute is true, then another instance of this algorithm is
    // already running. Abort that other instance of the algorithm without waiting for the step that
    // it is running to complete.
    if (m_seekTaskCancellationGroup.hasPendingTask()) {
        INFO_LOG(LOGIDENTIFIER, "cancelling pending seeks");
        m_seekTaskCancellationGroup.cancel();
        if (m_pendingSeek) {
            now = m_pendingSeek->now;
            m_pendingSeek = nullptr;
        }
        m_pendingSeekType = NoSeek;
    }

    // 4 - Set the seeking IDL attribute to true.
    // The flag will be cleared when the engine tells us the time has actually changed.
    setSeeking(true);
    if (m_playing) {
        if (m_lastSeekTime < now)
            addPlayedRange(m_lastSeekTime, now);
    }
    m_lastSeekTime = std::min(target.time, durationMediaTime());
    player->willSeekToTarget(target.time);

    // 5 - If the seek was in response to a DOM method call or setting of an IDL attribute, then continue
    // the script. The remainder of these steps must be run asynchronously.
    m_pendingSeek = makeUnique<PendingSeek>(now, target);
    if (fromDOM) {
        ALWAYS_LOG(LOGIDENTIFIER, "enqueuing seek from ", now, " to ", target.time);
        queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_seekTaskCancellationGroup, [](auto& element) { element.seekTask(); });
    } else
        seekTask();

    if (processingUserGestureForMedia())
        protectedMediaSession()->removeBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager);

    ImageOverlay::removeOverlaySoonIfNeeded(*this);
}

void HTMLMediaElement::seekTask()
{
    INFO_LOG(LOGIDENTIFIER);

    RefPtr player = m_player;
    if (!player) {
        clearSeeking();
        return;
    }

    ASSERT(m_pendingSeek);
    MediaTime now = m_pendingSeek->now;
    MediaTime time = m_pendingSeek->target.time;
    MediaTime negativeTolerance = m_pendingSeek->target.negativeThreshold;
    MediaTime positiveTolerance = m_pendingSeek->target.positiveThreshold;
    m_pendingSeek = nullptr;

    ASSERT(negativeTolerance.isValid());
    ASSERT(negativeTolerance >= MediaTime::zeroTime());
    ASSERT(positiveTolerance.isValid());
    ASSERT(positiveTolerance >= MediaTime::zeroTime());

    // 6 - If the new playback position is later than the end of the media resource, then let it be the end
    // of the media resource instead.
    time = std::min(time, durationMediaTime());

    // 7 - If the new playback position is less than the earliest possible position, let it be that position instead.
    MediaTime earliestTime = player->startTime();
    time = std::max(time, earliestTime);

    // Ask the media engine for the time value in the movie's time scale before comparing with current time. This
    // is necessary because if the seek time is not equal to currentTime but the delta is less than the movie's
    // time scale, we will ask the media engine to "seek" to the current movie time, which may be a noop and
    // not generate a timechanged callback. This means m_seeking will never be cleared and we will never
    // fire a 'seeked' event.
    if (willLog(WTFLogLevel::Info)) {
        MediaTime mediaTime = player->mediaTimeForTimeValue(time);
        if (time != mediaTime)
            INFO_LOG(LOGIDENTIFIER, time, " media timeline equivalent is ", mediaTime);
    }

    time = player->mediaTimeForTimeValue(time);

    // 8 - If the (possibly now changed) new playback position is not in one of the ranges given in the
    // seekable attribute, then let it be the position in one of the ranges given in the seekable attribute
    // that is the nearest to the new playback position. ... If there are no ranges given in the seekable
    // attribute then set the seeking IDL attribute to false and abort these steps.
    RefPtr seekableRanges = seekable();
    bool noSeekRequired = !seekableRanges->length();

    // Short circuit seeking to the current time by just firing the events if no seek is required.
    // Don't skip calling the media engine if 1) we are in poster mode (because a seek should always cancel
    // poster display), or 2) if there is a pending fast seek, or 3) if this seek is not an exact seek
    SeekType thisSeekType = (negativeTolerance == MediaTime::zeroTime() && positiveTolerance == MediaTime::zeroTime()) ? Precise : Fast;
    if (!noSeekRequired && time == now && thisSeekType == Precise && m_pendingSeekType != Fast && !showPosterFlag())
        noSeekRequired = true;

#if ENABLE(MEDIA_SOURCE)
    // Always notify the media engine of a seek if the source is not closed and there is seekable ranges.
    // This ensures that the source is always in a flushed state when the 'seeking' event fires.
    if (RefPtr mediaSource = m_mediaSource; mediaSource && !mediaSource->isClosed() && seekableRanges->length())
        noSeekRequired = false;
#endif

    if (noSeekRequired) {
        ALWAYS_LOG(LOGIDENTIFIER, "ignored seek to ", time);
        if (time == now) {
            scheduleEvent(eventNames().seekingEvent);
            scheduleTimeupdateEvent(false);
            scheduleEvent(eventNames().seekedEvent);

            if (protectedDocument()->quirks().needsCanPlayAfterSeekedQuirk() && m_readyState > HAVE_CURRENT_DATA)
                scheduleEvent(eventNames().canplayEvent);
        }
        clearSeeking();
        return;
    }
    time = seekableRanges->ranges().nearest(time);

    m_sentEndEvent = false;
    m_lastSeekTime = time;
    m_pendingSeekType = thisSeekType;
    setSeeking(true);

    // 10 - Queue a task to fire a simple event named seeking at the element.
    scheduleEvent(eventNames().seekingEvent);

    // 11 - Set the current playback position to the given new playback position
    m_seekRequested = true;
    player->seekToTarget({ time, negativeTolerance, positiveTolerance });

    // 12 - Wait until the user agent has established whether or not the media data for the new playback
    // position is available, and, if it is, until it has decoded enough data to play back that position.
    // 13 - Await a stable state. The synchronous section consists of all the remaining steps of this algorithm.

    if (!shouldSpeakCueTextForTime(time))
        cancelSpeakingCueText();
}

void HTMLMediaElement::clearSeeking()
{
    if (RefPtr player = m_player)
        player->willSeekToTarget(MediaTime::invalidTime());
    setSeeking(false);
    m_seekRequested = false;
    m_pendingSeekType = NoSeek;
    m_wasPlayingBeforeSeeking = false;
    invalidateOfficialPlaybackPosition();
}

void HTMLMediaElement::finishSeek()
{
    bool wasPlayingBeforeSeeking = m_wasPlayingBeforeSeeking;
    // 4.8.10.9 Seeking
    // 14 - Set the seeking IDL attribute to false.
    clearSeeking();

    ALWAYS_LOG(LOGIDENTIFIER, "current time = ", currentMediaTime(), ", pending seek = ", !!m_pendingSeek);

    if (!m_pendingSeek) {
        // Don't update text track cues immediately because there are frequently several seeks in quick
        // succession when time is changed by clicking in the media controls.
        queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_updateTextTracksTaskCancellationGroup, [](auto& element) {
            if (!element.m_ignoreTrackDisplayUpdate && element.m_inActiveDocument)
                element.updateActiveTextTrackCues(element.currentMediaTime());
        });
    }

    // 15 - Run the time maches on steps.
    // Handled by mediaPlayerTimeChanged().

    // 16 - Queue a task to fire a simple event named timeupdate at the element.
    scheduleEvent(eventNames().timeupdateEvent);

    // 17 - Queue a task to fire a simple event named seeked at the element.
    scheduleEvent(eventNames().seekedEvent);

    if (protectedDocument()->quirks().needsCanPlayAfterSeekedQuirk() && m_readyState > HAVE_CURRENT_DATA)
        scheduleEvent(eventNames().canplayEvent);

    if (RefPtr mediaSession = m_mediaSession)
        mediaSession->clientCharacteristicsChanged(true);

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource)
        mediaSource->monitorSourceBuffers();
#endif
    if (wasPlayingBeforeSeeking)
        playInternal();
}

HTMLMediaElement::ReadyState HTMLMediaElement::readyState() const
{
    return m_readyState;
}

MediaPlayer::MovieLoadType HTMLMediaElement::movieLoadType() const
{
    RefPtr player = m_player;
    return player ? player->movieLoadType() : MediaPlayer::MovieLoadType::Unknown;
}

std::optional<MediaSessionGroupIdentifier> HTMLMediaElement::mediaSessionGroupIdentifier() const
{
    RefPtr page = document().page();
    return page ? page->mediaSessionGroupIdentifier() : std::nullopt;
}

bool HTMLMediaElement::hasAudio() const
{
    RefPtr player = m_player;
    return player ? player->hasAudio() : false;
}

bool HTMLMediaElement::seeking() const
{
    return m_seeking;
}

void HTMLMediaElement::setSeeking(bool seeking)
{
    if (m_seeking == seeking)
        return;
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Seeking, seeking);
    m_seeking = seeking;
}

void HTMLMediaElement::invalidateOfficialPlaybackPosition()
{
    m_officialPlaybackPosition = MediaTime::invalidTime();
}

// playback state
double HTMLMediaElement::currentTime() const
{
    return currentMediaTime().toDouble();
}

MediaTime HTMLMediaElement::currentMediaTime() const
{
    RefPtr player = m_player;
    if (!player)
        return MediaTime::zeroTime();

    if (m_defaultPlaybackStartPosition != MediaTime::zeroTime())
        return m_defaultPlaybackStartPosition;

    if (m_seeking) {
        HTMLMEDIAELEMENT_RELEASE_LOG(CURRENTMEDIATIME_SEEKING, m_lastSeekTime.toFloat());
        return m_lastSeekTime;
    }

    if (m_officialPlaybackPosition.isValid() && m_paused)
        return m_officialPlaybackPosition;

    m_officialPlaybackPosition = player->currentTime();
    if (m_officialPlaybackPosition.isInvalid())
        return MediaTime::zeroTime();

    return m_officialPlaybackPosition;
}

void HTMLMediaElement::setCurrentTime(double time)
{
    setCurrentTime(MediaTime::createWithDouble(time));
}

void HTMLMediaElement::setCurrentTimeWithTolerance(double time, double toleranceBefore, double toleranceAfter)
{
    seekWithTolerance({ MediaTime::createWithDouble(time), MediaTime::createWithDouble(toleranceBefore), MediaTime::createWithDouble(toleranceAfter) }, true);
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
        return Exception { ExceptionCode::InvalidStateError };

    time = clampTo(time, 0.0);

    if (m_readyState == HAVE_NOTHING || !m_player) {
        m_defaultPlaybackStartPosition = MediaTime::createWithDouble(time);
        return { };
    }

    seek(MediaTime::createWithDouble(time));
    return { };
}

double HTMLMediaElement::duration() const
{
    return durationMediaTime().toDouble();
}

MediaTime HTMLMediaElement::durationMediaTime() const
{
#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource)
        return mediaSource->duration();
#endif

    if (RefPtr player = m_player; player && m_readyState >= HAVE_METADATA)
        return player->duration();

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

void HTMLMediaElement::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, {
        { CSSSelector::PseudoClass::Paused, paused },
        { CSSSelector::PseudoClass::Playing, !paused },
    });
    m_paused = paused;
    updateBufferingState();
    updateStalledState();
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
    RefPtr mediaController = m_mediaController;
    return mediaController ? mediaController->playbackRate() : m_reportedPlaybackRate;
}

double HTMLMediaElement::requestedPlaybackRate() const
{
    RefPtr mediaController = m_mediaController;
    return mediaController ? mediaController->playbackRate() : m_requestedPlaybackRate;
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
    HTMLMEDIAELEMENT_RELEASE_LOG(SETPLAYBACKRATE, rate);

#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // "playbackRate" - A MediaStream is not seekable. Therefore, this attribute must always
    // have the value 1.0 and any attempt to alter it must be ignored. Note that this also
    // means that the ratechange event will not fire.
    if (m_mediaStreamSrcObject)
        return;
#endif

    if (m_player && potentiallyPlaying() && !m_mediaController)
        RefPtr { m_player }->setRate(rate);

    if (m_requestedPlaybackRate != rate) {
        m_reportedPlaybackRate = m_requestedPlaybackRate = rate;
        scheduleEvent(eventNames().ratechangeEvent);
    }
}

void HTMLMediaElement::updatePlaybackRate()
{
    double requestedRate = requestedPlaybackRate();
    if (RefPtr player = m_player; player && potentiallyPlaying() && player->rate() != requestedRate)
        player->setRate(requestedRate);
}

bool HTMLMediaElement::preservesPitch() const
{
    return m_preservesPitch;
}

void HTMLMediaElement::setPreservesPitch(bool preservesPitch)
{
    INFO_LOG(LOGIDENTIFIER, preservesPitch);

    m_preservesPitch = preservesPitch;

    if (!m_player)
        return;

    RefPtr { m_player }->setPreservesPitch(preservesPitch);
}

double HTMLMediaElement::mediaPlayerCurrentTime() const
{
    if (RefPtr player = m_player)
        return player->currentTime().toDouble();
    return 0;
}

bool HTMLMediaElement::ended() const
{
#if ENABLE(MEDIA_STREAM)
    // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
    // When the MediaStream state moves from the active to the inactive state, the User Agent
    // must raise an ended event on the HTMLMediaElement and set its ended attribute to true.
    if (m_mediaStreamSrcObject) {
        if (RefPtr player = m_player; player && player->ended())
            return true;
    }
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
        return noneAtom();
#endif

    switch (m_preload) {
    case MediaPlayer::Preload::None:
        return noneAtom();
    case MediaPlayer::Preload::MetaData:
        return "metadata"_s;
    case MediaPlayer::Preload::Auto:
        return autoAtom();
    }

    ASSERT_NOT_REACHED();
    return String();
}

void HTMLMediaElement::setPreload(const AtomString& preload)
{
    ALWAYS_LOG(LOGIDENTIFIER, preload);
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
    HTMLMEDIAELEMENT_RELEASE_LOG(PLAY);

    Ref mediaSession = this->mediaSession();
    auto permitted = mediaSession->playbackStateChangePermitted(MediaPlaybackState::Playing);
    if (!permitted) {
        if (permitted.error() == MediaPlaybackDenialReason::UserGestureRequired)
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
        ERROR_LOG(LOGIDENTIFIER, "rejecting promise: ", permitted.error());
        promise.reject(ExceptionCode::NotAllowedError);
        return;
    }

    if (m_error && m_error->code() == MediaError::MEDIA_ERR_SRC_NOT_SUPPORTED) {
        ERROR_LOG(LOGIDENTIFIER, "rejecting promise because of error");
        promise.reject(ExceptionCode::NotSupportedError, "The operation is not supported."_s);
        return;
    }

    if (processingUserGestureForMedia())
        removeBehaviorRestrictionsAfterFirstUserGesture();
    else {
        // If we are allowed to explicitly play without a user gesture, remove the restriction
        // preventing invisible autoplay, as that will cause explicit playback to be interrupted
        // by updateShouldAutoplay().
        mediaSession->removeBehaviorRestriction(MediaElementSession::InvisibleAutoplayNotPermitted);
    }

    m_pendingPlayPromises.append(WTFMove(promise));
    playInternal();
}

void HTMLMediaElement::play()
{
    HTMLMEDIAELEMENT_RELEASE_LOG(PLAY);

    auto permitted = protectedMediaSession()->playbackStateChangePermitted(MediaPlaybackState::Playing);
    if (!permitted) {
        ERROR_LOG(LOGIDENTIFIER, "playback not permitted: ", permitted.error());
        if (permitted.error() == MediaPlaybackDenialReason::UserGestureRequired)
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
        return;
    }
    if (processingUserGestureForMedia())
        removeBehaviorRestrictionsAfterFirstUserGesture();

    playInternal();
}

void HTMLMediaElement::playInternal()
{
    HTMLMEDIAELEMENT_RELEASE_LOG(PLAYINTERNAL);

    if (isSuspended()) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning because context is suspended");
        return;
    }

    if (!document().hasBrowsingContext()) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning because there is no browsing context");
        return;
    }

    Ref mediaSession = this->mediaSession();
    mediaSession->setActive(true);
    if (!mediaSession->clientWillBeginPlayback()) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning because of interruption");
        return;
    }

    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY)
        selectMediaResource();

    if (endedPlayback())
        seekInternal(MediaTime::zeroTime());

    if (RefPtr mediaController = m_mediaController)
        mediaController->bringElementUpToSpeed(*this);

    if (m_paused) {
        setPaused(false);
        setShowPosterFlag(false);
        invalidateOfficialPlaybackPosition();

        // This avoids the first timeUpdated event after playback starts, when currentTime is still
        // the same as it was when the video was paused (and the time hasn't changed yet).
        m_lastTimeUpdateEventMovieTime = currentMediaTime();
        m_playbackStartedTime = m_lastTimeUpdateEventMovieTime.toDouble();

        scheduleEvent(eventNames().playEvent);

        // If the media element's readyState attribute has the value HAVE_NOTHING, HAVE_METADATA, or HAVE_CURRENT_DATA,
        // queue a media element task given the media element to fire an event named waiting at the element.
        // Otherwise, the media element's readyState attribute has the value HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA:
        // notify about playing for the element.
        if (m_readyState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().waitingEvent);
        else
            scheduleNotifyAboutPlaying();
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

    ImageOverlay::removeOverlaySoonIfNeeded(*this);
}

void HTMLMediaElement::pause()
{
    HTMLMEDIAELEMENT_RELEASE_LOG(PAUSE);

    m_temporarilyAllowingInlinePlaybackAfterFullscreen = false;

    if (m_waitingToEnterFullscreen)
        m_waitingToEnterFullscreen = false;

    if (!protectedMediaSession()->playbackStateChangePermitted(MediaPlaybackState::Paused))
        return;

    if (processingUserGestureForMedia())
        removeBehaviorRestrictionsAfterFirstUserGesture(MediaElementSession::RequireUserGestureToControlControlsManager);

    pauseInternal();
    // If we have a pending seek, ensure playback doesn't resume.
    m_wasPlayingBeforeSeeking = false;
}

void HTMLMediaElement::pauseInternal()
{
    HTMLMEDIAELEMENT_RELEASE_LOG(PAUSEINTERNAL);

    if (isSuspended()) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning because context is suspended");
        return;
    }

    if (!document().hasBrowsingContext()) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning because there is no browsing context");
        return;
    }

    Ref mediaSession = this->mediaSession();
    if (!mediaSession->clientWillPausePlayback()) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning because of interruption");
        return;
    }

    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY) {
        // Unless the restriction on media requiring user action has been lifted
        // don't trigger loading if a script calls pause().
        if (!mediaSession->playbackStateChangePermitted(MediaPlaybackState::Playing))
            return;
        selectMediaResource();
    }

    m_autoplaying = false;

    if (processingUserGestureForMedia())
        userDidInterfereWithAutoplay();

    setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);

    if (!m_paused && !m_pausedInternal) {
        setPaused(true);
        scheduleTimeupdateEvent(false);
        scheduleEvent(eventNames().pauseEvent);
        scheduleRejectPendingPlayPromises(DOMException::create(ExceptionCode::AbortError));
        if (MemoryPressureHandler::singleton().isUnderMemoryPressure())
            purgeBufferedDataIfPossible();
    }

    updatePlayState();
}

bool HTMLMediaElement::hasMediaSource() const
{
#if ENABLE(MEDIA_SOURCE)
    return m_mediaSource;
#else
    return false;
#endif
}

bool HTMLMediaElement::hasManagedMediaSource() const
{
#if ENABLE(MEDIA_SOURCE)
    RefPtr mediaSource = m_mediaSource;
    return mediaSource && mediaSource->isManaged();
#else
    return false;
#endif
}

#if ENABLE(MEDIA_SOURCE)

void HTMLMediaElement::detachMediaSource()
{
    if (RefPtr mediaSource = std::exchange(m_mediaSource, { })) {
        mediaSource->detachFromElement();
        mediaSource->setAsSrcObject(false);
    }
}

bool HTMLMediaElement::deferredMediaSourceOpenCanProgress() const
{
#if !ENABLE(WIRELESS_PLAYBACK_TARGET)
    return true;
#else
    return !document().settings().managedMediaSourceNeedsAirPlay()
        || isWirelessPlaybackTargetDisabled()
        || hasWirelessPlaybackTargetAlternative();
#endif
}

#endif

bool HTMLMediaElement::loop() const
{
    return hasAttributeWithoutSynchronization(loopAttr);
}

void HTMLMediaElement::setLoop(bool loop)
{
    ALWAYS_LOG(LOGIDENTIFIER, loop);
    setBooleanAttribute(loopAttr, loop);
    if (RefPtr player = m_player)
        player->isLoopingChanged();
}

bool HTMLMediaElement::controls() const
{
    RefPtr frame = document().frame();

    // always show controls when scripting is disabled
    if (frame && !frame->checkedScript()->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return true;

    return hasAttributeWithoutSynchronization(controlsAttr);
}

void HTMLMediaElement::setControls(bool controls)
{
    ALWAYS_LOG(LOGIDENTIFIER, controls);
    setBooleanAttribute(controlsAttr, controls);
}

double HTMLMediaElement::volume() const
{
    if (implicitlyMuted())
        return 0;

    return m_volume;
}

ExceptionOr<void> HTMLMediaElement::setVolume(double volume)
{
    HTMLMEDIAELEMENT_RELEASE_LOG(SETVOLUME, volume);

    if (!(volume >= 0 && volume <= 1))
        return Exception { ExceptionCode::IndexSizeError };

    auto quirkVolumeZero = !m_volumeLocked && protectedDocument()->quirks().implicitMuteWhenVolumeSetToZero();
    auto muteImplicitly = quirkVolumeZero && !volume;

    if (m_volume == volume && (!m_implicitlyMuted || *m_implicitlyMuted == muteImplicitly))
        return { };

    if (quirkVolumeZero) {
        if (implicitlyMuted() != muteImplicitly) {
            m_implicitlyMuted = muteImplicitly;
            setMutedInternal(m_muted, ForceMuteChange::True);
            if (volume)
                m_implicitlyMuted = std::nullopt;
        }
    }

    if (!m_volumeLocked) {
        if (volume && processingUserGestureForMedia())
            removeBehaviorRestrictionsAfterFirstUserGesture(MediaElementSession::AllRestrictions & ~MediaElementSession::RequireUserGestureToControlControlsManager);

        m_volume = volume;
        m_volumeInitialized = true;
        updateVolume();
        scheduleEvent(eventNames().volumechangeEvent);

        if (isPlaying() && !protectedMediaSession()->playbackStateChangePermitted(MediaPlaybackState::Playing)) {
            scheduleRejectPendingPlayPromises(DOMException::create(ExceptionCode::NotAllowedError));
            pauseInternal();
            setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
        }
        return { };
    }

    auto oldVolume = m_volume;
    m_volume = volume;

    if (m_volumeRevertTaskCancellationGroup.hasPendingTask())
        return { };

    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_volumeRevertTaskCancellationGroup, [oldVolume](auto& element) {
        element.m_volume = oldVolume;
    });

    return { };
}

bool HTMLMediaElement::muted() const
{
    if (implicitlyMuted())
        return true;

    if (m_explicitlyMuted)
        return m_muted;

    return hasAttributeWithoutSynchronization(mutedAttr);
}

void HTMLMediaElement::setMuted(bool muted)
{
    auto muteIsImplicit = implicitlyMuted();
    m_implicitlyMuted = std::nullopt;
    setMutedInternal(muted, muteIsImplicit ? ForceMuteChange::True : ForceMuteChange::False);
}

void HTMLMediaElement::setMutedInternal(bool muted, ForceMuteChange forceChange)
{
    HTMLMEDIAELEMENT_RELEASE_LOG(SETMUTEDINTERNAL, muted);

    bool mutedStateChanged = m_muted != muted || forceChange == ForceMuteChange::True;
    if (mutedStateChanged || !m_explicitlyMuted) {

        if (processingUserGestureForMedia()) {
            removeBehaviorRestrictionsAfterFirstUserGesture(MediaElementSession::AllRestrictions & ~MediaElementSession::RequireUserGestureToControlControlsManager);

            if (hasAudio() && muted)
                userDidInterfereWithAutoplay();
        }
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Muted, muted);
        m_muted = muted;
        if (!m_explicitlyMuted && !implicitlyMuted())
            m_explicitlyMuted = !m_explicitlyMuted && !implicitlyMuted();

        // Avoid recursion when the player reports volume changes.
        if (!processingMediaPlayerCallback()) {
            if (RefPtr player = m_player)
                player->setMuted(effectiveMuted());
        }

        if (mutedStateChanged) {
            scheduleEvent(eventNames().volumechangeEvent);
            scheduleUpdateShouldAutoplay();
        }

        updateShouldPlay();

        protectedDocument()->updateIsPlayingMedia();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        scheduleUpdateMediaState();
#endif
        protectedMediaSession()->canProduceAudioChanged();
        updateSleepDisabling();
    }

    schedulePlaybackControlsManagerUpdate();
}

void HTMLMediaElement::setVolumeLocked(bool volumeLocked)
{
    if (m_volumeLocked == volumeLocked)
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::VolumeLocked, volumeLocked);
    m_volumeLocked = volumeLocked;
    protectedPlayer()->setVolumeLocked(volumeLocked);
}

void HTMLMediaElement::updateBufferingState()
{
    // CSS Selectors Level 4; Editor's Draft, 2 July 2021
    // <https://drafts.csswg.org/selectors/>
    // 11.2. Media Loading State: the :buffering and :stalled pseudo-classes
    //
    // The :buffering pseudo-class represents an element that is capable of being “played” or “paused”,
    // when that element cannot continue playing because it is actively attempting to obtain media data
    // but has not yet obtained enough data to resume playback. (Note that the element is still considered
    // to be “playing” when it is “buffering”. Whenever :buffering matches an element, :playing also
    // matches the element.)
    bool buffering = !paused() && m_networkState == NETWORK_LOADING && m_readyState <= HAVE_CURRENT_DATA;
    if (m_buffering == buffering)
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Buffering, buffering);
    m_buffering = buffering;

    if (m_buffering)
        startBufferingStopwatch();
    else
        invalidateBufferingStopwatch();
}

void HTMLMediaElement::updateStalledState()
{
    // CSS Selectors Level 4; Editor's Draft, 2 July 2021
    // <https://drafts.csswg.org/selectors/>
    // 11.2. Media Loading State: the :buffering and :stalled pseudo-classes
    //
    // The :stalled pseudo-class represents an element when that element cannot continue playing because
    // it is actively attempting to obtain media data but it has failed to receive any data for some
    // amount of time. For the audio and video elements of HTML, this amount of time is the media element
    // stall timeout. [HTML] (Note that, like with the :buffering pseudo-class, the element is still
    // considered to be “playing” when it is “stalled”. Whenever :stalled matches an element, :playing
    // also matches the element.)
    bool stalled = !paused() && m_networkState == NETWORK_LOADING && m_readyState <= HAVE_CURRENT_DATA && m_sentStalledEvent;
    if (m_stalled != stalled) {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Stalled, stalled);
        m_stalled = stalled;
    }
}

#if USE(AUDIO_SESSION)

#if PLATFORM(MAC)
void HTMLMediaElement::hardwareMutedStateDidChange(const AudioSession& session)
{
    if (!session.isMuted())
        return;

    if (!hasAudio())
        return;

    if (effectiveMuted() || !volume())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    userDidInterfereWithAutoplay();
}
#endif

void HTMLMediaElement::routingContextUIDDidChange(const AudioSession& session)
{
    m_clients.forEach([routingContextUID = session.routingContextUID()] (auto& client) {
        client.routingContextUIDChanged(routingContextUID);
    });
}

#endif // USE(AUDIO_SESSION)

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

    protectedMediaSession()->removeBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager);
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
    double rate = std::min(ScanMaximumRate, std::abs(playbackRate() * 2));
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

    updateActiveTextTrackCues(currentMediaTime());

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource)
        mediaSource->monitorSourceBuffers();
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
    if (periodicEvent && timedelta < maxTimeupdateEventFrequency) {
        // Reschedule the timer to fire at the correct time, ensuring that no full cycles are skipped
        m_playbackProgressTimer.start(maxTimeupdateEventFrequency - timedelta, maxTimeupdateEventFrequency);
        return;
    }

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

void HTMLMediaElement::mediaPlayerDidAddAudioTrack(AudioTrackPrivate& track)
{
    if (isPlaying() && !protectedMediaSession()->playbackStateChangePermitted(MediaPlaybackState::Playing)) {
        scheduleRejectPendingPlayPromises(DOMException::create(ExceptionCode::NotAllowedError));
        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    }

    addAudioTrack(AudioTrack::create(protectedScriptExecutionContext().get(), track));
}

void HTMLMediaElement::mediaPlayerDidAddTextTrack(InbandTextTrackPrivate& track)
{
    // 4.8.10.12.2 Sourcing in-band text tracks
    // 1. Associate the relevant data with a new text track and its corresponding new TextTrack object.
    Ref textTrack = InbandTextTrack::create(protectedDocument(), track);

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
    addVideoTrack(VideoTrack::create(protectedScriptExecutionContext().get(), track));
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

void HTMLMediaElement::mediaPlayerDidReportGPUMemoryFootprint(size_t footPrint)
{
    RefPtr frame = document().frame();

    if (frame && !frame->isMainFrame())
        protectedDocument()->protectedFrameMemoryMonitor()->setUsage(footPrint);
}

void HTMLMediaElement::addAudioTrack(Ref<AudioTrack>&& track)
{
#if !RELEASE_LOG_DISABLED
    track->setLogger(protectedLogger(), logIdentifier());
#endif
    track->addClient(*this);
    HTMLMEDIAELEMENT_RELEASE_LOG(ADDAUDIOTRACK, track->id().string().utf8(), MediaElementSession::descriptionForTrack(track).utf8());
    ensureAudioTracks().append(WTFMove(track));
}

void HTMLMediaElement::addTextTrack(Ref<TextTrack>&& track)
{
#if !RELEASE_LOG_DISABLED
    track->setLogger(protectedLogger(), logIdentifier());
#endif

    if (!m_requireCaptionPreferencesChangedCallbacks) {
        m_requireCaptionPreferencesChangedCallbacks = true;
        Ref document = this->document();
        document->registerForCaptionPreferencesChangedCallbacks(*this);
        if (RefPtr page = document->page()) {
            auto& captionPreferences = page->checkedGroup()->ensureCaptionPreferences();
            m_captionDisplayMode = captionPreferences.captionDisplayMode();
            m_userPrefersTextDescriptions = captionPreferences.userPrefersTextDescriptions();
            m_userPrefersExtendedDescriptions = m_userPrefersTextDescriptions && document->settings().extendedAudioDescriptionsEnabled();
        }
    }

    track->addClient(*this);
    ensureTextTracks().append(WTFMove(track));
}

void HTMLMediaElement::addVideoTrack(Ref<VideoTrack>&& track)
{
#if !RELEASE_LOG_DISABLED
    track->setLogger(protectedLogger(), logIdentifier());
#endif
    track->addClient(*this);
    HTMLMEDIAELEMENT_RELEASE_LOG(ADDVIDEOTRACK, track->id().string().utf8(), MediaElementSession::descriptionForTrack(track).utf8());
    ensureVideoTracks().append(WTFMove(track));
}

void HTMLMediaElement::removeAudioTrack(Ref<AudioTrack>&& track)
{
    if (!m_audioTracks || !m_audioTracks->contains(track))
        return;
    track->clearClient(*this);
    HTMLMEDIAELEMENT_RELEASE_LOG(REMOVEAUDIOTRACK, track->id().string().utf8(), MediaElementSession::descriptionForTrack(track).utf8());
    m_audioTracks->remove(track.get());
}

void HTMLMediaElement::removeAudioTrack(TrackID trackID)
{
    if (!m_audioTracks)
        return;
    if (RefPtr track = m_audioTracks->find(trackID))
        removeAudioTrack(downcast<AudioTrack>(*track));
}

void HTMLMediaElement::removeTextTrack(TextTrack& track, bool scheduleEvent)
{
    if (!m_textTracks || !m_textTracks->contains(track))
        return;

    TrackDisplayUpdateScope scope { *this };
    if (RefPtr cues = track.cues())
        textTrackRemoveCues(track, *cues);
    track.clearClient(*this);
    if (m_textTracks)
        m_textTracks->remove(track, scheduleEvent);
}

void HTMLMediaElement::removeTextTrack(TrackID trackID, bool scheduleEvent)
{
    if (!m_textTracks)
        return;
    if (RefPtr track = m_textTracks->find(trackID))
        removeTextTrack(downcast<TextTrack>(*track), scheduleEvent);
}

void HTMLMediaElement::removeVideoTrack(Ref<VideoTrack>&& track)
{
    if (!m_videoTracks || !m_videoTracks->contains(track))
        return;
    track->clearClient(*this);
    ALWAYS_LOG(LOGIDENTIFIER, "id: "_s, track->id(), ", "_s, MediaElementSession::descriptionForTrack(track));
    m_videoTracks->remove(track);
}

void HTMLMediaElement::removeVideoTrack(TrackID trackID)
{
    if (!m_videoTracks)
        return;
    if (RefPtr track = m_videoTracks->find(trackID))
        removeVideoTrack(downcast<VideoTrack>(*track));
}

void HTMLMediaElement::forgetResourceSpecificTracks()
{
    while (m_audioTracks && m_audioTracks->length())
        removeAudioTrack(Ref { *m_audioTracks->lastItem() }.get());

    if (m_textTracks) {
        TrackDisplayUpdateScope scope { *this };
        for (int i = m_textTracks->length() - 1; i >= 0; --i) {
            Ref track = *m_textTracks->item(i);
            if (track->trackType() == TextTrack::InBand)
                removeTextTrack(track, false);
        }
    }

    while (m_videoTracks &&  m_videoTracks->length())
        removeVideoTrack(Ref { *m_videoTracks->lastItem() }.get());
}

#if ENABLE(WEB_AUDIO)
MediaElementAudioSourceNode* HTMLMediaElement::audioSourceNode()
{
    return m_audioSourceNode.get();
}
#endif

ExceptionOr<Ref<TextTrack>> HTMLMediaElement::addTextTrack(const AtomString& kind, const AtomString& label, const AtomString& language)
{
    // 4.8.10.12.4 Text track API
    // The addTextTrack(kind, label, language) method of media elements, when invoked, must run the following steps:

    // 1. If kind is not one of the following strings, then throw a SyntaxError exception and abort these steps
    if (!TextTrack::isValidKindKeyword(kind))
        return Exception { ExceptionCode::TypeError };

    // 2. If the label argument was omitted, let label be the empty string.
    // 3. If the language argument was omitted, let language be the empty string.
    // 4. Create a new TextTrack object.

    // 5. Create a new text track corresponding to the new object, and set its text track kind to kind, its text
    // track label to label, its text track language to language...
    Ref track = TextTrack::create(protectedDocument().ptr(), kind, emptyAtom(), label, language);
#if !RELEASE_LOG_DISABLED
    track->setLogger(protectedLogger(), logIdentifier());
#endif

    // Note, due to side effects when changing track parameters, we have to
    // first append the track to the text track list.

    // 6. Add the new text track to the media element's list of text tracks.
    addTextTrack(track.copyRef());

    // ... its text track readiness state to the text track loaded state ...
    track->setReadinessState(TextTrack::Loaded);

    // ... its text track mode to the text track hidden mode, and its text track list of cues to an empty list ...
    track->setMode(TextTrack::Mode::Hidden);

    return track;
}

AudioTrackList& HTMLMediaElement::ensureAudioTracks()
{
    if (!m_audioTracks) {
        lazyInitialize(m_audioTracks, AudioTrackList::create(ActiveDOMObject::protectedScriptExecutionContext().get()));
        m_audioTracks->setOpaqueRootObserver(m_opaqueRootProvider);
    }

    return *m_audioTracks;
}

TextTrackList& HTMLMediaElement::ensureTextTracks()
{
    if (!m_textTracks) {
        lazyInitialize(m_textTracks, TextTrackList::create(ActiveDOMObject::protectedScriptExecutionContext().get()));
        m_textTracks->setOpaqueRootObserver(m_opaqueRootProvider);
        m_textTracks->setDuration(durationMediaTime());
    }

    return *m_textTracks;
}

VideoTrackList& HTMLMediaElement::ensureVideoTracks()
{
    if (!m_videoTracks) {
        lazyInitialize(m_videoTracks, VideoTrackList::create(ActiveDOMObject::protectedScriptExecutionContext().get()));
        m_videoTracks->setOpaqueRootObserver(m_opaqueRootProvider);
    }

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
}

void HTMLMediaElement::didRemoveTextTrack(HTMLTrackElement& trackElement)
{
    ASSERT(trackElement.hasTagName(trackTag));

    Ref textTrack = trackElement.track();

    textTrack->setHasBeenConfigured(false);

    if (!m_textTracks)
        return;

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the old parent was a media element,
    // then the user agent must remove the track element's corresponding text track from the
    // media element's list of text tracks.
    removeTextTrack(textTrack);

    m_textTracksWhenResourceSelectionBegan.removeFirst(textTrack.ptr());
}

void HTMLMediaElement::configureMetadataTextTrackGroup(const TrackGroup& group)
{
    ASSERT(group.tracks.size());
    // https://html.spec.whatwg.org/multipage/embedded-content.html#honor-user-preferences-for-automatic-text-track-selection
    // 3. If there are any text tracks in the media element's list of text tracks whose text track kind is
    // chapters or metadata that correspond to track elements with a default attribute set whose text track mode
    // is set to disabled, then set the text track mode of all such tracks to hidden.
    for (auto& textTrack : group.tracks) {
        if (textTrack->mode() != TextTrack::Mode::Disabled)
            continue;
        if (!textTrack->isDefault())
            continue;
        textTrack->setMode(TextTrack::Mode::Hidden);
    }
}

void HTMLMediaElement::configureTextTrackGroup(const TrackGroup& group)
{
    ASSERT(group.tracks.size());

    RefPtr page = document().page();
    CaptionUserPreferences* captionPreferences = page ? &page->checkedGroup()->ensureCaptionPreferences() : nullptr;
    CaptionUserPreferences::CaptionDisplayMode displayMode = captionPreferences ? captionPreferences->captionDisplayMode() : CaptionUserPreferences::CaptionDisplayMode::Automatic;

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
        RefPtr textTrack = group.tracks[i];

        if (m_processingPreferenceChange && textTrack->mode() == TextTrack::Mode::Showing)
            currentlyEnabledTracks.append(textTrack);

        int trackScore = captionPreferences ? captionPreferences->textTrackSelectionScore(textTrack.get(), this) : 0;
        HTMLMEDIAELEMENT_RELEASE_LOG(CONFIGURETEXTTRACKGROUP, textTrack->kindKeyword().string().utf8(), textTrack->language().string().utf8(), textTrack->validBCP47Language().string().utf8(), trackScore);

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
            if (group.kind != TrackGroup::CaptionsAndSubtitles || displayMode != CaptionUserPreferences::CaptionDisplayMode::ForcedOnly)
                defaultTrack = textTrack;
        } else if (group.kind == TrackGroup::Description) {
            if (!defaultTrack && !fallbackTrack && m_userPrefersTextDescriptions)
                fallbackTrack = textTrack;
        }
    }

    if (displayMode != CaptionUserPreferences::CaptionDisplayMode::Manual) {
        if (!trackToEnable && defaultTrack)
            trackToEnable = defaultTrack;

        // If no track matches the user's preferred language, none was marked as 'default', and there is a forced subtitle track
        // in the same language as the language of the primary audio track, enable it.
        if (!trackToEnable && forcedSubitleTrack)
            trackToEnable = forcedSubitleTrack;

        // If no track matches, don't disable an already visible track unless preferences say they all should be off.
        if (group.kind != TrackGroup::CaptionsAndSubtitles || displayMode != CaptionUserPreferences::CaptionDisplayMode::ForcedOnly) {
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
    }
}

void HTMLMediaElement::layoutSizeChanged()
{
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& element) mutable {
        if (element.isContextStopped())
            return;

        if (RefPtr root = element.userAgentShadowRoot())
            root->dispatchEvent(Event::create(eventNames().resizeEvent, Event::CanBubble::No, Event::IsCancelable::No));

        if (RefPtr mediaControlsHost = element.m_mediaControlsHost.get())
            mediaControlsHost->updateCaptionDisplaySizes();
    });

    if (!m_receivedLayoutSizeChanged) {
        m_receivedLayoutSizeChanged = true;
        schedulePlaybackControlsManagerUpdate();
    }

    // If the video is a candidate for main content, we should register it for viewport visibility callbacks
    // if it hasn't already been registered.
    if (CheckedPtr renderer = this->renderer()) {
        if (RefPtr mediaSession = m_mediaSession; mediaSession && !mediaSession->wantsToObserveViewportVisibilityForAutoplay() && mediaSession->wantsToObserveViewportVisibilityForMediaControls())
            renderer->registerForVisibleInViewportCallback();
    }
}

void HTMLMediaElement::visibilityDidChange()
{
    scheduleUpdateShouldAutoplay();
}

void HTMLMediaElement::setSelectedTextTrack(TextTrack* trackToSelect)
{
    RefPtr trackList = textTracks();
    if (!trackList || !trackList->length())
        return;

    if (trackToSelect == &TextTrack::captionMenuAutomaticItem()) {
        if (captionDisplayMode() != CaptionUserPreferences::CaptionDisplayMode::Automatic)
            m_textTracks->scheduleChangeEvent();
    } else if (trackToSelect == &TextTrack::captionMenuOffItem()) {
        for (int i = 0, length = trackList->length(); i < length; ++i)
            RefPtr { trackList->item(i) }->setMode(TextTrack::Mode::Disabled);

        if (captionDisplayMode() != CaptionUserPreferences::CaptionDisplayMode::ForcedOnly && !trackList->isChangeEventScheduled())
            m_textTracks->scheduleChangeEvent();
    } else {
        if (!trackToSelect || !trackList->contains(*trackToSelect))
            return;

        for (int i = 0, length = trackList->length(); i < length; ++i) {
            Ref track = *trackList->item(i);
            if (track.ptr() != trackToSelect)
                track->setMode(TextTrack::Mode::Disabled);
            else
                track->setMode(TextTrack::Mode::Showing);
        }
    }

    RefPtr page = document().page();
    if (!page)
        return;

    auto& captionPreferences = page->checkedGroup()->ensureCaptionPreferences();
    CaptionUserPreferences::CaptionDisplayMode displayMode;
    if (trackToSelect == &TextTrack::captionMenuOffItem())
        displayMode = CaptionUserPreferences::CaptionDisplayMode::ForcedOnly;
    else if (trackToSelect == &TextTrack::captionMenuAutomaticItem())
        displayMode = CaptionUserPreferences::CaptionDisplayMode::Automatic;
    else {
        displayMode = CaptionUserPreferences::CaptionDisplayMode::AlwaysOn;
        if (trackToSelect->validBCP47Language().length())
            captionPreferences.setPreferredLanguage(trackToSelect->validBCP47Language());
    }

    captionPreferences.setCaptionDisplayMode(displayMode);
}

void HTMLMediaElement::scheduleConfigureTextTracks()
{
    if (m_configureTextTracksTaskCancellationGroup.hasPendingTask())
        return;

    HTMLMEDIAELEMENT_RELEASE_LOG(SCHEDULECONFIGURETEXTTRACKS_TASK_SCHEDULED);
    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_configureTextTracksTaskCancellationGroup, [](auto& element) {
        HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(&element, SCHEDULECONFIGURETEXTTRACKS_LAMBDA_TASK_FIRED);
        element.configureTextTracks();
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
        RefPtr textTrack = m_textTracks->item(i);
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
        configureMetadataTextTrackGroup(metadataTracks);
    if (otherTracks.tracks.size())
        configureTextTrackGroup(otherTracks);

    m_processingPreferenceChange = false;

    configureTextTrackDisplay();
}

bool HTMLMediaElement::havePotentialSourceChild()
{
    // Stash the current <source> node and next nodes so we can restore them after checking
    // to see there is another potential.
    RefPtr currentSourceNode = m_currentSourceNode;
    RefPtr nextNode = m_nextChildNodeToConsider;

    auto nextURL = selectNextSourceChild(nullptr, InvalidURLAction::DoNothing);

    m_currentSourceNode = currentSourceNode;
    m_nextChildNodeToConsider = nextNode;

    return nextURL.isValid();
}

URL HTMLMediaElement::selectNextSourceChild(ContentType* contentType, InvalidURLAction actionIfInvalid)
{
    // Don't log if this was just called to find out if there are any valid <source> elements.
    bool shouldLog = willLog(WTFLogLevel::Always) && actionIfInvalid != InvalidURLAction::DoNothing;
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
    for (auto next = m_nextChildNodeToConsider ? sources.beginAt(*m_nextChildNodeToConsider.copyRef()) : sources.begin(); next; ++next)
        potentialSourceNodes.append(Ref { *next });

    for (auto& source : potentialSourceNodes) {
        if (source->parentNode() != this)
            continue;

        // 2. If candidate does not have a src attribute, or if its src
        // attribute's value is the empty string ... jump down to the failed
        // step below
        auto& srcValue = source->attributeWithoutSynchronization(srcAttr);
        URL mediaURL;
        String type;
        if (shouldLog)
            INFO_LOG(LOGIDENTIFIER, "'src' is ", srcValue);
        if (srcValue.isEmpty())
            goto CheckAgain;

        // 3. Let urlString be the resulting URL string that would have resulted
        // from parsing the URL specified by candidate's src attribute's value
        // relative to the candidate's node document when the src attribute was
        // last changed.
        mediaURL = source->protectedDocument()->completeURL(srcValue);

        if (auto mediaQueryList = source->parsedMediaAttribute(protectedDocument()); !mediaQueryList.isEmpty()) {
            if (shouldLog)
                INFO_LOG(LOGIDENTIFIER, "'media' is ", source->attributeWithoutSynchronization(mediaAttr));
            CheckedPtr renderer = this->renderer();
            LOG(MediaQueries, "HTMLMediaElement %p selectNextSourceChild evaluating media queries", this);
            if (!MQ::MediaQueryEvaluator { screenAtom(), protectedDocument(), renderer ? &renderer->style() : nullptr }.evaluate(mediaQueryList))
                goto CheckAgain;
        }

        // 4. If urlString was not obtained successfully, then end the synchronous section,
        // and jump down to the failed with elements step below.
        if (!isSafeToLoadURL(mediaURL, actionIfInvalid))
            goto CheckAgain;

        // 5. If candidate has a type attribute whose value, when parsed as a
        // MIME type ...
        type = source->attributeWithoutSynchronization(typeAttr);
        if (type.isEmpty() && mediaURL.protocolIsData())
            type = mimeTypeFromDataURL(mediaURL.string());
        if (!type.isEmpty()) {
            if (shouldLog)
                INFO_LOG(LOGIDENTIFIER, "'type' is ", type);
            MediaEngineSupportParameters parameters;
            parameters.type = ContentType(type);
            parameters.url = mediaURL;
#if ENABLE(MEDIA_SOURCE)
            parameters.isMediaSource = mediaURL.protocolIs(mediaSourceBlobProtocol) && MediaSource::lookup(mediaURL.string());
#endif
            parameters.requiresRemotePlayback = !!m_remotePlaybackConfiguration;
            if (!document().settings().allowMediaContentTypesRequiringHardwareSupportAsFallback() || Traversal<HTMLSourceElement>::nextSkippingChildren(source))
                parameters.contentTypesRequiringHardwareSupport = mediaContentTypesRequiringHardwareSupport();
            parameters.supportsLimitedMatroska = limitedMatroskaSupportEnabled();

            if (MediaPlayer::supportsType(parameters) == MediaPlayer::SupportsType::IsNotSupported)
                goto CheckAgain;
        }

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
        if (actionIfInvalid == InvalidURLAction::Complain)
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
    if (willLog(WTFLogLevel::Info) && source.hasTagName(sourceTag)) {
        URL url = source.getNonEmptyURLAttribute(srcAttr);
        INFO_LOG(LOGIDENTIFIER, "'src' is ", url);
    }

    if (!document().hasBrowsingContext()) {
        INFO_LOG(LOGIDENTIFIER, "<source> inserted inside a document without a browsing context is not loaded");
        return;
    }

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource)
        mediaSource->openIfDeferredOpen();
#endif

    // We should only consider a <source> element when there is not src attribute at all.
    if (hasAttributeWithoutSynchronization(srcAttr))
        return;

    // 4.8.8 - If a source element is inserted as a child of a media element that has no src
    // attribute and whose networkState has the value NETWORK_EMPTY, the user agent must invoke
    // the media element's resource selection algorithm.
    if (m_networkState == NETWORK_EMPTY) {
        m_nextChildNodeToConsider = source;
#if PLATFORM(IOS_FAMILY)
        if (mediaSession().dataLoadingPermitted())
#endif
            selectMediaResource();
        return;
    }

    if (m_currentSourceNode && &source == Traversal<HTMLSourceElement>::nextSibling(*m_currentSourceNode.copyRef())) {
        INFO_LOG(LOGIDENTIFIER, "<source> inserted immediately after current source");
        m_nextChildNodeToConsider = source;
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
    m_nextChildNodeToConsider = source;
    scheduleNextSourceChild();
}

void HTMLMediaElement::sourceWasRemoved(HTMLSourceElement& source)
{
    if (willLog(WTFLogLevel::Info) && source.hasTagName(sourceTag)) {
        URL url = source.getNonEmptyURLAttribute(srcAttr);
        INFO_LOG(LOGIDENTIFIER, "'src' is ", url);
    }

    if (&source != m_currentSourceNode && &source != m_nextChildNodeToConsider)
        return;

    if (&source == m_nextChildNodeToConsider) {
        m_nextChildNodeToConsider = m_currentSourceNode ? Traversal<HTMLSourceElement>::nextSibling(*m_currentSourceNode.copyRef()) : nullptr;
        INFO_LOG(LOGIDENTIFIER);
    } else if (&source == m_currentSourceNode) {
        // Clear the current source node pointer, but don't change the movie as the spec says:
        // 4.8.8 - Dynamically modifying a source element and its attribute when the element is already
        // inserted in a video or audio element will have no effect.
        m_currentSourceNode = nullptr;
        INFO_LOG(LOGIDENTIFIER, "m_currentSourceNode cleared");
    }
}

void HTMLMediaElement::mediaPlayerTimeChanged()
{
    HTMLMEDIAELEMENT_RELEASE_LOG(MEDIAPLAYERTIMECHANGED);

    updateActiveTextTrackCues(currentMediaTime());

    beginProcessingMediaPlayerCallback();

    invalidateOfficialPlaybackPosition();
    bool wasSeeking = seeking();

    // 4.8.10.9 step 14 & 15.  Needed if no ReadyState change is associated with the seek.
    if (m_seekRequested && m_readyState >= HAVE_CURRENT_DATA && !protectedPlayer()->seeking())
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
    if ((dur || (!dur && !now)) && dur.isValid() && !dur.isPositiveInfinite() && !dur.isNegativeInfinite()) {
        // If the media element has a loop attribute specified and does not have a current media controller,
        if (loop() && !m_mediaController && playbackRate > 0) {
            m_sentEndEvent = false;
            // then seek to the earliest possible position of the media resource and abort these steps when the direction of
            // playback is forwards,
            if (now >= dur && (now + dur) > MediaTime::zeroTime()) {
                ALWAYS_LOG(LOGIDENTIFIER, "current time (", now, ") is greater then duration (", dur, "), looping");
                seekInternal(MediaTime::zeroTime());
            }
        } else if ((now <= MediaTime::zeroTime() && playbackRate < 0) || (now >= dur && playbackRate > 0)) {

            ALWAYS_LOG(LOGIDENTIFIER, "current time (", now, ") is greater then duration (", dur, ") or <= 0, pausing");

            // If the media element does not have a current media controller, and the media element
            // has still ended playback and paused is false,
            if (!m_mediaController && !m_paused) {
                // changes paused to true and fires a simple event named pause at the media element.
                setPaused(true);
                scheduleEvent(eventNames().pauseEvent);
                protectedMediaSession()->clientWillPausePlayback();
            }
            // Queue a task to fire a simple event named ended at the media element.
            if (!m_sentEndEvent) {
                m_sentEndEvent = true;
                scheduleEvent(eventNames().endedEvent);
                if (!wasSeeking)
                    addBehaviorRestrictionsOnEndIfNecessary();
                setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);
                if (now > m_lastSeekTime)
                    addPlayedRange(m_lastSeekTime, now);
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
            if (!m_sentEndEvent) {
                if (RefPtr player = m_player; player && player->ended()) {
                    m_sentEndEvent = true;
                    scheduleEvent(eventNames().endedEvent);
                    if (!wasSeeking)
                        addBehaviorRestrictionsOnEndIfNecessary();
                    setPaused(true);
                    setPlaying(false);
                }
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

    protectedMediaSession()->addBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager);
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
        if ((m_wasPlayingBeforeSeeking = !paused()))
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

    if (RefPtr manager = sessionManager())
        manager->sessionDidEndRemoteScrubbing(protectedMediaSession().get());
    m_isScrubbingRemotely = false;
    m_seekToPlaybackPositionEndedTimer.stop();
#endif
}

void HTMLMediaElement::mediaPlayerVolumeChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    beginProcessingMediaPlayerCallback();
    if (RefPtr player = m_player) {
        double vol = player->volume();
        if (vol != m_volume) {
            m_volume = vol;
            updateVolume();
            scheduleEvent(eventNames().volumechangeEvent);
        }
    }
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerMuteChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    beginProcessingMediaPlayerCallback();
    if (RefPtr player = m_player)
        setMuted(player->muted());
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerSeeked(const MediaTime&)
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource)
        mediaSource->monitorSourceBuffers(); // Update readyState.
#endif
}

void HTMLMediaElement::mediaPlayerDurationChanged()
{
    beginProcessingMediaPlayerCallback();

    durationChanged();
    mediaPlayerCharacteristicChanged();

    MediaTime now = currentMediaTime();
    MediaTime dur = durationMediaTime();
    HTMLMEDIAELEMENT_RELEASE_LOG(MEDIAPLAYERDURATIONCHANGED, dur.toFloat(), now.toFloat());
    if (now > dur)
        seekInternal(dur);

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerRateChanged()
{
    beginProcessingMediaPlayerCallback();

    // Stash the rate in case the one we tried to set isn't what the engine is
    // using (eg. it can't handle the rate we set)
    m_reportedPlaybackRate = protectedPlayer()->effectiveRate();

    HTMLMEDIAELEMENT_RELEASE_LOG(MEDIAPLAYERRATECHANGED, m_reportedPlaybackRate);

    if (m_reportedPlaybackRate)
        startWatchtimeTimer();
    else
        pauseWatchtimeTimer();

    updateSleepDisabling();

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerPlaybackStateChanged()
{
    RefPtr player = m_player;
    if (!player || m_pausedInternal)
        return;

    updateSleepDisabling();

    auto playerPaused = player->paused();
    bool shouldBePaused = !potentiallyPlaying();
    ALWAYS_LOG(LOGIDENTIFIER, "playerPaused: ", playerPaused, ", shouldBePaused: ", shouldBePaused);
    if (playerPaused == shouldBePaused)
        return;

    beginProcessingMediaPlayerCallback();
    if (playerPaused)
        pauseInternal();
    else
        playInternal();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerResourceNotSupported()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // The MediaPlayer came across content which no installed engine supports.
    mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
}

// MediaPlayerPresentation methods
void HTMLMediaElement::mediaPlayerRepaint()
{
    beginProcessingMediaPlayerCallback();
    if (CheckedPtr renderer = this->renderer())
        renderer->repaint();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerSizeChanged()
{
    RefPtr player = m_player;
    ASSERT(player);
    if (!player)
        return;

    auto naturalSize = player->naturalSize();
    HTMLMEDIAELEMENT_RELEASE_LOG(MEDIAPLAYERSIZECHANGED, naturalSize.width(), naturalSize.height());

    if (RefPtr mediaDocument = dynamicDowncast<MediaDocument>(document()))
        mediaDocument->mediaElementNaturalSizeChanged(expandedIntSize(naturalSize));

    beginProcessingMediaPlayerCallback();
    if (m_readyState > HAVE_NOTHING)
        scheduleResizeEventIfSizeChanged(naturalSize);
    updateRenderer();
    endProcessingMediaPlayerCallback();
}

bool HTMLMediaElement::mediaPlayerAcceleratedCompositingEnabled()
{
    return document().settings().acceleratedCompositingEnabled();
}

void HTMLMediaElement::scheduleMediaEngineWasUpdated()
{
    if (m_mediaEngineUpdatedTaskCancellationGroup.hasPendingTask())
        return;

    HTMLMEDIAELEMENT_RELEASE_LOG(SCHEDULEMEDIAENGINEWASUPDATED_TASK_SCHEDULED);
    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_mediaEngineUpdatedTaskCancellationGroup, [](auto& element) {
        HTMLMEDIAELEMENT_RELEASE_LOG_WITH_THIS(&element, SCHEDULEMEDIAENGINEWASUPDATED_LAMBDA_TASK_FIRED);
        element.mediaEngineWasUpdated();
    });
}

void HTMLMediaElement::mediaEngineWasUpdated()
{
    HTMLMEDIAELEMENT_RELEASE_LOG(MEDIAENGINEWASUPDATED);

    beginProcessingMediaPlayerCallback();
    updateRenderer();
    endProcessingMediaPlayerCallback();

    protectedMediaSession()->mediaEngineUpdated();

#if ENABLE(ENCRYPTED_MEDIA)
    if (RefPtr player = m_player; player && m_mediaKeys)
        player->cdmInstanceAttached(m_mediaKeys->cdmInstance());
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (RefPtr player = m_player; player && m_webKitMediaKeys)
        player->setCDM(&m_webKitMediaKeys->cdm());
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr player = m_player) {
        player->setVideoFullscreenFrame(m_videoFullscreenFrame);
        player->setVideoFullscreenGravity(m_videoFullscreenGravity);
        player->setVideoFullscreenLayer(m_videoFullscreenLayer.get());
    }
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    scheduleUpdateMediaState();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    updateShouldContinueAfterNeedKey();
#endif

    if (RefPtr page = document().page())
        page->mediaEngineChanged(*this);
}

void HTMLMediaElement::mediaPlayerEngineUpdated()
{
    HTMLMEDIAELEMENT_RELEASE_LOG(MEDIAPLAYERENGINEUPDATED, m_player->engineDescription().utf8());

#if ENABLE(MEDIA_SOURCE)
    m_droppedVideoFrames = 0;
#endif

    m_havePreparedToPlay = false;

    scheduleMediaEngineWasUpdated();
}

// Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS since this function does conditional locking, which is not
// supported by analysis.
void HTMLMediaElement::mediaPlayerWillInitializeMediaEngine() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(isMainThread());
#if ENABLE(WEB_AUDIO)
    // Make sure the MediaElementAudioSourceNode's process function does not try and access the media player while its engine is getting updated.
    if (RefPtr audioSourceNode = m_audioSourceNode.get())
        audioSourceNode->processLock().lock();
#endif
}

// Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS since this function does conditional unlocking, which is not
// supported by analysis.
void HTMLMediaElement::mediaPlayerDidInitializeMediaEngine() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(isMainThread());
#if ENABLE(WEB_AUDIO)
    if (RefPtr audioSourceNode = m_audioSourceNode.get()) {
        if (auto* provider = audioSourceProvider())
            provider->setClient(audioSourceNode.get());

        audioSourceNode->processLock().unlock();
    }
#endif
}

void HTMLMediaElement::mediaPlayerCharacteristicChanged()
{
    if (RefPtr mediaSession = m_mediaSession)
        HTMLMEDIAELEMENT_RELEASE_LOG(MEDIAPLAYERCHARACTERISTICSCHANGED, mediaSession->description().utf8());
    else
        HTMLMEDIAELEMENT_RELEASE_LOG(MEDIAPLAYERCHARACTERISTICSCHANGED_NO_MEDIASESSION);

    beginProcessingMediaPlayerCallback();

    if (captionDisplayMode() == CaptionUserPreferences::CaptionDisplayMode::Automatic) {
        auto languageOfPrimaryAudioTrack = protectedPlayer()->languageOfPrimaryAudioTrack();
        auto audioLanguageChanged = !m_languageOfPrimaryAudioTrack || *m_languageOfPrimaryAudioTrack != languageOfPrimaryAudioTrack;
        if (audioLanguageChanged && m_subtitleTrackLanguage != languageOfPrimaryAudioTrack) {
            m_languageOfPrimaryAudioTrack = languageOfPrimaryAudioTrack;
            markCaptionAndSubtitleTracksAsUnconfigured(AfterDelay);
        }
    }

    if (potentiallyPlaying())
        mediaPlayerRenderingModeChanged();

    updateRenderer();

    if (!paused() && !protectedMediaSession()->playbackStateChangePermitted(MediaPlaybackState::Playing)) {
        scheduleRejectPendingPlayPromises(DOMException::create(ExceptionCode::NotAllowedError));
        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    }

    protectedDocument()->updateIsPlayingMedia();

    checkForAudioAndVideo();
    updateSleepDisabling();

    endProcessingMediaPlayerCallback();
}

Ref<TimeRanges> HTMLMediaElement::buffered() const
{
    RefPtr player = m_player;
    if (!player)
        return TimeRanges::create();

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource)
        return TimeRanges::create(mediaSource->buffered());
#endif

    return TimeRanges::create(player->buffered());
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

    return Ref { *m_playedTimeRanges }->copy();
}

Ref<TimeRanges> HTMLMediaElement::seekable() const
{
    return TimeRanges::create(platformSeekable());
}

PlatformTimeRanges HTMLMediaElement::platformSeekable() const
{
#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource)
        return mediaSource->seekable();
#endif

    if (RefPtr player = m_player)
        return player->seekable();

    return { };
}

double HTMLMediaElement::seekableTimeRangesLastModifiedTime() const
{
    RefPtr player = m_player;
    return player ? player->seekableTimeRangesLastModifiedTime() : 0;
}

double HTMLMediaElement::liveUpdateInterval() const
{
    RefPtr player = m_player;
    return player ? player->liveUpdateInterval() : 0;
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

    RefPtr manager = sessionManager();
    if (!canProduceAudio() || (manager && manager->hasActiveAudioSession()))
        return true;

    Ref mediaSession = this->mediaSession();
    if (mediaSession->activeAudioSessionRequired() && mediaSession->blockedBySystemInterruption())
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
        RefPtr seekableRanges = seekable();
        if (!seekableRanges->contain(currentTime()))
            return true;
    }

    return false;
}

bool HTMLMediaElement::pausedForUserInteraction() const
{
    if (mediaSession().state() == PlatformMediaSession::State::Interrupted)
        return true;

    return false;
}

MediaTime HTMLMediaElement::minTimeSeekable() const
{
    RefPtr player = m_player;
    return player ? player->minTimeSeekable() : MediaTime::zeroTime();
}

MediaTime HTMLMediaElement::maxTimeSeekable() const
{
    RefPtr player = m_player;
    return player ? player->maxTimeSeekable() : MediaTime::zeroTime();
}

void HTMLMediaElement::updateVolume()
{
    RefPtr player = m_player;
    if (!player)
        return;

    if (!m_volumeLocked) {
        // Avoid recursion when the player reports volume changes.
        if (!processingMediaPlayerCallback()) {
            player->setVolumeLocked(m_volumeLocked);
            player->setMuted(effectiveMuted());
            player->setVolume(effectiveVolume());
        }

        protectedDocument()->updateIsPlayingMedia();
        return;
    }

    // Only the user can change audio volume so update the cached volume and post the changed event.
    float volume = player->volume();
    if (m_volume != volume) {
        m_volume = volume;
        scheduleEvent(eventNames().volumechangeEvent);
    }
}

void HTMLMediaElement::scheduleUpdatePlayState()
{
    if (m_updatePlayStateTaskCancellationGroup.hasPendingTask())
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    INFO_LOG(logSiteIdentifier, "task scheduled");
    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_updatePlayStateTaskCancellationGroup, [logSiteIdentifier](auto& element) {
        UNUSED_PARAM(logSiteIdentifier);
        INFO_LOG_WITH_THIS(&element, logSiteIdentifier, "lambda(), task fired");
        element.updatePlayState();
    });
}

void HTMLMediaElement::updatePlayState()
{
    RefPtr player = m_player;
    if (!player)
        return;

    if (m_pausedInternal) {
        if (!player->paused())
            pausePlayer();
        invalidateOfficialPlaybackPosition();
        m_playbackProgressTimer.stop();
        return;
    }

    bool shouldBePlaying = potentiallyPlaying();
    bool playerPaused = player->paused();

    HTMLMEDIAELEMENT_RELEASE_LOG(UPDATEPLAYSTATE, shouldBePlaying, playerPaused);

    Ref mediaSession = this->mediaSession();
    if (shouldBePlaying && playerPaused && mediaSession->requiresFullscreenForVideoPlayback() && (m_waitingToEnterFullscreen || !isFullscreen())) {
        if (!m_waitingToEnterFullscreen)
            enterFullscreen();

#if PLATFORM(WATCHOS)
        // FIXME: Investigate doing this for all builds.
        return;
#endif
    }

    schedulePlaybackControlsManagerUpdate();
    if (shouldBePlaying) {
        invalidateOfficialPlaybackPosition();

        if (playerPaused) {
            mediaSession->clientWillBeginPlayback();

            // Set rate, muted and volume before calling play in case they were set before the media engine was set up.
            // The media engine should just stash the rate, muted and volume values since it isn't already playing.
            player->setRate(requestedPlaybackRate());
            player->setVolumeLocked(m_volumeLocked);
            player->setMuted(effectiveMuted());
            player->setVolume(effectiveVolume());

            if (m_firstTimePlaying) {
                // Log that a media element was played.
                if (RefPtr page = protectedDocument()->page())
                    page->checkedDiagnosticLoggingClient()->logDiagnosticMessage(isVideo() ? DiagnosticLoggingKeys::videoKey() : DiagnosticLoggingKeys::audioKey(), DiagnosticLoggingKeys::playedKey(), ShouldSample::No);
                m_firstTimePlaying = false;
            }

            playPlayer();
            resumeSpeakingCueText();
        }

        startPlaybackProgressTimer();
        setPlaying(true);
    } else {
        if (!playerPaused) {
            pausePlayer();
            pauseSpeakingCueText();
        }

        m_playbackProgressTimer.stop();
        setPlaying(false);
        MediaTime time = currentMediaTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);

        if (couldPlayIfEnoughData())
            prepareToPlay();
    }

    updateMediaController();
    updateRenderer();

    checkForAudioAndVideo();
}

void HTMLMediaElement::playPlayer()
{
    RefPtr player = m_player;
    ASSERT(player);
    if (!player)
        return;

#if USE(AUDIO_SESSION)
    m_categoryAtMostRecentPlayback = AudioSession::singleton().category();
    m_modeAtMostRecentPlayback = AudioSession::singleton().mode();
#endif

#if ENABLE(MEDIA_SESSION) && ENABLE(MEDIA_SESSION_COORDINATOR)
    do {
        if (!player->supportsPlayAtHostTime())
            break;

        RefPtr mediaSession = protectedMediaSession()->mediaSession();
        if (!mediaSession)
            break;

        if (mediaSession->activeMediaElement() != this)
            break;

        auto currentPlaySessionCommand = mediaSession->coordinator().takeCurrentPlaySessionCommand();
        if (!currentPlaySessionCommand)
            break;

        if (!currentPlaySessionCommand->hostTime)
            break;

        player->playAtHostTime(*currentPlaySessionCommand->hostTime);
        return;
    } while (false);
#endif

    player->play();
}

void HTMLMediaElement::pausePlayer()
{
    RefPtr player = m_player;
    ASSERT(player);
    if (!player)
        return;

    player->pause();
}

void HTMLMediaElement::checkForAudioAndVideo()
{
    m_hasEverHadAudio |= hasAudio();
    m_hasEverHadVideo |= hasVideo();
    protectedMediaSession()->canProduceAudioChanged();
}

void HTMLMediaElement::setPlaying(bool playing)
{
    if (playing) {
        if (RefPtr mediaSession = m_mediaSession)
            mediaSession->removeBehaviorRestriction(MediaElementSession::RequirePlaybackToControlControlsManager);
    }

    if (m_playing == playing)
        return;

    m_playing = playing;

    protectedDocument()->updateIsPlayingMedia();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    scheduleUpdateMediaState();
#endif
}

void HTMLMediaElement::setPausedInternal(bool paused)
{
    ALWAYS_LOG(LOGIDENTIFIER, paused);
    m_pausedInternal = paused;
    scheduleUpdatePlayState();
}

void HTMLMediaElement::pauseAndUpdatePlayStateImmediately()
{
    m_pausedInternal = true;
    updatePlayState();
}

void HTMLMediaElement::stopPeriodicTimers()
{
    m_progressEventTimer.stop();
    m_playbackProgressTimer.stop();
    m_checkPlaybackTargetCompatibilityTimer.stop();
}

void HTMLMediaElement::cancelPendingTasks()
{
    m_configureTextTracksTaskCancellationGroup.cancel();
    m_updateTextTracksTaskCancellationGroup.cancel();
    m_updateMediaStateTaskCancellationGroup.cancel();
    m_mediaEngineUpdatedTaskCancellationGroup.cancel();
    m_updatePlayStateTaskCancellationGroup.cancel();
    m_resumeTaskCancellationGroup.cancel();
    m_seekTaskCancellationGroup.cancel();
    m_playbackControlsManagerBehaviorRestrictionsTaskCancellationGroup.cancel();
    m_updateShouldAutoplayTaskCancellationGroup.cancel();
    if (m_volumeLocked)
        m_volumeRevertTaskCancellationGroup.cancel();
    cancelSniffer();
}

void HTMLMediaElement::cancelSniffer()
{
    if (auto sniffer = std::exchange(m_sniffer, { }))
        sniffer->cancel();
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
    m_error = MediaError::create(MediaError::MEDIA_ERR_ABORTED, "Load was aborted"_s);

    // 3 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(eventNames().abortEvent);

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    // 4 - If the media element's readyState attribute has a value equal to HAVE_NOTHING, set the
    // element's networkState attribute to the NETWORK_EMPTY value, set the element's show poster
    // flag to true, and fire an event named emptied at the element.
    if (m_readyState == HAVE_NOTHING) {
        m_networkState = NETWORK_EMPTY;
        setShowPosterFlag(true);
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

    if (isSuspended())
        return; // Document is about to be destructed. Avoid updating layout in updateActiveTextTrackCues.

    updateActiveTextTrackCues(MediaTime::zeroTime());
}

void HTMLMediaElement::clearMediaPlayer()
{
    invalidateWatchtimeTimer();
    invalidateBufferingStopwatch();

#if ENABLE(MEDIA_STREAM)
    if (!m_settingMediaStreamSrcObject)
        m_mediaStreamSrcObject = nullptr;
#endif

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    m_blob = nullptr;

    forgetResourceSpecificTracks();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (hasTargetAvailabilityListeners()) {
        m_hasPlaybackTargetAvailabilityListeners = false;
        if (RefPtr mediaSession = m_mediaSession)
            mediaSession->setHasPlaybackTargetAvailabilityListeners(false);

        // Send an availability event in case scripts want to hide the picker when the element
        // doesn't support playback to a target.
        if (!isWirelessPlaybackTargetDisabled())
            enqueuePlaybackTargetAvailabilityChangedEvent(EnqueueBehavior::Always);
    }

    if (m_isPlayingToWirelessTarget)
        setIsPlayingToWirelessTarget(false);
#endif

    if (m_isWaitingUntilMediaCanStart) {
        m_isWaitingUntilMediaCanStart = false;
        protectedDocument()->removeMediaCanStartListener(*this);
    }

    if (RefPtr player = m_player) {
        player->invalidate();
        m_player = nullptr;
    }
    schedulePlaybackControlsManagerUpdate();

    stopPeriodicTimers();
    cancelPendingTasks();

    m_loadState = WaitingForSource;

    if (m_textTracks)
        configureTextTrackDisplay();

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& element) {
        if (RefPtr mediaSession = element.m_mediaSession) {
            mediaSession->clientCharacteristicsChanged(false);
            mediaSession->canProduceAudioChanged();
        }
    });

    m_resourceSelectionTaskCancellationGroup.cancel();

    updateSleepDisabling();
    updateRenderer();
}

void HTMLMediaElement::stopWithoutDestroyingMediaPlayer()
{
    INFO_LOG(LOGIDENTIFIER);

    invalidateWatchtimeTimer();
    invalidateBufferingStopwatch();

    if (m_videoFullscreenMode != VideoFullscreenModeNone)
        exitFullscreen();

    setPreparedToReturnVideoLayerToInline(true);

    schedulePlaybackControlsManagerUpdate();
    setInActiveDocument(false);

    // Stop the playback without generating events
    setPlaying(false);
    pauseAndUpdatePlayStateImmediately();
    if (RefPtr mediaSession = m_mediaSession)
        mediaSession->clientWillBeDOMSuspended();

    setAutoplayEventPlaybackState(AutoplayEventPlaybackState::None);

    userCancelledLoad();

    updateRenderer();

    stopPeriodicTimers();

    updateSleepDisabling();
}

void HTMLMediaElement::closeTaskQueues()
{
    cancelPendingTasks();
    m_resourceSelectionTaskCancellationGroup.cancel();
    m_asyncEventsCancellationGroup.cancel();
}

void HTMLMediaElement::contextDestroyed()
{
    closeTaskQueues();
    m_pendingPlayPromises.clear();

    ActiveDOMObject::contextDestroyed();
}

void HTMLMediaElement::stop()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    Ref protectedThis { *this };
    stopWithoutDestroyingMediaPlayer();
    closeTaskQueues();

    // Once an active DOM object has been stopped it can not be restarted, so we can deallocate
    // the media player now. Note that userCancelledLoad will already called clearMediaPlayer
    // if the media was not fully loaded, but we need the same cleanup if the file was completely
    // loaded and calling it again won't cause any problems.
    clearMediaPlayer();

    if (RefPtr mediaSession = m_mediaSession)
        mediaSession->stopSession();
}

void HTMLMediaElement::suspend(ReasonForSuspension reason)
{
    ALWAYS_LOG(LOGIDENTIFIER, static_cast<int>(reason));
    Ref protectedThis { *this };

    m_resumeTaskCancellationGroup.cancel();

    switch (reason) {
    case ReasonForSuspension::BackForwardCache:
        stopWithoutDestroyingMediaPlayer();
        setBufferingPolicy(BufferingPolicy::MakeResourcesPurgeable);
        if (RefPtr mediaSession = m_mediaSession)
            mediaSession->addBehaviorRestriction(MediaElementSession::RequirePageConsentToResumeMedia);
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
    ALWAYS_LOG(LOGIDENTIFIER);

    setInActiveDocument(true);

    RefPtr mediaSession = m_mediaSession;
    if (mediaSession && !mediaSession->pageAllowsPlaybackAfterResuming())
        protectedDocument()->addMediaCanStartListener(*this);
    else {
        setPausedInternal(false);
        dispatchPlayPauseEventsIfNeedsQuirks();
    }

    if (mediaSession) {
        mediaSession->removeBehaviorRestriction(MediaElementSession::RequirePageConsentToResumeMedia);
        mediaSession->updateBufferingPolicy();
    }

    if (m_error && m_error->code() == MediaError::MEDIA_ERR_ABORTED && !m_resumeTaskCancellationGroup.hasPendingTask()) {
        // Restart the load if it was aborted in the middle by moving the document to the back/forward cache.
        // m_error is only left at MEDIA_ERR_ABORTED when the document becomes inactive (it is set to
        //  MEDIA_ERR_ABORTED while the abortEvent is being sent, but cleared immediately afterwards).
        // This behavior is not specified but it seems like a sensible thing to do.
        // As it is not safe to immedately start loading now, let's schedule a load.
        queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_resumeTaskCancellationGroup, [](auto& element) { element.prepareForLoad(); });
    }

    updateRenderer();
}

bool HTMLMediaElement::virtualHasPendingActivity() const
{
    // NOTE: This method will be called from a non-main thread.

    // A media element has pending activity if:
    // * It is initializing its media controls
    if (m_controlsState == ControlsState::Initializing)
        return true;

    // A paused media element may become a playing media element
    // if it was paused due to an interruption:
    bool isPlayingOrPossiblyCouldPlay = [&] {
        if (isPlaying())
            return true;

        // This function could be called on a non-main thread.
        SUPPRESS_UNCOUNTED_LOCAL auto* mediaSession = this->mediaSessionIfExists();
        if (!mediaSession)
            return false;

        if (mediaSession->state() != PlatformMediaSession::State::Interrupted)
            return false;

        auto stateToRestore = mediaSession->stateToRestore();
        return stateToRestore == PlatformMediaSession::State::Autoplaying
            || stateToRestore == PlatformMediaSession::State::Playing;
    }();

    // * It is playing, and is audible to the user:
    if (isPlayingOrPossiblyCouldPlay && canProduceAudio())
        return true;

    // If a media element is not directly observable by the user, it cannot
    // have pending activity if it does not have event listeners:
    if (!hasEventListeners() && (!m_player || !m_player->isGatheringVideoFrameMetadata()))
        return false;

    // A media element has pending activity if it has event listeners and:
    // * The load algorithm is pending, and will thus fire "loadstart" events:
    if (m_resourceSelectionTaskCancellationGroup.hasPendingTask())
        return true;

    // * It has a media engine and:
    if (m_player && m_player->hasMediaEngine()) {
        // * It is playing, and will thus fire "timeupdate" and "ended" events:
        if (isPlayingOrPossiblyCouldPlay)
            return true;

        // * It is seeking, and will thus fire "seeked" events:
        if (seeking())
            return true;

        // * It is loading, and will thus fire "progress" or "stalled" events:
        if (m_networkState == NETWORK_LOADING)
            return true;

#if ENABLE(MEDIA_STREAM)
        // * It has a live MediaStream object:
        if (m_mediaStreamSrcObject)
            return true;
#endif
    }

    // Otherwise, the media element will fire no events at event listeners, and
    // thus does not have observable pending activity.
    return false;
}

void HTMLMediaElement::mediaVolumeDidChange()
{
    // FIXME: We should try to reconcile this so there's no difference for m_volumeLocked.
    if (m_volumeLocked)
        return;

    INFO_LOG(LOGIDENTIFIER);
    updateVolume();
}

bool HTMLMediaElement::elementIsHidden() const
{
#if ENABLE(FULLSCREEN_API)
    auto* documentFullscreen = document().fullscreenIfExists();
    if (isVideo() && documentFullscreen && documentFullscreen->isFullscreen() && documentFullscreen->fullscreenElement())
        return false;
#endif

    if (m_videoFullscreenMode != VideoFullscreenModeNone)
        return false;

    return protectedDocument()->hidden() && (!m_player || !m_player->isVisibleForCanvas());
}

void HTMLMediaElement::visibilityStateChanged()
{
    bool elementIsHidden = this->elementIsHidden();
    if (elementIsHidden == m_elementIsHidden)
        return;

    m_elementIsHidden = elementIsHidden;
    HTMLMEDIAELEMENT_RELEASE_LOG(VISIBILITYSTATECHANGED, !m_elementIsHidden);

    updateSleepDisabling();
    protectedMediaSession()->visibilityChanged();
    if (RefPtr player = m_player)
        player->setPageIsVisible(!m_elementIsHidden);

#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

void HTMLMediaElement::setTextTrackRepresentataionBounds(const IntRect& bounds)
{
    m_textTrackRepresentationBounds = bounds;
    if (!m_requiresTextTrackRepresentation)
        return;

    if (!ensureMediaControls())
        return;

    if (auto* textTrackRepresentation = m_mediaControlsHost->textTrackRepresentation())
        textTrackRepresentation->setBounds(bounds);
}

void HTMLMediaElement::setRequiresTextTrackRepresentation(bool requiresTextTrackRepresentation)
{
    if (m_requiresTextTrackRepresentation == requiresTextTrackRepresentation)
        return;

    m_requiresTextTrackRepresentation = requiresTextTrackRepresentation;
    if (!ensureMediaControls())
        return;

    m_mediaControlsHost->requiresTextTrackRepresentationChanged();

    if (m_textTrackRepresentationBounds.isEmpty() || !m_requiresTextTrackRepresentation)
        return;

    if (auto* textTrackRepresentation = m_mediaControlsHost->textTrackRepresentation())
        textTrackRepresentation->setBounds(m_textTrackRepresentationBounds);
}

bool HTMLMediaElement::requiresTextTrackRepresentation() const
{
    return m_requiresTextTrackRepresentation;
}

void HTMLMediaElement::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    if (RefPtr player = m_player)
        player->setTextTrackRepresentation(representation);

    if (!representation) {
        protectedDocument()->clearMediaElementShowingTextTrack();
        return;
    }

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (representation->bounds().isEmpty()) {
        if (!m_textTrackRepresentationBounds.isEmpty())
            representation->setBounds(m_textTrackRepresentationBounds);
        else if (!m_videoFullscreenFrame.isEmpty())
            representation->setBounds(enclosingIntRect(m_videoFullscreenFrame));
    }
#endif

    protectedDocument()->setMediaElementShowingTextTrack(*this);
}

void HTMLMediaElement::syncTextTrackBounds()
{
    if (RefPtr player = m_player)
        player->syncTextTrackBounds();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void HTMLMediaElement::webkitShowPlaybackTargetPicker()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (processingUserGestureForMedia())
        removeBehaviorRestrictionsAfterFirstUserGesture();
    protectedMediaSession()->showPlaybackTargetPicker();
}

void HTMLMediaElement::wirelessRoutesAvailableDidChange()
{
    if (isWirelessPlaybackTargetDisabled())
        return;

    bool hasTargets = protectedMediaSession()->hasWirelessPlaybackTargets();
    Ref { m_remote }->availabilityChanged(hasTargets);

    enqueuePlaybackTargetAvailabilityChangedEvent(EnqueueBehavior::OnlyWhenChanged);
}

void HTMLMediaElement::mediaPlayerCurrentPlaybackTargetIsWirelessChanged(bool isCurrentPlayBackTargetWireless)
{
    setIsPlayingToWirelessTarget(m_player && isCurrentPlayBackTargetWireless);
}

void HTMLMediaElement::setIsPlayingToWirelessTarget(bool isPlayingToWirelessTarget)
{
    auto logSiteIdentifier = LOGIDENTIFIER;
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [isPlayingToWirelessTarget, logSiteIdentifier](auto& element) {
        if (element.isContextStopped())
            return;

        auto newValue = isPlayingToWirelessTarget && element.m_player && element.m_player->isCurrentPlaybackTargetWireless();
        if (newValue == element.m_isPlayingToWirelessTarget)
            return;

        UNUSED_PARAM(logSiteIdentifier);

        element.m_isPlayingToWirelessTarget = newValue;
        Ref { element.m_remote }->isPlayingToRemoteTargetChanged(element.m_isPlayingToWirelessTarget);
        ALWAYS_LOG_WITH_THIS(&element, logSiteIdentifier, element.m_isPlayingToWirelessTarget);
        element.configureMediaControls();
        element.mediaSession().isPlayingToWirelessPlaybackTargetChanged(element.m_isPlayingToWirelessTarget);
        element.mediaSession().canProduceAudioChanged();
        element.scheduleUpdateMediaState();
        element.updateSleepDisabling();

        element.m_failedToPlayToWirelessTarget = false;
        element.m_checkPlaybackTargetCompatibilityTimer.startOneShot(500_ms);

        if (!element.isContextStopped())
            element.dispatchEvent(Event::create(eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent, Event::CanBubble::No, Event::IsCancelable::Yes));
    });
}

void HTMLMediaElement::enqueuePlaybackTargetAvailabilityChangedEvent(EnqueueBehavior behavior)
{
    bool hasTargets = !isWirelessPlaybackTargetDisabled() && m_mediaSession && protectedMediaSession()->hasWirelessPlaybackTargets();
    if (behavior == EnqueueBehavior::OnlyWhenChanged && hasTargets == m_lastTargetAvailabilityEventState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "hasTargets = ", hasTargets);
    m_lastTargetAvailabilityEventState = hasTargets;
#if ENABLE(WIRELESS_PLAYBACK_TARGET_AVAILABILITY_API)
    Ref event = WebKitPlaybackTargetAvailabilityEvent::create(eventNames().webkitplaybacktargetavailabilitychangedEvent, hasTargets);
    scheduleEvent(WTFMove(event));
#endif
    scheduleUpdateMediaState();
}

void HTMLMediaElement::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& device)
{
    bool hasActiveRoute = device->hasActiveRoute();
    ALWAYS_LOG(LOGIDENTIFIER, hasActiveRoute);

    fireAndRestartWatchtimeTimer();

    if (RefPtr player = m_player)
        player->setWirelessPlaybackTarget(WTFMove(device));
    Ref { m_remote }->shouldPlayToRemoteTargetChanged(hasActiveRoute);
}

void HTMLMediaElement::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    if (RefPtr player = m_player) {
        player->setShouldPlayToPlaybackTarget(shouldPlay);
        setIsPlayingToWirelessTarget(player->isCurrentPlaybackTargetWireless());
    }
}

void HTMLMediaElement::playbackTargetPickerWasDismissed()
{
    Ref { m_remote }->playbackTargetPickerWasDismissed();
}

void HTMLMediaElement::remoteHasAvailabilityCallbacksChanged()
{
    bool hasListeners = hasEnabledTargetAvailabilityListeners();
    if (m_hasPlaybackTargetAvailabilityListeners == hasListeners)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "hasListeners: ", hasListeners);
    m_hasPlaybackTargetAvailabilityListeners = hasListeners;
    protectedMediaSession()->setHasPlaybackTargetAvailabilityListeners(hasListeners);
    scheduleUpdateMediaState();
}

bool HTMLMediaElement::hasWirelessPlaybackTargetAlternative() const
{
    if (m_loadState != LoadingFromSourceElement)
        return false;
    for (Ref source : childrenOfType<HTMLSourceElement>(*this)) {
        auto mediaURL = source->getNonEmptyURLAttribute(srcAttr);
        bool maybeSuitable = !mediaURL.isEmpty();
#if ENABLE(MEDIA_SOURCE)
        maybeSuitable &= !mediaURL.protocolIs(mediaSourceBlobProtocol);
#endif
        if (!maybeSuitable || !isSafeToLoadURL(mediaURL, InvalidURLAction::DoNothing, false))
            continue;

        return true;
    }
    return false;
}

bool HTMLMediaElement::hasTargetAvailabilityListeners()
{
    return hasEventListeners(eventNames().webkitplaybacktargetavailabilitychangedEvent) || m_remote->hasAvailabilityCallbacks();
}

bool HTMLMediaElement::hasEnabledTargetAvailabilityListeners()
{
    return !m_wirelessPlaybackTargetDisabled && hasTargetAvailabilityListeners();
}

void HTMLMediaElement::isWirelessPlaybackTargetDisabledChanged()
{
    bool disabled = equalLettersIgnoringASCIICase(attributeWithoutSynchronization(HTMLNames::webkitairplayAttr), "deny"_s)
        || hasAttributeWithoutSynchronization(HTMLNames::webkitwirelessvideoplaybackdisabledAttr)
        || hasAttributeWithoutSynchronization(HTMLNames::disableremoteplaybackAttr);
    if (m_wirelessPlaybackTargetDisabled == disabled)
        return;

    m_wirelessPlaybackTargetDisabled = disabled;

    if (hasTargetAvailabilityListeners()) {
        Ref mediaSession = this->mediaSession();
        if (!m_wirelessPlaybackTargetDisabled) {
            m_hasPlaybackTargetAvailabilityListeners = true;
            mediaSession->setActive(true);
            mediaSession->setHasPlaybackTargetAvailabilityListeners(true);
            enqueuePlaybackTargetAvailabilityChangedEvent(EnqueueBehavior::Always);
        } else {
            m_hasPlaybackTargetAvailabilityListeners = false;
            mediaSession->setHasPlaybackTargetAvailabilityListeners(false);

            // If the client has disabled remote playback, also has availability listeners,
            // and the last state sent to the client was that targets were available,
            // fire one last event indicating no pickable targets exist. This has the effect
            // of having players disable their remote playback picker buttons.
            if (m_lastTargetAvailabilityEventState)
                enqueuePlaybackTargetAvailabilityChangedEvent(EnqueueBehavior::Always);
        }
    }
    scheduleUpdateMediaState();
}

bool HTMLMediaElement::isWirelessPlaybackTargetDisabled() const
{
    return m_wirelessPlaybackTargetDisabled;
}

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

void HTMLMediaElement::dispatchEvent(Event& event)
{
    DEBUG_LOG(LOGIDENTIFIER, event.type());

    if (event.type() == eventNames().endedEvent) {
        if (m_removedBehaviorRestrictionsAfterFirstUserGesture)
            protectedDocument()->userActivatedMediaFinishedPlaying();

        updateSleepDisabling();
    }

    HTMLElement::dispatchEvent(event);

    // Forward the fullscreenchange event to the UserAgentShadowRoot so that
    // the media controls code can add "fullscreenchange" listeners without
    // changing the behavior of existing clients listening for the prefixed
    // "webkitfullscreenchange" event.
    if (event.type() == eventNames().fullscreenchangeEvent) {
        if (RefPtr root = userAgentShadowRoot())
            root->dispatchEvent(Event::create(eventNames().fullscreenchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }

    // Some pages may change the position/size of an inline video element
    // when/after the video element enters fullscreen (rdar://problem/55814988).
    // We need to fire the end fullscreen event to notify the page
    // to change the position/size back *before* exiting fullscreen.
    // Otherwise, the exit fullscreen animation will be incorrect.
    if (!m_videoFullscreenStandby && m_videoFullscreenMode == VideoFullscreenModeNone && event.type() == eventNames().webkitendfullscreenEvent)
        document().protectedPage()->chrome().client().exitVideoFullscreenForVideoElement(downcast<HTMLVideoElement>(*this));
}

bool HTMLMediaElement::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    if (eventType == eventNames().webkitneedkeyEvent)
        updateShouldContinueAfterNeedKey();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (eventType != eventNames().webkitplaybacktargetavailabilitychangedEvent)
        return Node::addEventListener(eventType, WTFMove(listener), options);

    bool isFirstAvailabilityChangedListener = !hasTargetAvailabilityListeners();

    if (!Node::addEventListener(eventType, WTFMove(listener), options))
        return false;

    if (isWirelessPlaybackTargetDisabled())
        return true;

    if (isFirstAvailabilityChangedListener) {
        m_hasPlaybackTargetAvailabilityListeners = true;
        Ref mediaSession = this->mediaSession();
        mediaSession->setActive(true);
        mediaSession->setHasPlaybackTargetAvailabilityListeners(true);
    }

    ALWAYS_LOG(LOGIDENTIFIER, "'webkitplaybacktargetavailabilitychanged'");

    enqueuePlaybackTargetAvailabilityChangedEvent(EnqueueBehavior::Always); // Ensure the event listener gets at least one event.
    return true;
#else
    return Node::addEventListener(eventType, WTFMove(listener), options);
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
}

bool HTMLMediaElement::removeEventListener(const AtomString& eventType, EventListener& listener, const EventListenerOptions& options)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    if (eventType == eventNames().webkitneedkeyEvent)
        updateShouldContinueAfterNeedKey();
#endif

    bool listenerWasRemoved = Node::removeEventListener(eventType, listener, options);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (eventType != eventNames().webkitplaybacktargetavailabilitychangedEvent)
        return listenerWasRemoved;

    if (!listenerWasRemoved)
        return false;

    bool didRemoveLastAvailabilityChangedListener = !hasTargetAvailabilityListeners();
    ALWAYS_LOG(LOGIDENTIFIER, "removed last listener = ", didRemoveLastAvailabilityChangedListener);
    if (didRemoveLastAvailabilityChangedListener) {
        m_hasPlaybackTargetAvailabilityListeners = false;
        protectedMediaSession()->setHasPlaybackTargetAvailabilityListeners(false);
        scheduleUpdateMediaState();
    }
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

    return listenerWasRemoved;
}

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
    RefPtr player = m_player;
    return player ? player->minFastReverseRate() : 0;
}

double HTMLMediaElement::maxFastForwardRate() const
{
    RefPtr player = m_player;
    return player ? player->maxFastForwardRate() : 0;
}

bool HTMLMediaElement::taintsOrigin(const SecurityOrigin& origin) const
{
    if (didPassCORSAccessCheck())
        return false;
    RefPtr player = m_player;
    return player && player->isCrossOrigin(origin);
}

bool HTMLMediaElement::isInFullscreenOrPictureInPicture() const
{
    bool inFullscreenOrPictureInPicture = isFullscreen();
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (RefPtr asVideo = dynamicDowncast<HTMLVideoElement>(*this))
        inFullscreenOrPictureInPicture |= asVideo->isInExternalPlayback();
#endif

    return inFullscreenOrPictureInPicture;
}

bool HTMLMediaElement::isFullscreen() const
{
#if ENABLE(FULLSCREEN_API)
    RefPtr documentFullscreen = document().fullscreenIfExists();
    if (documentFullscreen && documentFullscreen->isFullscreen() && documentFullscreen->fullscreenElement() == this)
        return true;
#endif

    return m_videoFullscreenMode != VideoFullscreenModeNone;
}

bool HTMLMediaElement::isStandardFullscreen() const
{
#if ENABLE(FULLSCREEN_API)
    RefPtr documentFullscreen = document().fullscreenIfExists();
    if (documentFullscreen && documentFullscreen->isFullscreen() && documentFullscreen->fullscreenElement() == this)
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

bool HTMLMediaElement::videoUsesElementFullscreen() const
{
#if ENABLE(FULLSCREEN_API)
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (document().settings().linearMediaPlayerEnabled()) {
        if (RefPtr player = m_player; player && player->supportsLinearMediaPlayer())
            return false;
    }
#endif

#if PLATFORM(IOS_FAMILY)
    if (document().settings().videoFullscreenRequiresElementFullscreen())
        return true;
#else
    return true;
#endif // PLATFORM(IOS_FAMILY)
#endif

    return false;
}

void HTMLMediaElement::setPlayerIdentifierForVideoElement()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    RefPtr page = document().page();
    if (!page || page->mediaPlaybackIsSuspended())
        return;

    RefPtr window = document().window();
    if (!window)
        return;

    if (RefPtr asVideo = dynamicDowncast<HTMLVideoElement>(*this)) {
        auto& client = document().page()->chrome().client();
        client.setPlayerIdentifierForVideoElement(*asVideo);
    }
}

void HTMLMediaElement::enterFullscreen(VideoFullscreenMode mode)
{
    ALWAYS_LOG(LOGIDENTIFIER, ", m_videoFullscreenMode = ", m_videoFullscreenMode, ", mode = ", mode);
    ASSERT(mode != VideoFullscreenModeNone);

    RefPtr page = document().page();
    if (!page || page->mediaPlaybackIsSuspended())
        return;

    RefPtr window = document().window();
    if (!window)
        return;

    if (m_videoFullscreenMode == mode)
        return;

    if (m_waitingToEnterFullscreen)
        return;

    m_changingVideoFullscreenMode = true;

    fireAndRestartWatchtimeTimer();

#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    if (videoUsesElementFullscreen() && page->isDocumentFullscreenEnabled() && isInWindowOrStandardFullscreen(mode)) {
        m_temporarilyAllowingInlinePlaybackAfterFullscreen = false;
        m_waitingToEnterFullscreen = true;
        auto fullscreenCheckType = m_ignoreFullscreenPermissionsPolicy ? DocumentFullscreen::ExemptIFrameAllowFullscreenRequirement : DocumentFullscreen::EnforceIFrameAllowFullscreenRequirement;
        m_ignoreFullscreenPermissionsPolicy = false;
        protectedDocument()->protectedFullscreen()->requestFullscreen(*this, fullscreenCheckType, [weakThis = WeakPtr { *this }](ExceptionOr<void> result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !result.hasException())
                return;
            protectedThis->m_changingVideoFullscreenMode = false;
            protectedThis->m_waitingToEnterFullscreen = false;
        }, mode);
        return;
    }
#endif

    if (mediaSession().hasBehaviorRestriction(MediaElementSession::RequireUserGestureForFullscreen))
        window->consumeTransientActivation();

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [mode, logIdentifier = LOGIDENTIFIER](auto& element) {
        if (element.isContextStopped())
            return;

        if (element.document().hidden() && mode != HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) {
            ALWAYS_LOG_WITH_THIS(&element, logIdentifier, " returning because document is hidden");
            element.m_changingVideoFullscreenMode = false;
            return;
        }

        if (RefPtr asVideo = dynamicDowncast<HTMLVideoElement>(element)) {
            auto& client = element.document().page()->chrome().client();
            auto supportsFullscreen = client.supportsVideoFullscreen(mode);
            auto canEnterFullscreen = client.canEnterVideoFullscreen(*asVideo, mode);
            if (supportsFullscreen && canEnterFullscreen) {
                ALWAYS_LOG_WITH_THIS(&element, logIdentifier, "Entering fullscreen mode ", mode, ", element.m_videoFullscreenStandby = ", element.m_videoFullscreenStandby);

                element.m_temporarilyAllowingInlinePlaybackAfterFullscreen = false;
                if (isInWindowOrStandardFullscreen(mode))
                    element.m_waitingToEnterFullscreen = true;

                auto oldMode = element.m_videoFullscreenMode;
                element.setFullscreenMode(mode);
                element.configureMediaControls();

                client.enterVideoFullscreenForVideoElement(*asVideo, element.m_videoFullscreenMode, element.m_videoFullscreenStandby);
                if (element.m_videoFullscreenStandby)
                    return;

                if (isInWindowOrStandardFullscreen(mode))
                    element.scheduleEvent(eventNames().webkitbeginfullscreenEvent);
                else if (isInWindowOrStandardFullscreen(oldMode)  && !element.document().quirks().shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk())
                    element.scheduleEvent(eventNames().webkitendfullscreenEvent);

                return;
            }
            ALWAYS_LOG_WITH_THIS(&element, logIdentifier, "Could not enter fullscreen mode ", mode, ", support = ", supportsFullscreen, ", canEnter = ", canEnterFullscreen);
        }

        element.m_changingVideoFullscreenMode = false;
    });
}

void HTMLMediaElement::enterFullscreen()
{
    enterFullscreen(VideoFullscreenModeStandard);
}

void HTMLMediaElement::exitFullscreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_waitingToEnterFullscreen = false;

#if ENABLE(FULLSCREEN_API)
    Ref fullscreen = protectedDocument()->fullscreen();
    if (fullscreen->fullscreenElement() == this) {
        if (fullscreen->isFullscreen()) {
            m_changingVideoFullscreenMode = true;
            fullscreen->fullyExitFullscreen();
        }

        if (isInWindowOrStandardFullscreen(m_videoFullscreenMode))
            return;
    }
#endif

    ASSERT(m_videoFullscreenMode != VideoFullscreenModeNone);
    VideoFullscreenMode oldVideoFullscreenMode = m_videoFullscreenMode;

    if (!document().page())
        return;

    auto* videoElement = dynamicDowncast<HTMLVideoElement>(*this);
    if (!videoElement)
        return;

    if (!paused() && protectedMediaSession()->requiresFullscreenForVideoPlayback()) {
        if (!document().settings().allowsInlineMediaPlaybackAfterFullscreen() || isVideoTooSmallForInlinePlayback())
            pauseInternal();
        else {
            // Allow inline playback, but set a flag so pausing and starting again (e.g. when scrubbing or looping) won't go back to fullscreen.
            // Also set the controls attribute so the user will be able to control playback.
            m_temporarilyAllowingInlinePlaybackAfterFullscreen = true;
            setControls(true);
        }
    }

    if (isSuspended()) {
        setFullscreenMode(VideoFullscreenModeNone);
        document().protectedPage()->chrome().client().exitVideoFullscreenToModeWithoutAnimation(*videoElement, VideoFullscreenModeNone);
    } else if (document().protectedPage()->chrome().client().supportsVideoFullscreen(oldVideoFullscreenMode)) {
        if (m_videoFullscreenStandby) {
            setFullscreenMode(VideoFullscreenModeNone);
            m_changingVideoFullscreenMode = true;
            document().protectedPage()->chrome().client().enterVideoFullscreenForVideoElement(*videoElement, m_videoFullscreenMode, m_videoFullscreenStandby);
            return;
        }

        m_changingVideoFullscreenMode = true;

        if (isInWindowOrStandardFullscreen(oldVideoFullscreenMode)) {
            setFullscreenMode(VideoFullscreenModeNone);
            // The exit fullscreen request will be sent in dispatchEvent().
            scheduleEvent(eventNames().webkitendfullscreenEvent);
            return;
        }

        setFullscreenMode(VideoFullscreenModeNone);
        if (RefPtr page = document().page())
            page->chrome().client().exitVideoFullscreenForVideoElement(*videoElement);
    }
}

void HTMLMediaElement::prepareForVideoFullscreenStandby()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (!document().page())
        return;

    document().protectedPage()->chrome().client().prepareForVideoFullscreen();
#endif
}

void HTMLMediaElement::willBecomeFullscreenElement(VideoFullscreenMode mode)
{
    fireAndRestartWatchtimeTimer();

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    HTMLMediaElementEnums::VideoFullscreenMode oldVideoFullscreenMode = m_videoFullscreenMode;
#endif

    if (!isInWindowOrStandardFullscreen(m_videoFullscreenMode))
        setFullscreenMode(mode);

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    if (oldVideoFullscreenMode == VideoFullscreenModePictureInPicture) {
        if (RefPtr video = dynamicDowncast<HTMLVideoElement>(*this)) {
            if (RefPtr page = document().page(); page && mode == VideoFullscreenModeInWindow)
                page->chrome().client().exitVideoFullscreenForVideoElement(*video);
            else
                video->exitToFullscreenModeWithoutAnimationIfPossible(oldVideoFullscreenMode, mode);
        }
    }
#endif

    Element::willBecomeFullscreenElement();
}

void HTMLMediaElement::didBecomeFullscreenElement()
{
    ALWAYS_LOG(LOGIDENTIFIER, ", fullscreen mode = ", fullscreenMode());
    m_waitingToEnterFullscreen = false;
    m_changingVideoFullscreenMode = false;
    scheduleUpdatePlayState();
}

void HTMLMediaElement::willStopBeingFullscreenElement()
{
    fireAndRestartWatchtimeTimer();

    if (isInWindowOrStandardFullscreen(fullscreenMode()))
        setFullscreenMode(VideoFullscreenModeNone);
}

void HTMLMediaElement::didStopBeingFullscreenElement()
{
    m_changingVideoFullscreenMode = false;
}

#if ENABLE(FULLSCREEN_API)
void HTMLMediaElement::documentFullscreenChanged(bool isChildOfElementFullscreen)
{
    m_isChildOfElementFullscreen = isChildOfElementFullscreen;
    updatePlayerDynamicRangeLimit();
}
#endif

PlatformLayer* HTMLMediaElement::platformLayer() const
{
    RefPtr player = m_player;
    return player ? player->platformLayer() : nullptr;
}

void HTMLMediaElement::setPreparedToReturnVideoLayerToInline(bool value)
{
    m_preparedForInline = value;
    if (m_preparedForInline && m_preparedForInlineCompletionHandler) {
        m_preparedForInlineCompletionHandler();
        m_preparedForInlineCompletionHandler = nullptr;
    }
}

void HTMLMediaElement::waitForPreparedForInlineThen(Function<void()>&& completionHandler)
{
    INFO_LOG(LOGIDENTIFIER);
    ASSERT(!m_preparedForInlineCompletionHandler);
    if (m_preparedForInline)  {
        completionHandler();
        return;
    }

    m_preparedForInlineCompletionHandler = WTFMove(completionHandler);
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

void HTMLMediaElement::willExitFullscreen()
{
    if (RefPtr player = m_player)
        player->updateVideoFullscreenInlineImage();
}

bool HTMLMediaElement::isVideoLayerInline()
{
    return !m_videoFullscreenLayer;
}

RetainPtr<PlatformLayer> HTMLMediaElement::createVideoFullscreenLayer()
{
    if (RefPtr player = m_player)
        return player->createVideoFullscreenLayer();
    return nullptr;
}

void HTMLMediaElement::setVideoFullscreenLayer(PlatformLayer* platformLayer, Function<void()>&& completionHandler)
{
    INFO_LOG(LOGIDENTIFIER);
    m_videoFullscreenLayer = platformLayer;
    if (!m_player) {
        completionHandler();
        return;
    }

    RefPtr { m_player }->setVideoFullscreenLayer(platformLayer, WTFMove(completionHandler));
    invalidateStyleAndLayerComposition();
    updateTextTrackDisplay();
}

void HTMLMediaElement::setVideoFullscreenFrame(const FloatRect& frame)
{
    m_videoFullscreenFrame = frame;
    if (RefPtr player = m_player)
        player->setVideoFullscreenFrame(frame);
}

void HTMLMediaElement::setVideoFullscreenGravity(MediaPlayer::VideoGravity gravity)
{
    m_videoFullscreenGravity = gravity;
    if (RefPtr player = m_player)
        player->setVideoFullscreenGravity(gravity);
}

#else

bool HTMLMediaElement::isVideoLayerInline()
{
    return true;
};

#endif

bool HTMLMediaElement::hasClosedCaptions() const
{
    RefPtr player = m_player;
    if (player && player->hasClosedCaptions())
        return true;

    if (!m_textTracks)
        return false;

    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
        auto& track = *m_textTracks->item(i);
        if (track.readinessState() == TextTrack::FailedToLoad)
            continue;
        if (track.kind() == TextTrack::Kind::Captions || track.kind() == TextTrack::Kind::Subtitles)
            return true;
    }

    return false;
}

bool HTMLMediaElement::closedCaptionsVisible() const
{
    return m_closedCaptionsVisible;
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
        updateTextTrackDisplay();
    }
    if (RefPtr player = m_player; player && m_textTracksWhenResourceSelectionBegan.contains(track)) {
        if (track->readinessState() != TextTrack::Loading)
            setReadyState(player->readyState());
    }
}

void HTMLMediaElement::configureTextTrackDisplay(TextTrackVisibilityCheckType checkType)
{
    HTMLMEDIAELEMENT_RELEASE_LOG(CONFIGURETEXTTRACKDISPLAY, convertEnumerationToString(checkType).utf8());
    ASSERT(m_textTracks);

    if (m_processingPreferenceChange)
        return;

    if (isSuspended() || isContextStopped())
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

    if (!m_haveVisibleTextTrack)
        return;

    updateTextTrackDisplay();
}

void HTMLMediaElement::updateTextTrackDisplay()
{
    if (ensureMediaControls())
        m_mediaControlsHost->updateTextTrackContainer();
}

void HTMLMediaElement::updateTextTrackRepresentationImageIfNeeded()
{
    if (ensureMediaControls())
        m_mediaControlsHost->updateTextTrackRepresentationImageIfNeeded();
}

void HTMLMediaElement::showCaptionDisplaySettingsPreview()
{
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (RefPtr mediaControlsHost = m_mediaControlsHost.get())
        mediaControlsHost->showCaptionDisplaySettingsPreview();
#endif
}

void HTMLMediaElement::hideCaptionDisplaySettingsPreview()
{
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (RefPtr mediaControlsHost = m_mediaControlsHost.get())
        mediaControlsHost->hideCaptionDisplaySettingsPreview();
#endif
}

void HTMLMediaElement::setClosedCaptionsVisible(bool closedCaptionVisible)
{
    ALWAYS_LOG(LOGIDENTIFIER, closedCaptionVisible);

    m_closedCaptionsVisible = false;

    if (!m_player || !hasClosedCaptions())
        return;

    m_closedCaptionsVisible = closedCaptionVisible;
    RefPtr { m_player }->setClosedCaptionsVisible(closedCaptionVisible);

    markCaptionAndSubtitleTracksAsUnconfigured(Immediately);
    updateTextTrackDisplay();
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
    ALWAYS_LOG(LOGIDENTIFIER, "waiting = ", m_isWaitingUntilMediaCanStart, ", paused = ", m_pausedInternal);

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

    HTMLMEDIAELEMENT_RELEASE_LOG(SETSHOULDDELAYLOADEVENT, shouldDelay);

    m_shouldDelayLoadEvent = shouldDelay;
    if (shouldDelay)
        protectedDocument()->incrementLoadEventDelayCount();
    else
        protectedDocument()->decrementLoadEventDelayCount();
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

HashSet<SecurityOriginData> HTMLMediaElement::originsInMediaCache(const String& path)
{
    return MediaPlayer::originsInMediaCache(path);
}

void HTMLMediaElement::clearMediaCache(const String& path, WallTime modifiedSince)
{
    MediaPlayer::clearMediaCache(path, modifiedSince);
}

void HTMLMediaElement::clearMediaCacheForOrigins(const String& path, const HashSet<SecurityOriginData>& origins)
{
    MediaPlayer::clearMediaCacheForOrigins(path, origins);
}

void HTMLMediaElement::privateBrowsingStateDidChange(PAL::SessionID sessionID)
{
    if (RefPtr player = m_player)
        player->setPrivateBrowsingMode(sessionID.isEphemeral());
}

bool HTMLMediaElement::shouldForceControlsDisplay() const
{
    if (isFullscreen() && videoUsesElementFullscreen())
        return true;

    // Always create controls for autoplay video that requires user gesture due to being in low power mode.
    return isVideo() && autoplay() && (mediaSession().hasBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoDueToLowPowerMode) || mediaSession().hasBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoDueToAggressiveThermalMitigation));
}

void HTMLMediaElement::configureMediaControls()
{
    bool requireControls = controls();

    // Always create controls for video when fullscreen playback is required.
    if (isVideo() && protectedMediaSession()->requiresFullscreenForVideoPlayback())
        requireControls = true;

    if (shouldForceControlsDisplay())
        requireControls = true;

    // Always create controls when in full screen mode.
    if (isFullscreen() && videoUsesElementFullscreen())
        requireControls = true;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (m_isPlayingToWirelessTarget)
        requireControls = true;
#endif

    if (!requireControls || !isConnected() || !inActiveDocument())
        return;

    ensureMediaControls();
}

void HTMLMediaElement::captionPreferencesChanged()
{
    if (!isVideo())
        return;

    if (RefPtr mediaControlsHost = m_mediaControlsHost.get())
        mediaControlsHost->captionPreferencesChanged();

    if (RefPtr player = m_player)
        player->tracksChanged();

    RefPtr page = document().page();
    if (!page)
        return;

    auto& captionPreferences = page->checkedGroup()->ensureCaptionPreferences();
    m_userPrefersTextDescriptions = captionPreferences.userPrefersTextDescriptions();
    m_userPrefersExtendedDescriptions = m_userPrefersTextDescriptions && document().settings().extendedAudioDescriptionsEnabled();

    CaptionUserPreferences::CaptionDisplayMode displayMode = captionPreferences.captionDisplayMode();
    if (captionDisplayMode() == displayMode)
        return;

    m_captionDisplayMode = displayMode;
    setClosedCaptionsVisible(captionDisplayMode() == CaptionUserPreferences::CaptionDisplayMode::AlwaysOn);
}

CaptionUserPreferences::CaptionDisplayMode HTMLMediaElement::captionDisplayMode()
{
    if (!m_captionDisplayMode) {
        if (RefPtr page = document().page())
            m_captionDisplayMode = page->checkedGroup()->ensureProtectedCaptionPreferences()->captionDisplayMode();
        else
            m_captionDisplayMode = CaptionUserPreferences::CaptionDisplayMode::Automatic;
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
    m_configureTextTracksTaskCancellationGroup.cancel();
    if (mode == Immediately) {
        Ref protectedThis { *this }; // configureTextTracks calls methods that can trigger arbitrary DOM mutations.
        configureTextTracks();
    }
    else
        scheduleConfigureTextTracks();
}

PlatformDynamicRangeLimit HTMLMediaElement::computePlayerDynamicRangeLimit() const
{
    constexpr auto maxLimitWhenSuppressingHDR = PlatformDynamicRangeLimit::defaultWhenSuppressingHDRInVideos();
    if (m_platformDynamicRangeLimit <= maxLimitWhenSuppressingHDR)
        return m_platformDynamicRangeLimit;

    bool shouldSuppressHDR = [this]() {
        if (!document().settings().suppressHDRShouldBeAllowedInFullscreenVideo()) {
            if (m_videoFullscreenMode == VideoFullscreenModeStandard)
                return false;

#if ENABLE(FULLSCREEN_API)
            if (m_isChildOfElementFullscreen)
                return false;
#endif
        }

        if (Page* page = document().page())
            return page->shouldSuppressHDR();

        return false;
    }();
    return shouldSuppressHDR ? maxLimitWhenSuppressingHDR : m_platformDynamicRangeLimit;
}

// Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS because this function does conditional locking of m_audioSourceNode->processLock()
// which analysis doesn't support.
void HTMLMediaElement::createMediaPlayer() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    HTMLMEDIAELEMENT_RELEASE_LOG(CREATEMEDIAPLAYER);

    invalidateWatchtimeTimer();
    invalidateBufferingStopwatch();

    Ref mediaSession = this->mediaSession();
    mediaSession->setActive(true);

#if ENABLE(WEB_AUDIO)
    RefPtr protectedAudioSourceNode = m_audioSourceNode.get();
    std::optional<Locker<Lock>> audioSourceNodeLocker;
    if (m_audioSourceNode)
        audioSourceNodeLocker.emplace(m_audioSourceNode->processLock());
#endif

#if ENABLE(MEDIA_SOURCE)
    detachMediaSource();
#endif

    forgetResourceSpecificTracks();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (m_isPlayingToWirelessTarget)
        setIsPlayingToWirelessTarget(false);
#endif

    m_networkErrorOccured = false;
    m_lastContentTypeUsed.reset();
    if (RefPtr player = std::exchange(m_player, { })) {
        // The sniffer completionHandler would have taken a reference to the old MediaPlayer.
        cancelSniffer();
        player->invalidate();
    }

    m_player = MediaPlayer::create(*this);
    RefPtr player = m_player;
    player->setMessageClientForTesting(m_internalMessageClient.get());
    player->setBufferingPolicy(m_bufferingPolicy);
    player->setPreferredDynamicRangeMode(m_overrideDynamicRangeMode.value_or(preferredDynamicRangeMode(document().protectedView().get())));
    player->setShouldDisableHDR(shouldDisableHDR());
    player->setPlatformDynamicRangeLimit(computePlayerDynamicRangeLimit());
    player->setVolumeLocked(m_volumeLocked);
    player->setMuted(effectiveMuted());
    RefPtr page = document().page();
    player->setPageIsVisible(!m_elementIsHidden);
    player->setVisibleInViewport(isVisibleInViewport());
    player->setInFullscreenOrPictureInPicture(isInFullscreenOrPictureInPicture());

    schedulePlaybackControlsManagerUpdate();
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    updateShouldContinueAfterNeedKey();
#endif

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    player->setPrefersSpatialAudioExperience(document().settings().preferSpatialAudioExperience());
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif

#if PLATFORM(IOS_FAMILY)
    sceneIdentifierDidChange();
#endif

#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode) {
        // When creating the player, make sure its AudioSourceProvider knows about the MediaElementAudioSourceNode.
        if (audioSourceProvider())
            audioSourceProvider()->setClient(m_audioSourceNode.get());
    }
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (hasEnabledTargetAvailabilityListeners()) {
        m_hasPlaybackTargetAvailabilityListeners = true;
        mediaSession->setHasPlaybackTargetAvailabilityListeners(true);
        enqueuePlaybackTargetAvailabilityChangedEvent(EnqueueBehavior::Always); // Ensure the event listener gets at least one event.
    }
#endif

    updateSleepDisabling();
    updateRenderer();
}

#if ENABLE(WEB_AUDIO)

void HTMLMediaElement::setAudioSourceNode(MediaElementAudioSourceNode* sourceNode)
{
    m_audioSourceNode = sourceNode;

    if (audioSourceProvider())
        audioSourceProvider()->setClient(m_audioSourceNode.get());
}

// This may get called on the audio thread by MediaElementAudioSourceNode.
AudioSourceProvider* HTMLMediaElement::audioSourceProvider()
{
    if (RefPtr player = m_player)
        return player->audioSourceProvider();

    return nullptr;
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
    // FIXME: It does not seem OK that this algorithm iterates the media elements in a random order.
    protectedDocument()->forEachMediaElement([&] (HTMLMediaElement& element) {
        // and which also has a mediagroup attribute, and whose mediagroup attribute has the same value as
        // the new value of m's mediagroup attribute,
        if (&element != this && !controller() && element.mediaGroup() == group) {
            //  then let controller be that media element's current media controller.
            setController(element.controller());
        }
    });

    // Otherwise, let controller be a newly created MediaController.
    if (!controller())
        setController(MediaController::create(protectedDocument()));
}

MediaController* HTMLMediaElement::controller() const
{
    return m_mediaController.get();
}

void HTMLMediaElement::setController(RefPtr<MediaController>&& controller)
{
    if (RefPtr mediaController = m_mediaController)
        mediaController->removeMediaElement(*this);

    m_mediaController = WTFMove(controller);

    if (RefPtr mediaController = m_mediaController)
        mediaController->addMediaElement(*this);
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
    if (RefPtr mediaController = m_mediaController)
        mediaController->reportControllerState();
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
    RefPtr mediaController = m_mediaController;
    if (!mediaController)
        return false;

    // A media element is blocked on its media controller if the MediaController is a blocked
    // media controller,
    if (mediaController->isBlocked())
        return true;

    // or if its media controller position is either before the media resource's earliest possible
    // position relative to the MediaController's timeline or after the end of the media resource
    // relative to the MediaController's timeline.
    double mediaControllerPosition = mediaController->currentTime();
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
            m_sleepDisabler = makeUnique<SleepDisabler>("com.apple.WebCore: HTMLMediaElement playback"_s, type, protectedDocument()->pageID());
    }

    if (RefPtr player = m_player)
        player->setShouldDisableSleep(shouldDisableSleep == SleepType::Display);
}

#if ENABLE(MEDIA_STREAM)
static inline bool isRemoteMediaStreamVideoTrack(const Ref<MediaStreamTrack>& item)
{
    auto& track = item.get();
    return track.privateTrack().type() == RealtimeMediaSource::Type::Video && !track.isCaptureTrack() && !track.isCanvas();
}
#endif

HTMLMediaElement::SleepType HTMLMediaElement::shouldDisableSleep() const
{
#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WPE)
    return SleepType::None;
#endif
    if (m_sentEndEvent)
        return SleepType::None;

    if (RefPtr player = m_player; !player || !player->timeIsProgressing() || loop())
        return SleepType::None;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    // If the media is playing remotely, we can't know definitively whether it has audio or video tracks.
    if (m_isPlayingToWirelessTarget)
        return SleepType::System;
#endif

    if (RefPtr manager = sessionManager(); manager && manager->processIsSuspended())
        return SleepType::None;

    bool shouldBeAbleToSleep = mediaType() != PlatformMediaSession::MediaType::VideoAudio;
#if ENABLE(MEDIA_STREAM)
    // Remote media stream video tracks may have their corresponding audio tracks being played outside of the media element. Let's ensure to not IDLE the screen in that case.
    // FIXME: We should check that audio is being/to be played. Ideally, we would come up with a media stream agnostic heuristisc.
    RefPtr mediaStreamSrcObject = m_mediaStreamSrcObject;
    shouldBeAbleToSleep = shouldBeAbleToSleep && !(mediaStreamSrcObject && mediaStreamSrcObject->hasMatchingTrack(isRemoteMediaStreamVideoTrack));
#endif

    if (shouldBeAbleToSleep)
        return SleepType::None;

#if HAVE(IDLE_SLEEP_STATE)
    if (m_elementIsHidden)
        return SleepType::System;
#endif

    return SleepType::Display;
}

String HTMLMediaElement::mediaPlayerReferrer() const
{
    RefPtr frame = document().frame();
    if (!frame)
        return String();

    return SecurityPolicy::generateReferrerHeader(document().referrerPolicy(), m_currentSrc, frame->loader().outgoingReferrerURL(), OriginAccessPatternsForWebProcess::singleton());
}

String HTMLMediaElement::mediaPlayerUserAgent() const
{
    RefPtr frame = document().frame();
    if (!frame)
        return String();

    return frame->loader().userAgent(m_currentSrc);
}

static inline PlatformTextTrackData::TrackKind toPlatform(TextTrack::Kind kind)
{
    switch (kind) {
    case TextTrack::Kind::Captions:
        return PlatformTextTrackData::TrackKind::Caption;
    case TextTrack::Kind::Chapters:
        return PlatformTextTrackData::TrackKind::Chapter;
    case TextTrack::Kind::Descriptions:
        return PlatformTextTrackData::TrackKind::Description;
    case TextTrack::Kind::Forced:
        return PlatformTextTrackData::TrackKind::Forced;
    case TextTrack::Kind::Metadata:
        return PlatformTextTrackData::TrackKind::MetaData;
    case TextTrack::Kind::Subtitles:
        return PlatformTextTrackData::TrackKind::Subtitle;
    }
    ASSERT_NOT_REACHED();
    return PlatformTextTrackData::TrackKind::Caption;
}

static inline PlatformTextTrackData::TrackMode toPlatform(TextTrack::Mode mode)
{
    switch (mode) {
    case TextTrack::Mode::Disabled:
        return PlatformTextTrackData::TrackMode::Disabled;
    case TextTrack::Mode::Hidden:
        return PlatformTextTrackData::TrackMode::Hidden;
    case TextTrack::Mode::Showing:
        return PlatformTextTrackData::TrackMode::Showing;
    }
    ASSERT_NOT_REACHED();
    return PlatformTextTrackData::TrackMode::Disabled;
}

Vector<RefPtr<PlatformTextTrack>> HTMLMediaElement::outOfBandTrackSources()
{
    Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources;
    for (Ref trackElement : childrenOfType<HTMLTrackElement>(*this)) {
        URL url = trackElement->getNonEmptyURLAttribute(srcAttr);
        if (url.isEmpty())
            continue;

        if (!isAllowedToLoadMediaURL(*this, url, trackElement->isInUserAgentShadowTree()))
            continue;

        auto& track = trackElement->track();
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

        outOfBandTrackSources.append(PlatformTextTrack::createOutOfBand(trackElement->attributeWithoutSynchronization(labelAttr), trackElement->attributeWithoutSynchronization(srclangAttr), url.string(), toPlatform(track.mode()), toPlatform(kind), track.uniqueId(), trackElement->isDefault()));
    }

    return outOfBandTrackSources;
}

bool HTMLMediaElement::mediaPlayerIsFullscreen() const
{
    return isFullscreen();
}

bool HTMLMediaElement::mediaPlayerIsFullscreenPermitted() const
{
    return protectedMediaSession()->fullscreenPermitted();
}

bool HTMLMediaElement::mediaPlayerIsVideo() const
{
    return isVideo();
}

LayoutRect HTMLMediaElement::mediaPlayerContentBoxRect() const
{
    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return { };
    CheckedRef protectedEnclosingBox { renderer->enclosingBox() };
    return protectedEnclosingBox->contentBoxRect();
}

float HTMLMediaElement::mediaPlayerContentsScale() const
{
    if (auto page = document().page())
        return page->pageScaleFactor() * page->deviceScaleFactor();
    return 1;
}

bool HTMLMediaElement::mediaPlayerPlatformVolumeConfigurationRequired() const
{
    return !m_volumeInitialized;
}

bool HTMLMediaElement::mediaPlayerIsLooping() const
{
    return loop();
}

CachedResourceLoader* HTMLMediaElement::mediaPlayerCachedResourceLoader()
{
    return &protectedDocument()->cachedResourceLoader();
}

Ref<PlatformMediaResourceLoader> HTMLMediaElement::mediaPlayerCreateResourceLoader()
{
    auto destination = isVideo() ? FetchOptions::Destination::Video : FetchOptions::Destination::Audio;
    Ref mediaResourceLoader = MediaResourceLoader::create(protectedDocument(), *this, crossOrigin(), destination);

    m_lastMediaResourceLoaderForTesting = mediaResourceLoader.get();

    return mediaResourceLoader;
}

const MediaResourceLoader* HTMLMediaElement::lastMediaResourceLoaderForTesting() const
{
    return m_lastMediaResourceLoaderForTesting.get();
}

bool HTMLMediaElement::mediaPlayerShouldUsePersistentCache() const
{
    if (Page* page = document().page())
        return !page->usesEphemeralSession() && !page->isResourceCachingDisabledByWebInspector();

    return false;
}

const String& HTMLMediaElement::mediaPlayerMediaCacheDirectory() const
{
    return mediaCacheDirectory();
}

String HTMLMediaElement::sourceApplicationIdentifier() const
{
    if (RefPtr frame = document().frame()) {
        if (NetworkingContext* networkingContext = frame->loader().networkingContext())
            return networkingContext->sourceApplicationIdentifier();
    }
    return emptyString();
}

void HTMLMediaElement::setPreferredDynamicRangeMode(DynamicRangeMode mode)
{
    if (!m_player || m_overrideDynamicRangeMode)
        return;

    Ref player = *m_player;
    player->setPreferredDynamicRangeMode(mode);
    player->setShouldDisableHDR(shouldDisableHDR());
}

void HTMLMediaElement::setOverridePreferredDynamicRangeMode(DynamicRangeMode mode)
{
    m_overrideDynamicRangeMode = mode;
    if (!m_player)
        return;

    Ref player = *m_player;
    player->setPreferredDynamicRangeMode(mode);
    player->setShouldDisableHDR(shouldDisableHDR());
}

void HTMLMediaElement::updatePlayerDynamicRangeLimit() const
{
    if (RefPtr player = m_player)
        player->setPlatformDynamicRangeLimit(computePlayerDynamicRangeLimit());
}

void HTMLMediaElement::dynamicRangeLimitDidChange(PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    m_platformDynamicRangeLimit = platformDynamicRangeLimit;
    updatePlayerDynamicRangeLimit();
}

void HTMLMediaElement::shouldSuppressHDRDidChange()
{
    updatePlayerDynamicRangeLimit();
}

Vector<String> HTMLMediaElement::mediaPlayerPreferredAudioCharacteristics() const
{
    if (RefPtr page = document().page())
        return page->checkedGroup()->ensureProtectedCaptionPreferences()->preferredAudioCharacteristics();
    return { };
}

#if PLATFORM(IOS_FAMILY)

String HTMLMediaElement::mediaPlayerNetworkInterfaceName() const
{
    return DeprecatedGlobalSettings::networkInterfaceName();
}

void HTMLMediaElement::mediaPlayerGetRawCookies(const URL& url, MediaPlayerClient::GetRawCookiesCallback&& completionHandler) const
{
    RefPtr page = document().page();
    if (!page) {
        completionHandler({ });
        return;
    }

    Vector<Cookie> cookies;
    page->cookieJar().getRawCookies(document(), url, cookies);
    completionHandler(WTFMove(cookies));
}

#endif

void HTMLMediaElement::mediaPlayerEngineFailedToLoad()
{
    RefPtr player = m_player;
    if (!player)
        return;

    if (player->networkState() == MediaPlayer::NetworkState::NetworkError)
        m_networkErrorOccured = true;

    if (RefPtr page = protectedDocument()->page())
        page->checkedDiagnosticLoggingClient()->logDiagnosticMessageWithValue(DiagnosticLoggingKeys::engineFailedToLoadKey(), player->engineDescription(), player->platformErrorCode(), 4, ShouldSample::No);
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

const std::optional<Vector<String>>& HTMLMediaElement::allowedMediaContainerTypes() const
{
    return document().settings().allowedMediaContainerTypes();
}

const std::optional<Vector<String>>& HTMLMediaElement::allowedMediaCodecTypes() const
{
    return document().settings().allowedMediaCodecTypes();
}

const std::optional<Vector<FourCC>>& HTMLMediaElement::allowedMediaVideoCodecIDs() const
{
    return document().settings().allowedMediaVideoCodecIDs();
}

const std::optional<Vector<FourCC>>& HTMLMediaElement::allowedMediaAudioCodecIDs() const
{
    return document().settings().allowedMediaAudioCodecIDs();
}

const std::optional<Vector<FourCC>>& HTMLMediaElement::allowedMediaCaptionFormatTypes() const
{
    return document().settings().allowedMediaCaptionFormatTypes();
}

void HTMLMediaElement::mediaPlayerBufferedTimeRangesChanged()
{
    if (!m_textTracks || m_readyState < HAVE_ENOUGH_DATA || m_bufferedTimeRangesChangedTaskCancellationGroup.hasPendingTask())
        return;

    auto duration = durationMediaTime();
    if (!duration.isValid() || duration.toDouble() < 60.)
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier, "task scheduled");
    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_bufferedTimeRangesChangedTaskCancellationGroup, [logSiteIdentifier](auto& element) {
        UNUSED_PARAM(logSiteIdentifier);
        ALWAYS_LOG_WITH_THIS(&element, logSiteIdentifier, "lambda(), task fired");
        if (!element.m_player || !element.m_textTracks)
            return;

        for (unsigned i = 0; i < element.m_textTracks->length(); ++i) {
            Ref track = *element.m_textTracks->item(i);
            if (!track->shouldPurgeCuesFromUnbufferedRanges())
                continue;

            track->removeCuesNotInTimeRanges(
#if ENABLE(MEDIA_SOURCE)
                element.m_mediaSource ? element.m_mediaSource->buffered() :
#endif
                element.m_player->buffered());
        }
    });
}

void HTMLMediaElement::removeBehaviorRestrictionsAfterFirstUserGesture(MediaElementSession::BehaviorRestrictions mask)
{
    MediaElementSession::BehaviorRestrictions restrictionsToRemove = mask &
        (MediaElementSession::RequireUserGestureForLoad
        | MediaElementSession::AutoPreloadingNotPermitted
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        | MediaElementSession::RequireUserGestureToShowPlaybackTargetPicker
        | MediaElementSession::RequireUserGestureToAutoplayToExternalDevice
#endif
        | MediaElementSession::RequireUserGestureForVideoRateChange
        | MediaElementSession::RequireUserGestureForAudioRateChange
        | MediaElementSession::RequireUserGestureForFullscreen
        | MediaElementSession::RequireUserGestureForVideoDueToLowPowerMode
        | MediaElementSession::RequireUserGestureForVideoDueToAggressiveThermalMitigation
        | MediaElementSession::InvisibleAutoplayNotPermitted
        | MediaElementSession::RequireUserGestureToControlControlsManager);

    m_removedBehaviorRestrictionsAfterFirstUserGesture = true;

    protectedMediaSession()->removeBehaviorRestriction(restrictionsToRemove);

    if (RefPtr mainFrameDocument = protectedDocument()->mainFrameDocument())
        mainFrameDocument->noteUserInteractionWithMediaElement();
    else
        LOG_ONCE(SiteIsolation, "Unable to fully perform HTMLMediaElement::removeBehaviorRestrictionsAfterFirstUserGesture() without access to the main frame document ");
}

void HTMLMediaElement::updateRateChangeRestrictions()
{
    Ref document = this->document();
    if (!document->ownerElement() && document->isMediaDocument())
        return;

    RefPtr page = document->page();
    if (!page)
        return;

    Ref mediaSession = this->mediaSession();
    if (page->requiresUserGestureForVideoPlayback())
        mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoRateChange);
    else
        mediaSession->removeBehaviorRestriction(MediaElementSession::RequireUserGestureForVideoRateChange);

    if (page->requiresUserGestureForAudioPlayback())
        mediaSession->addBehaviorRestriction(MediaElementSession::RequireUserGestureForAudioRateChange);
    else
        mediaSession->removeBehaviorRestriction(MediaElementSession::RequireUserGestureForAudioRateChange);
}

RefPtr<VideoPlaybackQuality> HTMLMediaElement::getVideoPlaybackQuality() const
{
    RefPtr window = document().window();
    double timestamp = window ? window->nowTimestamp().milliseconds() : 0;

    VideoPlaybackQualityMetrics currentVideoPlaybackQuality;
#if ENABLE(MEDIA_SOURCE)
    currentVideoPlaybackQuality.totalVideoFrames = m_droppedVideoFrames;
    currentVideoPlaybackQuality.droppedVideoFrames = m_droppedVideoFrames;
#endif

    RefPtr player = m_player;
    if (auto metrics = player ? player->videoPlaybackQualityMetrics() : std::nullopt)
        currentVideoPlaybackQuality += *metrics;

    return VideoPlaybackQuality::create(timestamp, currentVideoPlaybackQuality);
}

void HTMLMediaElement::updatePageScaleFactorJSProperty()
{
    Page* page = document().page();
    if (!page)
        return;

    setControllerJSProperty("pageScaleFactor"_s, JSC::jsNumber(page->pageScaleFactor()));
}

void HTMLMediaElement::updateUsesLTRUserInterfaceLayoutDirectionJSProperty()
{
    Page* page = document().page();
    if (!page)
        return;

    bool usesLTRUserInterfaceLayoutDirectionProperty = page->userInterfaceLayoutDirection() == UserInterfaceLayoutDirection::LTR;
    setControllerJSProperty("usesLTRUserInterfaceLayoutDirection"_s, JSC::jsBoolean(usesLTRUserInterfaceLayoutDirectionProperty));
}

void HTMLMediaElement::setControllerJSProperty(ASCIILiteral propertyName, JSC::JSValue propertyValue)
{
    DocumentMediaElement::from(protectedDocument()).setupAndCallMediaControlsJS([this, propertyName, propertyValue](JSDOMGlobalObject& globalObject, JSC::JSGlobalObject& lexicalGlobalObject, ScriptController&, DOMWrapperWorld&) {
        auto& vm = globalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (!m_mediaControlsHost)
            return false;

        auto controllerValue = m_mediaControlsHost->controllerWrapper().getValue();
        EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || controllerValue.isNull());
        if (controllerValue.isNull())
            return false;

        JSC::PutPropertySlot propertySlot(controllerValue);
        auto* controllerObject = controllerValue.toObject(&lexicalGlobalObject);
        if (!controllerObject)
            return false;

        scope.release();
        controllerObject->methodTable()->put(controllerObject, &lexicalGlobalObject, JSC::Identifier::fromString(vm, propertyName), propertyValue, propertySlot);

        return true;
    });
}

bool HTMLMediaElement::ensureMediaControls()
{
    if (m_controlsState == ControlsState::Ready)
        return true;

    auto mediaControlsScripts = RenderTheme::singleton().mediaControlsScripts();
    if (mediaControlsScripts.isEmpty() || isSuspended())
        return false;

    INFO_LOG(LOGIDENTIFIER);

    ControlsState oldControlsState = m_controlsState;
    m_controlsState = ControlsState::Initializing;

    auto controlsReady = false;
    if (oldControlsState == ControlsState::None) {
        controlsReady = DocumentMediaElement::from(protectedDocument()).setupAndCallMediaControlsJS([this, mediaControlsScripts = WTFMove(mediaControlsScripts)](JSDOMGlobalObject& globalObject, JSC::JSGlobalObject& lexicalGlobalObject, ScriptController&, DOMWrapperWorld&) {
            auto& vm = globalObject.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            // The media controls script must provide a method with the following details.
            // Name: createControls
            // Parameters:
            //     1. The ShadowRoot element that will hold the controls.
            //     2. This object (and HTMLMediaElement).
            //     3. The MediaControlsHost object.
            // Return value:
            //     A reference to the created media controller instance.

            auto functionValue = globalObject.get(&lexicalGlobalObject, JSC::Identifier::fromString(vm, "createControls"_s));
            if (functionValue.isUndefinedOrNull())
                return false;

            if (!m_mediaControlsHost)
                lazyInitialize(m_mediaControlsHost, makeUniqueWithoutRefCountedCheck<MediaControlsHost>(*this));

            auto mediaJSWrapper = toJS(&lexicalGlobalObject, &globalObject, *this);
            auto mediaControlsHostJSWrapper = toJS(&lexicalGlobalObject, &globalObject, *m_mediaControlsHost);

            JSC::MarkedArgumentBuffer argList;
            argList.append(toJS(&lexicalGlobalObject, &globalObject, Ref { ensureUserAgentShadowRoot() }));
            argList.append(mediaJSWrapper);
            argList.append(mediaControlsHostJSWrapper);
            ASSERT(!argList.hasOverflowed());

            auto* function = functionValue.toObject(&lexicalGlobalObject);
            RETURN_IF_EXCEPTION(scope, false);
            auto callData = JSC::getCallData(function);
            if (callData.type == JSC::CallData::Type::None)
                return false;

            auto controllerValue = JSC::call(&lexicalGlobalObject, function, callData, &globalObject, argList);
            RETURN_IF_EXCEPTION(scope, false);
            ASSERT_UNUSED(controllerValue, controllerValue);

            if (m_mediaControlsDependOnPageScaleFactor) {
                updatePageScaleFactorJSProperty();
                RETURN_IF_EXCEPTION(scope, false);
            }

            updateUsesLTRUserInterfaceLayoutDirectionJSProperty();
            RETURN_IF_EXCEPTION(scope, false);

            return true;
        });
    } else if (oldControlsState == ControlsState::PartiallyDeinitialized) {
        controlsReady = DocumentMediaElement::from(protectedDocument()).setupAndCallMediaControlsJS([this](JSDOMGlobalObject& globalObject, JSC::JSGlobalObject& lexicalGlobalObject, ScriptController&, DOMWrapperWorld&) {
            auto& vm = globalObject.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            if (!m_mediaControlsHost)
                return false;

            auto controllerValue = m_mediaControlsHost->controllerWrapper().getValue();
            RETURN_IF_EXCEPTION(scope, false);
            auto* controllerObject = controllerValue.toObject(&lexicalGlobalObject);
            RETURN_IF_EXCEPTION(scope, false);

            auto functionValue = controllerObject->get(&lexicalGlobalObject, JSC::Identifier::fromString(vm, "reinitialize"_s));
            if (scope.exception()) [[unlikely]]
                return false;
            if (functionValue.isUndefinedOrNull())
                return false;

            if (!m_mediaControlsHost)
                lazyInitialize(m_mediaControlsHost, makeUniqueWithoutRefCountedCheck<MediaControlsHost>(*this));

            auto mediaJSWrapper = toJS(&lexicalGlobalObject, &globalObject, *this);
            auto mediaControlsHostJSWrapper = toJS(&lexicalGlobalObject, &globalObject, *m_mediaControlsHost);

            JSC::MarkedArgumentBuffer argList;
            argList.append(toJS(&lexicalGlobalObject, &globalObject, Ref { ensureUserAgentShadowRoot() }));
            argList.append(mediaJSWrapper);
            argList.append(mediaControlsHostJSWrapper);
            ASSERT(!argList.hasOverflowed());

            auto* function = functionValue.toObject(&lexicalGlobalObject);
            RETURN_IF_EXCEPTION(scope, false);

            auto callData = JSC::getCallData(function);
            if (callData.type == JSC::CallData::Type::None)
                return false;

            auto resultValue = JSC::call(&lexicalGlobalObject, function, callData, controllerObject, argList);
            RETURN_IF_EXCEPTION(scope, false);

            return resultValue.toBoolean(&lexicalGlobalObject);
        });
    } else
        ASSERT_NOT_REACHED();

    m_controlsState = controlsReady ? ControlsState::Ready : oldControlsState;
    return controlsReady;
}

void HTMLMediaElement::setMediaControlsDependOnPageScaleFactor(bool dependsOnPageScale)
{
    INFO_LOG(LOGIDENTIFIER, dependsOnPageScale);

    if (document().settings().mediaControlsScaleWithPageZoom() || (is<HTMLAudioElement>(*this) && document().settings().audioControlsScaleWithPageZoom())) {
        INFO_LOG(LOGIDENTIFIER, "forced to false by Settings value");
        m_mediaControlsDependOnPageScaleFactor = false;
        return;
    }

    m_mediaControlsDependOnPageScaleFactor = dependsOnPageScale;
}

void HTMLMediaElement::pageScaleFactorChanged()
{
    if (m_mediaControlsDependOnPageScaleFactor) {
        queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& element) {
            element.updatePageScaleFactorJSProperty();
        });
    }
}

void HTMLMediaElement::userInterfaceLayoutDirectionChanged()
{
    updateUsesLTRUserInterfaceLayoutDirectionJSProperty();
}

void HTMLMediaElement::setMediaControlsMaximumRightContainerButtonCountOverride(size_t count)
{
    setControllerJSProperty("maximumRightContainerButtonCountOverride"_s, JSC::jsNumber(count));
}

void HTMLMediaElement::setMediaControlsHidePlaybackRates(bool hidePlaybackRates)
{
    setControllerJSProperty("hidePlaybackRates"_s, JSC::jsBoolean(hidePlaybackRates));
}

unsigned long long HTMLMediaElement::fileSize() const
{
    if (RefPtr player = m_player)
        return player->fileSize();

    return 0;
}

PlatformMediaSession::MediaType HTMLMediaElement::mediaType() const
{
    if (m_player && m_readyState >= HAVE_METADATA) {
        auto hasVideo = this->hasVideo();
        if (hasVideo && canProduceAudio())
            return PlatformMediaSession::MediaType::VideoAudio;
        return hasVideo ? PlatformMediaSession::MediaType::Video : PlatformMediaSession::MediaType::Audio;
    }

    return presentationType();
}

PlatformMediaSession::MediaType HTMLMediaElement::presentationType() const
{
    if (hasTagName(HTMLNames::videoTag))
        return muted() ? PlatformMediaSession::MediaType::Video : PlatformMediaSession::MediaType::VideoAudio;

    return PlatformMediaSession::MediaType::Audio;
}

PlatformMediaSession::DisplayType HTMLMediaElement::displayType() const
{
    if (m_videoFullscreenMode == VideoFullscreenModeStandard)
        return PlatformMediaSession::DisplayType::Fullscreen;
    if (m_videoFullscreenMode & VideoFullscreenModePictureInPicture)
        return PlatformMediaSession::DisplayType::Optimized;
    if (m_videoFullscreenMode == VideoFullscreenModeNone)
        return PlatformMediaSession::DisplayType::Normal;

    ASSERT_NOT_REACHED();
    return PlatformMediaSession::DisplayType::Normal;
}

bool HTMLMediaElement::canProduceAudio() const
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    // Because the remote target could unmute playback without notifying us, we must assume
    // that we may be playing audio.
    if (m_isPlayingToWirelessTarget)
        return true;
#endif

    if (isSuspended())
        return false;

    if (!volume())
        return false;

    // For GStreamer ports the semantics of IsPlayingAudio slightly differ from Apple ports. The
    // webkit_web_view_is_playing_audio() API is expected to return true if a page is producing
    // audio even though it might be muted.
#if !USE(GSTREAMER)
    if (muted())
        return false;
#endif

    if (m_player && m_readyState >= HAVE_METADATA)
        return hasAudio();

    return hasEverHadAudio();
}

bool HTMLMediaElement::isSuspended() const
{
    ASSERT(Node::scriptExecutionContext() == ActiveDOMObject::scriptExecutionContext()
        || (!ActiveDOMObject::scriptExecutionContext() && document().activeDOMObjectsAreStopped()));
    return document().activeDOMObjectsAreSuspended() || document().activeDOMObjectsAreStopped();
}

void HTMLMediaElement::suspendPlayback()
{
    ALWAYS_LOG(LOGIDENTIFIER, "paused = ", paused());
    if (!paused())
        pauseInternal();
}

void HTMLMediaElement::resumeAutoplaying()
{
    ALWAYS_LOG(LOGIDENTIFIER, "paused = ", paused());
    m_autoplaying = true;

    if (canTransitionFromAutoplayToPlay())
        play();
}

void HTMLMediaElement::mayResumePlayback(bool shouldResume)
{
    ALWAYS_LOG(LOGIDENTIFIER, "paused = ", paused());
    if (!ended() && paused() && shouldResume)
        play();
}

String HTMLMediaElement::mediaSessionTitle() const
{
    RefPtr page = document().page();
    if (!page)
        return emptyString();

    if (page->usesEphemeralSession() && !document().settings().allowPrivacySensitiveOperationsInNonPersistentDataStores())
        return emptyString();

    auto title = String(attributeWithoutSynchronization(titleAttr)).trim(deprecatedIsSpaceOrNewline).simplifyWhiteSpace(deprecatedIsSpaceOrNewline);
    if (!title.isEmpty())
        return title;

    title = document().title().trim(deprecatedIsSpaceOrNewline).simplifyWhiteSpace(deprecatedIsSpaceOrNewline);
    if (!title.isEmpty())
        return title;

    auto domain = RegistrableDomain { m_currentSrc };
    if (!domain.isEmpty())
        title = domain.string();

    return title;
}

void HTMLMediaElement::setCurrentSrc(const URL& src)
{
    m_currentSrc = src;
    m_currentIdentifier = MediaUniqueIdentifier::generate();
}

MediaUniqueIdentifier HTMLMediaElement::mediaUniqueIdentifier() const
{
    return m_currentIdentifier;
}

void HTMLMediaElement::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    ALWAYS_LOG(LOGIDENTIFIER, command);

    UserGestureIndicator remoteControlUserGesture(IsProcessingUserGesture::Yes, &document());
    const double defaultSkipAmount = 15;
    switch (command) {
    case PlatformMediaSession::RemoteControlCommandType::PlayCommand:
        play();
        break;
    case PlatformMediaSession::RemoteControlCommandType::StopCommand:
    case PlatformMediaSession::RemoteControlCommandType::PauseCommand:
        pause();
        break;
    case PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand:
        canPlay() ? play() : pause();
        break;
    case PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand:
        beginScanning(Backward);
        break;
    case PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand:
        beginScanning(Forward);
        break;
    case PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand:
    case PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand:
        endScanning();
        break;
    case PlatformMediaSession::RemoteControlCommandType::BeginScrubbingCommand:
        beginScrubbing();
        break;
    case PlatformMediaSession::RemoteControlCommandType::EndScrubbingCommand:
        endScrubbing();
        break;
    case PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand: {
        auto delta = argument.time ? argument.time.value() : defaultSkipAmount;
        handleSeekToPlaybackPosition(std::min(currentTime() + delta, duration()));
        break;
    }
    case PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand: {
        auto delta = argument.time ? argument.time.value() : defaultSkipAmount;
        handleSeekToPlaybackPosition(std::max(currentTime() - delta, 0.));
        break;
    }
    case PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand:
        ASSERT(argument.time);
        if (argument.time)
            handleSeekToPlaybackPosition(argument.time.value());
        break;
    default:
        { } // Do nothing
    }
}

bool HTMLMediaElement::supportsSeeking() const
{
    return !protectedDocument()->quirks().needsSeekingSupportDisabled();
}

bool HTMLMediaElement::shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType type) const
{
    if (type == PlatformMediaSession::InterruptionType::EnteringBackground) {
        if (isPlayingToExternalTarget()) {
            INFO_LOG(LOGIDENTIFIER, "returning true because isPlayingToExternalTarget() is true");
            return true;
        }
        if (RefPtr manager = sessionManager(); manager && manager->isPlayingToAutomotiveHeadUnit()) {
            INFO_LOG(LOGIDENTIFIER, "returning true because isPlayingToAutomotiveHeadUnit() is true");
            return true;
        }
#if ENABLE(VIDEO_PRESENTATION_MODE)
        if (m_videoFullscreenMode == VideoFullscreenModePictureInPicture) {
            INFO_LOG(LOGIDENTIFIER, "returning true, in PiP");
            return true;
        }
#endif
#if PLATFORM(VISION) && ENABLE(WEBXR)
        if (RefPtr page = document().page()) {
            if (page->hasActiveImmersiveSession()) {
                INFO_LOG(LOGIDENTIFIER, "returning true due to active immersive session");
                return true;
            }
        }
#endif
#if ENABLE(MEDIA_STREAM)
        if (hasMediaStreamSrcObject() && mediaState().containsAny(MediaProducerMediaState::IsPlayingAudio) && document().mediaState().containsAny(MediaProducerMediaState::HasActiveAudioCaptureDevice)) {
            INFO_LOG(LOGIDENTIFIER, "returning true because playing an audio MediaStreamTrack");
            return true;
        }
#endif
    } else if (type == PlatformMediaSession::InterruptionType::SuspendedUnderLock) {
        if (isPlayingToExternalTarget()) {
            INFO_LOG(LOGIDENTIFIER, "returning true because isPlayingToExternalTarget() is true");
            return true;
        }
        if (RefPtr manager = sessionManager(); manager && manager->isPlayingToAutomotiveHeadUnit()) {
            INFO_LOG(LOGIDENTIFIER, "returning true because isPlayingToAutomotiveHeadUnit() is true");
            return true;
        }
#if ENABLE(MEDIA_STREAM)
        if (hasMediaStreamSrcObject() && mediaState().containsAny(MediaProducerMediaState::IsPlayingAudio) && document().mediaState().containsAny(MediaProducerMediaState::HasActiveAudioCaptureDevice)) {
            INFO_LOG(LOGIDENTIFIER, "returning true because playing an audio MediaStreamTrack");
            return true;
        }
#endif
    }
    return false;
}

bool HTMLMediaElement::processingUserGestureForMedia() const
{
    return protectedDocument()->processingUserGestureForMedia();
}

void HTMLMediaElement::processIsSuspendedChanged()
{
    updateSleepDisabling();
}

bool HTMLMediaElement::shouldOverridePauseDuringRouteChange() const
{
#if ENABLE(MEDIA_STREAM)
    return hasMediaStreamSrcObject();
#else
    return false;
#endif
}

void HTMLMediaElement::requestHostingContext(Function<void(HostingContext)>&& completionHandler)
{
    if (RefPtr player = m_player) {
        player->requestHostingContext(WTFMove(completionHandler));
        return;
    }

    completionHandler({ });
}

HostingContext HTMLMediaElement::layerHostingContext()
{
    if (RefPtr player = m_player)
        return player->hostingContext();
    return { };
}

FloatSize HTMLMediaElement::naturalSize()
{
    if (RefPtr player = m_player)
        return player->naturalSize();
    return { };
}

FloatSize HTMLMediaElement::videoLayerSize() const
{
    return m_videoLayerSize;
}

void HTMLMediaElement::setVideoLayerSizeFenced(const FloatSize& size, WTF::MachSendRightAnnotated&& fence)
{
    if (m_videoLayerSize == size)
        return;

    m_videoLayerSize = size;
    if (RefPtr player = m_player)
        player->setVideoLayerSizeFenced(size, WTFMove(fence));
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void HTMLMediaElement::scheduleUpdateMediaState()
{
    if (m_updateMediaStateTaskCancellationGroup.hasPendingTask())
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    INFO_LOG(logSiteIdentifier, "task scheduled");
    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_updateMediaStateTaskCancellationGroup, [logSiteIdentifier](auto& element) {
        UNUSED_PARAM(logSiteIdentifier);
        INFO_LOG_WITH_THIS(&element, logSiteIdentifier, "lambda(), task fired");
        element.updateMediaState();
    });
}

#endif

void HTMLMediaElement::updateMediaState()
{
    MediaProducerMediaStateFlags state = mediaState();
    if (m_mediaState == state)
        return;

    m_mediaState = state;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    protectedMediaSession()->mediaStateDidChange(m_mediaState);
#endif

    protectedDocument()->updateIsPlayingMedia();
}

MediaProducerMediaStateFlags HTMLMediaElement::mediaState() const
{
    MediaStateFlags state;

    bool hasActiveVideo = isVideo() && hasVideo();
    bool hasAudio = this->hasAudio();
    if (isPlayingToExternalTarget())
        state.add(MediaProducerMediaState::IsPlayingToExternalDevice);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (m_hasPlaybackTargetAvailabilityListeners) {
        state.add(MediaProducerMediaState::HasPlaybackTargetAvailabilityListener);
        if (!protectedMediaSession()->wirelessVideoPlaybackDisabled())
            state.add(MediaProducerMediaState::RequiresPlaybackTargetMonitoring);
    }

    bool requireUserGesture = m_mediaSession && protectedMediaSession()->hasBehaviorRestriction(MediaElementSession::RequireUserGestureToAutoplayToExternalDevice);
    if (m_readyState >= HAVE_METADATA && !requireUserGesture && !m_failedToPlayToWirelessTarget)
        state.add(MediaProducerMediaState::ExternalDeviceAutoPlayCandidate);

    if (hasActiveVideo || hasAudio)
        state.add(MediaProducerMediaState::HasAudioOrVideo);

    if (hasActiveVideo && endedPlayback())
        state.add(MediaProducerMediaState::DidPlayToEnd);
#else
    UNUSED_VARIABLE(hasAudio);
#endif

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource; mediaSource && mediaSource->isStreamingContent())
        state.add(MediaProducerMediaState::HasStreamingActivity);
#endif

    if (!isPlaying())
        return state;

    if (canProduceAudio())
        state.add(MediaProducerMediaState::IsPlayingAudio);

    if (hasActiveVideo)
        state.add(MediaProducerMediaState::IsPlayingVideo);

    return state;
}

void HTMLMediaElement::handleAutoplayEvent(AutoplayEvent event)
{
    if (RefPtr page = document().page()) {
        bool hasAudio = this->hasAudio() && !muted() && volume();
        bool wasPlaybackPrevented = m_autoplayEventPlaybackState == AutoplayEventPlaybackState::PreventedAutoplay;
        RefPtr mediaSession = m_mediaSession;
        bool hasMainContent = mediaSession && mediaSession->isMainContentForPurposesOfAutoplayEvents();
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
    HTMLMEDIAELEMENT_RELEASE_LOG(SETAUTOPLAYEVENTPLAYBACKSTATE, convertEnumerationToString(reason).utf8());
    m_autoplayEventPlaybackState = reason;

    if (reason == AutoplayEventPlaybackState::PreventedAutoplay) {
        dispatchPlayPauseEventsIfNeedsQuirks();
        handleAutoplayEvent(AutoplayEvent::DidPreventMediaFromPlaying);
    }
}

void HTMLMediaElement::visibilityAdjustmentStateDidChange()
{
    auto currentValue = isInVisibilityAdjustmentSubtree();
    if (m_cachedIsInVisibilityAdjustmentSubtree == currentValue)
        return;

    bool wasMuted = effectiveMuted();
    m_cachedIsInVisibilityAdjustmentSubtree = currentValue;
    bool muted = effectiveMuted();
    if (wasMuted == muted)
        return;

    RefPtr player = m_player;
    if (!player)
        return;

    player->setMuted(muted);
}

#if PLATFORM(IOS_FAMILY)
void HTMLMediaElement::sceneIdentifierDidChange()
{
    if (RefPtr page = document().page()) {
        HTMLMEDIAELEMENT_RELEASE_LOG(SCENEIDENTIFIERDIDCHANGE, page->sceneIdentifier().utf8());
        if (RefPtr player = m_player)
            player->setSceneIdentifier(page->sceneIdentifier());
    }
}
#endif

void HTMLMediaElement::pageMutedStateDidChange()
{
    if (RefPtr page = document().page()) {
        // Propagate the new state to the platform player.
        if (RefPtr player = m_player)
            player->setMuted(effectiveMuted());
        if (hasAudio() && !muted() && page->isAudioMuted())
            userDidInterfereWithAutoplay();
    }
}

double HTMLMediaElement::effectiveVolume() const
{
    auto* page = document().page();
    double volumeMultiplier = m_volumeMultiplierForSpeechSynthesis * (page ? page->mediaVolume() : 1);
    if (m_mediaController)
        volumeMultiplier *= m_mediaController->volume();
    return m_volume * volumeMultiplier;
}

bool HTMLMediaElement::effectiveMuted() const
{
    if (muted())
        return true;

    if (m_mediaController && m_mediaController->muted())
        return true;

    if (RefPtr page = document().page(); page && page->isAudioMuted())
        return true;

    if (m_cachedIsInVisibilityAdjustmentSubtree)
        return true;

    return false;
}

bool HTMLMediaElement::doesHaveAttribute(const AtomString& attribute, AtomString* value) const
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

void HTMLMediaElement::setBufferingPolicy(BufferingPolicy policy)
{
    if (policy == m_bufferingPolicy)
        return;

    HTMLMEDIAELEMENT_RELEASE_LOG(SETBUFFERINGPOLICY, static_cast<uint8_t>(policy));

    m_bufferingPolicy = policy;
    if (RefPtr player = m_player)
        player->setBufferingPolicy(policy);
#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource; mediaSource && policy == BufferingPolicy::PurgeResources)
        mediaSource->memoryPressure();
#endif
}

void HTMLMediaElement::purgeBufferedDataIfPossible()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    bool isPausedOrMSE = [&] {
#if ENABLE(MEDIA_SOURCE)
        if (m_mediaSource)
            return true;
#endif
        return paused();
    }();

    if (!isPausedOrMSE)
        return;

    if (!MemoryPressureHandler::singleton().isUnderMemoryPressure() && protectedMediaSession()->preferredBufferingPolicy() == BufferingPolicy::Default)
        return;

    if (isPlayingToExternalTarget()) {
        ALWAYS_LOG(LOGIDENTIFIER, "early return because playing to wireless target");
        return;
    }

    setBufferingPolicy(BufferingPolicy::PurgeResources);
}

bool HTMLMediaElement::canSaveMediaData() const
{
    if (RefPtr player = m_player)
        return player->canSaveMediaData();

    return false;
}

void HTMLMediaElement::allowsMediaDocumentInlinePlaybackChanged()
{
    if (potentiallyPlaying() && protectedMediaSession()->requiresFullscreenForVideoPlayback() && !isFullscreen())
        enterFullscreen();
}

bool HTMLMediaElement::isVideoTooSmallForInlinePlayback()
{
    CheckedPtr renderer = dynamicDowncast<RenderVideo>(this->renderer());
    if (!renderer)
        return true;

    IntRect videoBox = renderer->videoBox();
    return videoBox.width() <= 1 || videoBox.height() <= 1;
}

void HTMLMediaElement::isVisibleInViewportChanged()
{
    if (RefPtr player = m_player)
        player->setVisibleInViewport(isVisibleInViewport());

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& element) {
        if (element.isContextStopped())
            return;
        element.mediaSession().isVisibleInViewportChanged();
        element.updateShouldAutoplay();
        element.schedulePlaybackControlsManagerUpdate();
    });
}

void HTMLMediaElement::scheduleUpdateShouldAutoplay()
{
    if (m_updateShouldAutoplayTaskCancellationGroup.hasPendingTask())
        return;

    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_updateShouldAutoplayTaskCancellationGroup, [](auto& element) {
        element.updateShouldAutoplay();
    });
}

void HTMLMediaElement::updateShouldAutoplay()
{
    if (!autoplay())
        return;

    Ref mediaSession = this->mediaSession();
    if (!mediaSession->hasBehaviorRestriction(MediaElementSession::InvisibleAutoplayNotPermitted) && !m_wasInterruptedForInvisibleAutoplay)
        return;

    bool canAutoplay = mediaSession->autoplayPermitted();

    if (canAutoplay) {
        if (m_wasInterruptedForInvisibleAutoplay) {
            m_wasInterruptedForInvisibleAutoplay = false;
            mediaSession->endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
            return;
        }
        if (!isPlaying())
            resumeAutoplaying();
        return;
    }

    if (mediaSession->state() == PlatformMediaSession::State::Interrupted)
        return;

    if (m_wasInterruptedForInvisibleAutoplay) {
        m_wasInterruptedForInvisibleAutoplay = false;
        mediaSession->endInterruption(PlatformMediaSession::EndInterruptionFlags::NoFlags);
    }

    m_wasInterruptedForInvisibleAutoplay = true;
    mediaSession->beginInterruption(PlatformMediaSession::InterruptionType::InvisibleAutoplay);
}

void HTMLMediaElement::updateShouldPlay()
{
    if (!paused() && !protectedMediaSession()->playbackStateChangePermitted(MediaPlaybackState::Playing)) {
        scheduleRejectPendingPlayPromises(DOMException::create(ExceptionCode::NotAllowedError));
        pauseInternal();
        setAutoplayEventPlaybackState(AutoplayEventPlaybackState::PreventedAutoplay);
    } else if (canTransitionFromAutoplayToPlay())
        play();
}

void HTMLMediaElement::resetPlaybackSessionState()
{
    if (RefPtr mediaSession = m_mediaSession)
        mediaSession->resetPlaybackSessionState();
}

bool HTMLMediaElement::isVisibleInViewport() const
{
    auto renderer = this->renderer();
    return renderer && renderer->visibleInViewportState() == VisibleInViewportState::Yes;
}

void HTMLMediaElement::schedulePlaybackControlsManagerUpdate()
{
    if (RefPtr<Page> page = document().page())
        page->schedulePlaybackControlsManagerUpdate();
}

void HTMLMediaElement::playbackControlsManagerBehaviorRestrictionsTimerFired()
{
    if (m_playbackControlsManagerBehaviorRestrictionsTaskCancellationGroup.hasPendingTask())
        return;

    if (!mediaSession().hasBehaviorRestriction(MediaElementSession::RequireUserGestureToControlControlsManager))
        return;

    queueCancellableTaskKeepingObjectAlive(*this, TaskSource::MediaElement, m_playbackControlsManagerBehaviorRestrictionsTaskCancellationGroup, [](auto& element) {
        auto& mediaElementSession = element.mediaSession();
        if (element.isPlaying() || mediaElementSession.state() == PlatformMediaSession::State::Autoplaying || mediaElementSession.state() == PlatformMediaSession::State::Playing)
            return;

        mediaElementSession.addBehaviorRestriction(MediaElementSession::RequirePlaybackToControlControlsManager);
        element.schedulePlaybackControlsManagerUpdate();
    });
}

bool HTMLMediaElement::shouldOverrideBackgroundLoadingRestriction() const
{
    if (isPlayingToExternalTarget())
        return true;

    return m_videoFullscreenMode == VideoFullscreenModePictureInPicture;
}

void HTMLMediaElement::setFullscreenMode(VideoFullscreenMode mode)
{
    INFO_LOG(LOGIDENTIFIER, "changed from ", fullscreenMode(), ", to ", mode);
#if ENABLE(VIDEO_PRESENTATION_MODE)
    scheduleEvent(eventNames().webkitpresentationmodechangedEvent);
#endif

    setPreparedToReturnVideoLayerToInline(mode != HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr player = this->player())
        player->setVideoFullscreenMode(mode);
#endif

    m_videoFullscreenMode = mode;
    visibilityStateChanged();
    schedulePlaybackControlsManagerUpdate();

    computeAcceleratedRenderingStateAndUpdateMediaPlayer();
    updatePlayerDynamicRangeLimit();
}

void HTMLMediaElement::addClient(HTMLMediaElementClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
}

void HTMLMediaElement::removeClient(const HTMLMediaElementClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

void HTMLMediaElement::addMessageClientForTesting(MessageClientForTesting& client)
{
    RefPtr internalMessageClient = m_internalMessageClient;
    if (!internalMessageClient) {
        internalMessageClient = AggregateMessageClientForTesting::create();
        m_internalMessageClient = internalMessageClient.copyRef();
        if (RefPtr player = m_player)
            player->setMessageClientForTesting(internalMessageClient.get());
    }
    internalMessageClient->addClient(client);
}

void HTMLMediaElement::removeMessageClientForTesting(const MessageClientForTesting& client)
{
    RefPtr internalMessageClient = m_internalMessageClient;
    if (!internalMessageClient)
        return;
    internalMessageClient->removeClient(client);
    if (internalMessageClient->isEmpty()) {
        if (RefPtr player = m_player)
            player->setMessageClientForTesting(nullptr);
        m_internalMessageClient = nullptr;
    }
}

void HTMLMediaElement::audioSessionCategoryChanged(AudioSessionCategory category, AudioSessionMode mode, RouteSharingPolicy policy)
{
    m_clients.forEach([category, mode, policy] (auto& client) {
        client.audioSessionCategoryChanged(category, mode, policy);
    });
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
    if (RefPtr player = m_player)
        player->applicationWillResignActive();
}

void HTMLMediaElement::applicationDidBecomeActive()
{
    if (RefPtr player = m_player)
        player->applicationDidBecomeActive();
}

void HTMLMediaElement::setInActiveDocument(bool inActiveDocument)
{
    if (inActiveDocument == m_inActiveDocument)
        return;

    m_inActiveDocument = inActiveDocument;
    if (RefPtr mediaSession = m_mediaSession)
        mediaSession->inActiveDocumentChanged();
}

HTMLMediaElementEnums::BufferingPolicy HTMLMediaElement::bufferingPolicy() const
{
    return m_bufferingPolicy;
}

MediaTime HTMLMediaElement::mediaSessionDuration() const
{
    return loop() ? MediaTime::positiveInfiniteTime() : durationMediaTime();
}

bool HTMLMediaElement::hasMediaStreamSource() const
{
#if ENABLE(MEDIA_STREAM)
    return hasMediaStreamSrcObject();
#else
    return false;
#endif
}

#if ENABLE(MEDIA_STREAM)
void HTMLMediaElement::mediaStreamCaptureStarted()
{
    if (canTransitionFromAutoplayToPlay())
        play();
}
#endif

SecurityOriginData HTMLMediaElement::documentSecurityOrigin() const
{
    return protectedDocument()->securityOrigin().data();
}

void HTMLMediaElement::setShowPosterFlag(bool flag)
{
    if (m_showPoster == flag)
        return;

    HTMLMEDIAELEMENT_RELEASE_LOG(SETSHOWPOSTERFLAG, flag);

    m_showPoster = flag;
    invalidateStyleAndLayerComposition();
}

MediaElementSession& HTMLMediaElement::mediaSession() const
{
    if (!m_mediaSession)
        const_cast<HTMLMediaElement&>(*this).initializeMediaSession();
    return *m_mediaSession;
}

void HTMLMediaElement::updateMediaPlayer(IntSize presentationSize, bool shouldMaintainAspectRatio)
{
    INFO_LOG(LOGIDENTIFIER);
    RefPtr player = m_player;
    player->setPresentationSize(presentationSize);
    visibilityStateChanged();
    player->setVisibleInViewport(isVisibleInViewport());

    if (protectedDocument()->quirks().needsVideoShouldMaintainAspectRatioQuirk())
        shouldMaintainAspectRatio = true;

    player->setShouldMaintainAspectRatio(shouldMaintainAspectRatio);
}

void HTMLMediaElement::mediaPlayerQueueTaskOnEventLoop(Function<void()>&& task)
{
    protectedDocument()->checkedEventLoop()->queueTask(TaskSource::MediaElement, WTFMove(task));
}

template<typename T> void HTMLMediaElement::scheduleEventOn(T& target, Ref<Event>&& event)
{
    target.queueCancellableTaskToDispatchEvent(target, TaskSource::MediaElement, m_asyncEventsCancellationGroup, WTFMove(event));
}

void HTMLMediaElement::setShowingStats(bool shouldShowStats)
{
    if (m_showingStats == shouldShowStats)
        return;

    if (!ensureMediaControls())
        return;

    m_showingStats = DocumentMediaElement::from(protectedDocument()).setupAndCallMediaControlsJS([this, shouldShowStats](JSDOMGlobalObject& globalObject, JSC::JSGlobalObject& lexicalGlobalObject, ScriptController&, DOMWrapperWorld&) {
        auto& vm = globalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (!m_mediaControlsHost)
            return false;

        auto controllerValue = m_mediaControlsHost->controllerWrapper().getValue();
        RETURN_IF_EXCEPTION(scope, false);
        auto* controllerObject = controllerValue.toObject(&lexicalGlobalObject);
        RETURN_IF_EXCEPTION(scope, false);

        auto functionValue = controllerObject->get(&lexicalGlobalObject, JSC::Identifier::fromString(vm, "setShowingStats"_s));
        if (scope.exception()) [[unlikely]]
            return false;
        if (functionValue.isUndefinedOrNull())
            return false;

        auto* function = functionValue.toObject(&lexicalGlobalObject);
        RETURN_IF_EXCEPTION(scope, false);

        auto callData = JSC::getCallData(function);
        if (callData.type == JSC::CallData::Type::None)
            return false;

        JSC::MarkedArgumentBuffer argList;
        argList.append(JSC::jsBoolean(shouldShowStats));
        ASSERT(!argList.hasOverflowed());

        auto resultValue = JSC::call(&lexicalGlobalObject, function, callData, controllerObject, argList);
        RETURN_IF_EXCEPTION(scope, false);

        return resultValue.toBoolean(&lexicalGlobalObject);
    });
}

bool HTMLMediaElement::shouldDisableHDR() const
{
    return !screenSupportsHighDynamicRange(document().protectedView().get());
}

auto HTMLMediaElement::sourceType() const -> std::optional<SourceType>
{
    if (hasMediaStreamSource())
        return SourceType::MediaStream;

#if ENABLE(MEDIA_SOURCE)
    if (hasManagedMediaSource())
        return SourceType::ManagedMediaSource;

    if (hasMediaSource())
        return SourceType::MediaSource;
#endif

    switch (movieLoadType()) {
    case HTMLMediaElement::MovieLoadType::Unknown: return std::nullopt;
    case HTMLMediaElement::MovieLoadType::Download: return SourceType::File;
    case HTMLMediaElement::MovieLoadType::LiveStream: return SourceType::LiveStream;
    case HTMLMediaElement::MovieLoadType::StoredStream: return SourceType::StoredStream;
    case HTMLMediaElement::MovieLoadType::HttpLiveStream: return SourceType::HLS;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

String HTMLMediaElement::localizedSourceType() const
{
    auto sourceType = this->sourceType();
    if (!sourceType)
        return { };

    switch (*sourceType) {
    case HTMLMediaElement::SourceType::File:
        return WEB_UI_STRING_KEY("File", "File (Media Element Source Type)", "Media Element Source Type");
    case HTMLMediaElement::SourceType::HLS:
        return WEB_UI_STRING_KEY("HLS", "HLS (Media Element Source Type)", "Media Element Source Type");
    case HTMLMediaElement::SourceType::MediaSource:
        return WEB_UI_STRING_KEY("Media Source", "MediaSource (Media Element Source Type)", "Media Element Source Type");
    case HTMLMediaElement::SourceType::ManagedMediaSource:
        return WEB_UI_STRING_KEY("Managed Media Source", "ManagedMediaSource (Media Element Source Type)", "Media Element Source Type");
    case HTMLMediaElement::SourceType::MediaStream:
        return WEB_UI_STRING_KEY("Media Stream", "MediaStream (Media Element Source Type)", "Media Element Source Type");
    case HTMLMediaElement::SourceType::LiveStream:
        return WEB_UI_STRING_KEY("Live Stream", "LiveStream (Media Element Source Type)", "Media Element Source Type");
    case HTMLMediaElement::SourceType::StoredStream:
        return WEB_UI_STRING_KEY("Stored Stream", "StoredStream (Media Element Source Type)", "Media Element Source Type");
    }

    ASSERT_NOT_REACHED();
    return { };
}

bool HTMLMediaElement::isActiveNowPlayingSession() const
{
    return m_mediaSession && m_mediaSession->isActiveNowPlayingSession();
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void HTMLMediaElement::updateSpatialTrackingLabel()
{
    RefPtr player = m_player;
    if (!player)
        return;

    player->setSpatialTrackingLabel(m_spatialTrackingLabel);

    RefPtr page = document().page();
    if (!page)
        return;

    player->setDefaultSpatialTrackingLabel(page->defaultSpatialTrackingLabel());
}

String HTMLMediaElement::spatialTrackingLabel() const
{
    return m_spatialTrackingLabel;
}

void HTMLMediaElement::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (m_spatialTrackingLabel == spatialTrackingLabel)
        return;
    m_spatialTrackingLabel = spatialTrackingLabel;

    if (RefPtr player = m_player)
        player->setSpatialTrackingLabel(spatialTrackingLabel);
}

void HTMLMediaElement::defaultSpatialTrackingLabelChanged(const String& defaultSpatialTrackingLabel)
{
    if (RefPtr player = m_player)
        player->setDefaultSpatialTrackingLabel(defaultSpatialTrackingLabel);
}
#endif

void HTMLMediaElement::setSoundStageSize(SoundStageSize size)
{
    if (m_soundStageSize == size)
        return;
    m_soundStageSize = size;

    if (RefPtr player = m_player)
        player->soundStageSizeDidChange();
}

bool HTMLMediaElement::shouldLogWatchtimeEvent() const
{
    // Autoplaying content should not produce watchtime diagnostics:
    if (!m_mediaSession || m_mediaSession->hasBehaviorRestriction(MediaElementSession::RequireUserGestureForAudioRateChange))
        return false;

    return true;
}

bool HTMLMediaElement::isWatchtimeTimerActive() const
{
    return m_watchtimeTimer && m_watchtimeTimer->isActive();
}

void HTMLMediaElement::startWatchtimeTimer()
{
    if (!m_watchtimeTimer) {
        m_watchtimeTimer = makeUnique<PausableIntervalTimer>(WatchtimeTimerInterval, [weakThis = WeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->watchtimeTimerFired();
        });
    }
    m_watchtimeTimer->start();
}

void HTMLMediaElement::pauseWatchtimeTimer()
{
    if (m_watchtimeTimer)
        m_watchtimeTimer->pause();
}

void HTMLMediaElement::fireAndRestartWatchtimeTimer()
{
    if (m_watchtimeTimer)
        m_watchtimeTimer->fireAndRestart();
}

void HTMLMediaElement::invalidateWatchtimeTimer()
{
    if (!m_watchtimeTimer)
        return;

    watchtimeTimerFired();
    m_watchtimeTimer->stop();
    m_watchtimeTimer = nullptr;
}

void HTMLMediaElement::logTextTrackDiagnostics(Ref<TextTrack> track, double numberOfSeconds)
{
    if (track->mode() == TextTrack::Mode::Disabled)
        return;

    WebCore::DiagnosticLoggingClient::ValueDictionary textTrackDictionary;
    textTrackDictionary.set(DiagnosticLoggingKeys::textTrackTypeKey(), static_cast<uint64_t>(track->trackType()));
    textTrackDictionary.set(DiagnosticLoggingKeys::textTrackKindKey(), static_cast<uint64_t>(track->kind()));
    textTrackDictionary.set(DiagnosticLoggingKeys::textTrackModeKey(), static_cast<uint64_t>(track->mode()));
    textTrackDictionary.set(DiagnosticLoggingKeys::secondsKey(), numberOfSeconds);

    document().protectedPage()->checkedDiagnosticLoggingClient()->logDiagnosticMessageWithValueDictionary(DiagnosticLoggingKeys::mediaTextTrackWatchTimeKey(), "Media Watchtime Interval By Enabled Text Track"_s, textTrackDictionary, ShouldSample::Yes);
}

void HTMLMediaElement::watchtimeTimerFired()
{
    if (!m_watchtimeTimer)
        return;

    if (!shouldLogWatchtimeEvent())
        return;

    RefPtr page = document().page();
    if (!page)
        return;

    // Bucket the watchtime seconds to the nearest 10s:
    double numberOfSeconds = m_watchtimeTimer->secondsCompleted().seconds();
    numberOfSeconds = round(numberOfSeconds / 10) * 10;

    // First log watchtime messages per-source-type:
    if (auto sourceType = this->sourceType()) {
        WebCore::DiagnosticLoggingClient::ValueDictionary sourceTypeDictionary;
        sourceTypeDictionary.set(DiagnosticLoggingKeys::sourceTypeKey(), static_cast<uint64_t>(*sourceType));
        sourceTypeDictionary.set(DiagnosticLoggingKeys::secondsKey(), numberOfSeconds);
        page->checkedDiagnosticLoggingClient()->logDiagnosticMessageWithValueDictionary(DiagnosticLoggingKeys::mediaSourceTypeWatchTimeKey(), "Media Watchtime Interval By Source Type"_s, sourceTypeDictionary, ShouldSample::Yes);
    }

    // Then log watchtime messages per-video-codec-type:
    [&] {
        RefPtr videoTracks = this->videoTracks();
        if (!videoTracks)
            return;

        RefPtr selectedVideoTrack = videoTracks->selectedItem();
        if (!selectedVideoTrack)
            return;

        // Convert the codec string to a 4CC code representing the codec type, and log only the codec type
        auto videoCodecString = selectedVideoTrack->configuration().codec();
        if (videoCodecString.length() < 4)
            return;

        auto videoCodecType = FourCC::fromString(StringView(videoCodecString).substring(0, 4));
        if (!videoCodecType)
            return;

        WebCore::DiagnosticLoggingClient::ValueDictionary videoCodecDictionary;
        videoCodecDictionary.set(DiagnosticLoggingKeys::videoCodecKey(), static_cast<uint64_t>(videoCodecType->value));
        videoCodecDictionary.set(DiagnosticLoggingKeys::secondsKey(), numberOfSeconds);
        page->checkedDiagnosticLoggingClient()->logDiagnosticMessageWithValueDictionary(DiagnosticLoggingKeys::mediaVideoCodecWatchTimeKey(), "Media Watchtime Interval By Video Codec"_s, videoCodecDictionary, ShouldSample::Yes);
    }();

    // Then log watchtime messages per-audio-codec-type:
    [&] {
        RefPtr audioTracks = this->audioTracks();
        if (!audioTracks)
            return;

        RefPtr selectedAudioTrack = audioTracks->firstEnabled();
        if (!selectedAudioTrack)
            return;

        // Convert the codec string to a 4CC code representing the codec type, and log only the codec type
        auto audioCodecString = selectedAudioTrack->configuration().codec();
        if (audioCodecString.length() < 4)
            return;

        auto audioCodecType = FourCC::fromString(StringView(audioCodecString).substring(0, 4));
        if (!audioCodecType)
            return;

        WebCore::DiagnosticLoggingClient::ValueDictionary audioCodecDictionary;
        audioCodecDictionary.set(DiagnosticLoggingKeys::audioCodecKey(), static_cast<uint64_t>(audioCodecType->value));
        audioCodecDictionary.set(DiagnosticLoggingKeys::secondsKey(), numberOfSeconds);
        page->checkedDiagnosticLoggingClient()->logDiagnosticMessageWithValueDictionary(DiagnosticLoggingKeys::mediaAudioCodecWatchTimeKey(), "Media Watchtime Interval By Audio Codec"_s, audioCodecDictionary, ShouldSample::Yes);
    }();

    // Then log watchtime messages per-presentation-type:
    enum class PresentationType : uint8_t {
        None,
        Inline,
        PictureInPicture,
        NativeFullscreen,
        ElementFullscreen,
        AirPlay,
        TV,
        AudioOnly,
    };
    auto presentationType = [&] {
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (RefPtr player = m_player; player && player->isCurrentPlaybackTargetWireless()) {
            if (player->wirelessPlaybackTargetType() == MediaPlayer::WirelessPlaybackTargetType::TargetTypeAirPlay)
                return PresentationType::AirPlay;
            if (player->wirelessPlaybackTargetType() == MediaPlayer::WirelessPlaybackTargetType::TargetTypeTVOut)
                return PresentationType::TV;
        }
#endif
        if (fullscreenMode() == VideoFullscreenModePictureInPicture)
            return PresentationType::PictureInPicture;
        if (fullscreenMode() == VideoFullscreenModeStandard)
            return PresentationType::NativeFullscreen;
#if ENABLE(FULLSCREEN_API)
        if (RefPtr fullscreen = protectedDocument()->fullscreenIfExists()) {
            if (RefPtr fullscreenElement = fullscreen->fullscreenElement(); fullscreenElement && fullscreenElement->contains(*this))
                return PresentationType::ElementFullscreen;
        }
#endif
        if (mediaType() == PlatformMediaSession::MediaType::Audio)
            return PresentationType::AudioOnly;
        if (!renderer())
            return PresentationType::None;
        return PresentationType::Inline;
    }();
    WebCore::DiagnosticLoggingClient::ValueDictionary presentationTypeDictionary;
    presentationTypeDictionary.set(DiagnosticLoggingKeys::presentationTypeKey(), static_cast<uint64_t>(presentationType));
    presentationTypeDictionary.set(DiagnosticLoggingKeys::secondsKey(), numberOfSeconds);
    page->checkedDiagnosticLoggingClient()->logDiagnosticMessageWithValueDictionary(DiagnosticLoggingKeys::mediaPresentationTypeWatchTimeKey(), "Media Watchtime Interval By Presentation Type"_s, presentationTypeDictionary, ShouldSample::Yes);

    if (m_textTracks) {
        for (unsigned i = 0; i < m_textTracks->length(); ++i)
            logTextTrackDiagnostics(Ref { *m_textTracks->item(i) }, numberOfSeconds);
    }
}

void HTMLMediaElement::startBufferingStopwatch()
{
    if (!shouldLogWatchtimeEvent())
        return;

    // Do not log during the initial buffering period after playback is initiated,
    // but before media data in advance of the current time has been loaded.
    if (m_readyStateMaximum <= HAVE_CURRENT_DATA)
        return;

    m_bufferingStopwatch = Stopwatch::create();
    Ref { *m_bufferingStopwatch }->start();
}

void HTMLMediaElement::invalidateBufferingStopwatch()
{
    RefPtr bufferingStopwatch = m_bufferingStopwatch;
    if (!bufferingStopwatch || !bufferingStopwatch->isActive())
        return;

    RefPtr page = document().page();
    if (!page)
        return;

    bufferingStopwatch->stop();
    auto bufferingDuration = bufferingStopwatch->elapsedTime();

    // Do not log when the source type is unknown (which should never happen).
    auto sourceType = this->sourceType();
    if (!sourceType)
        return;

    WebCore::DiagnosticLoggingClient::ValueDictionary bufferingDictionary;
    bufferingDictionary.set(DiagnosticLoggingKeys::sourceTypeKey(), static_cast<uint64_t>(*sourceType));
    bufferingDictionary.set(DiagnosticLoggingKeys::secondsKey(), bufferingDuration.seconds());
    page->checkedDiagnosticLoggingClient()->logDiagnosticMessageWithValueDictionary(DiagnosticLoggingKeys::mediaBufferingWatchTimeKey(), "Media Watchtime Buffering Event By Source Type"_s, bufferingDictionary, ShouldSample::Yes);
}

bool HTMLMediaElement::limitedMatroskaSupportEnabled() const
{
#if ENABLE(MEDIA_RECORDER_WEBM)
    return protectedDocument()->quirks().needsLimitedMatroskaSupport() || document().settings().limitedMatroskaSupportEnabled();
#else
    return false;
#endif
}

RefPtr<MediaSessionManagerInterface> HTMLMediaElement::sessionManager() const
{
    if (RefPtr page = document().page())
        return page->mediaSessionManager();

    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
