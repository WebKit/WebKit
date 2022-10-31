/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CaptureDevice {
public:
    enum class DeviceType { Unknown, Microphone, Speaker, Camera, Screen, Window, SystemAudio };

    CaptureDevice(const String& persistentId, DeviceType type, const String& label, const String& groupId = emptyString(), bool isEnabled = false, bool isDefault = false, bool isMock = false, bool isEphemeral = false)
        : m_persistentId(persistentId)
        , m_type(type)
        , m_label(label)
        , m_groupId(groupId)
        , m_enabled(isEnabled)
        , m_default(isDefault)
        , m_isMockDevice(isMock)
        , m_isEphemeral(isEphemeral)
    {
    }

    CaptureDevice() = default;

    void setPersistentId(const String& persistentId) { m_persistentId = persistentId; }
    const String& persistentId() const { return m_persistentId; }

    void setLabel(const String& label) { m_label = label; }
    const String& label() const
    {
        static NeverDestroyed<String> airPods(MAKE_STATIC_STRING_IMPL("AirPods"));

        if ((m_type == DeviceType::Microphone || m_type == DeviceType::Speaker) && m_label.contains(airPods.get()))
            return airPods;

        return m_label;
    }

    void setGroupId(const String& groupId) { m_groupId = groupId; }
    const String& groupId() const { return m_groupId; }

    DeviceType type() const { return m_type; }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    bool isDefault() const { return m_default; }
    void setIsDefault(bool isDefault) { m_default = isDefault; }

    bool isMockDevice() const { return m_isMockDevice; }
    void setIsMockDevice(bool isMockDevice) { m_isMockDevice = isMockDevice; }

    bool isEphemeral() const { return m_isEphemeral; }
    void setIsEphemeral(bool isEphemeral) { m_isEphemeral = isEphemeral; }

    explicit operator bool() const { return m_type != DeviceType::Unknown; }

    CaptureDevice isolatedCopy() &&;

#if ENABLE(MEDIA_STREAM)
    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << m_persistentId;
        encoder << m_label;
        encoder << m_groupId;
        encoder << m_enabled;
        encoder << m_default;
        encoder << m_type;
        encoder << m_isMockDevice;
        encoder << m_isEphemeral;
    }

    template <class Decoder>
    static std::optional<CaptureDevice> decode(Decoder& decoder)
    {
        std::optional<String> persistentId;
        decoder >> persistentId;
        if (!persistentId)
            return std::nullopt;

        std::optional<String> label;
        decoder >> label;
        if (!label)
            return std::nullopt;

        std::optional<String> groupId;
        decoder >> groupId;
        if (!groupId)
            return std::nullopt;

        std::optional<bool> enabled;
        decoder >> enabled;
        if (!enabled)
            return std::nullopt;

        std::optional<bool> isDefault;
        decoder >> isDefault;
        if (!isDefault)
            return std::nullopt;

        std::optional<CaptureDevice::DeviceType> type;
        decoder >> type;
        if (!type)
            return std::nullopt;

        std::optional<bool> isMockDevice;
        decoder >> isMockDevice;
        if (!isMockDevice)
            return std::nullopt;

        std::optional<bool> isEphemeral;
        decoder >> isEphemeral;
        if (!isEphemeral)
            return std::nullopt;

        std::optional<CaptureDevice> device = {{ WTFMove(*persistentId), WTFMove(*type), WTFMove(*label), WTFMove(*groupId) }};
        device->setEnabled(*enabled);
        device->setIsDefault(*isDefault);
        device->setIsMockDevice(*isMockDevice);
        device->setIsEphemeral(*isEphemeral);
        return device;
    }
#endif

protected:
    String m_persistentId;
    DeviceType m_type { DeviceType::Unknown };
    String m_label;
    String m_groupId;
    bool m_enabled { false };
    bool m_default { false };
    bool m_isMockDevice { false };
    bool m_isEphemeral { false };
};

inline bool haveDevicesChanged(const Vector<CaptureDevice>& oldDevices, const Vector<CaptureDevice>& newDevices)
{
    if (oldDevices.size() != newDevices.size())
        return true;

    for (auto& newDevice : newDevices) {
        if (newDevice.type() != CaptureDevice::DeviceType::Camera && newDevice.type() != CaptureDevice::DeviceType::Microphone)
            continue;

        auto index = oldDevices.findIf([&newDevice](auto& oldDevice) {
            return newDevice.persistentId() == oldDevice.persistentId() && newDevice.enabled() != oldDevice.enabled();
        });

        if (index == notFound)
            return true;
    }

    return false;
}

inline CaptureDevice CaptureDevice::isolatedCopy() &&
{
    return {
        WTFMove(m_persistentId).isolatedCopy(),
        m_type,
        WTFMove(m_label).isolatedCopy(),
        WTFMove(m_groupId).isolatedCopy(),
        m_enabled,
        m_default,
        m_isMockDevice,
        m_isEphemeral
    };
}

} // namespace WebCore

#if ENABLE(MEDIA_STREAM)
namespace WTF {

template<> struct EnumTraits<WebCore::CaptureDevice::DeviceType> {
    using values = EnumValues<
        WebCore::CaptureDevice::DeviceType,
        WebCore::CaptureDevice::DeviceType::Unknown,
        WebCore::CaptureDevice::DeviceType::Microphone,
        WebCore::CaptureDevice::DeviceType::Speaker,
        WebCore::CaptureDevice::DeviceType::Camera,
        WebCore::CaptureDevice::DeviceType::Screen,
        WebCore::CaptureDevice::DeviceType::Window,
        WebCore::CaptureDevice::DeviceType::SystemAudio
    >;
};

} // namespace WTF
#endif

