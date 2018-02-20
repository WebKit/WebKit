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

#include "config.h"
#include "VRPlatformDisplayOpenVR.h"

#if USE(OPENVR)

#include <wtf/text/StringBuilder.h>

namespace WebCore {

VRPlatformDisplayOpenVR::VRPlatformDisplayOpenVR(vr::IVRSystem* system, vr::IVRChaperone* chaperone, vr::IVRCompositor* compositor)
    : m_system(system)
    , m_chaperone(chaperone)
    , m_compositor(compositor)
{
    m_displayInfo.isConnected = m_system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd);

    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("OpenVR HMD");
    char HMDName[128];
    if (auto length = m_system->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, HMDName, 128)) {
        stringBuilder.append(" (");
        stringBuilder.append(HMDName, length);
        stringBuilder.append(')');
    }
    m_displayInfo.displayName = stringBuilder.toString();

    m_displayInfo.isMounted = false;
    // FIXME: We're assuming an HTC Vive HMD here. Get this info from OpenVR?.
    m_displayInfo.capabilityFlags = VRDisplayCapabilityFlags::None |
        VRDisplayCapabilityFlags::Position |
        VRDisplayCapabilityFlags::Orientation |
        VRDisplayCapabilityFlags::ExternalDisplay |
        VRDisplayCapabilityFlags::Present;

    updateEyeParameters();
}

VRPlatformDisplayInfo::FieldOfView VRPlatformDisplayOpenVR::computeFieldOfView(vr::Hmd_Eye eye)
{
    float left, right, top, bottom;
    // OpenVR returns the tangents of the half-angles from the center view axis.
    m_system->GetProjectionRaw(static_cast<vr::Hmd_Eye>(eye), &left, &right, &top, &bottom);
    return { -rad2deg(atanf(top)), rad2deg(atanf(bottom)), -rad2deg(atanf(left)), rad2deg(atanf(right)) };
}

void VRPlatformDisplayOpenVR::updateEyeParameters()
{
    for (unsigned eye = 0; eye < VRPlatformDisplayInfo::NumEyes; ++eye) {
        m_displayInfo.eyeFieldOfView[eye] = computeFieldOfView(static_cast<vr::Hmd_Eye>(eye));

        vr::HmdMatrix34_t eyeToHead = m_system->GetEyeToHeadTransform(static_cast<vr::Hmd_Eye>(eye));
        m_displayInfo.eyeTranslation[eye].set(eyeToHead.m[0][3], eyeToHead.m[1][3], eyeToHead.m[2][3]);
    }

    uint32_t width;
    uint32_t height;
    m_system->GetRecommendedRenderTargetSize(&width, &height);
    m_displayInfo.renderSize = { width, height };
}

}; // namespace WebCore

#endif // USE(OPENVR)
