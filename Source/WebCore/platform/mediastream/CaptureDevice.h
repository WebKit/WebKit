/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/text/WTFString.h>

namespace WebCore {

class CaptureDevice {
public:
    enum class DeviceType { Unknown, Microphone, Camera, Screen, Window };

    CaptureDevice(const String& persistentId, DeviceType type, const String& label, const String& groupId = emptyString())
        : m_persistentId(persistentId)
        , m_type(type)
        , m_label(label)
        , m_groupId(groupId)
    {
    }

    CaptureDevice() = default;

    const String& persistentId() const { return m_persistentId; }

    const String& label() const
    {
        static NeverDestroyed<String> airPods(MAKE_STATIC_STRING_IMPL("AirPods"));

        if (m_type == DeviceType::Microphone && m_label.contains(airPods))
            return airPods;

        return m_label;
    }

    const String& groupId() const { return m_groupId; }

    DeviceType type() const { return m_type; }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    explicit operator bool() const { return m_type != DeviceType::Unknown; }

#if ENABLE(MEDIA_STREAM)
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << m_persistentId;
        encoder << m_label;
        encoder << m_groupId;
        encoder << m_enabled;
        encoder << m_type;
    }

    template <class Decoder>
    static Optional<CaptureDevice> decode(Decoder& decoder)
    {
        Optional<String> persistentId;
        decoder >> persistentId;
        if (!persistentId)
            return WTF::nullopt;

        Optional<String> label;
        decoder >> label;
        if (!label)
            return WTF::nullopt;

        Optional<String> groupId;
        decoder >> groupId;
        if (!groupId)
            return WTF::nullopt;

        Optional<bool> enabled;
        decoder >> enabled;
        if (!enabled)
            return WTF::nullopt;

        Optional<CaptureDevice::DeviceType> type;
        decoder >> type;
        if (!type)
            return WTF::nullopt;

        Optional<CaptureDevice> device = {{ WTFMove(*persistentId), WTFMove(*type), WTFMove(*label), WTFMove(*groupId) }};
        device->setEnabled(*enabled);
        return device;
    }
#endif

private:
    String m_persistentId;
    DeviceType m_type { DeviceType::Unknown };
    String m_label;
    String m_groupId;
    bool m_enabled { false };
};

} // namespace WebCore

#if ENABLE(MEDIA_STREAM)
namespace WTF {

template<> struct EnumTraits<WebCore::CaptureDevice::DeviceType> {
    using values = EnumValues<
        WebCore::CaptureDevice::DeviceType,
        WebCore::CaptureDevice::DeviceType::Unknown,
        WebCore::CaptureDevice::DeviceType::Microphone,
        WebCore::CaptureDevice::DeviceType::Camera,
        WebCore::CaptureDevice::DeviceType::Screen,
        WebCore::CaptureDevice::DeviceType::Window
    >;
};

} // namespace WTF
#endif

