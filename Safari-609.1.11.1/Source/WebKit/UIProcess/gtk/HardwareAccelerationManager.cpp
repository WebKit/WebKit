/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "HardwareAccelerationManager.h"

#include "AcceleratedBackingStore.h"

namespace WebKit {
using namespace WebCore;

HardwareAccelerationManager& HardwareAccelerationManager::singleton()
{
    static NeverDestroyed<HardwareAccelerationManager> manager;
    return manager;
}

HardwareAccelerationManager::HardwareAccelerationManager()
    : m_canUseHardwareAcceleration(true)
    , m_forceHardwareAcceleration(false)
{
#if !ENABLE(OPENGL)
    m_canUseHardwareAcceleration = false;
    return;
#endif

    const char* disableCompositing = getenv("WEBKIT_DISABLE_COMPOSITING_MODE");
    if (disableCompositing && strcmp(disableCompositing, "0")) {
        m_canUseHardwareAcceleration = false;
        return;
    }

    if (!AcceleratedBackingStore::checkRequirements()) {
        m_canUseHardwareAcceleration = false;
        return;
    }

    const char* forceCompositing = getenv("WEBKIT_FORCE_COMPOSITING_MODE");
    if (forceCompositing && strcmp(forceCompositing, "0"))
        m_forceHardwareAcceleration = true;
}

} // namespace WebKit
