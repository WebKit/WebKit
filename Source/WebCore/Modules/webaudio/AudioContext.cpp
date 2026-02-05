/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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
#include "DocumentPage.h"
#include "DocumentQuirks.h"
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
#include "Settings.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MediaTime.h>
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioContext);

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
    RefPtr mainDocument = document.mainFrameDocument();
    if (document.quirks().shouldAutoplayWebAudioForArbitraryUserGesture() && mainDocument && mainDocument->hasHadUserInteraction())
        return true;
    RefPtr window = document.window();
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
    , m_mediaSession(PlatformMediaSession::create(*this))
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
    RefPtr page = document()->page();
    if (!page || page->requiresUserGestureForAudioPlayback())
        addBehaviorRestriction(BehaviorRestrictionFlags::RequireUserGestureForAudioStartRestriction);

#if PLATFORM(COCOA)
    addBehaviorRestriction(BehaviorRestrictionFlags::RequirePageConsentForAudioStartRestriction);
#endif
}

AudioContext::~AudioContext()
{
    m_mediaSession->invalidateClient();

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

    return static_cast<double>(protect(destination())->framesPerBuffer()) / sampleRate();
}

double AudioContext::outputLatency()
{
    lazyInitialize();

    if (!isPlaying())
        return 0;
    if (noiseInjectionPolicies())
        return 512 / sampleRate(); // A fixed, but reasonable value for most platforms.

    return protect(destination())->outputLatency().toDouble();
}

AudioTimestamp AudioContext::getOutputTimestamp()
{
    auto position = outputPosition();

    // The timestamp of what is currently being played (contextTime) cannot be
    // later than what is being rendered. (currentTime)
    position.position = Seconds { std::min(position.position.seconds(), currentTime()) };

    DOMHighResTimeStamp performanceTime = 0.0;
    RefPtr document = this->document();
    if (document && document->window())
        performanceTime = std::max(document->window()->protectedPerformance()->relativeTimeFromTimeOriginInReducedResolution(position.timestamp), 0.0);

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

    addReaction(State::Closed, WTF::move(promise));

    lazyInitialize();

    protect(destination())->close([activity = makePendingActivity(*this)] {
        activity->object().setState(State::Closed);
        activity->object().uninitialize();
        activity->object().m_mediaSession->setActive(false);
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
        addReaction(State::Suspended, WTF::move(promise));
        return;
    }

    lazyInitialize();

    protect(destination())->suspend([activity = makePendingActivity(*this), promise = WTF::move(promise)](std::optional<Exception>&& exception) mutable {
        if (exception) {
            promise.reject(WTF::move(*exception));
            return;
        }
        activity->object().setState(State::Suspended);
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

    willBeginPlayback([weakThis = WeakPtr { *this }, promise = WTF::move(promise)](bool willBegin) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!willBegin) {
            protectedThis->addReaction(State::Running, WTF::move(promise));
            return;
        }

        protectedThis->lazyInitialize();

        protect(protectedThis->destination())->resume([activity = protectedThis->makePendingActivity(*protectedThis), promise = WTF::move(promise)](std::optional<Exception>&& exception) mutable {
            if (exception) {
                promise.reject(WTF::move(*exception));
                return;
            }

            // Since we update the state asynchronously, we may have been interrupted after the
            // call to resume() and before this lambda runs. In this case, we don't want to
            // reset the state to running.
            bool interrupted = activity->object().m_mediaSession->state() == PlatformMediaSession::State::Interrupted;
            activity->object().setState(interrupted ? State::Interrupted : State::Running);
            if (interrupted)
                activity->object().addReaction(State::Running, WTF::move(promise));
            else
                promise.resolve();
        });
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
    if (isStopped() || m_wasSuspendedByScript)
        return;

    willBeginPlayback([weakThis = WeakPtr { *this }](bool willBegin) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !willBegin)
            return;

        protectedThis->lazyInitialize();
        protect(protectedThis->destination())->startRendering([pendingActivity = protectedThis->makePendingActivity(*protectedThis), protectedThis = WTF::move(protectedThis)](std::optional<Exception>&& exception) {
            if (!exception)
                protectedThis->setState(State::Running);
        });
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
        removeBehaviorRestriction(BehaviorRestrictionFlags::RequireUserGestureForAudioStartRestriction);
    }

    if (pageConsentRequiredForAudioStart()) {
        RefPtr page = document->page();
        if (page && !page->canStartMedia()) {
            document->addMediaCanStartListener(*this);
            return false;
        }
        removeBehaviorRestriction(BehaviorRestrictionFlags::RequirePageConsentForAudioStartRestriction);
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

    willBeginPlayback([weakThis = WeakPtr { *this }](bool willBegin) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !willBegin)
            return;

        protectedThis->lazyInitialize();

        protect(protectedThis->destination())->resume([pendingActivity = protectedThis->makePendingActivity(*protectedThis), protectedThis = WTF::move(protectedThis)](std::optional<Exception>&& exception) {
            protectedThis->setState(exception ? State::Suspended : State::Running);
        });
    });
}

void AudioContext::willBeginPlayback(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr document = this->document();
    if (!document) {
        completionHandler(false);
        return;
    }

    auto logSiteIdentifier = LOGIDENTIFIER;
    if (userGestureRequiredForAudioStart()) {
        if (!shouldDocumentAllowWebAudioToAutoPlay(*document)) {
            ALWAYS_LOG(logSiteIdentifier, "returning false, not processing user gesture or capturing");
            completionHandler(false);
            return;
        }
        removeBehaviorRestriction(BehaviorRestrictionFlags::RequireUserGestureForAudioStartRestriction);
    }

    if (pageConsentRequiredForAudioStart()) {
        RefPtr page = document->page();
        if (page && !page->canStartMedia()) {
            document->addMediaCanStartListener(*this);
            ALWAYS_LOG(logSiteIdentifier, "returning false, page doesn't allow media to start");
            completionHandler(false);
            return;
        }
        removeBehaviorRestriction(BehaviorRestrictionFlags::RequirePageConsentForAudioStartRestriction);
    }

    m_mediaSession->clientWillBeginPlayback([weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler), logSiteIdentifier = WTF::move(logSiteIdentifier)](bool willBegin) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            completionHandler(false);
            return;
        }

        protectedThis->m_mediaSession->setActive(true);

        ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "returning ", willBegin);
        completionHandler(willBegin);
    });
}

void AudioContext::suspend(ReasonForSuspension)
{
    if (isClosed() || m_wasSuspendedByScript)
        return;

    m_mediaSession->beginInterruption(PlatformMediaSession::InterruptionType::PlaybackSuspended);
    protect(document())->updateIsPlayingMedia();
}

void AudioContext::resume()
{
    if (isClosed() || m_wasSuspendedByScript)
        return;

    m_mediaSession->endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    protect(document())->updateIsPlayingMedia();
}

void AudioContext::suspendPlayback()
{
    if (state() == State::Closed || !isInitialized())
        return;

    lazyInitialize();

    protect(destination())->suspend([protectedThis = Ref { *this }, pendingActivity = makePendingActivity(*this)](std::optional<Exception>&& exception) {
        if (exception)
            return;

        bool interrupted = protectedThis->m_mediaSession->state() == PlatformMediaSession::State::Interrupted;
        protectedThis->setState(interrupted ? State::Interrupted : State::Suspended);
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
    RefPtr document = this->document();
    return document && document->page() ? protect(document->page())->mediaSessionGroupIdentifier() : std::nullopt;
}

static bool hasPlayBackAudioSession(Document* document)
{
#if ENABLE(DOM_AUDIO_SESSION)
    RefPtr window = document ? document->window() : nullptr;

    RefPtr navigator = window ? window->optionalNavigator() : nullptr;
    if (!navigator)
        return false;

    Ref audioSession = NavigatorAudioSession::audioSession(*navigator);
    return audioSession->type() == DOMAudioSessionType::Playback || audioSession->type() == DOMAudioSessionType::PlayAndRecord;
#else
    UNUSED_PARAM(document);
    return false;
#endif
}

bool AudioContext::isNowPlayingEligible() const
{
    if (!protect(destination())->isConnected() || m_wasSuspendedByScript)
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
    RefPtr window = document ? document->window() : nullptr;
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
        cryptographicallyRandomNumber<uint64_t>(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
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

WeakPtr<PlatformMediaSessionInterface> AudioContext::selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSessionInterface>>& sessions, PlatformMediaSession::PlaybackControlsPurpose purpose)
{
    if (purpose != PlatformMediaSession::PlaybackControlsPurpose::NowPlaying)
        return nullptr;

    WeakPtr<PlatformMediaSessionInterface> audibleSession;
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
    RefPtr document = this->document();
    return !document || document->activeDOMObjectsAreSuspended() || document->activeDOMObjectsAreStopped();
}

bool AudioContext::isPlaying() const
{
    return state() == State::Running;
}

void AudioContext::pageMutedStateDidChange()
{
    if (RefPtr document = this->document(); document && document->page())
        destination().setMuted(document->page()->isAudioMuted());
}

#if PLATFORM(IOS_FAMILY)
void AudioContext::sceneIdentifierDidChange()
{
    RefPtr document = this->document();
    if (!document)
        return;

    if (RefPtr page = document->page()) {
        ALWAYS_LOG(LOGIDENTIFIER, page->sceneIdentifier());
        destination().setSceneIdentifier(page->sceneIdentifier());
    }
}

const String& AudioContext::sceneIdentifier() const
{
    if (RefPtr document = this->document(); document && document->page())
        return document->page()->sceneIdentifier();
    return nullString();
}
#endif

void AudioContext::mediaCanStart(Document& document)
{
    ASSERT_UNUSED(document, &document == this->document());
    removeBehaviorRestriction(BehaviorRestrictionFlags::RequirePageConsentForAudioStartRestriction);
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

    if (m_canOverrideBackgroundPlaybackRestriction && !protect(destination())->isConnected())
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
    RefPtr manager = mediaSessionManagerIfExists();
    if (manager && (!manager->isApplicationInBackground() || m_mediaSession->state() == PlatformMediaSession::State::Interrupted)) {
        manager->updateNowPlayingInfoIfNecessary();
        return;
    }

    // We end the overriden interruption (if any) to get the right count of interruptions and start a new interruption.
    m_mediaSession->endInterruption(PlatformMediaSession::EndInterruptionFlags::NoFlags);

    m_canOverrideBackgroundPlaybackRestriction = false;
    m_mediaSession->beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
    m_canOverrideBackgroundPlaybackRestriction = true;
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
    return MediaElementAudioSourceNode::create(*this, { mediaElement });
}

#endif

#if ENABLE(MEDIA_STREAM)

ExceptionOr<Ref<MediaStreamAudioSourceNode>> AudioContext::createMediaStreamSource(MediaStream& mediaStream)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());

    return MediaStreamAudioSourceNode::create(*this, { mediaStream });
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

RefPtr<MediaSessionManagerInterface> AudioContext::sessionManager() const
{
    return BaseAudioContext::mediaSessionManager();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
