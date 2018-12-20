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

Optional<VRManager::VRDisplaysVector> VRManager::getVRDisplays()
{
    if (!m_platformManager)
        return WTF::nullopt;

    auto displays = m_platformManager->getVRDisplays();
    VRDisplaysHashMap newDisplays;
    for (auto& display : displays) {
        ASSERT(display);
        newDisplays.add(display->getDisplayInfo().displayIdentifier(), display);

        VRPlatformDisplayInfo info = display->getDisplayInfo();
        auto iterator = m_displays.find(info.displayIdentifier());
        if (iterator == m_displays.end())
            continue;

        display->updateDisplayInfo();
        VRPlatformDisplayInfo newInfo = display->getDisplayInfo();
        if (info.isConnected() != newInfo.isConnected())
            display->notifyVRPlatformDisplayEvent(newInfo.isConnected() ? VRPlatformDisplay::Event::Connected : VRPlatformDisplay::Event::Disconnected);
        if (info.isMounted() != newInfo.isMounted())
            display->notifyVRPlatformDisplayEvent(newInfo.isMounted() ? VRPlatformDisplay::Event::Mounted : VRPlatformDisplay::Event::Unmounted);
    }

    for (auto& iter : m_displays) {
        ASSERT(iter.value);
        if (!newDisplays.contains(iter.key))
            iter.value->notifyVRPlatformDisplayEvent(VRPlatformDisplay::Event::Disconnected);
    }

    m_displays = WTFMove(newDisplays);
    return displays;
}

}; // namespace WebCore
