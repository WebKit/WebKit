/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScreenManager.h"

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
#include "WebProcessMessages.h"
#include "WebProcessPool.h"

namespace WebKit {

ScreenManager& ScreenManager::singleton()
{
    static NeverDestroyed<ScreenManager> manager;
    return manager;
}

PlatformDisplayID ScreenManager::displayID(PlatformScreen* screen) const
{
    return m_screenToDisplayIDMap.get(screen);
}

PlatformScreen* ScreenManager::screen(PlatformDisplayID displayID) const
{
    for (const auto& iter : m_screenToDisplayIDMap) {
        if (iter.value == displayID)
            return iter.key;
    }
    return nullptr;
}

void ScreenManager::addScreen(PlatformScreen* screen)
{
    m_screens.append(screen);
    m_screenToDisplayIDMap.add(screen, generatePlatformDisplayID(screen));
}

void ScreenManager::removeScreen(PlatformScreen* screen)
{
    m_screenToDisplayIDMap.remove(screen);
    m_screens.removeFirstMatching([screen](const auto& item) {
        return item.get() == screen;
    });
}

void ScreenManager::propertiesDidChange() const
{
    auto properties = collectScreenProperties();
    for (auto& pool : WebProcessPool::allProcessPools())
        pool->sendToAllProcesses(Messages::WebProcess::SetScreenProperties(properties));
}

} // namespace WebKit

#endif // PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
