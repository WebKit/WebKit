/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "AsyncAudioDecoder.h"

#include "AudioBuffer.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/TypedArrayAdaptors.h>
#include <wtf/NativePromise.h>

namespace WebCore {

AsyncAudioDecoder::AsyncAudioDecoder()
    : m_runLoop(RunLoop::create("Audio Decoder", ThreadType::Audio))
{
}

Ref<DecodingTaskPromise> AsyncAudioDecoder::decodeAsync(Ref<ArrayBuffer>&& audioData, float sampleRate)
{
    return WTF::invokeAsync(m_runLoop, [audioData = WTFMove(audioData), sampleRate] () mutable {
        auto audioBuffer = AudioBuffer::createFromAudioFileData(audioData->data(), audioData->byteLength(), false, sampleRate);
        // The ArrayBuffer must be deleted on the main thread, send it back there to be derefed.
        callOnMainThread([audioData = WTFMove(audioData)] { });
        if (!audioBuffer)
            return DecodingTaskPromise::createAndReject(Exception { ExceptionCode::EncodingError, "Decoding failed"_s });
        return DecodingTaskPromise::createAndResolve(audioBuffer.releaseNonNull());
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
