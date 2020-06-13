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
#include "RealtimeVideoSource.h"

namespace WebCore {

struct MockMicrophoneProperties {
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << static_cast<int32_t>(defaultSampleRate);
    }

    template <class Decoder>
    static Optional<MockMicrophoneProperties> decode(Decoder& decoder)
    {
        Optional<int32_t> defaultSampleRate;
        decoder >> defaultSampleRate;
        if (!defaultSampleRate)
            return WTF::nullopt;
        return MockMicrophoneProperties { *defaultSampleRate };
    }

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
    static Optional<MockCameraProperties> decode(Decoder& decoder)
    {
        Optional<double> defaultFrameRate;
        decoder >> defaultFrameRate;
        if (!defaultFrameRate)
            return WTF::nullopt;

        Optional<RealtimeMediaSourceSettings::VideoFacingMode> facingMode;
        decoder >> facingMode;
        if (!facingMode)
            return WTF::nullopt;

        Optional<Vector<VideoPresetData>> presets;
        decoder >> presets;
        if (!presets)
            return WTF::nullopt;

        Optional<Color> fillColor;
        decoder >> fillColor;
        if (!fillColor)
            return WTF::nullopt;

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
    static Optional<MockDisplayProperties> decode(Decoder& decoder)
    {
        Optional<CaptureDevice::DeviceType> type;
        decoder >> type;
            return WTF::nullopt;

        Optional<Color> fillColor;
        decoder >> fillColor;
        if (!fillColor)
            return WTF::nullopt;

        Optional<IntSize> defaultSize;
        decoder >> defaultSize;
        if (!defaultSize)
            return WTF::nullopt;

        return MockDisplayProperties { *type, *fillColor, *defaultSize };
    }

    CaptureDevice::DeviceType type;
    Color fillColor { Color::lightGray };
    IntSize defaultSize;
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
        return WTF::get<MockDisplayProperties>(properties).type;
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
    static Optional<MockMediaDevice> decodeMockMediaDevice(Decoder& decoder, String&& persistentId, String&& label)
    {
        Optional<Properties> properties;
        decoder >> properties;
        if (!properties)
            return WTF::nullopt;
        return MockMediaDevice { WTFMove(persistentId), WTFMove(label), WTFMove(*properties) };
    }

    template <class Decoder>
    static Optional<MockMediaDevice> decode(Decoder& decoder)
    {
        Optional<String> persistentId;
        decoder >> persistentId;
        if (!persistentId)
            return WTF::nullopt;

        Optional<String> label;
        decoder >> label;
        if (!label)
            return WTF::nullopt;

        Optional<uint8_t> index;
        decoder >> index;
        if (!index)
            return WTF::nullopt;

        switch (*index) {
        case 1:
            return decodeMockMediaDevice<MockMicrophoneProperties>(decoder, WTFMove(*persistentId), WTFMove(*label));
        case 2:
            return decodeMockMediaDevice<MockCameraProperties>(decoder, WTFMove(*persistentId), WTFMove(*label));
        case 3:
            return decodeMockMediaDevice<MockDisplayProperties>(decoder, WTFMove(*persistentId), WTFMove(*label));
        }
        return WTF::nullopt;
    }

    String persistentId;
    String label;
    Variant<MockMicrophoneProperties, MockCameraProperties, MockDisplayProperties> properties;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
