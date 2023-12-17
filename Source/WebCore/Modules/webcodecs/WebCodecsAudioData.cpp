/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCodecsAudioData.h"

#if ENABLE(WEB_CODECS)

#include "ContextDestructionObserverInlines.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "PlatformRawAudioData.h"
#include "WebCodecsAudioDataAlgorithms.h"

namespace WebCore {

// https://www.w3.org/TR/webcodecs/#dom-audiodata-audiodata
ExceptionOr<Ref<WebCodecsAudioData>> WebCodecsAudioData::create(ScriptExecutionContext& context, Init&& init)
{
    if (!isValidAudioDataInit(init))
        return Exception { ExceptionCode::TypeError, "Invalid init data"_s };

    auto rawData = init.data.span();
    auto data = PlatformRawAudioData::create(WTFMove(rawData), init.format, init.sampleRate, init.timestamp, init.numberOfFrames, init.numberOfChannels);

    if (!data)
        return Exception { ExceptionCode::NotSupportedError, "AudioData creation failed"_s };

    return adoptRef(*new WebCodecsAudioData(context, WebCodecsAudioInternalData { WTFMove(data) }));
}

Ref<WebCodecsAudioData> WebCodecsAudioData::create(ScriptExecutionContext& context, Ref<PlatformRawAudioData>&& data)
{
    return adoptRef(*new WebCodecsAudioData(context, WebCodecsAudioInternalData { WTFMove(data) }));
}

WebCodecsAudioData::WebCodecsAudioData(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
{
}

WebCodecsAudioData::WebCodecsAudioData(ScriptExecutionContext& context, WebCodecsAudioInternalData&& data)
    : ContextDestructionObserver(&context)
    , m_data(WTFMove(data))
{
}

WebCodecsAudioData::~WebCodecsAudioData()
{
    if (m_isDetached)
        return;
    if (auto* context = scriptExecutionContext()) {
        context->postTask([](auto& context) {
            context.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "A AudioData was destroyed without having been closed explicitly"_s);
        });
    }
}

std::optional<AudioSampleFormat> WebCodecsAudioData::format() const
{
    return m_data.audioData ? std::make_optional(m_data.audioData->format()) : std::nullopt;
}

float WebCodecsAudioData::sampleRate() const
{
    return m_data.audioData ? m_data.audioData->sampleRate() : 0;
}

size_t WebCodecsAudioData::numberOfFrames() const
{
    return m_data.audioData ? m_data.audioData->numberOfFrames() : 0;
}

size_t WebCodecsAudioData::numberOfChannels() const
{
    return m_data.audioData ? m_data.audioData->numberOfChannels() : 0;
}

std::optional<uint64_t> WebCodecsAudioData::duration()
{
    return m_data.audioData ? m_data.audioData->duration() : std::nullopt;
}

int64_t WebCodecsAudioData::timestamp() const
{
    return m_data.audioData ? m_data.audioData->timestamp() : 0;
}

// https://www.w3.org/TR/webcodecs/#dom-audiodata-allocationsize
ExceptionOr<size_t> WebCodecsAudioData::allocationSize(const CopyToOptions& options)
{
    if (isDetached())
        return Exception { ExceptionCode::InvalidStateError, "AudioData is detached"_s };

    auto copyElementCount = computeCopyElementCount(*this, options);
    if (copyElementCount.hasException())
        return copyElementCount.releaseException();

    auto destFormat = options.format.value_or(*format());
    auto bytesPerSample = computeBytesPerSample(destFormat);
    return copyElementCount.releaseReturnValue() * bytesPerSample;
}

// https://www.w3.org/TR/webcodecs/#dom-audiodata-copyto
ExceptionOr<void> WebCodecsAudioData::copyTo(BufferSource&& source, CopyToOptions&& options)
{
    if (isDetached())
        return Exception { ExceptionCode::InvalidStateError, "AudioData is detached"_s };

    auto copyElementCount = computeCopyElementCount(*this, options);
    if (copyElementCount.hasException())
        return copyElementCount.releaseException();

    auto destFormat = options.format.value_or(*format());
    auto bytesPerSample = computeBytesPerSample(destFormat);
    auto maxCopyElementCount = copyElementCount.releaseReturnValue();
    unsigned long allocationSize;
    if (!WTF::safeMultiply(maxCopyElementCount, bytesPerSample, allocationSize))
        return Exception { ExceptionCode::RangeError, "Calculated destination buffer size overflows"_s };

    if (allocationSize > source.length())
        return Exception { ExceptionCode::RangeError, "Buffer is too small"_s };

    std::span<uint8_t> buffer { static_cast<uint8_t*>(source.mutableData()), source.length() };
    m_data.audioData->copyTo(buffer, destFormat, options.planeIndex, options.frameOffset, options.frameCount, maxCopyElementCount);
    return { };
}

// https://www.w3.org/TR/webcodecs/#dom-audiodata-clone
ExceptionOr<Ref<WebCodecsAudioData>> WebCodecsAudioData::clone(ScriptExecutionContext& context)
{
    if (isDetached())
        return Exception { ExceptionCode::InvalidStateError,  "AudioData is detached"_s };

    return adoptRef(*new WebCodecsAudioData(context, WebCodecsAudioInternalData { m_data }));
}

// https://www.w3.org/TR/webcodecs/#dom-audiodata-close
void WebCodecsAudioData::close()
{
    m_data.audioData = nullptr;

    m_isDetached = true;
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
