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
#include <wtf/EnumTraits.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class RealtimeMediaSourceSettings {
public:
    enum SourceType { None, Camera, Microphone };
    enum VideoFacingMode { Unknown, User, Environment, Left, Right };

    static const AtomicString& facingMode(RealtimeMediaSourceSettings::VideoFacingMode);

    static RealtimeMediaSourceSettings::VideoFacingMode videoFacingModeEnum(const String&);

    explicit RealtimeMediaSourceSettings()
    {
    }

    bool supportsWidth() const { return m_supportedConstraints.supportsWidth(); }
    uint32_t width() const { return m_width; }
    void setWidth(uint32_t width) { m_width = width; }

    bool supportsHeight() const { return m_supportedConstraints.supportsHeight(); }
    uint32_t height() const { return m_height; }
    void setHeight(uint32_t height) { m_height = height; }

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
    uint32_t sampleRate() const { return m_sampleRate; }
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }

    bool supportsSampleSize() const { return m_supportedConstraints.supportsSampleSize(); }
    uint32_t sampleSize() const { return m_sampleSize; }
    void setSampleSize(uint32_t sampleSize) { m_sampleSize = sampleSize; }

    bool supportsEchoCancellation() const { return m_supportedConstraints.supportsEchoCancellation(); }
    bool echoCancellation() const { return m_echoCancellation; }
    void setEchoCancellation(bool echoCancellation) { m_echoCancellation = echoCancellation; }

    bool supportsDeviceId() const { return m_supportedConstraints.supportsDeviceId(); }
    const AtomicString& deviceId() const { return m_deviceId; }
    void setDeviceId(const AtomicString& deviceId) { m_deviceId = deviceId; }

    bool supportsGroupId() const { return m_supportedConstraints.supportsGroupId(); }
    const AtomicString& groupId() const { return m_groupId; }
    void setGroupId(const AtomicString& groupId) { m_groupId = groupId; }

    const RealtimeMediaSourceSupportedConstraints& supportedConstraints() const { return m_supportedConstraints; }
    void setSupportedConstraints(const RealtimeMediaSourceSupportedConstraints& supportedConstraints) { m_supportedConstraints = supportedConstraints; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, RealtimeMediaSourceSettings&);

private:
    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    float m_aspectRatio { 0 };
    float m_frameRate { 0 };
    VideoFacingMode m_facingMode { Unknown };
    double m_volume { 0 };
    uint32_t m_sampleRate { 0 };
    uint32_t m_sampleSize { 0 };
    bool m_echoCancellation { 0 };

    AtomicString m_deviceId;
    AtomicString m_groupId;

    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
};

template<class Encoder>
void RealtimeMediaSourceSettings::encode(Encoder& encoder) const
{
    encoder << m_width
        << m_height
        << m_aspectRatio
        << m_frameRate
        << m_volume
        << m_sampleRate
        << m_sampleSize
        << m_echoCancellation
        << m_deviceId
        << m_groupId
        << m_supportedConstraints;
    encoder.encodeEnum(m_facingMode);
}

template<class Decoder>
bool RealtimeMediaSourceSettings::decode(Decoder& decoder, RealtimeMediaSourceSettings& settings)
{
    return decoder.decode(settings.m_width)
        && decoder.decode(settings.m_height)
        && decoder.decode(settings.m_aspectRatio)
        && decoder.decode(settings.m_frameRate)
        && decoder.decode(settings.m_volume)
        && decoder.decode(settings.m_sampleRate)
        && decoder.decode(settings.m_sampleSize)
        && decoder.decode(settings.m_echoCancellation)
        && decoder.decode(settings.m_deviceId)
        && decoder.decode(settings.m_groupId)
        && decoder.decode(settings.m_supportedConstraints)
        && decoder.decodeEnum(settings.m_facingMode);
}

} // namespace WebCore

namespace WTF {

template <> struct EnumTraits<WebCore::RealtimeMediaSourceSettings::VideoFacingMode> {
    using values = EnumValues<
        WebCore::RealtimeMediaSourceSettings::VideoFacingMode,
        WebCore::RealtimeMediaSourceSettings::VideoFacingMode::Unknown,
        WebCore::RealtimeMediaSourceSettings::VideoFacingMode::User,
        WebCore::RealtimeMediaSourceSettings::VideoFacingMode::Environment,
        WebCore::RealtimeMediaSourceSettings::VideoFacingMode::Left,
        WebCore::RealtimeMediaSourceSettings::VideoFacingMode::Right
    >;
};

}

#endif // RealtimeMediaSourceSettings_h

#endif
