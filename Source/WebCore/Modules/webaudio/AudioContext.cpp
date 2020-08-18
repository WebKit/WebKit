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
#include "DefaultAudioDestinationNode.h"
#include "JSDOMPromiseDeferred.h"
#include "Performance.h"
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

#if OS(WINDOWS)
// Don't allow more than this number of simultaneous AudioContexts talking to hardware.
constexpr unsigned maxHardwareContexts = 4;
#endif

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioContext);

ExceptionOr<Ref<AudioContext>> AudioContext::create(Document& document, const AudioContextOptions& contextOptions)
{
    ASSERT(isMainThread());
#if OS(WINDOWS)
    if (s_hardwareContextCount >= maxHardwareContexts)
        return Exception { QuotaExceededError };
#endif
    
    if (!document.isFullyActive())
        return Exception { InvalidStateError, "Document is not fully active"_s };
    
    // FIXME: Figure out where latencyHint should go.

    if (contextOptions.sampleRate.hasValue() && !isSupportedSampleRate(contextOptions.sampleRate.value()))
        return Exception { SyntaxError, "sampleRate is not in range"_s };
    
    auto audioContext = adoptRef(*new AudioContext(document, contextOptions));
    audioContext->suspendIfNeeded();
    return audioContext;
}

// Constructor for rendering to the audio hardware.
AudioContext::AudioContext(Document& document, const AudioContextOptions& contextOptions)
    : BaseAudioContext(document, contextOptions)
{
}

// Constructor for offline (non-realtime) rendering.
AudioContext::AudioContext(Document& document, AudioBuffer* renderTarget)
    : BaseAudioContext(document, renderTarget)
{
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
