/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "JSDOMConvertInterface.h"
#include "JSDOMPromiseDeferred.h"
#include "OfflineAudioCompletionEvent.h"
#include "OfflineAudioContextOptions.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OfflineAudioContext);

OfflineAudioContext::~OfflineAudioContext() = default;

OfflineAudioContext::OfflineAudioContext(Document& document, const OfflineAudioContextOptions& options)
    : BaseAudioContext(document)
    , m_destinationNode(makeUniqueRefWithoutRefCountedCheck<OfflineAudioDestinationNode>(*this, options.numberOfChannels, options.sampleRate))
    , m_length(options.length)
{
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

std::optional<size_t> OfflineAudioContext::renderLengthForChunk(std::optional<unsigned> chunkSize) const
{
    ASSERT(m_committedFrames < m_length);
    size_t remaining = static_cast<size_t>(m_length) - m_committedFrames;
    if (!chunkSize)
        return remaining;

    CheckedUint64 bufferLength { *chunkSize };
    uint64_t remainder = static_cast<uint64_t>(*chunkSize) % AudioUtilities::renderQuantumSize;
    if (remainder) {
        bufferLength += AudioUtilities::renderQuantumSize - remainder;
        if (bufferLength.hasOverflowed())
            return std::nullopt;
    }
    if (bufferLength.value() > remaining)
        return remaining;
    return static_cast<size_t>(bufferLength.value());
}

void OfflineAudioContext::stampNoise(AudioBuffer& buffer)
{
    if (!noiseInjectionPolicies())
        return;

    if (noiseInjectionPolicies().contains(NoiseInjectionPolicy::Minimal))
        buffer.increaseNoiseInjectionMultiplier();

    float multiplier = 0;
    {
        Locker locker { graphLock() };

        // The graph may contain feedback cycles (which are legal in Web Audio as long as a
        // DelayNode breaks the loop), and non-tree topologies may reach the same node via
        // multiple paths. Track visited nodes so we neither loop forever nor double-count a
        // node's noise multiplier.
        HashSet<CheckedPtr<AudioNode>> visited;
        Vector<AudioConnectionRef<AudioNode>, 1> remainingNodes;
        for (auto& node : referencedSourceNodes()) {
            if (visited.add(node.ptr()).isNewEntry)
                remainingNodes.append(node.copyRef());
        }

        while (!remainingNodes.isEmpty()) {
            auto node = remainingNodes.takeLast();
            multiplier += node->noiseInjectionMultiplier();
            for (unsigned i = 0; i < node->numberOfOutputs(); ++i) {
                CheckedPtr output = node->output(i);
                if (!output)
                    continue;

                output->forEachInputNode([&](auto& inputNode) {
                    if (visited.add(&inputNode).isNewEntry)
                        remainingNodes.append(inputNode);
                });
            }
        }
    }

    if (multiplier)
        buffer.increaseNoiseInjectionMultiplier(multiplier);
}

void OfflineAudioContext::uninitialize()
{
    destination().requestStop();

    if (!isInitialized())
        return;

    BaseAudioContext::uninitialize();

    if (!isContextStopped())
        rejectPendingRenders(ExceptionCode::InvalidStateError, "Context is going away"_s);
}

void OfflineAudioContext::startRendering(std::optional<unsigned> chunkSize, Ref<DeferredPromise>&& promise)
{
    if (isStopped()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is stopped"_s });
        return;
    }

    RefPtr document = this->document();
    if (!document || !document->isFullyActive()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Document is not fully active"_s });
        return;
    }

    if (m_closeRequested) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is closed"_s });
        return;
    }

    if (m_committedFrames >= m_length) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Rendering was already started"_s });
        return;
    }

    if (isClosed()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is closed"_s });
        return;
    }

    if (chunkSize && !*chunkSize) {
        promise->reject(Exception { ExceptionCode::NotSupportedError, "chunkSize cannot be 0"_s });
        return;
    }

    auto bufferLength = renderLengthForChunk(chunkSize);
    if (!bufferLength) {
        promise->reject(Exception { ExceptionCode::NotSupportedError, "The requested chunk size is too large."_s });
        return;
    }

    lazyInitialize();

    RefPtr<AudioBuffer> buffer = AudioBuffer::create(destination().channelCount(), *bufferLength, sampleRate());
    if (!buffer) {
        promise->reject(Exception { ExceptionCode::NotSupportedError, "Failed to create audio buffer"_s });
        return;
    }
    stampNoise(*buffer);

    m_committedFrames += buffer->length();
    m_pendingRenders.append(PendingRender { WTF::move(promise), buffer.releaseNonNull() });
    if (!m_chunkInProgress)
        renderFront();
}

void OfflineAudioContext::renderFront()
{
    ASSERT(!m_pendingRenders.isEmpty());
    m_chunkInProgress = true;
    destination().prepareRender(m_pendingRenders.first().buffer.copyRef());
    protect(destination())->startRendering([pendingActivity = makePendingActivity(*this)](std::optional<Exception>&& exception) mutable {
        if (exception) {
            pendingActivity->object().didFailRender();
            return;
        }

        pendingActivity->object().m_didStartRendering = true;
        pendingActivity->object().setIsRendering(true);
        pendingActivity->object().setState(State::Running);
    });
}

void OfflineAudioContext::close(Ref<DeferredPromise>&& promise)
{
    if (isStopped()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is stopped"_s });
        return;
    }

    RefPtr document = this->document();
    if (!document || !document->isFullyActive()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Document is not fully active"_s });
        return;
    }

    if (m_closeRequested || isClosed()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is closed"_s });
        return;
    }

    m_closeRequested = true;
    m_closePromise = WTF::move(promise);
    if (destination().isRendering()) {
        destination().requestStop();
        return;
    }

    finishClose();
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

    if (isClosed() || m_closeRequested) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Context is closed"_s });
        return;
    }

    Locker locker { graphLock() };
    auto addResult = m_suspendRequests.add(frame, promise);
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
    if (isClosed() || m_closeRequested) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Cannot resume an offline audio context that is closed"_s });
        return;
    }
    if (state() == AudioContextState::Running) {
        promise->resolve();
        return;
    }
    ASSERT(state() == AudioContextState::Suspended);

    protect(destination())->startRendering([promise = WTF::move(promise), pendingActivity = makePendingActivity(*this)](std::optional<Exception>&& exception) mutable {
        if (exception) {
            promise->reject(WTF::move(*exception));
            return;
        }

        pendingActivity->object().setIsRendering(true);
        pendingActivity->object().setState(State::Running);
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

void OfflineAudioContext::didRenderChunk()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_pendingRenders.isEmpty() || m_didFinishClose)
        return;

    // virtualHasPendingActivity() is false once the render thread has stopped.
    auto pendingActivity = makePendingActivity(*this);

    auto pending = m_pendingRenders.takeFirst();
    Ref promise = WTF::move(pending.promise);
    Ref renderedBuffer = WTF::move(pending.buffer);
    promise->resolve<IDLInterface<AudioBuffer>>(renderedBuffer);

    if (m_closeRequested || m_didFinishClose)
        return;

    if (!m_pendingRenders.isEmpty()) {
        renderFront();
        return;
    }

    m_chunkInProgress = false;
    if (m_committedFrames >= m_length)
        completeRendering(WTF::move(renderedBuffer));
}

void OfflineAudioContext::didFailRender()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_didFinishClose)
        return;

    m_didFinishClose = true;
    m_chunkInProgress = false;
    rejectPendingRenders(ExceptionCode::InvalidStateError, "Offline rendering failed"_s);
    rejectPendingSuspends();

    // Setting the state to Closed may cause our JS wrapper to get collected early.
    auto pendingActivity = makePendingActivity(*this);
    setState(State::Closed);
    uninitialize();
    clear();
}

void OfflineAudioContext::didStopRender()
{
    ASSERT(isMainThread());
    finishClose();
}

void OfflineAudioContext::completeRendering(Ref<AudioBuffer>&& renderedBuffer)
{
    ASSERT(isMainThread());
    if (m_didFinishClose || m_closeRequested)
        return;

    m_didFinishClose = true;
    m_chunkInProgress = false;

    // Setting the state to Closed may cause our JS wrapper to get collected early.
    auto pendingActivity = makePendingActivity(*this);
    setState(State::Closed);

    if (!isStopped())
        queueTaskToDispatchEvent(*this, TaskSource::MediaElement, OfflineAudioCompletionEvent::create(WTF::move(renderedBuffer)));

    uninitialize();
    clear();
}

void OfflineAudioContext::finishClose()
{
    if (m_didFinishClose)
        return;

    m_didFinishClose = true;
    m_chunkInProgress = false;
    rejectPendingRenders(ExceptionCode::AbortError, "Audio context is going away"_s);
    rejectPendingSuspends();
    uninitialize();
    clear();
    setState(State::Closed);

    if (auto closePromise = std::exchange(m_closePromise, nullptr))
        closePromise->resolve();
}

void OfflineAudioContext::rejectPendingRenders(ExceptionCode code, const String& message)
{
    auto pending = std::exchange(m_pendingRenders, Deque<PendingRender, 0> { });
    while (!pending.isEmpty()) {
        auto entry = pending.takeFirst();
        Ref promise = WTF::move(entry.promise);
        promise->reject(Exception { code, message });
    }
}

void OfflineAudioContext::rejectPendingSuspends()
{
    Vector<RefPtr<DeferredPromise>> promises;
    {
        Locker locker { graphLock() };
        while (!m_suspendRequests.isEmpty())
            promises.append(m_suspendRequests.take(m_suspendRequests.begin()->key));
    }

    while (!promises.isEmpty()) {
        if (RefPtr promise = promises.takeLast())
            promise->reject(Exception { ExceptionCode::InvalidStateError, "Audio context is going away"_s });
    }
}

bool OfflineAudioContext::virtualHasPendingActivity() const
{
    return state() == State::Running && m_isRendering;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
