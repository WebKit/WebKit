/*
 * Copyright (C) 2017-2018 Igalia, S.L. All right reserved.
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

#if USE(OPENVR)

#include "VRPlatformDisplay.h"

#include <openvr.h>

namespace WebCore {

class VRPlatformDisplayOpenVR : public VRPlatformDisplay {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit VRPlatformDisplayOpenVR(vr::IVRSystem*, vr::IVRChaperone*, vr::IVRCompositor*);

    ~VRPlatformDisplayOpenVR() = default;
    VRPlatformDisplayInfo getDisplayInfo() override { return m_displayInfo; }
    VRPlatformTrackingInfo getTrackingInfo() override;
    void updateDisplayInfo() override;

private:
    VRPlatformDisplayInfo::FieldOfView computeFieldOfView(vr::Hmd_Eye);
    void updateEyeParameters();
    void updateStageParameters();

    static uint32_t s_displayIdentifier;

    vr::IVRSystem* m_system;
    vr::IVRChaperone* m_chaperone;
    vr::IVRCompositor* m_compositor;

    VRPlatformDisplayInfo m_displayInfo;
    VRPlatformTrackingInfo m_trackingInfo;
};

}; // namespace WebCore

#endif // USE(OPENVR)
