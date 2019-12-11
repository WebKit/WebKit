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

#pragma once

#if USE(OPENVR)

#include "VRPlatformManager.h"

#include <openvr.h>

namespace WebCore {

class VRPlatformDisplayOpenVR;

class VRPlatformManagerOpenVR : public VRPlatformManager {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<VRPlatformManagerOpenVR> create();
    explicit VRPlatformManagerOpenVR() = default;

    Vector<WeakPtr<VRPlatformDisplay>> getVRDisplays() override;

    ~VRPlatformManagerOpenVR();
private:
    bool initOpenVR();

    vr::IVRSystem* m_system { nullptr };
    std::unique_ptr<VRPlatformDisplayOpenVR> m_display;
};

}; // namespace WebCore

#endif // USE(OPENVR)
