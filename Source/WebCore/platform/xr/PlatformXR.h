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

#include "FloatPoint3D.h"
#include "IntSize.h"
#include <memory>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/UniqueRef.h>
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

class TrackingAndRenderingClient : public CanMakeWeakPtr<TrackingAndRenderingClient> {
public:
    virtual ~TrackingAndRenderingClient() = default;

    virtual void sessionDidEnd() = 0;
    // FIXME: handle frame update
    // FIXME: handle visibility changes
};

class Device : public CanMakeWeakPtr<Device> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Device);
public:
    virtual ~Device() = default;

    using ListOfEnabledFeatures = Vector<ReferenceSpaceType>;
    bool supports(SessionMode mode) const { return m_enabledFeaturesMap.contains(mode); }
    void setEnabledFeatures(SessionMode mode, const ListOfEnabledFeatures& features) { m_enabledFeaturesMap.set(mode, features); }
    ListOfEnabledFeatures enabledFeatures(SessionMode mode) const { return m_enabledFeaturesMap.get(mode); }

    virtual WebCore::IntSize recommendedResolution(SessionMode) { return { 1, 1 }; }

    bool supportsOrientationTracking() const { return m_supportsOrientationTracking; }

    virtual void initializeTrackingAndRendering(SessionMode) = 0;
    virtual void shutDownTrackingAndRendering() = 0;
    void setTrackingAndRenderingClient(WeakPtr<TrackingAndRenderingClient>&& client) { m_trackingAndRenderingClient = WTFMove(client); }

    // If this method returns true, that means the device will notify TrackingAndRenderingClient
    // when the platform has completed all steps to shut down the XR session.
    virtual bool supportsSessionShutdownNotification() const { return false; }
    virtual void initializeReferenceSpace(ReferenceSpaceType) = 0;

    struct FrameData {
        long predictedDisplayTime;
        struct ViewData {
            struct {
                WebCore::FloatPoint3D position;
                struct {
                    float x;
                    float y;
                    float z;
                    float w;
                } orientation;
            } pose;
            struct {
                float rUp;
                float rDown;
                float rLeft;
                float rRight;
            } fov;
        };
        Vector<ViewData> viewPoses;
    };
    using RequestFrameCallback = Function<void(FrameData&&)>;
    virtual void requestFrame(RequestFrameCallback&&) = 0;

protected:
    Device() = default;

    // https://immersive-web.github.io/webxr/#xr-device-concept
    // Each XR device has a list of enabled features for each XRSessionMode in its list of supported modes,
    // which is a list of feature descriptors which MUST be initially an empty list.
    using EnabledFeaturesPerModeMap = WTF::HashMap<SessionMode, ListOfEnabledFeatures, WTF::IntHash<SessionMode>, WTF::StrongEnumHashTraits<SessionMode>>;
    EnabledFeaturesPerModeMap m_enabledFeaturesMap;

    bool m_supportsOrientationTracking { false };
    WeakPtr<TrackingAndRenderingClient> m_trackingAndRenderingClient;
};

class Instance {
public:
    static Instance& singleton();

    using DeviceList = Vector<UniqueRef<Device>>;
    void enumerateImmersiveXRDevices(CompletionHandler<void(const DeviceList&)>&&);

private:
    friend LazyNeverDestroyed<Instance>;
    Instance();
    ~Instance() = default;

    struct Impl;
    UniqueRef<Impl> m_impl;

    DeviceList m_immersiveXRDevices;
};

#endif // ENABLE(WEBXR)

} // namespace PlatformXR
