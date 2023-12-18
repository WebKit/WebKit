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
#include "WebCodecsAudioDataAlgorithms.h"

#if ENABLE(WEB_CODECS)

#include "ExceptionCode.h"
#include "ExceptionOr.h"

namespace WebCore {

// https://www.w3.org/TR/webcodecs/#valid-audiodatainit
bool isValidAudioDataInit(const WebCodecsAudioData::Init& init)
{
    if (init.sampleRate <= 0)
        return false;

    if (!init.numberOfFrames)
        return false;

    if (!init.numberOfChannels)
        return false;

    size_t totalSamples, totalSize;
    if (!WTF::safeMultiply(init.numberOfFrames, init.numberOfChannels, totalSamples))
        return false;

    auto bytesPerSample = computeBytesPerSample(init.format);
    if (!WTF::safeMultiply(totalSamples, bytesPerSample, totalSize))
        return false;

    auto dataSize = init.data.span().size();
    return dataSize >= totalSize;
}

bool isAudioSampleFormatInterleaved(const AudioSampleFormat& format)
{
    switch (format) {
    case AudioSampleFormat::U8:
    case AudioSampleFormat::S16:
    case AudioSampleFormat::S32:
    case AudioSampleFormat::F32:
        return true;
    case AudioSampleFormat::U8Planar:
    case AudioSampleFormat::S16Planar:
    case AudioSampleFormat::S32Planar:
    case AudioSampleFormat::F32Planar:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

size_t computeBytesPerSample(const AudioSampleFormat& format)
{
    switch (format) {
    case AudioSampleFormat::U8:
    case AudioSampleFormat::U8Planar:
        return 1;
    case AudioSampleFormat::S16:
    case AudioSampleFormat::S16Planar:
        return 2;
    case AudioSampleFormat::S32:
    case AudioSampleFormat::F32:
    case AudioSampleFormat::S32Planar:
    case AudioSampleFormat::F32Planar:
        return 4;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

// https://www.w3.org/TR/webcodecs/#compute-copy-element-count
ExceptionOr<size_t> computeCopyElementCount(const WebCodecsAudioData& data, const WebCodecsAudioData::CopyToOptions& options)
{
    auto platformData = data.data().audioData;
    if (!platformData)
        return Exception { ExceptionCode::InvalidAccessError, "Internal AudioData storage is null"_s };

    auto destFormat = options.format.value_or(platformData->format());

    auto isInterleaved = isAudioSampleFormatInterleaved(destFormat);
    if (isInterleaved && options.planeIndex > 0)
        return Exception { ExceptionCode::RangeError, "Invalid planeIndex for interleaved format"_s };
    if (options.planeIndex >= data.numberOfChannels())
        return Exception { ExceptionCode::RangeError, "Invalid planeIndex for planar format"_s };

    if (options.format && *options.format != destFormat && destFormat != AudioSampleFormat::F32Planar)
        return Exception { ExceptionCode::NotSupportedError, "AudioData currently only supports copy conversion to f32-planar"_s };

    auto frameCount = data.numberOfFrames() / data.numberOfChannels();
    if (options.frameOffset && *options.frameOffset > frameCount)
        return Exception { ExceptionCode::RangeError, "frameOffset is too large"_s };

    auto copyFrameCount = frameCount;
    if (options.frameOffset)
        copyFrameCount -= *options.frameOffset;
    if (options.frameCount) {
        if (*options.frameCount > copyFrameCount)
            return Exception { ExceptionCode::RangeError, "frameCount is too large"_s };

        copyFrameCount = *options.frameCount;
    }

    if (isInterleaved) {
        size_t aggregatedFrameCount;
        if (!WTF::safeMultiply(copyFrameCount, data.numberOfChannels(), aggregatedFrameCount))
            return Exception { ExceptionCode::RangeError, "Provided options are causing an overflow"_s };

        return aggregatedFrameCount;
    }

    return copyFrameCount;
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
