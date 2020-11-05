/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "AudioTimestamp.h"
#include "DOMWindow.h"
#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
#include "Page.h"
#include "Performance.h"
#include "PlatformMediaSessionManager.h"
#include "Quirks.h"
#include <wtf/IsoMallocInlines.h>

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

namespace WebCore {

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(document() && document()->page() && document()->page()->isAlwaysOnLoggingAllowed(), Media, "%p - AudioContext::" fmt, this, ##__VA_ARGS__)

#if OS(WINDOWS)
// Don't allow more than this number of simultaneous AudioContexts talking to hardware.
constexpr unsigned maxHardwareContexts = 4;
#endif

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioContext);

static Optional<float>& defaultSampleRateForTesting()
{
    static Optional<float> sampleRate;
    return sampleRate;
}

static bool shouldDocumentAllowWebAudioToAutoPlay(const Document& document)
{
    if (document.processingUserGestureForMedia() || document.isCapturing())
        return true;
    return document.quirks().shouldAutoplayWebAudioForArbitraryUserGesture() && document.topDocument().hasHadUserInteraction();
}

void AudioContext::setDefaultSampleRateForTesting(Optional<float> sampleRate)
{
    defaultSampleRateForTesting() = sampleRate;
}

ExceptionOr<Ref<AudioContext>> AudioContext::create(Document& document, AudioContextOptions&& contextOptions)
{
    ASSERT(isMainThread());
#if OS(WINDOWS)
    if (s_hardwareContextCount >= maxHardwareContexts)
        return Exception { QuotaExceededError, "Reached maximum number of hardware contexts on this platform"_s };
#endif
    
    if (!document.isFullyActive())
        return Exception { InvalidStateError, "Document is not fully active"_s };
    
    // FIXME: Figure out where latencyHint should go.

    if (!contextOptions.sampleRate && defaultSampleRateForTesting())
        contextOptions.sampleRate = *defaultSampleRateForTesting();

    if (contextOptions.sampleRate.hasValue() && !isSupportedSampleRate(contextOptions.sampleRate.value()))
        return Exception { SyntaxError, "sampleRate is not in range"_s };
    
    auto audioContext = adoptRef(*new AudioContext(document, contextOptions));
    audioContext->suspendIfNeeded();
    return audioContext;
}

// Constructor for rendering to the audio hardware.
AudioContext::AudioContext(Document& document, const AudioContextOptions& contextOptions)
    : BaseAudioContext(document, contextOptions)
    , m_mediaSession(PlatformMediaSession::create(PlatformMediaSessionManager::sharedManager(), *this))
{
    constructCommon();

    // Initialize the destination node's muted state to match the page's current muted state.
    pageMutedStateDidChange();

    document.addAudioProducer(*this);
    document.registerForVisibilityStateChangedCallbacks(*this);
}

// Only needed for WebKitOfflineAudioContext.
AudioContext::AudioContext(Document& document, unsigned numberOfChannels, float sampleRate, RefPtr<AudioBuffer>&& renderTarget)
    : BaseAudioContext(document, numberOfChannels, sampleRate, WTFMove(renderTarget))
    , m_mediaSession(PlatformMediaSession::create(PlatformMediaSessionManager::sharedManager(), *this))
{
    constructCommon();
}

void AudioContext::constructCommon()
{
    ASSERT(document());
    if (document()->audioPlaybackRequiresUserGesture())
        addBehaviorRestriction(RequireUserGestureForAudioStartRestriction);
    else
        m_restrictions = NoRestrictions;

#if PLATFORM(COCOA)
    addBehaviorRestriction(RequirePageConsentForAudioStartRestriction);
#endif
}

AudioContext::~AudioContext()
{
    if (!isOfflineContext() && scriptExecutionContext()) {
        document()->removeAudioProducer(*this);
        document()->unregisterForVisibilityStateChangedCallbacks(*this);
    }
}

double AudioContext::baseLatency()
{
    lazyInitialize();

    auto* destination = this->destination();
    return destination ? static_cast<double>(destination->framesPerBuffer()) / sampleRate() : 0.;
}

AudioTimestamp AudioContext::getOutputTimestamp(DOMWindow& window)
{
    if (!destination())
        return { 0, 0 };

    auto& performance = window.performance();

    auto position = outputPosition();

    // The timestamp of what is currently being played (contextTime) cannot be
    // later than what is being rendered. (currentTime)
    position.position = Seconds { std::min(position.position.seconds(), currentTime()) };

    auto performanceTime = performance.relativeTimeFromTimeOriginInReducedResolution(position.timestamp);
    performanceTime = std::max(performanceTime, 0.0);

    return { position.position.seconds(), performanceTime };
}

void AudioContext::close(DOMPromiseDeferred<void>&& promise)
{
    if (isOfflineContext() || isStopped()) {
        promise.reject(InvalidStateError);
        return;
    }

    if (state() == State::Closed || !destinationNode()) {
        promise.resolve();
        return;
    }

    addReaction(State::Closed, WTFMove(promise));

    lazyInitialize();

    destinationNode()->close([this, protectedThis = makeRef(*this)] {
        setState(State::Closed);
        uninitialize();
    });
}

DefaultAudioDestinationNode* AudioContext::destination()
{
    return static_cast<DefaultAudioDestinationNode*>(BaseAudioContext::destination());
}

void AudioContext::suspendRendering(DOMPromiseDeferred<void>&& promise)
{
    if (isOfflineContext()) {
        promise.reject(Exception { InvalidStateError, "Cannot call suspend() on an OfflineAudioContext"_s });
        return;
    }

    if (isStopped() || state() == State::Closed || !destinationNode()) {
        promise.reject(Exception { InvalidStateError, "Context is closed"_s });
        return;
    }

    m_wasSuspendedByScript = true;

    if (!willPausePlayback()) {
        addReaction(State::Suspended, WTFMove(promise));
        return;
    }

    lazyInitialize();

    destinationNode()->suspend([this, protectedThis = makeRef(*this), promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
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
    if (isOfflineContext()) {
        promise.reject(Exception { InvalidStateError, "Cannot call resume() on an OfflineAudioContext"_s });
        return;
    }

    if (isStopped() || state() == State::Closed || !destinationNode()) {
        promise.reject(Exception { InvalidStateError, "Context is closed"_s });
        return;
    }

    m_wasSuspendedByScript = false;

    if (!willBeginPlayback()) {
        addReaction(State::Running, WTFMove(promise));
        return;
    }

    lazyInitialize();

    destinationNode()->resume([this, protectedThis = makeRef(*this), promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
        if (exception) {
            promise.reject(WTFMove(*exception));
            return;
        }

        // Since we update the state asynchronously, we may have been interrupted after the
        // call to resume() and before this lambda runs. In this case, we don't want to
        // reset the state to running.
        bool interrupted = m_mediaSession->state() == PlatformMediaSession::Interrupted;
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

    makePendingActivity();

    lazyInitialize();
    destination()->startRendering([this, protectedThis = makeRef(*this)](Optional<Exception>&& exception) {
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
        if (destinationNode() && state() != State::Running) {
            // This starts the audio thread. The destination node's provideInput() method will now be called repeatedly to render audio.
            // Each time provideInput() is called, a portion of the audio stream is rendered. Let's call this time period a "render quantum".
            // NOTE: for now default AudioContext does not need an explicit startRendering() call from JavaScript.
            // We may want to consider requiring it for symmetry with OfflineAudioContext.
            startRendering();
            ++s_hardwareContextCount;
        }
    }
}

bool AudioContext::willPausePlayback()
{
    auto* document = this->document();
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

MediaProducer::MediaStateFlags AudioContext::mediaState() const
{
    if (!isStopped() && destinationNode() && destinationNode()->isPlayingAudio())
        return MediaProducer::IsPlayingAudio;

    return MediaProducer::IsNotPlaying;
}

void AudioContext::mayResumePlayback(bool shouldResume)
{
    if (!destinationNode() || state() == State::Closed || state() == State::Running)
        return;

    if (!shouldResume) {
        setState(State::Suspended);
        return;
    }

    if (!willBeginPlayback())
        return;

    lazyInitialize();

    destinationNode()->resume([this, protectedThis = makeRef(*this)](Optional<Exception>&& exception) {
        setState(exception ? State::Suspended : State::Running);
    });
}

bool AudioContext::willBeginPlayback()
{
    auto* document = this->document();
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

    auto willBegin = m_mediaSession->clientWillBeginPlayback();
    ALWAYS_LOG(LOGIDENTIFIER, "returning ", willBegin);

    return willBegin;
}

void AudioContext::visibilityStateChanged()
{
    // Do not suspend if audio is audible.
    if (!document() || mediaState() == MediaProducer::IsPlayingAudio || isStopped())
        return;

    if (document()->hidden()) {
        if (state() == State::Running) {
            ALWAYS_LOG(LOGIDENTIFIER, "Suspending playback after going to the background");
            m_mediaSession->beginInterruption(PlatformMediaSession::EnteringBackground);
        }
    } else {
        if (state() == State::Interrupted) {
            ALWAYS_LOG(LOGIDENTIFIER, "Resuming playback after entering foreground");
            m_mediaSession->endInterruption(PlatformMediaSession::MayResumePlaying);
        }
    }
}

void AudioContext::suspend(ReasonForSuspension)
{
    if (isClosed() || m_wasSuspendedByScript)
        return;

    m_mediaSession->beginInterruption(PlatformMediaSession::PlaybackSuspended);
    document()->updateIsPlayingMedia();
}

void AudioContext::resume()
{
    if (isClosed() || m_wasSuspendedByScript)
        return;

    m_mediaSession->endInterruption(PlatformMediaSession::MayResumePlaying);
    document()->updateIsPlayingMedia();
}

void AudioContext::suspendPlayback()
{
    if (!destinationNode() || state() == State::Closed)
        return;

    lazyInitialize();

    destinationNode()->suspend([this, protectedThis = makeRef(*this)](Optional<Exception>&& exception) {
        if (exception)
            return;

        bool interrupted = m_mediaSession->state() == PlatformMediaSession::Interrupted;
        setState(interrupted ? State::Interrupted : State::Suspended);
    });
}

DocumentIdentifier AudioContext::hostingDocumentIdentifier() const
{
    auto* document = downcast<Document>(m_scriptExecutionContext);
    return document ? document->identifier() : DocumentIdentifier { };
}

bool AudioContext::isSuspended() const
{
    return !document() || document()->activeDOMObjectsAreSuspended() || document()->activeDOMObjectsAreStopped();
}

void AudioContext::pageMutedStateDidChange()
{
    if (destinationNode() && document() && document()->page())
        destinationNode()->setMuted(document()->page()->isAudioMuted());
}

void AudioContext::mediaCanStart(Document& document)
{
    ASSERT_UNUSED(document, &document == this->document());
    removeBehaviorRestriction(RequirePageConsentForAudioStartRestriction);
    mayResumePlayback(true);
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

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
