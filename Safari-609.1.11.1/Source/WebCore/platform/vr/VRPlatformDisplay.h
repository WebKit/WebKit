/*
 * Copyright (C) 2018 Igalia, S.L. All right reserved.
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "FloatPoint3D.h"
#include "TransformationMatrix.h"
#include "VRPlatformDisplayClient.h"

#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

typedef unsigned VRDisplayCapabilityFlags;

enum VRDisplayCapabilityFlag {
    VRDisplayCapabilityFlagNone = 0,
    VRDisplayCapabilityFlagPosition = 1 << 1,
    VRDisplayCapabilityFlagOrientation = 1 << 2,
    VRDisplayCapabilityFlagExternalDisplay = 1 << 3,
    VRDisplayCapabilityFlagPresent = 1 << 4
};

/* The purpose of this class is to encapsulate all the info about the display in a single class/data
 * structure. This way we wouldn't have to replicate all the API calls of VRDisplay in
 * VRPlatformDisplay. Also the client of this API would only have to issue a single call to get all
 * the info from the display. Note that it's fairly unlikely that a VR application would only
 * require just a few pieces of information from the device.
*/
class VRPlatformDisplayInfo {
public:
    const String displayName() const { return m_displayName; }
    void setDisplayName(String&& displayName) { m_displayName = WTFMove(displayName); }

    bool isConnected() const { return m_isConnected; }
    void setIsConnected(bool isConnected) { m_isConnected = isConnected; }

    bool isMounted() const { return m_isMounted; }
    void setIsMounted(bool isMounted) { m_isMounted = isMounted; }

    const VRDisplayCapabilityFlags& capabilityFlags() const { return m_capabilityFlags; }
    void setCapabilityFlags(const VRDisplayCapabilityFlags& flags) { m_capabilityFlags = flags; }

    uint32_t displayIdentifier() const { return m_displayIdentifier; }
    void setDisplayIdentifier(uint32_t displayIdentifier) { m_displayIdentifier = displayIdentifier; }

    enum Eye { EyeLeft = 0, EyeRight, NumEyes };
    const FloatPoint3D& eyeTranslation(Eye eye) const { return m_eyeTranslation[eye]; }
    void setEyeTranslation(Eye eye, const FloatPoint3D& translation) { m_eyeTranslation[eye] = translation; }

    struct FieldOfView {
        double upDegrees;
        double downDegrees;
        double leftDegrees;
        double rightDegrees;
    };
    const FieldOfView& eyeFieldOfView(Eye eye) const { return m_eyeFieldOfView[eye]; }
    void setEyeFieldOfView(Eye eye, const FieldOfView& fieldOfView) { m_eyeFieldOfView[eye] = fieldOfView; }

    struct RenderSize {
        unsigned width;
        unsigned height;
    };
    const RenderSize& renderSize() const { return m_renderSize; }
    void setRenderSize(const RenderSize& renderSize) { m_renderSize = renderSize; }

    void setPlayAreaBounds(const FloatSize& playAreaBounds) { m_playAreaBounds = playAreaBounds; }
    const Optional<FloatSize>& playAreaBounds() const { return m_playAreaBounds; }

    void setSittingToStandingTransform(const TransformationMatrix& transform) { m_sittingToStandingTransform = transform; }
    const Optional<TransformationMatrix>& sittingToStandingTransform() const { return m_sittingToStandingTransform; }

private:
    String m_displayName;
    bool m_isConnected;
    bool m_isMounted;
    VRDisplayCapabilityFlags m_capabilityFlags;
    uint32_t m_displayIdentifier;

    FloatPoint3D m_eyeTranslation[Eye::NumEyes];

    RenderSize m_renderSize;
    FieldOfView m_eyeFieldOfView[Eye::NumEyes];

    Optional<FloatSize> m_playAreaBounds;
    Optional<TransformationMatrix> m_sittingToStandingTransform;
};

struct VRPlatformTrackingInfo {
    struct Quaternion {
        Quaternion()
            : x(0), y(0), z(0), w(1) { }

        Quaternion(float x, float y, float z, float w)
            : x(x), y(y), z(z), w(w) { }

        Quaternion& conjugate()
        {
            x *= -1;
            y *= -1;
            z *= -1;
            return *this;
        }

        Quaternion& operator*(float factor)
        {
            x *= factor;
            y *= factor;
            z *= factor;
            w *= factor;
            return *this;
        }
        float x, y, z, w;
    };

    struct Float3 {
        Float3(float a, float b, float c)
            : data { a, b, c } { }

        float data[3];
    };

    void clear()
    {
        timestamp = 0;
        position = WTF::nullopt;
        orientation = WTF::nullopt;
        angularAcceleration = WTF::nullopt;
        angularVelocity = WTF::nullopt;
        linearAcceleration = WTF::nullopt;
        linearVelocity = WTF::nullopt;
    }

    Optional<Quaternion> orientation;
    Optional<FloatPoint3D> position;
    Optional<Float3> angularAcceleration;
    Optional<Float3> angularVelocity;
    Optional<Float3> linearAcceleration;
    Optional<Float3> linearVelocity;
    double timestamp { 0 };
};

class VRPlatformDisplay : public CanMakeWeakPtr<VRPlatformDisplay> {
public:
    virtual VRPlatformDisplayInfo getDisplayInfo() = 0;
    virtual VRPlatformTrackingInfo getTrackingInfo() = 0;
    virtual void updateDisplayInfo() = 0;
    virtual ~VRPlatformDisplay() = default;

    void setClient(VRPlatformDisplayClient*);

    enum class Event {
        Connected,
        Disconnected,
        Mounted,
        Unmounted,
    };

    void notifyVRPlatformDisplayEvent(Event);

private:
    VRPlatformDisplayClient* m_client { nullptr };
};

}; // namespace WebCore
