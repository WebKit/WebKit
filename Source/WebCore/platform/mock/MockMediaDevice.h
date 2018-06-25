/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "CaptureDevice.h"
#include "RealtimeMediaSource.h"

namespace WebCore {

struct MockMicrophoneProperties {
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << static_cast<int32_t>(defaultSampleRate);
    }

    template <class Decoder>
    static std::optional<MockMicrophoneProperties> decode(Decoder& decoder)
    {
        std::optional<int32_t> defaultSampleRate;
        decoder >> defaultSampleRate;
        if (!defaultSampleRate)
            return std::nullopt;
        return MockMicrophoneProperties { *defaultSampleRate };
    }

    int defaultSampleRate { 44100 };
};

// FIXME: Add support for other properties and serialization of colors.
struct MockCameraProperties {
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << defaultFrameRate;
        encoder << facingModeCapability;
    }

    template <class Decoder>
    static std::optional<MockCameraProperties> decode(Decoder& decoder)
    {
        std::optional<double> defaultFrameRate;
        decoder >> defaultFrameRate;
        if (!defaultFrameRate)
            return std::nullopt;

        std::optional<RealtimeMediaSourceSettings::VideoFacingMode> facingModeCapability;
        decoder >> facingModeCapability;
        if (!facingModeCapability)
            return std::nullopt;

        return MockCameraProperties { *defaultFrameRate, *facingModeCapability, Color::black };
    }

    double defaultFrameRate { 30 };
    RealtimeMediaSourceSettings::VideoFacingMode facingModeCapability { RealtimeMediaSourceSettings::VideoFacingMode::User };
    Color fillColor { Color::black };
};

struct MockDisplayProperties {
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << defaultFrameRate;
    }

    template <class Decoder>
    static std::optional<MockDisplayProperties> decode(Decoder& decoder)
    {
        std::optional<double> defaultFrameRate;
        decoder >> defaultFrameRate;
        if (!defaultFrameRate)
            return std::nullopt;

        return MockDisplayProperties { *defaultFrameRate, Color::lightGray };
    }

    double defaultFrameRate { 30 };
    Color fillColor { Color::lightGray };
};

struct MockMediaDevice {
    bool isMicrophone() const { return WTF::holds_alternative<MockMicrophoneProperties>(properties); }
    bool isCamera() const { return WTF::holds_alternative<MockCameraProperties>(properties); }
    bool isDisplay() const { return WTF::holds_alternative<MockDisplayProperties>(properties); }

    CaptureDevice::DeviceType type() const
    {
        if (isMicrophone())
            return CaptureDevice::DeviceType::Microphone;
        if (isCamera())
            return CaptureDevice::DeviceType::Camera;
        ASSERT(isDisplay());
        return CaptureDevice::DeviceType::Screen;
    }

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << persistentId;
        encoder << label;
        switchOn(properties, [&](const MockMicrophoneProperties& properties) {
            encoder << (uint8_t)1;
            encoder << properties;
        }, [&](const MockCameraProperties& properties) {
            encoder << (uint8_t)2;
            encoder << properties;
        }, [&](const MockDisplayProperties& properties) {
            encoder << (uint8_t)3;
            encoder << properties;
        });
    }

    template <typename Properties, typename Decoder>
    static std::optional<MockMediaDevice> decodeMockMediaDevice(Decoder& decoder, String&& persistentId, String&& label)
    {
        std::optional<Properties> properties;
        decoder >> properties;
        if (!properties)
            return std::nullopt;
        return MockMediaDevice { WTFMove(persistentId), WTFMove(label), WTFMove(*properties) };
    }

    template <class Decoder>
    static std::optional<MockMediaDevice> decode(Decoder& decoder)
    {
        std::optional<String> persistentId;
        decoder >> persistentId;
        if (!persistentId)
            return std::nullopt;

        std::optional<String> label;
        decoder >> label;
        if (!label)
            return std::nullopt;

        std::optional<uint8_t> index;
        decoder >> index;
        if (!index)
            return std::nullopt;

        switch (*index) {
        case 1:
            return decodeMockMediaDevice<MockMicrophoneProperties>(decoder, WTFMove(*persistentId), WTFMove(*label));
        case 2:
            return decodeMockMediaDevice<MockCameraProperties>(decoder, WTFMove(*persistentId), WTFMove(*label));
        case 3:
            return decodeMockMediaDevice<MockDisplayProperties>(decoder, WTFMove(*persistentId), WTFMove(*label));
        }
        return std::nullopt;
    }

    String persistentId;
    String label;
    Variant<MockMicrophoneProperties, MockCameraProperties, MockDisplayProperties> properties;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
