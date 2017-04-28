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

#include "CAAudioStreamDescription.h"
#include "CaptureDevice.h"
#include "RealtimeMediaSource.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct OpaqueCMClock* CMClockRef;

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioSampleBufferList;
class AudioSampleDataSource;
class CaptureDeviceInfo;

class CoreAudioCaptureSource : public RealtimeMediaSource {
public:

    static CaptureSourceOrError create(const String& deviceID, const MediaConstraints*);

    WEBCORE_EXPORT static AudioCaptureFactory& factory();

    void addEchoCancellationSource(AudioSampleDataSource&);
    void removeEchoCancellationSource(AudioSampleDataSource&);

    using MicrophoneDataCallback = std::function<void(const MediaTime& sampleTime, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t sampleCount)>;

    uint64_t addMicrophoneDataConsumer(MicrophoneDataCallback&&);
    void removeMicrophoneDataConsumer(uint64_t);

    CMClockRef timebaseClock();

private:
    CoreAudioCaptureSource(const String& deviceID, const String& label, uint32_t persistentID);
    virtual ~CoreAudioCaptureSource();

    bool isCaptureSource() const final { return true; }
    void startProducingData() final;
    void stopProducingData() final;
    bool isProducingData() const final { return m_ioUnitStarted; }

    OSStatus suspend();
    OSStatus resume();

    bool applyVolume(double) override { return true; }
    bool applySampleRate(int) override { return true; }
    bool applyEchoCancellation(bool) override { return true; }

    const RealtimeMediaSourceCapabilities& capabilities() const final;
    const RealtimeMediaSourceSettings& settings() const final;
    void settingsDidChange() final;

    OSStatus setupAudioUnits();
    void cleanupAudioUnits();
    OSStatus configureSpeakerProc();
    OSStatus configureMicrophoneProc();
    OSStatus defaultOutputDevice(uint32_t*);
    OSStatus defaultInputDevice(uint32_t*);

    static OSStatus microphoneCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus processMicrophoneSamples(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList*);

    static OSStatus speakerCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus provideSpeakerData(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList*);

    static double preferredSampleRate();
    static size_t preferredIOBufferSize();

    AudioUnit m_ioUnit { nullptr };

    // Only read/modified from the IO thread.
    Vector<Ref<AudioSampleDataSource>> m_activeSources;

    enum QueueAction { Add, Remove };
    Vector<std::pair<QueueAction, Ref<AudioSampleDataSource>>> m_pendingSources;

    uint32_t m_captureDeviceID { 0 };

    CAAudioStreamDescription m_microphoneProcFormat;
    RefPtr<AudioSampleBufferList> m_microphoneSampleBuffer;
    uint64_t m_latestMicTimeStamp { 0 };

    CAAudioStreamDescription m_speakerProcFormat;
    RefPtr<AudioSampleBufferList> m_speakerSampleBuffer;

    double m_DTSConversionRatio { 0 };

    bool m_ioUnitInitialized { false };
    bool m_ioUnitStarted { false };

    Lock m_pendingSourceQueueLock;
    Lock m_internalStateLock;

    mutable std::unique_ptr<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
    mutable std::optional<RealtimeMediaSourceSettings> m_currentSettings;

#if !LOG_DISABLED
    void checkTimestamps(const AudioTimeStamp&, uint64_t, double);

    String m_ioUnitName;
    uint64_t m_speakerProcsCalled { 0 };
    uint64_t m_microphoneProcsCalled { 0 };
#endif
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
