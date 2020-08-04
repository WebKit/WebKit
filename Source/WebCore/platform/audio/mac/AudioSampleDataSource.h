/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "AudioSampleBufferList.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class CAAudioStreamDescription;
class CARingBuffer;

class AudioSampleDataSource : public ThreadSafeRefCounted<AudioSampleDataSource, WTF::DestructionThread::MainRunLoop>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
    {
public:
    static Ref<AudioSampleDataSource> create(size_t, WTF::LoggerHelper&);

    ~AudioSampleDataSource();

    OSStatus setInputFormat(const CAAudioStreamDescription&);
    OSStatus setOutputFormat(const CAAudioStreamDescription&);

    void pushSamples(const MediaTime&, const PlatformAudioData&, size_t);
    void pushSamples(const AudioStreamBasicDescription&, CMSampleBufferRef);

    enum PullMode { Copy, Mix };
    bool pullSamples(AudioSampleBufferList&, size_t, uint64_t, double, PullMode);
    bool pullSamples(AudioBufferList&, size_t, uint64_t, double, PullMode);

    bool pullAvalaibleSamplesAsChunks(AudioBufferList&, size_t frameCount, uint64_t timeStamp, Function<void()>&&);

    void setVolume(float volume) { m_volume = volume; }
    float volume() const { return m_volume; }

    void setMuted(bool muted) { m_muted = muted; }
    bool muted() const { return m_muted; }

    const CAAudioStreamDescription* inputDescription() const { return m_inputDescription.get(); }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    void setLogger(Ref<const Logger>&&, const void*);
#endif

    static constexpr float EquivalentToMaxVolume = 0.95;

private:
    AudioSampleDataSource(size_t, LoggerHelper&);

    OSStatus setupConverter();
    bool pullSamplesInternal(AudioBufferList&, size_t&, uint64_t, double, PullMode);

    void pushSamplesInternal(const AudioBufferList&, const MediaTime&, size_t frameCount);

    std::unique_ptr<CAAudioStreamDescription> m_inputDescription;
    std::unique_ptr<CAAudioStreamDescription> m_outputDescription;

    MediaTime hostTime() const;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "AudioSampleDataSource"; }
    WTFLogChannel& logChannel() const final;
#endif

    uint64_t m_lastPushedSampleCount { 0 };
    MediaTime m_expectedNextPushedSampleTime { MediaTime::invalidTime() };

    MediaTime m_inputSampleOffset;
    int64_t m_outputSampleOffset { 0 };

    AudioConverterRef m_converter;
    RefPtr<AudioSampleBufferList> m_scratchBuffer;

    std::unique_ptr<CARingBuffer> m_ringBuffer;
    size_t m_maximumSampleCount { 0 };

    float m_volume { 1.0 };
    bool m_muted { false };
    bool m_transitioningFromPaused { true };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
