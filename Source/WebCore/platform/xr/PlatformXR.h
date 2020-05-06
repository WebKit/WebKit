/*
 * Copyright (C) 2020 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#pragma once

#include <memory>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace PlatformXR {

enum class SessionMode {
    Inline,
    ImmersiveVr,
    ImmersiveAr,
};

enum class ReferenceSpaceType {
    Viewer,
    Local,
    LocalFloor,
    BoundedFloor,
    Unbounded
};

#if ENABLE(WEBXR)

class Device : public CanMakeWeakPtr<Device> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Device);
public:
    using DeviceId = uint32_t;
    Device();
    DeviceId id() const { return m_id; }

    using ListOfSupportedModes = Vector<SessionMode>;
    using ListOfEnabledFeatures = Vector<ReferenceSpaceType>;

    bool supports(SessionMode mode) const { return m_supportedModes.contains(mode); }
    void setSupportedModes(const ListOfSupportedModes& modes) { m_supportedModes = modes; }
    void setEnabledFeatures(const ListOfEnabledFeatures& features) { m_enabledFeatures = features; }

    inline bool operator==(const Device& other) const { return m_id == other.m_id; }

protected:
    ListOfSupportedModes m_supportedModes;
    ListOfEnabledFeatures m_enabledFeatures;

private:
    DeviceId m_id;
};

class Instance {
public:
    static Instance& singleton();

    static Device::DeviceId nextDeviceId();

    void enumerateImmersiveXRDevices();
    const Vector<std::unique_ptr<Device>>& immersiveXRDevices() const { return m_immersiveXRDevices; }
private:
    Instance();
    ~Instance();

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    Vector<std::unique_ptr<Device>> m_immersiveXRDevices;
};

#endif // ENABLE(WEBXR)

} // namespace PlatformXR
