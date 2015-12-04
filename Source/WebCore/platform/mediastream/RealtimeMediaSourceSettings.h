/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef RealtimeMediaSourceSettings_h
#define RealtimeMediaSourceSettings_h

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSourceSupportedConstraints.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class RealtimeMediaSourceSettings {
public:
    enum SourceType { None, Camera, Microphone };
    enum VideoFacingMode { Unknown, User, Environment, Left, Right };

    static const AtomicString& facingMode(RealtimeMediaSourceSettings::VideoFacingMode);

    explicit RealtimeMediaSourceSettings()
    {
    }

    bool supportsWidth() const { return m_supportedConstraints.supportsWidth(); }
    unsigned long width() const { return m_width; }
    void setWidth(unsigned long width) { m_width = width; }

    bool supportsHeight() const { return m_supportedConstraints.supportsHeight(); }
    unsigned long height() const { return m_height; }
    void setHeight(unsigned long height) { m_height = height; }

    bool supportsAspectRatio() const { return m_supportedConstraints.supportsAspectRatio(); }
    float aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(float aspectRatio) { m_aspectRatio = aspectRatio; }

    bool supportsFrameRate() const { return m_supportedConstraints.supportsFrameRate(); }
    float frameRate() const { return m_frameRate; }
    void setFrameRate(float frameRate) { m_frameRate = frameRate; }

    bool supportsFacingMode() const { return m_supportedConstraints.supportsFacingMode(); }
    VideoFacingMode facingMode() const { return m_facingMode; }
    void setFacingMode(VideoFacingMode facingMode) { m_facingMode = facingMode; }

    bool supportsVolume() const { return m_supportedConstraints.supportsVolume(); }
    double volume() const { return m_volume; }
    void setVolume(double volume) { m_volume = volume; }

    bool supportsSampleRate() const { return m_supportedConstraints.supportsSampleRate(); }
    unsigned long sampleRate() const { return m_sampleRate; }
    void setSampleRate(unsigned long sampleRate) { m_sampleRate = sampleRate; }

    bool supportsSampleSize() const { return m_supportedConstraints.supportsSampleSize(); }
    unsigned long sampleSize() const { return m_sampleSize; }
    void setSampleSize(unsigned long sampleSize) { m_sampleSize = sampleSize; }

    bool supportsEchoCancellation() const { return m_supportedConstraints.supportsEchoCancellation(); }
    bool echoCancellation() const { return m_echoCancellation; }
    void setEchoCancellation(bool echoCancellation) { m_echoCancellation = echoCancellation; }

    bool supportsDeviceId() const { return m_supportedConstraints.supportsDeviceId(); }
    const AtomicString& deviceId() const { return m_deviceId; }
    void setDeviceId(const AtomicString& deviceId) { m_deviceId = deviceId; }

    bool supportsGroupId() const { return m_supportedConstraints.supportsGroupId(); }
    const AtomicString& groupId() const { return m_groupId; }
    void setGroupId(const AtomicString& groupId) { m_groupId = groupId; }

    void setSupportedConstraits(const RealtimeMediaSourceSupportedConstraints& supportedConstraints) { m_supportedConstraints = supportedConstraints; }

private:
    unsigned long m_width { 0 };
    unsigned long m_height { 0 };
    float m_aspectRatio { 0 };
    float m_frameRate { 0 };
    VideoFacingMode m_facingMode { Unknown };
    double m_volume { 0 };
    unsigned long m_sampleRate { 0 };
    unsigned long m_sampleSize { 0 };
    bool m_echoCancellation { 0 };

    AtomicString m_deviceId;
    AtomicString m_groupId;

    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
};

} // namespace WebCore

#endif // RealtimeMediaSourceSettings_h

#endif
