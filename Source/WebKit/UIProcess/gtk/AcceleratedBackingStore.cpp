/*
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedBackingStore.h"

#include "HardwareAccelerationManager.h"
#include "WebPageProxy.h"
#include <WebCore/CairoUtilities.h>
#include <WebCore/PlatformDisplay.h>

#if PLATFORM(WAYLAND)
#include "AcceleratedBackingStoreWayland.h"
#endif

#if PLATFORM(X11)
#include "AcceleratedBackingStoreX11.h"
#endif

namespace WebKit {
using namespace WebCore;

bool AcceleratedBackingStore::checkRequirements()
{
#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland)
        return AcceleratedBackingStoreWayland::checkRequirements();
#endif
#if PLATFORM(X11)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11)
        return AcceleratedBackingStoreX11::checkRequirements();
#endif

    return false;
}

std::unique_ptr<AcceleratedBackingStore> AcceleratedBackingStore::create(WebPageProxy& webPage)
{
    if (!HardwareAccelerationManager::singleton().canUseHardwareAcceleration())
        return nullptr;

#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland)
        return AcceleratedBackingStoreWayland::create(webPage);
#endif
#if PLATFORM(X11)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11)
        return AcceleratedBackingStoreX11::create(webPage);
#endif
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

AcceleratedBackingStore::AcceleratedBackingStore(WebPageProxy& webPage)
    : m_webPage(webPage)
{
}

} // namespace WebKit
