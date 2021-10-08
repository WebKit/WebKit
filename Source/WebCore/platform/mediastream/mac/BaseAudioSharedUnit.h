/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "RealtimeMediaSourceCapabilities.h"
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/MediaTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioStreamDescription;
class CaptureDevice;
class CoreAudioCaptureSource;
class PlatformAudioData;

class BaseAudioSharedUnit {
public:
    BaseAudioSharedUnit();
    virtual ~BaseAudioSharedUnit() = default;

    void startProducingData();
    void stopProducingData();
    void reconfigure();
    virtual bool isProducingData() const = 0;

    virtual void delaySamples(Seconds) { }

    void prepareForNewCapture();

    OSStatus resume();
    OSStatus suspend();

    bool isSuspended() const { return m_suspended; }

    double volume() const { return m_volume; }
    int sampleRate() const { return m_sampleRate; }
    bool enableEchoCancellation() const { return m_enableEchoCancellation; }

    void setVolume(double volume) { m_volume = volume; }
    void setSampleRate(int sampleRate) { m_sampleRate = sampleRate; }
    void setEnableEchoCancellation(bool enableEchoCancellation) { m_enableEchoCancellation = enableEchoCancellation; }

    void addClient(CoreAudioCaptureSource&);
    void removeClient(CoreAudioCaptureSource&);
    void clearClients();

    virtual bool hasAudioUnit() const = 0;
    void setCaptureDevice(String&&, uint32_t);

    virtual CapabilityValueOrRange sampleRateCapacities() const = 0;

    void devicesChanged(const Vector<CaptureDevice>&);

protected:
    void forEachClient(const Function<void(CoreAudioCaptureSource&)>&) const;
    bool hasClients() const { return !m_clients.isEmpty(); }
    void captureFailed();

    virtual void cleanupAudioUnit() = 0;
    virtual OSStatus startInternal() = 0;
    virtual void stopInternal() = 0;
    virtual OSStatus reconfigureAudioUnit() = 0;
    virtual void resetSampleRate();
    virtual void captureDeviceChanged() = 0;

    void setSuspended(bool value) { m_suspended = value; }

    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t /*numberOfFrames*/);

    const String& persistentID() const { return m_capturingDevice ? m_capturingDevice->first : emptyString(); }
    uint32_t captureDeviceID() const { return m_capturingDevice ? m_capturingDevice->second : 0; }

private:
    OSStatus startUnit();

    bool m_enableEchoCancellation { true };
    double m_volume { 1 };
    int m_sampleRate;
    bool m_suspended { false };
    bool m_needsReconfiguration { false };

    int32_t m_producingCount { 0 };

    std::optional<std::pair<String, uint32_t>> m_capturingDevice;

    HashSet<CoreAudioCaptureSource*> m_clients;
    Vector<CoreAudioCaptureSource*> m_audioThreadClients WTF_GUARDED_BY_LOCK(m_audioThreadClientsLock);
    Lock m_audioThreadClientsLock;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
