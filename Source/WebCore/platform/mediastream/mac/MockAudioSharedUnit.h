/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "BaseAudioSharedUnit.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>

OBJC_CLASS AVAudioPCMBuffer;
typedef struct OpaqueCMClock* CMClockRef;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace WebCore {

class WebAudioBufferList;
class WebAudioSourceProviderAVFObjC;

class MockAudioSharedUnit final : public BaseAudioSharedUnit {
public:
    WEBCORE_EXPORT static MockAudioSharedUnit& singleton();
    MockAudioSharedUnit();

    void setDeviceID(const String& deviceID) { m_deviceID = deviceID; }

private:
    bool hasAudioUnit() const final;
    void setCaptureDevice(String&&, uint32_t) final;
    OSStatus reconfigureAudioUnit() final;
    void resetSampleRate() final;

    void cleanupAudioUnit() final;
    OSStatus startInternal() final;
    void stopInternal() final;
    bool isProducingData() const final;

    void delaySamples(Seconds) final;

    CapabilityValueOrRange sampleRateCapacities() const final { return CapabilityValueOrRange(44100, 48000); }

    void tick();

    void render(Seconds);
    void emitSampleBuffers(uint32_t frameCount);
    void reconfigure();

    static Seconds renderInterval() { return 60_ms; }

    std::unique_ptr<WebAudioBufferList> m_audioBufferList;

    uint32_t m_maximiumFrameCount;
    uint64_t m_samplesEmitted { 0 };
    uint64_t m_samplesRendered { 0 };

    RetainPtr<CMFormatDescriptionRef> m_formatDescription;
    AudioStreamBasicDescription m_streamFormat;

    Vector<float> m_bipBopBuffer;
    bool m_hasAudioUnit { false };

    RunLoop::Timer<MockAudioSharedUnit> m_timer;
    MonotonicTime m_lastRenderTime { MonotonicTime::nan() };
    MonotonicTime m_delayUntil;

    Ref<WorkQueue> m_workQueue;
    unsigned m_channelCount { 2 };
    String m_deviceID;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

