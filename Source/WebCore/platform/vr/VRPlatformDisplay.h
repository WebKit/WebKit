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

#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum VRDisplayCapabilityFlags {
    None = 0,
    Position = 1 << 1,
    Orientation = 1 << 2,
    ExternalDisplay = 1 << 3,
    Present = 1 << 4
};

/* The purpose of this class is to encapsulate all the info about the display in a single class/data
 * structure. This way we wouldn't have to replicate all the API calls of VRDisplay in
 * VRPlatformDisplay. Also the client of this API would only have to issue a single call to get all
 * the info from the display. Note that it's fairly unlikely that a VR application would only
 * require just a few pieces of information from the device.
*/
struct VRPlatformDisplayInfo {
    String displayName;
    bool isConnected;
    bool isMounted;
    unsigned capabilityFlags;

    enum Eye { EyeLeft = 0, EyeRight, NumEyes };
    FloatPoint3D eyeTranslation[Eye::NumEyes];

    struct FieldOfView {
        double upDegrees;
        double downDegrees;
        double leftDegrees;
        double rightDegrees;
    } eyeFieldOfView[Eye::NumEyes];

    struct RenderSize {
        unsigned width;
        unsigned height;
    } renderSize;

    std::optional<FloatSize> playAreaBounds;
    std::optional<TransformationMatrix> sittingToStandingTransform;
};

class VRPlatformDisplay {
public:
    virtual VRPlatformDisplayInfo getDisplayInfo() = 0;
    virtual ~VRPlatformDisplay() = default;

    WeakPtr<VRPlatformDisplay> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(*this); }
private:
    WeakPtrFactory<VRPlatformDisplay> m_weakPtrFactory;
};

}; // namespace WebCore
