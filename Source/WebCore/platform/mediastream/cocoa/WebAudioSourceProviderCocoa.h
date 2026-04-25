/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if ENABLE(WEB_AUDIO)

#include <CoreAudio/CoreAudioTypes.h>
#include <WebCore/AudioSampleDataSource.h>
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/WebAudioSourceProvider.h>
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/SystemFree.h>

typedef struct AudioBufferList AudioBufferList;
typedef struct OpaqueAudioConverter* AudioConverterRef;
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WTF {
class LoggerHelper;
}

namespace WebCore {

class CAAudioStreamDescription;
class MultiChannelResampler;
class PitchShiftAudioUnit;
class PlatformAudioData;
class WebAudioBufferList;

class WebAudioSourceProviderCocoa
    : public WebAudioSourceProvider {
public:
    WEBCORE_EXPORT WebAudioSourceProviderCocoa();
    WEBCORE_EXPORT ~WebAudioSourceProviderCocoa();

    CARingBuffer* ringBuffer() const { return m_ringBuffer.get(); }

    WEBCORE_EXPORT void setPlaybackRate(double);
    double playbackRate() const { return m_playbackRate; }

    WEBCORE_EXPORT void setPreservesPitch(bool);
    bool preservesPitch() const { return m_preservesPitch; }

    WEBCORE_EXPORT void setVolume(double);
    double volume() const { return m_volume; }

    WEBCORE_EXPORT void audioStorageChanged(std::unique_ptr<CARingBuffer>&&, const AudioStreamDescription&);
    WEBCORE_EXPORT void receivedNewAudioSamples(const PlatformAudioData&, const AudioStreamDescription&, size_t);
    WEBCORE_EXPORT void setNeedsFlush();

protected:
    void setPollSamplesCount(size_t);

private:
    virtual void hasNewClient(AudioSourceProviderClient*) = 0;
#if !RELEASE_LOG_DISABLED
    virtual WTF::LoggerHelper& loggerHelper() = 0;
#endif

    // AudioSourceProvider
    WEBCORE_EXPORT void provideInput(AudioBus&, size_t) final;
    bool provideInputInternal(AudioBus&, size_t);
    WEBCORE_EXPORT void setClient(WeakPtr<AudioSourceProviderClient>&&) final;

    void prepare(const AudioStreamBasicDescription&);

    Lock m_lock;
    WeakPtr<AudioSourceProviderClient> m_client;

    std::unique_ptr<PitchShiftAudioUnit> m_pitchShifter;
    std::unique_ptr<MultiChannelResampler> m_multiChannelResampler;
    std::optional<CAAudioStreamDescription> m_inputDescription;
    std::optional<CAAudioStreamDescription> m_outputDescription;
    std::unique_ptr<AudioBufferList, WTF::SystemFree<AudioBufferList>> m_list;
    AudioConverterRef m_converter;
    std::unique_ptr<CARingBuffer> m_ringBuffer;

    std::atomic<double> m_playbackRate { 1 };
    std::atomic<bool> m_preservesPitch { true };
    std::atomic<double> m_volume { 1 };
    size_t m_pollSamplesCount { 3 };
    uint64_t m_readCount { 0 };
    bool m_underflowed { true };
};

inline void WebAudioSourceProviderCocoa::setPollSamplesCount(size_t count)
{
    m_pollSamplesCount = count;
}

}

#endif // ENABLE(WEB_AUDIO)
