/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 * Copyright (C) 2020-2021, Apple Inc. All rights reserved.
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

#include "OfflineAudioContext.h"

#include "AudioBuffer.h"
#include "AudioNodeOutput.h"
#include "AudioUtilities.h"
#include "Document.h"
#include "JSAudioBuffer.h"
#include "JSDOMPromiseDeferred.h"
#include "OfflineAudioCompletionEvent.h"
#include "OfflineAudioContextOptions.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Scope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OfflineAudioContext);

OfflineAudioContext::OfflineAudioContext(Document& document, const OfflineAudioContextOptions& options)
    : BaseAudioContext(document)
    , m_destinationNode(makeUniqueRef<OfflineAudioDestinationNode>(*this, options.numberOfChannels, options.sampleRate, AudioBuffer::create(options.numberOfChannels, options.length, options.sampleRate)))
    , m_length(options.length)
{
    if (!renderTarget())
        document.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("Failed to construct internal AudioBuffer with ", options.numberOfChannels, " channel(s), a sample rate of ", options.sampleRate, " and a length of ", options.length, "."));
    else if (noiseInjectionPolicy() == NoiseInjectionPolicy::Minimal)
        renderTarget()->increaseNoiseInjectionMultiplier();
}

ExceptionOr<Ref<OfflineAudioContext>> OfflineAudioContext::create(ScriptExecutionContext& context, const OfflineAudioContextOptions& options)
{
    auto* document = dynamicDowncast<Document>(context);
    if (!document)
        return Exception { ExceptionCode::NotSupportedError, "OfflineAudioContext is only supported in Document contexts"_s };
    if (!options.numberOfChannels || options.numberOfChannels > maxNumberOfChannels)
        return Exception { ExceptionCode::NotSupportedError, "Number of channels is not in range"_s };
    if (!options.length)
        return Exception { ExceptionCode::NotSupportedError, "length cannot be 0"_s };
    if (!isSupportedSampleRate(options.sampleRate))
        return Exception { ExceptionCode::NotSupportedError, "sampleRate is not in range"_s };

    Ref audioContext = adoptRef(*new OfflineAudioContext(*document, options));
    audioContext->suspendIfNeeded();
    return audioContext;
}

ExceptionOr<Ref<OfflineAudioContext>> OfflineAudioContext::create(ScriptExecutionContext& context, unsigned numberOfChannels, unsigned length, float sampleRate)
{
    return create(context, { numberOfChannels, length, sampleRate });
}

void OfflineAudioContext::lazyInitialize()
{
    BaseAudioContext::lazyInitialize();

    increaseNoiseMultiplierIfNeeded();
}

void OfflineAudioContext::increaseNoiseMultiplierIfNeeded()
{
    if (noiseInjectionPolicy() == NoiseInjectionPolicy::None)
        return;

    Locker locker { graphLock() };

    RefPtr target = renderTarget();
    if (!target)
        return;

    Vector<AudioConnectionRefPtr<AudioNode>, 1> remainingNodes;
    for (auto& node : referencedSourceNodes())
        remainingNodes.append(node.copyRef());

    while (!remainingNodes.isEmpty()) {
        auto node = remainingNodes.takeLast();
        target->increaseNoiseInjectionMultiplier(node->noiseInjectionMultiplier());
        for (unsigned i = 0; i < node->numberOfOutputs(); ++i) {
            auto* output = node->output(i);
            if (!output)
                continue;

            output->forEachInputNode([&](auto& inputNode) {
                remainingNodes.append(&inputNode);
            });
        }
    }
}

void OfflineAudioContext::uninitialize()
{
    if (!isInitialized())
        return;

    BaseAudioContext::uninitialize();

    if (auto promise = std::exchange(m_pendingRenderingPromise, nullptr); promise && !isContextStopped())
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is going away"_s });
}

const char* OfflineAudioContext::activeDOMObjectName() const
{
    return "OfflineAudioContext";
}

void OfflineAudioContext::startRendering(Ref<DeferredPromise>&& promise)
{
    if (isStopped()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is stopped"_s });
        return;
    }

    if (m_didStartRendering) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Rendering was already started"_s });
        return;
    }

    if (!renderTarget()) {
        promise->reject(Exception { ExceptionCode::NotSupportedError, "Failed to create audio buffer"_s });
        return;
    }

    lazyInitialize();

    destination().startRendering([this, promise = WTFMove(promise), pendingActivity = makePendingActivity(*this)](std::optional<Exception>&& exception) mutable {
        if (exception) {
            promise->reject(WTFMove(*exception));
            return;
        }

        m_pendingRenderingPromise = WTFMove(promise);
        m_didStartRendering = true;
        setState(State::Running);
    });
}

void OfflineAudioContext::suspendRendering(double suspendTime, Ref<DeferredPromise>&& promise)
{
    if (isStopped()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is stopped"_s });
        return;
    }

    if (suspendTime < 0) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "suspendTime cannot be negative"_s });
        return;
    }

    double totalRenderDuration = length() / sampleRate();
    if (totalRenderDuration <= suspendTime) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "suspendTime cannot be greater than total rendering duration"_s });
        return;
    }

    size_t frame = AudioUtilities::timeToSampleFrame(suspendTime, sampleRate());
    frame = AudioUtilities::renderQuantumSize * ((frame + AudioUtilities::renderQuantumSize - 1) / AudioUtilities::renderQuantumSize);
    if (frame < currentSampleFrame()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Suspension frame is earlier than current frame"_s });
        return;
    }

    Locker locker { graphLock() };
    auto addResult = m_suspendRequests.add(frame, promise.ptr());
    if (!addResult.isNewEntry) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "There is already a pending suspend request at this frame"_s });
        return;
    }
}

void OfflineAudioContext::resumeRendering(Ref<DeferredPromise>&& promise)
{
    if (!m_didStartRendering) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Cannot resume an offline audio context that has not started"_s });
        return;
    }
    if (isClosed()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Cannot resume an offline audio context that is closed"_s });
        return;
    }
    if (state() == AudioContextState::Running) {
        promise->resolve();
        return;
    }
    ASSERT(state() == AudioContextState::Suspended);

    destination().startRendering([this, promise = WTFMove(promise), pendingActivity = makePendingActivity(*this)](std::optional<Exception>&& exception) mutable {
        if (exception) {
            promise->reject(WTFMove(*exception));
            return;
        }

        setState(State::Running);
        promise->resolve();
    });
}

bool OfflineAudioContext::shouldSuspend()
{
    ASSERT(!isMainThread());
    // Note that we are not using a tryLock() here. We usually avoid blocking the AudioThread
    // on lock() but we don't have a choice here since the suspension need to be exact.
    // Also, this not a real-time AudioContext so blocking the AudioThread is not as harmful.
    Locker locker { graphLock() };
    return m_suspendRequests.contains(currentSampleFrame());
}

void OfflineAudioContext::didSuspendRendering(size_t frame)
{
    setState(State::Suspended);

    RefPtr<DeferredPromise> promise;
    {
        Locker locker { graphLock() };
        promise = m_suspendRequests.take(frame);
    }
    ASSERT(promise);
    if (promise)
        promise->resolve();
}

void OfflineAudioContext::finishedRendering(bool didRendering)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    auto uninitializeOnExit = makeScopeExit([this] {
        uninitialize();
        clear();
    });

    // Make sure our JSwrapper stays alive long enough to resolve the promise and queue the completion event.
    // Otherwise, setting the state to Closed may cause our JS wrapper to get collected early.
    auto protectedJSWrapper = makePendingActivity(*this);
    setState(State::Closed);

    // Avoid firing the event if the document has already gone away.
    if (isStopped())
        return;

    RefPtr<AudioBuffer> renderedBuffer = renderTarget();
    ASSERT(renderedBuffer);

    if (didRendering) {
        queueTaskToDispatchEvent(*this, TaskSource::MediaElement, OfflineAudioCompletionEvent::create(*renderedBuffer));
        settleRenderingPromise(renderedBuffer.releaseNonNull());
    } else
        settleRenderingPromise(Exception { ExceptionCode::InvalidStateError, "Offline rendering failed"_s });
}

void OfflineAudioContext::settleRenderingPromise(ExceptionOr<Ref<AudioBuffer>>&& result)
{
    auto promise = std::exchange(m_pendingRenderingPromise, nullptr);
    if (!promise)
        return;

    if (result.hasException()) {
        promise->reject(result.releaseException());
        return;
    }
    promise->resolve<IDLInterface<AudioBuffer>>(result.releaseReturnValue());
}

bool OfflineAudioContext::virtualHasPendingActivity() const
{
    return state() == State::Running;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
