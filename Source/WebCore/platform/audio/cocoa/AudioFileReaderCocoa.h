/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_AUDIO)

#include <CoreAudio/CoreAudioTypes.h>
#include <optional>
#include <wtf/LoggerHelper.h>
#include <wtf/RefPtr.h>

typedef struct opaqueCMSampleBuffer* CMSampleBufferRef;

namespace WebCore {

class AudioBus;
class SourceBufferParserWebM;
struct AudioFileReaderData;
class AudioFileReaderAVFData;

// Wrapper class for reading audio files using AVAssetReader and in-memory versions of them...

class AudioFileReader
#if !RELEASE_LOG_DISABLED
    : public LoggerHelper
#endif
{
public:
    explicit AudioFileReader(std::span<const uint8_t> data);
    ~AudioFileReader();

    static bool isAvailable();

    RefPtr<AudioBus> createBus(float sampleRate, bool mixToMono); // Returns nullptr on error

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    ASCIILiteral logClassName() const final { return "AudioFileReaderCocoa"_s; }
#endif

private:

#if ENABLE(MEDIA_SOURCE)
    bool NODELETE isMaybeWebM(std::span<const uint8_t>) const;
    std::unique_ptr<AudioFileReaderData> demuxWebMData(std::span<const uint8_t>) const;
#endif
    std::optional<size_t> decodeData(AudioBufferList&, size_t numberOfFrames, const AudioStreamBasicDescription& inFormat, const AudioStreamBasicDescription& outFormat) const;
    std::unique_ptr<AudioFileReaderData> demuxAVFData(std::span<const uint8_t>) const;
    std::optional<AudioStreamBasicDescription> fileDataFormat() const;
    AudioStreamBasicDescription NODELETE clientDataFormat(const AudioStreamBasicDescription& inFormat, float sampleRate) const;

    std::span<const uint8_t> m_data;

    std::unique_ptr<AudioFileReaderData> m_readerData;

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif

};

}

#endif // ENABLE(WEB_AUDIO)
