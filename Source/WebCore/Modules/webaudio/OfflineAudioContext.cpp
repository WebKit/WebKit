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
#include "Document.h"
#include "JSAudioBuffer.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OfflineAudioContext);

inline OfflineAudioContext::OfflineAudioContext(Document& document, AudioBuffer* renderTarget)
    : BaseAudioContext(document, renderTarget)
{
}

ExceptionOr<Ref<OfflineAudioContext>> OfflineAudioContext::create(ScriptExecutionContext& context, unsigned numberOfChannels, size_t length, float sampleRate)
{
    // FIXME: Add support for workers.
    if (!is<Document>(context))
        return Exception { NotSupportedError };
    if (!numberOfChannels || numberOfChannels > 10)
        return Exception { SyntaxError, "Number of channels is not in range"_s };
    if (!length)
        return Exception { SyntaxError, "length cannot be 0"_s };
    if (!isSupportedSampleRate(sampleRate))
        return Exception { SyntaxError, "sampleRate is not in range"_s };
    auto renderTarget = AudioBuffer::create(numberOfChannels, length, sampleRate);
    if (!renderTarget)
        return Exception { SyntaxError, "Unable to create AudioBuffer"_s };

    auto audioContext = adoptRef(*new OfflineAudioContext(downcast<Document>(context), renderTarget.get()));
    audioContext->suspendIfNeeded();
    return audioContext;
}

ExceptionOr<Ref<OfflineAudioContext>> OfflineAudioContext::create(ScriptExecutionContext& context, const OfflineAudioContextOptions& contextOptions)
{
    return create(context, contextOptions.numberOfChannels, contextOptions.length, contextOptions.sampleRate);
}

void OfflineAudioContext::startOfflineRendering(Ref<DeferredPromise>&& promise)
{
    if (isStopped() || !willBeginPlayback()) {
        promise->reject(Exception { InvalidStateError });
        return;
    }

    lazyInitialize();

    auto result = destination()->startRendering();
    if (result.hasException()) {
        promise->reject(result.releaseException());
        return;
    }

    makePendingActivity();
    m_pendingOfflineRenderingPromise = WTFMove(promise);
    setState(State::Running);
}

void OfflineAudioContext::didFinishOfflineRendering(ExceptionOr<Ref<AudioBuffer>>&& result)
{
    if (!m_pendingOfflineRenderingPromise)
        return;

    auto promise = std::exchange(m_pendingOfflineRenderingPromise, nullptr);
    if (result.hasException()) {
        promise->reject(result.releaseException());
        return;
    }
    promise->resolve<IDLInterface<AudioBuffer>>(result.releaseReturnValue());
}

unsigned OfflineAudioContext::length() const
{
    return renderTarget()->length();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
