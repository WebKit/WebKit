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
#include "VRManager.h"

#include "VRPlatformDisplay.h"
#include "VRPlatformManager.h"

#if USE(OPENVR)
#include "VRPlatformManagerOpenVR.h"
#endif

namespace WebCore {

VRManager& VRManager::singleton()
{
    static NeverDestroyed<VRManager> instance;
    return instance;
}

VRManager::VRManager()
{
#if USE(OPENVR)
    m_platformManager = VRPlatformManagerOpenVR::create();
#endif
}

VRManager::~VRManager()
{
    m_platformManager = nullptr;
}

std::optional<VRManager::VRDisplaysVector> VRManager::getVRDisplays()
{
    if (!m_platformManager)
        return std::nullopt;

    return m_platformManager->getVRDisplays();
}

}; // namespace WebCore
