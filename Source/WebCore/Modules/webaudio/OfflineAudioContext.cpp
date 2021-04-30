/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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
#include "AudioUtilities.h"
#include "Document.h"
#include "JSAudioBuffer.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Scope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OfflineAudioContext);

inline OfflineAudioContext::OfflineAudioContext(Document& document, unsigned numberOfChannels, unsigned length, float sampleRate, RefPtr<AudioBuffer>&& renderTarget)
    : BaseAudioContext(document, IsLegacyWebKitAudioContext::No, numberOfChannels, sampleRate, WTFMove(renderTarget))
    , m_length(length)
{
}

ExceptionOr<Ref<OfflineAudioContext>> OfflineAudioContext::create(ScriptExecutionContext& context, unsigned numberOfChannels, unsigned length, float sampleRate)
{
    if (!is<Document>(context))
        return Exception { NotSupportedError, "OfflineAudioContext is only supported in Document contexts"_s };
    if (!numberOfChannels || numberOfChannels > maxNumberOfChannels())
        return Exception { SyntaxError, "Number of channels is not in range"_s };
    if (!length)
        return Exception { SyntaxError, "length cannot be 0"_s };
    if (!isSupportedSampleRate(sampleRate))
        return Exception { SyntaxError, "sampleRate is not in range"_s };

    auto renderTarget = AudioBuffer::create(numberOfChannels, length, sampleRate);
    if (!renderTarget)
        context.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("Failed to construct internal AudioBuffer with ", numberOfChannels, " channel(s), a sample rate of ", sampleRate, " and a length of ", length, "."));

    auto audioContext = adoptRef(*new OfflineAudioContext(downcast<Document>(context), numberOfChannels, length, sampleRate, WTFMove(renderTarget)));
    audioContext->suspendIfNeeded();
    return audioContext;
}

ExceptionOr<Ref<OfflineAudioContext>> OfflineAudioContext::create(ScriptExecutionContext& context, const OfflineAudioContextOptions& contextOptions)
{
    return create(context, contextOptions.numberOfChannels, contextOptions.length, contextOptions.sampleRate);
}

void OfflineAudioContext::uninitialize()
{
    BaseAudioContext::uninitialize();

    if (auto promise = std::exchange(m_pendingOfflineRenderingPromise, nullptr))
        promise->reject(Exception { InvalidStateError, "Context is going away"_s });
}

void OfflineAudioContext::startOfflineRendering(Ref<DeferredPromise>&& promise)
{
    if (isStopped()) {
        promise->reject(Exception { InvalidStateError, "Context is stopped"_s });
        return;
    }

    if (m_didStartOfflineRendering) {
        promise->reject(Exception { InvalidStateError, "Rendering was already started"_s });
        return;
    }

    if (!renderTarget()) {
        promise->reject(Exception { NotSupportedError, "Failed to create audio buffer"_s });
        return;
    }

    lazyInitialize();

    destination().startRendering([this, promise = WTFMove(promise), pendingActivity = makePendingActivity(*this)](Optional<Exception>&& exception) mutable {
        if (exception) {
            promise->reject(WTFMove(*exception));
            return;
        }

        setPendingActivity();
        m_pendingOfflineRenderingPromise = WTFMove(promise);
        m_didStartOfflineRendering = true;
        setState(State::Running);
    });
}

void OfflineAudioContext::suspendOfflineRendering(double suspendTime, Ref<DeferredPromise>&& promise)
{
    if (isStopped()) {
        promise->reject(Exception { InvalidStateError, "Context is stopped"_s });
        return;
    }

    if (suspendTime < 0) {
        promise->reject(Exception { InvalidStateError, "suspendTime cannot be negative"_s });
        return;
    }

    double totalRenderDuration = length() / sampleRate();
    if (totalRenderDuration <= suspendTime) {
        promise->reject(Exception { InvalidStateError, "suspendTime cannot be greater than total rendering duration"_s });
        return;
    }

    size_t frame = AudioUtilities::timeToSampleFrame(suspendTime, sampleRate());
    frame = AudioUtilities::renderQuantumSize * ((frame + AudioUtilities::renderQuantumSize - 1) / AudioUtilities::renderQuantumSize);
    if (frame < currentSampleFrame()) {
        promise->reject(Exception { InvalidStateError, "Suspension frame is earlier than current frame"_s });
        return;
    }

    AutoLocker locker(*this);
    auto addResult = m_suspendRequests.add(frame, promise.ptr());
    if (!addResult.isNewEntry) {
        promise->reject(Exception { InvalidStateError, "There is already a pending suspend request at this frame"_s });
        return;
    }
}

void OfflineAudioContext::resumeOfflineRendering(Ref<DeferredPromise>&& promise)
{
    if (!m_didStartOfflineRendering) {
        promise->reject(Exception { InvalidStateError, "Cannot resume an offline audio context that has not started"_s });
        return;
    }
    if (isClosed()) {
        promise->reject(Exception { InvalidStateError, "Cannot resume an offline audio context that is closed"_s });
        return;
    }
    if (state() == AudioContextState::Running) {
        promise->resolve();
        return;
    }
    ASSERT(state() == AudioContextState::Suspended);

    destination().startRendering([this, promise = WTFMove(promise), pendingActivity = makePendingActivity(*this)](Optional<Exception>&& exception) mutable {
        if (exception) {
            promise->reject(WTFMove(*exception));
            return;
        }

        setPendingActivity();
        setState(State::Running);
        promise->resolve();
    });
}

bool OfflineAudioContext::shouldSuspend()
{
    ASSERT(!isMainThread());
    // Note that OfflineAutoLocker uses lock() instead of tryLock(). We usually avoid blocking the AudioThread
    // on lock() but we don't have a choice here since the suspension need to be exact.
    OfflineAutoLocker locker(*this);
    return m_suspendRequests.contains(currentSampleFrame());
}

void OfflineAudioContext::didSuspendRendering(size_t frame)
{
    BaseAudioContext::didSuspendRendering(frame);

    clearPendingActivity();

    RefPtr<DeferredPromise> promise;
    {
        AutoLocker locker(*this);
        promise = m_suspendRequests.take(frame);
    }
    ASSERT(promise);
    if (promise)
        promise->resolve();
}

void OfflineAudioContext::didFinishOfflineRendering(ExceptionOr<Ref<AudioBuffer>>&& result)
{
    auto finishedRenderingScope = WTF::makeScopeExit([this] {
        uninitialize();
        clear();
    });

    if (!m_pendingOfflineRenderingPromise)
        return;

    auto promise = std::exchange(m_pendingOfflineRenderingPromise, nullptr);
    if (result.hasException()) {
        promise->reject(result.releaseException());
        return;
    }
    promise->resolve<IDLInterface<AudioBuffer>>(result.releaseReturnValue());
}

void OfflineAudioContext::offlineLock(bool& mustReleaseLock)
{
    ASSERT(!isMainThread());
    lockInternal(mustReleaseLock);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
