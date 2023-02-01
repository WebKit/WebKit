/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ScreenOrientationProvider.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

Ref<ScreenOrientationProvider> ScreenOrientationProvider::create()
{
    return adoptRef(*new ScreenOrientationProvider);
}

ScreenOrientationProvider::ScreenOrientationProvider() = default;

ScreenOrientationProvider::~ScreenOrientationProvider()
{
    platformStopListeningForChanges();
}

void ScreenOrientationProvider::addObserver(Observer& observer)
{
    bool wasEmpty = m_observers.isEmptyIgnoringNullReferences();
    m_observers.add(observer);
    if (wasEmpty)
        platformStartListeningForChanges();
}

void ScreenOrientationProvider::removeObserver(Observer& observer)
{
    m_observers.remove(observer);
    if (m_observers.isEmptyIgnoringNullReferences()) {
        m_currentOrientation = std::nullopt;
        platformStopListeningForChanges();
    }
}

void ScreenOrientationProvider::screenOrientationDidChange()
{
    auto newOrientation = platformCurrentOrientation();
    if (!newOrientation || newOrientation == m_currentOrientation)
        return;

    m_currentOrientation = *newOrientation;
    for (auto& observer : m_observers)
        observer.screenOrientationDidChange(*newOrientation);
}

ScreenOrientationType ScreenOrientationProvider::currentOrientation()
{
    if (m_currentOrientation)
        return *m_currentOrientation;

    auto orientation = platformCurrentOrientation().value_or(ScreenOrientationType::PortraitPrimary);
    if (!m_observers.isEmptyIgnoringNullReferences())
        m_currentOrientation = orientation;
    return orientation;
}

#if !PLATFORM(IOS_FAMILY)
std::optional<ScreenOrientationType> ScreenOrientationProvider::platformCurrentOrientation()
{
    return std::nullopt;
}

void ScreenOrientationProvider::platformStartListeningForChanges()
{
}

void ScreenOrientationProvider::platformStopListeningForChanges()
{
}
#endif

} // namespace WebCore
