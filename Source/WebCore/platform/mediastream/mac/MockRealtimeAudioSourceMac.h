/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "AudioCaptureSourceProviderObjC.h"
#include "FontCascade.h"
#include "MockRealtimeAudioSource.h"
#include <CoreAudio/CoreAudioTypes.h>

OBJC_CLASS AVAudioPCMBuffer;
typedef struct OpaqueCMClock* CMClockRef;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace WebCore {

class WebAudioBufferList;
class WebAudioSourceProviderAVFObjC;

class MockRealtimeAudioSourceMac final : public MockRealtimeAudioSource, public AudioCaptureSourceProviderObjC {
public:

    // AudioCaptureSourceProviderObjC
    void addObserver(AudioSourceObserverObjC&) final;
    void removeObserver(AudioSourceObserverObjC&) final;
    void start() final;

private:
    friend class MockRealtimeAudioSource;
    MockRealtimeAudioSourceMac(const String&);

    bool applySampleRate(int) final;
    bool applySampleSize(int) final { return false; }

    void emitSampleBuffers(uint32_t);
    void render(double) final;
    void reconfigure();

    AudioSourceProvider* audioSourceProvider() final;

    size_t m_audioBufferListBufferSize { 0 };
    std::unique_ptr<WebAudioBufferList> m_audioBufferList;

    uint32_t m_maximiumFrameCount;
    uint32_t m_sampleRate { 44100 };
    uint64_t m_bytesEmitted { 0 };

    RetainPtr<CMFormatDescriptionRef> m_formatDescription;
    AudioStreamBasicDescription m_streamFormat;
    RefPtr<WebAudioSourceProviderAVFObjC> m_audioSourceProvider;
    Vector<AudioSourceObserverObjC*> m_observers;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

