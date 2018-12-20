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

#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class VRPlatformDisplay;
class VRPlatformManager;

class VRManager final {
    friend class WTF::NeverDestroyed<VRManager>;
public:
    using VRDisplaysVector = Vector<WeakPtr<VRPlatformDisplay>>;

    ~VRManager();

    WEBCORE_EXPORT static VRManager& singleton();

    Optional<VRDisplaysVector> getVRDisplays();

private:
    VRManager();

    using VRDisplaysHashMap = HashMap<uint32_t, WeakPtr<VRPlatformDisplay>>;
    VRDisplaysHashMap m_displays;
    std::unique_ptr<VRPlatformManager> m_platformManager;
};

}; // namespace WebCore
