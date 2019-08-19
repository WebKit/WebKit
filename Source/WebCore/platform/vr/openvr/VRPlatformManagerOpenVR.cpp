/*
 * Copyright (C) 2017 Igalia, S.L. All right reserved.
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
#include "VRPlatformManagerOpenVR.h"

#include "VRPlatformDisplayOpenVR.h"

#if USE(OPENVR)

#include "JSVRDisplay.h"
#include "VRDisplay.h"

namespace WebCore {

std::unique_ptr<VRPlatformManagerOpenVR> VRPlatformManagerOpenVR::create()
{
    if (!vr::VR_IsRuntimeInstalled())
        return nullptr;

    return makeUnique<VRPlatformManagerOpenVR>();
}

VRPlatformManagerOpenVR::~VRPlatformManagerOpenVR()
{
    vr::VR_Shutdown();
}

bool VRPlatformManagerOpenVR::initOpenVR()
{
    vr::HmdError error;
    m_system = vr::VR_Init(&error, vr::VRApplication_Scene);
    if (error)
        return false;

    return true;
}

Vector<WeakPtr<VRPlatformDisplay>> VRPlatformManagerOpenVR::getVRDisplays()
{
    // Quickly check for HMDs. Much faster than initializing the whole OpenVR API.
    if (!vr::VR_IsHmdPresent()) {
        m_system = nullptr;
        return { };
    }

    if (!m_system && !initOpenVR()) {
        m_system = nullptr;
        vr::VR_Shutdown();
        return { };
    }

    vr::HmdError error;
    vr::IVRChaperone* chaperone = static_cast<vr::IVRChaperone*>(vr::VR_GetGenericInterface(vr::IVRChaperone_Version, &error));
    if (error || !chaperone)
        return { };

    vr::IVRCompositor* compositor = static_cast<vr::IVRCompositor*>(vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &error));
    if (error || !compositor)
        return { };

    Vector<WeakPtr<VRPlatformDisplay>> displays;
    if (!m_display)
        m_display = makeUnique<VRPlatformDisplayOpenVR>(m_system, chaperone, compositor);
    displays.append(makeWeakPtr(*m_display));
    return displays;
}

}; // namespace WebCore

#endif // USE(OPENVR)
