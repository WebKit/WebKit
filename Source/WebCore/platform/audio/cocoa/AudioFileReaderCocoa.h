/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

using ExtAudioFileRef = struct OpaqueExtAudioFile*;
using AudioFileID = struct OpaqueAudioFileID*;
typedef struct opaqueCMSampleBuffer* CMSampleBufferRef;

namespace WebCore {

class AudioBus;
class SourceBufferParserWebM;
class AudioFileReaderWebMData;

// Wrapper class for AudioFile and ExtAudioFile CoreAudio APIs for reading files and in-memory versions of them...

class AudioFileReader
#if !RELEASE_LOG_DISABLED
    : public LoggerHelper
#endif
{
public:
    AudioFileReader(const void* data, size_t dataSize);
    ~AudioFileReader();

    RefPtr<AudioBus> createBus(float sampleRate, bool mixToMono); // Returns nullptr on error

    const void* data() const { return m_data; }
    size_t dataSize() const { return m_dataSize; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const char* logClassName() const final { return "AudioFileReaderCocoa"; }
#endif

private:
#if ENABLE(MEDIA_SOURCE)
    bool isMaybeWebM(const uint8_t* data, size_t dataSize) const;
    std::unique_ptr<AudioFileReaderWebMData> demuxWebMData(const uint8_t* data, size_t dataSize) const;
    Vector<AudioStreamPacketDescription> getPacketDescriptions(CMSampleBufferRef) const;
    std::optional<size_t> decodeWebMData(AudioBufferList&, size_t numberOfFrames, const AudioStreamBasicDescription& inFormat, const AudioStreamBasicDescription& outFormat) const;
#endif
    static OSStatus readProc(void* clientData, SInt64 position, UInt32 requestCount, void* buffer, UInt32* actualCount);
    static SInt64 getSizeProc(void* clientData);
    ssize_t numberOfFrames() const;
    std::optional<AudioStreamBasicDescription> fileDataFormat() const;
    AudioStreamBasicDescription clientDataFormat(const AudioStreamBasicDescription& inFormat, float sampleRate) const;

    const void* m_data = { nullptr };
    size_t m_dataSize = { 0 };

    AudioFileID m_audioFileID = { nullptr };
    ExtAudioFileRef m_extAudioFileRef = { nullptr };

    std::unique_ptr<AudioFileReaderWebMData> m_webmData;

#if !RELEASE_LOG_DISABLED
    const Ref<Logger> m_logger;
    const void* m_logIdentifier;
#endif

};

}

#endif // ENABLE(WEB_AUDIO)
