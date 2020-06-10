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
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace PlatformXR {

enum class SessionMode : uint8_t {
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
    virtual ~Device() = default;

    using ListOfEnabledFeatures = Vector<ReferenceSpaceType>;
    bool supports(SessionMode mode) const { return m_enabledFeaturesMap.contains(mode); }
    void setEnabledFeatures(SessionMode mode, const ListOfEnabledFeatures& features) { m_enabledFeaturesMap.set(mode, features); }
    ListOfEnabledFeatures enabledFeatures(SessionMode mode) const { return m_enabledFeaturesMap.get(mode); }

protected:
    Device() = default;

    // https://immersive-web.github.io/webxr/#xr-device-concept
    // Each XR device has a list of enabled features for each XRSessionMode in its list of supported modes,
    // which is a list of feature descriptors which MUST be initially an empty list.
    using EnabledFeaturesPerModeMap = WTF::HashMap<SessionMode, ListOfEnabledFeatures, WTF::IntHash<SessionMode>, WTF::StrongEnumHashTraits<SessionMode>>;
    EnabledFeaturesPerModeMap m_enabledFeaturesMap;
};

class Instance {
public:
    static Instance& singleton();

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
