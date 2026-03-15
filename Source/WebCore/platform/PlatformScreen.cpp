/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformScreen.h"

#if PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))

#include "ScreenProperties.h"
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static Lock& platformScreenLock()
{
    static Lock lock;
    return lock;
}

Ref<PlatformScreen>& PlatformScreen::instance() WTF_REQUIRES_LOCK(platformScreenLock())
{
    static NeverDestroyed<Ref<PlatformScreen>> platformScreen = PlatformScreen::create({ });
    static NeverDestroyed<Ref<PlatformScreen>> ref { platformScreen.get() };
    return ref.get();
}

PlatformScreen::PlatformScreen(ScreenProperties&& properties)
    : m_properties(WTF::move(properties))
{
}

Ref<PlatformScreen> PlatformScreen::create(ScreenProperties&& properties)
{
    return adoptRef(*new PlatformScreen(WTF::move(properties)));
}

Ref<const PlatformScreen> PlatformScreen::singleton()
{
    Locker locker { platformScreenLock() };
    return instance().get();
}

const ScreenData* PlatformScreen::screenData(PlatformDisplayID screenDisplayID) const
{
    if (m_properties.screenDataMap.isEmpty())
        return nullptr;

    // Return property of the first screen if the screen is not found in the map.
    if (auto displayID = screenDisplayID ? screenDisplayID : primaryScreenDisplayID()) {
        auto properties = m_properties.screenDataMap.find(displayID);
        if (properties != m_properties.screenDataMap.end())
            return &properties->value;
    }

    // Last resort: use the first item in the screen list.
    return &m_properties.screenDataMap.begin()->value;
}

PlatformDisplayID PlatformScreen::primaryScreenDisplayID() const
{
    return m_properties.primaryDisplayID;
}

const ScreenProperties& PlatformScreen::screenProperties() const
{
    return m_properties;
}

const ScreenDataMap& PlatformScreen::screenDatas() const
{
    return screenProperties().screenDataMap;
}

#if HAVE(SUPPORT_HDR_DISPLAY)
OptionSet<ContentsFormat> PlatformScreen::screenContentsFormatsForTesting() const
{
    return m_properties.screenContentsFormatsForTesting;
}
#endif

void PlatformScreen::updateSingletonProperties(ScreenProperties&& properties)
{
    Locker locker { platformScreenLock() };
    Ref<PlatformScreen>& platformScreenRef = PlatformScreen::instance();

    // If we have the only reference, we can update in place
    if (platformScreenRef->hasOneRef())
        platformScreenRef->m_properties = WTF::move(properties);
    else
        platformScreenRef = PlatformScreen::create(WTF::move(properties));
}

#if HAVE(SUPPORT_HDR_DISPLAY)
void PlatformScreen::updateSingletonContentsFormatsForTesting(OptionSet<ContentsFormat> contentsFormats)
{
    auto properties = singleton()->screenProperties();
    properties.screenContentsFormatsForTesting = contentsFormats;
    updateSingletonProperties(WTF::move(properties));
}
#endif

} // namespace WebCore

#endif // PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
