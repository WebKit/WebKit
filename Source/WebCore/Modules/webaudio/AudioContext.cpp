/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioContext.h"
#include "AudioContextOptions.h"
#include "AudioTimestamp.h"
#include "DOMAudioSession.h"
#include "DocumentInlines.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "MediaSession.h"
#include "Navigator.h"
#include "NavigatorAudioSession.h"
#include "NavigatorMediaSession.h"
#include "NowPlayingInfo.h"
#include "PageInlines.h"
#include "Performance.h"
#include "PlatformMediaSessionManager.h"
#include "Quirks.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MEDIA_STREAM)
#include "MediaStream.h"
#include "MediaStreamAudioDestinationNode.h"
#include "MediaStreamAudioSource.h"
#include "MediaStreamAudioSourceNode.h"
#include "MediaStreamAudioSourceOptions.h"
#endif

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#include "MediaElementAudioSourceNode.h"
#include "MediaElementAudioSourceOptions.h"
#endif

#if PLATFORM(VISION) && ENABLE(WEBXR)
#include "Page.h"
#endif

namespace WebCore {

#define AUDIOCONTEXT_RELEASE_LOG(fmt, ...) RELEASE_LOG(Media, "%p - AudioContext::" fmt, this, ##__VA_ARGS__)

#if OS(WINDOWS)
// Don't allow more than this number of simultaneous AudioContexts talking to hardware.
constexpr unsigned maxHardwareContexts = 4;
#endif

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(AudioContext);

#if OS(WINDOWS)
static unsigned hardwareContextCount;
#endif

static std::optional<float>& defaultSampleRateForTesting()
{
    static std::optional<float> sampleRate;
    return sampleRate;
}

static bool shouldDocumentAllowWebAudioToAutoPlay(const Document& document)
{
    if (document.isCapturing())
        return true;
    if (document.quirks().shouldAutoplayWebAudioForArbitraryUserGesture() && document.topDocument().hasHadUserInteraction())
        return true;
    RefPtr window = document.domWindow();
    return window && window->hasTransientActivation();
}

void AudioContext::setDefaultSampleRateForTesting(std::optional<float> sampleRate)
{
    defaultSampleRateForTesting() = sampleRate;
}

ExceptionOr<Ref<AudioContext>> AudioContext::create(Document& document, AudioContextOptions&& contextOptions)
{
    ASSERT(isMainThread());
#if OS(WINDOWS)
    if (hardwareContextCount >= maxHardwareContexts)
        return Exception { ExceptionCode::QuotaExceededError, "Reached maximum number of hardware contexts on this platform"_s };
#endif
    
    if (!document.isFullyActive())
        return Exception { ExceptionCode::InvalidStateError, "Document is not fully active"_s };
    
    // FIXME: Figure out where latencyHint should go.

    if (!contextOptions.sampleRate && defaultSampleRateForTesting())
        contextOptions.sampleRate = *defaultSampleRateForTesting();

    if (contextOptions.sampleRate && !isSupportedSampleRate(*contextOptions.sampleRate))
        return Exception { ExceptionCode::NotSupportedError, "sampleRate is not in range"_s };
    
    auto audioContext = adoptRef(*new AudioContext(document, contextOptions));
    audioContext->suspendIfNeeded();
    return audioContext;
}

AudioContext::AudioContext(Document& document, const AudioContextOptions& contextOptions)
    : BaseAudioContext(document)
    , m_destinationNode(makeUniqueRefWithoutRefCountedCheck<DefaultAudioDestinationNode>(*this, contextOptions.sampleRate))
    , m_mediaSession(PlatformMediaSession::create(PlatformMediaSessionManager::singleton(), *this))
    , m_currentIdentifier(MediaUniqueIdentifier::generate())
{
    constructCommon();

    // Initialize the destination node's muted state to match the page's current muted state.
    pageMutedStateDidChange();

    document.addAudioProducer(*this);

    // Unlike OfflineAudioContext, AudioContext does not require calling resume() to start rendering.
    // Lazy initialization starts rendering so we schedule a task here to make sure lazy initialization
    // ends up happening, even if no audio node gets constructed.
    postTask([this, pendingActivity = makePendingActivity(*this)] {
        if (!isStopped())
            lazyInitialize();
    });
}

void AudioContext::constructCommon()
{
    ASSERT(document());
    if (document()->topDocument().requiresUserGestureForAudioPlayback())
        addBehaviorRestriction(RequireUserGestureForAudioStartRestriction);
    else
        m_restrictions = NoRestrictions;

#if PLATFORM(COCOA)
    addBehaviorRestriction(RequirePageConsentForAudioStartRestriction);
#endif
}

AudioContext::~AudioContext()
{
    if (RefPtr document = this->document())
        document->removeAudioProducer(*this);
}

void AudioContext::uninitialize()
{
    if (!isInitialized())
        return;

    BaseAudioContext::uninitialize();

#if OS(WINDOWS)
    ASSERT(hardwareContextCount);
    --hardwareContextCount;
#endif

    setState(State::Closed);
}

double AudioContext::baseLatency()
{
    lazyInitialize();

    return static_cast<double>(destination().framesPerBuffer()) / sampleRate();
}

AudioTimestamp AudioContext::getOutputTimestamp()
{
    auto position = outputPosition();

    // The timestamp of what is currently being played (contextTime) cannot be
    // later than what is being rendered. (currentTime)
    position.position = Seconds { std::min(position.position.seconds(), currentTime()) };

    DOMHighResTimeStamp performanceTime = 0.0;
    if (document() && document()->domWindow())
        performanceTime = std::max(document()->domWindow()->performance().relativeTimeFromTimeOriginInReducedResolution(position.timestamp), 0.0);

    return { position.position.seconds(), performanceTime };
}

void AudioContext::close(DOMPromiseDeferred<void>&& promise)
{
    if (isStopped()) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    if (state() == State::Closed) {
        promise.resolve();
        return;
    }

    addReaction(State::Closed, WTFMove(promise));

    lazyInitialize();

    destination().close([this, activity = makePendingActivity(*this)] {
        setState(State::Closed);
        uninitialize();
        m_mediaSession->setActive(false);
    });
}

void AudioContext::suspendRendering(DOMPromiseDeferred<void>&& promise)
{
    if (isStopped() || state() == State::Closed) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Context is closed"_s });
        return;
    }

    m_wasSuspendedByScript = true;

    if (!willPausePlayback()) {
        addReaction(State::Suspended, WTFMove(promise));
        return;
    }

    lazyInitialize();

    destination().suspend([this, activity = makePendingActivity(*this), promise = WTFMove(promise)](std::optional<Exception>&& exception) mutable {
        if (exception) {
            promise.reject(WTFMove(*exception));
            return;
        }
        setState(State::Suspended);
        promise.resolve();
    });
}

void AudioContext::resumeRendering(DOMPromiseDeferred<void>&& promise)
{
    if (isStopped() || state() == State::Closed) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Context is closed"_s });
        return;
    }

    m_wasSuspendedByScript = false;

    if (!willBeginPlayback()) {
        addReaction(State::Running, WTFMove(promise));
        return;
    }

    lazyInitialize();

    destination().resume([this, activity = makePendingActivity(*this), promise = WTFMove(promise)](std::optional<Exception>&& exception) mutable {
        if (exception) {
            promise.reject(WTFMove(*exception));
            return;
        }

        // Since we update the state asynchronously, we may have been interrupted after the
        // call to resume() and before this lambda runs. In this case, we don't want to
        // reset the state to running.
        bool interrupted = m_mediaSession->state() == PlatformMediaSession::State::Interrupted;
        setState(interrupted ? State::Interrupted : State::Running);
        if (interrupted)
            addReaction(State::Running, WTFMove(promise));
        else
            promise.resolve();
    });
}

void AudioContext::sourceNodeWillBeginPlayback(AudioNode& audioNode)
{
    BaseAudioContext::sourceNodeWillBeginPlayback(audioNode);

    // Called by scheduled AudioNodes when clients schedule their start times.
    // Prior to the introduction of suspend(), resume(), and stop(), starting
    // a scheduled AudioNode would remove the user-gesture restriction, if present,
    // and would thus unmute the context. Now that AudioContext stays in the
    // "suspended" state if a user-gesture restriction is present, starting a
    // schedule AudioNode should set the state to "running", but only if the
    // user-gesture restriction is set.
    if (userGestureRequiredForAudioStart())
        startRendering();
}

void AudioContext::startRendering()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isStopped() || !willBeginPlayback() || m_wasSuspendedByScript)
        return;

    lazyInitialize();
    destination().startRendering([this, protectedThis = Ref { *this }, pendingActivity = makePendingActivity(*this)](std::optional<Exception>&& exception) {
        if (!exception)
            setState(State::Running);
    });
}

void AudioContext::lazyInitialize()
{
    if (isInitialized())
        return;

    BaseAudioContext::lazyInitialize();
    if (isInitialized()) {
        if (state() != State::Running) {
            // This starts the audio thread. The destination node's provideInput() method will now be called repeatedly to render audio.
            // Each time provideInput() is called, a portion of the audio stream is rendered. Let's call this time period a "render quantum".
            // NOTE: for now default AudioContext does not need an explicit startRendering() call from JavaScript.
            // We may want to consider requiring it for symmetry with OfflineAudioContext.
            startRendering();
#if OS(WINDOWS)
            ++hardwareContextCount;
#endif
        }
    }
}

bool AudioContext::willPausePlayback()
{
    RefPtr document = this->document();
    if (!document)
        return false;

    if (userGestureRequiredForAudioStart()) {
        if (!document->processingUserGestureForMedia())
            return false;
        removeBehaviorRestriction(RequireUserGestureForAudioStartRestriction);
    }

    if (pageConsentRequiredForAudioStart()) {
        auto* page = document->page();
        if (page && !page->canStartMedia()) {
            document->addMediaCanStartListener(*this);
            return false;
        }
        removeBehaviorRestriction(RequirePageConsentForAudioStartRestriction);
    }

    return m_mediaSession->clientWillPausePlayback();
}

MediaProducerMediaStateFlags AudioContext::mediaState() const
{
    return isAudible() ? MediaProducerMediaState::IsPlayingAudio : MediaProducer::IsNotPlaying;
}

bool AudioContext::isAudible() const
{
    return !isStopped() && destination().isPlayingAudio();
}

void AudioContext::mayResumePlayback(bool shouldResume)
{
    if (state() == State::Closed || !isInitialized() || state() == State::Running)
        return;

    if (!shouldResume) {
        setState(State::Suspended);
        return;
    }

    if (m_wasSuspendedByScript)
        return;

    if (!willBeginPlayback())
        return;

    lazyInitialize();

    destination().resume([this, protectedThis = Ref { *this }, pendingActivity = makePendingActivity(*this)](std::optional<Exception>&& exception) {
        setState(exception ? State::Suspended : State::Running);
    });
}

bool AudioContext::willBeginPlayback()
{
    RefPtr document = this->document();
    if (!document)
        return false;

    if (userGestureRequiredForAudioStart()) {
        if (!shouldDocumentAllowWebAudioToAutoPlay(*document)) {
            ALWAYS_LOG(LOGIDENTIFIER, "returning false, not processing user gesture or capturing");
            return false;
        }
        removeBehaviorRestriction(RequireUserGestureForAudioStartRestriction);
    }

    if (pageConsentRequiredForAudioStart()) {
        auto* page = document->page();
        if (page && !page->canStartMedia()) {
            document->addMediaCanStartListener(*this);
            ALWAYS_LOG(LOGIDENTIFIER, "returning false, page doesn't allow media to start");
            return false;
        }
        removeBehaviorRestriction(RequirePageConsentForAudioStartRestriction);
    }

    m_mediaSession->setActive(true);

    auto willBegin = m_mediaSession->clientWillBeginPlayback();
    ALWAYS_LOG(LOGIDENTIFIER, "returning ", willBegin);

    return willBegin;
}

void AudioContext::suspend(ReasonForSuspension)
{
    if (isClosed() || m_wasSuspendedByScript)
        return;

    m_mediaSession->beginInterruption(PlatformMediaSession::InterruptionType::PlaybackSuspended);
    document()->updateIsPlayingMedia();
}

void AudioContext::resume()
{
    if (isClosed() || m_wasSuspendedByScript)
        return;

    m_mediaSession->endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    document()->updateIsPlayingMedia();
}

void AudioContext::suspendPlayback()
{
    if (state() == State::Closed || !isInitialized())
        return;

    lazyInitialize();

    destination().suspend([this, protectedThis = Ref { *this }, pendingActivity = makePendingActivity(*this)](std::optional<Exception>&& exception) {
        if (exception)
            return;

        bool interrupted = m_mediaSession->state() == PlatformMediaSession::State::Interrupted;
        setState(interrupted ? State::Interrupted : State::Suspended);
    });
}

bool AudioContext::canReceiveRemoteControlCommands() const
{
#if ENABLE(DOM_AUDIO_SESSION)
    return isNowPlayingEligible();
#else
    return false;
#endif
}

void AudioContext::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument&)
{
    switch (command) {
    case PlatformMediaSession::RemoteControlCommandType::PlayCommand:
        mayResumePlayback(true);
        break;
    case PlatformMediaSession::RemoteControlCommandType::StopCommand:
    case PlatformMediaSession::RemoteControlCommandType::PauseCommand:
        suspendPlayback();
        break;
    case PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand:
        if (state() == State::Interrupted || state() == State::Suspended)
            mayResumePlayback(true);
        else
            suspendPlayback();
        break;
    case PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand:
    case PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand:
    case PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand:
    case PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand:
    case PlatformMediaSession::RemoteControlCommandType::BeginScrubbingCommand:
    case PlatformMediaSession::RemoteControlCommandType::EndScrubbingCommand:
    case PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand:
    case PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand:
    case PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand:
    default:
        ASSERT_NOT_REACHED();
    }
}

std::optional<MediaSessionGroupIdentifier> AudioContext::mediaSessionGroupIdentifier() const
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    return document && document->page() ? document->page()->mediaSessionGroupIdentifier() : std::nullopt;
}

static bool hasPlayBackAudioSession(Document* document)
{
#if ENABLE(DOM_AUDIO_SESSION)
    RefPtr window = document ? document->domWindow() : nullptr;

    RefPtr navigator = window ? window->optionalNavigator() : nullptr;
    RefPtr audioSession = navigator ? NavigatorAudioSession::audioSession(*navigator) : nullptr;

    auto audioSessionType = audioSession ? audioSession->type() : DOMAudioSessionType::Auto;
    return audioSessionType == DOMAudioSessionType::Playback || audioSessionType == DOMAudioSessionType::PlayAndRecord;
#else
    UNUSED_PARAM(document);
    return false;
#endif
}

bool AudioContext::isNowPlayingEligible() const
{
    if (!destination().isConnected() || m_wasSuspendedByScript)
        return false;

    RefPtr document = this->document();
    if (!document)
        return false;

    RefPtr page = document->page();
    if (page && page->mediaPlaybackIsSuspended())
        return false;

    return hasPlayBackAudioSession(document.get());
}

std::optional<NowPlayingInfo> AudioContext::nowPlayingInfo() const
{
    if (!isNowPlayingEligible())
        return { };

    RefPtr document = this->document();
    RefPtr page = document ? document->page() : nullptr;
    RefPtr window = document ? document->domWindow() : nullptr;
    if (!page || !window)
        return { };

    NowPlayingInfo nowPlayingInfo {
        {
            { },
            { },
            { },
            { },
            { }
        },
        MediaPlayer::invalidTime(),
        MediaPlayer::invalidTime(),
        1.0,
        false,
        m_currentIdentifier,
        isPlaying(),
        !page->isVisibleAndActive(),
        false
    };

    if (page->usesEphemeralSession() && !document->settings().allowPrivacySensitiveOperationsInNonPersistentDataStores())
        return nowPlayingInfo;

#if ENABLE(MEDIA_SESSION)
    if (RefPtr mediaSession = NavigatorMediaSession::mediaSessionIfExists(window->protectedNavigator()))
        mediaSession->updateNowPlayingInfo(nowPlayingInfo);
#endif

    if (nowPlayingInfo.metadata.title.isEmpty()) {
        RegistrableDomain domain { document->securityOrigin().data() };
        if (!domain.isEmpty())
            nowPlayingInfo.metadata.title = domain.string();
    }

    return nowPlayingInfo;
}

WeakPtr<PlatformMediaSession> AudioContext::selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSession>>& sessions, PlatformMediaSession::PlaybackControlsPurpose purpose)
{
    if (purpose != PlatformMediaSession::PlaybackControlsPurpose::NowPlaying)
        return nullptr;

    WeakPtr<PlatformMediaSession> audibleSession;
    for (auto& session : sessions) {
        if (!isNowPlayingEligible())
            continue;

        if (!audibleSession)
            audibleSession = session;

        if (session->isPlaying())
            return session;
    }

    return audibleSession;
}

bool AudioContext::isSuspended() const
{
    return !document() || document()->activeDOMObjectsAreSuspended() || document()->activeDOMObjectsAreStopped();
}

bool AudioContext::isPlaying() const
{
    return state() == State::Running;
}

void AudioContext::pageMutedStateDidChange()
{
    if (document() && document()->page())
        destination().setMuted(document()->page()->isAudioMuted());
}

void AudioContext::mediaCanStart(Document& document)
{
    ASSERT_UNUSED(document, &document == this->document());
    removeBehaviorRestriction(RequirePageConsentForAudioStartRestriction);
    mayResumePlayback(true);
}

void AudioContext::isPlayingAudioDidChange()
{
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    // Make sure to call Document::updateIsPlayingMedia() on the main thread, since
    // we could be on the audio I/O thread here and the call into WebCore could block.
    callOnMainThread([protectedThis = Ref { *this }] {
        if (RefPtr document = protectedThis->document())
            document->updateIsPlayingMedia();
    });
}

bool AudioContext::shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType interruption) const
{
    if (interruption != PlatformMediaSession::InterruptionType::EnteringBackground)
        return false;

    if (m_canOverrideBackgroundPlaybackRestriction && !destination().isConnected())
        return true;

    RefPtr document = this->document();
    if (!document)
        return false;

#if PLATFORM(VISION) && ENABLE(WEBXR)
    RefPtr page = document->page();
    if (page && page->hasActiveImmersiveSession())
        return true;
#endif

    return hasPlayBackAudioSession(document.get());
}

void AudioContext::defaultDestinationWillBecomeConnected()
{
    // We might need to interrupt if we previously overrode a background interruption.
    if (!PlatformMediaSessionManager::singleton().isApplicationInBackground() || m_mediaSession->state() == PlatformMediaSession::State::Interrupted) {
        PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary();
        return;
    }

    // We end the overriden interruption (if any) to get the right count of interruptions and start a new interruption.
    m_mediaSession->endInterruption(PlatformMediaSession::EndInterruptionFlags::NoFlags);

    m_canOverrideBackgroundPlaybackRestriction = false;
    m_mediaSession->beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
    m_canOverrideBackgroundPlaybackRestriction = true;
}

void AudioContext::isActiveNowPlayingSessionChanged()
{
    if (RefPtr document = this->document()) {
        if (RefPtr page = document->protectedPage())
            page->hasActiveNowPlayingSessionChanged();
    }
}

ProcessID AudioContext::presentingApplicationPID() const
{
    if (RefPtr document = this->document()) {
        if (RefPtr page = document->protectedPage())
            return page->presentingApplicationPID();
    }

    return { };
}

#if !RELEASE_LOG_DISABLED
const Logger& AudioContext::logger() const
{
    return BaseAudioContext::logger();
}
#endif

#if ENABLE(VIDEO)

ExceptionOr<Ref<MediaElementAudioSourceNode>> AudioContext::createMediaElementSource(HTMLMediaElement& mediaElement)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
    return MediaElementAudioSourceNode::create(*this, { &mediaElement });
}

#endif

#if ENABLE(MEDIA_STREAM)

ExceptionOr<Ref<MediaStreamAudioSourceNode>> AudioContext::createMediaStreamSource(MediaStream& mediaStream)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());

    return MediaStreamAudioSourceNode::create(*this, { &mediaStream });
}

ExceptionOr<Ref<MediaStreamAudioDestinationNode>> AudioContext::createMediaStreamDestination()
{
    return MediaStreamAudioDestinationNode::create(*this);
}

#endif

bool AudioContext::virtualHasPendingActivity() const
{
    return !isClosed();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
