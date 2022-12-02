/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "RealtimeVideoSource.h"

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

struct MockSpeakerProperties {
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << relatedMicrophoneId << static_cast<int32_t>(defaultSampleRate);
    }

    template <class Decoder>
    static std::optional<MockSpeakerProperties> decode(Decoder& decoder)
    {
        std::optional<int32_t> defaultSampleRate;
        decoder >> defaultSampleRate;
        if (!defaultSampleRate)
            return std::nullopt;

        std::optional<String> relatedMicrophoneId;
        decoder >> relatedMicrophoneId;
        if (!relatedMicrophoneId)
            return std::nullopt;

        return MockSpeakerProperties { WTFMove(*relatedMicrophoneId), *defaultSampleRate };
    }

    String relatedMicrophoneId;
    int defaultSampleRate { 44100 };
};

// FIXME: Add support for other properties.
struct MockCameraProperties {
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << defaultFrameRate;
        encoder << facingMode;
        encoder << presets;
        encoder << fillColor;
    }

    template <class Decoder>
    static std::optional<MockCameraProperties> decode(Decoder& decoder)
    {
        std::optional<double> defaultFrameRate;
        decoder >> defaultFrameRate;
        if (!defaultFrameRate)
            return std::nullopt;

        std::optional<RealtimeMediaSourceSettings::VideoFacingMode> facingMode;
        decoder >> facingMode;
        if (!facingMode)
            return std::nullopt;

        std::optional<Vector<VideoPresetData>> presets;
        decoder >> presets;
        if (!presets)
            return std::nullopt;

        std::optional<Color> fillColor;
        decoder >> fillColor;
        if (!fillColor)
            return std::nullopt;

        return MockCameraProperties { *defaultFrameRate, *facingMode, WTFMove(*presets), *fillColor };
    }

    double defaultFrameRate { 30 };
    RealtimeMediaSourceSettings::VideoFacingMode facingMode { RealtimeMediaSourceSettings::VideoFacingMode::User };
    Vector<VideoPresetData> presets { { { 640, 480 }, { { 30, 30}, { 15, 15 } } } };
    Color fillColor { Color::black };
};

struct MockDisplayProperties {
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << type;
        encoder << fillColor;
        encoder << defaultSize;
    }

    template <class Decoder>
    static std::optional<MockDisplayProperties> decode(Decoder& decoder)
    {
        std::optional<CaptureDevice::DeviceType> type;
        decoder >> type;
            return std::nullopt;

        std::optional<Color> fillColor;
        decoder >> fillColor;
        if (!fillColor)
            return std::nullopt;

        std::optional<IntSize> defaultSize;
        decoder >> defaultSize;
        if (!defaultSize)
            return std::nullopt;

        return MockDisplayProperties { *type, *fillColor, *defaultSize };
    }

    CaptureDevice::DeviceType type;
    Color fillColor { Color::lightGray };
    IntSize defaultSize;
};

struct MockMediaDevice {
    bool isMicrophone() const { return std::holds_alternative<MockMicrophoneProperties>(properties); }
    bool isSpeaker() const { return std::holds_alternative<MockSpeakerProperties>(properties); }
    bool isCamera() const { return std::holds_alternative<MockCameraProperties>(properties); }
    bool isDisplay() const { return std::holds_alternative<MockDisplayProperties>(properties); }

    CaptureDevice captureDevice() const
    {
        if (isMicrophone())
            return CaptureDevice { persistentId, CaptureDevice::DeviceType::Microphone, label, persistentId, true, false, true, isEphemeral };

        if (isSpeaker())
            return CaptureDevice { persistentId, CaptureDevice::DeviceType::Speaker, label, speakerProperties()->relatedMicrophoneId, true, false, true, isEphemeral };

        if (isCamera())
            return CaptureDevice { persistentId, CaptureDevice::DeviceType::Camera, label, persistentId, true, false, true, isEphemeral };

        ASSERT(isDisplay());
        return CaptureDevice { persistentId, std::get<MockDisplayProperties>(properties).type, label, emptyString(), true, false, true, isEphemeral };
    }

    CaptureDevice::DeviceType type() const
    {
        if (isMicrophone())
            return CaptureDevice::DeviceType::Microphone;
        if (isSpeaker())
            return CaptureDevice::DeviceType::Speaker;
        if (isCamera())
            return CaptureDevice::DeviceType::Camera;

        ASSERT(isDisplay());
        return std::get<MockDisplayProperties>(properties).type;
    }

    const MockSpeakerProperties* speakerProperties() const
    {
        return isSpeaker() ? &std::get<MockSpeakerProperties>(properties) : nullptr;
    }

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << persistentId;
        encoder << label;
        encoder << isEphemeral;
        WTF::switchOn(properties, [&](const MockMicrophoneProperties& properties) {
            encoder << (uint8_t)1;
            encoder << properties;
        }, [&](const MockSpeakerProperties& properties) {
            encoder << (uint8_t)2;
            encoder << properties;
        }, [&](const MockCameraProperties& properties) {
            encoder << (uint8_t)3;
            encoder << properties;
        }, [&](const MockDisplayProperties& properties) {
            encoder << (uint8_t)4;
            encoder << properties;
        });
    }

    template <typename Properties, typename Decoder>
    static std::optional<MockMediaDevice> decodeMockMediaDevice(Decoder& decoder, String&& persistentId, String&& label, bool isEphemeral)
    {
        std::optional<Properties> properties;
        decoder >> properties;
        if (!properties)
            return std::nullopt;
        return MockMediaDevice { WTFMove(persistentId), WTFMove(label), isEphemeral, WTFMove(*properties) };
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

        std::optional<bool> isEphemeral;
        decoder >> isEphemeral;
        if (!isEphemeral)
            return std::nullopt;

        std::optional<uint8_t> index;
        decoder >> index;
        if (!index)
            return std::nullopt;

        switch (*index) {
        case 1:
            return decodeMockMediaDevice<MockMicrophoneProperties>(decoder, WTFMove(*persistentId), WTFMove(*label), *isEphemeral);
        case 2:
            return decodeMockMediaDevice<MockSpeakerProperties>(decoder, WTFMove(*persistentId), WTFMove(*label), *isEphemeral);
        case 3:
            return decodeMockMediaDevice<MockCameraProperties>(decoder, WTFMove(*persistentId), WTFMove(*label), *isEphemeral);
        case 4:
            return decodeMockMediaDevice<MockDisplayProperties>(decoder, WTFMove(*persistentId), WTFMove(*label), *isEphemeral);
        }
        return std::nullopt;
    }

    String persistentId;
    String label;
    bool isEphemeral;
    std::variant<MockMicrophoneProperties, MockSpeakerProperties, MockCameraProperties, MockDisplayProperties> properties;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
