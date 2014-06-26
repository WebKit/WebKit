/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "NavigatorGamepad.h"

#if ENABLE(GAMEPAD)

#include "DocumentLoader.h"
#include "Frame.h"
#include "Gamepad.h"
#include "GamepadManager.h"
#include "GamepadStrategy.h"
#include "Navigator.h"
#include "PlatformGamepad.h"
#include "PlatformStrategies.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebCore {

NavigatorGamepad::NavigatorGamepad()
    : m_navigationStart(std::numeric_limits<double>::infinity())
{
    GamepadManager::shared().registerNavigator(this);
}

NavigatorGamepad::~NavigatorGamepad()
{
    GamepadManager::shared().unregisterNavigator(this);
}

const char* NavigatorGamepad::supplementName()
{
    return "NavigatorGamepad";
}

NavigatorGamepad* NavigatorGamepad::from(Navigator* navigator)
{
    NavigatorGamepad* supplement = static_cast<NavigatorGamepad*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = std::make_unique<NavigatorGamepad>();
        supplement = newSupplement.get();
        provideTo(navigator, supplementName(), std::move(newSupplement));

        if (Frame* frame = navigator->frame()) {
            if (DocumentLoader* documentLoader = frame->loader().documentLoader())
                supplement->m_navigationStart = documentLoader->timing()->navigationStart();
        }
    }
    return supplement;
}

const Vector<RefPtr<Gamepad>>& NavigatorGamepad::getGamepads(Navigator* navigator)
{
    return NavigatorGamepad::from(navigator)->gamepads();
}

const Vector<RefPtr<Gamepad>>& NavigatorGamepad::gamepads()
{
    if (m_gamepads.isEmpty())
        return m_gamepads;

    const Vector<PlatformGamepad*>& platformGamepads = platformStrategies()->gamepadStrategy()->platformGamepads();

    for (unsigned i = 0; i < platformGamepads.size(); ++i) {
        if (!platformGamepads[i]) {
            ASSERT(!m_gamepads[i]);
            continue;
        }

        ASSERT(m_gamepads[i]);
        m_gamepads[i]->updateFromPlatformGamepad(*platformGamepads[i]);
    }

    return m_gamepads;
}

void NavigatorGamepad::gamepadsBecameVisible()
{
    const Vector<PlatformGamepad*>& platformGamepads = platformStrategies()->gamepadStrategy()->platformGamepads();
    m_gamepads.resize(platformGamepads.size());

    for (unsigned i = 0; i < platformGamepads.size(); ++i) {
        if (!platformGamepads[i])
            continue;

        m_gamepads[i] = Gamepad::create(*platformGamepads[i], i);
    }
}

void NavigatorGamepad::gamepadConnected(unsigned index)
{
    // If this is the first gamepad this Navigator object has seen, then all gamepads just became visible.
    if (m_gamepads.isEmpty()) {
        gamepadsBecameVisible();
        return;
    }

    // The new index should already fit in the existing array, or should be exactly one past-the-end of the existing array.
    ASSERT(index <= m_gamepads.size());

    const Vector<PlatformGamepad*>& platformGamepads = platformStrategies()->gamepadStrategy()->platformGamepads();
    ASSERT(index < platformGamepads.size());
    ASSERT(platformGamepads[index]);

    if (index < m_gamepads.size())
        m_gamepads[index] = Gamepad::create(*platformGamepads[index], index);
    else if (index == m_gamepads.size())
        m_gamepads.append(Gamepad::create(*platformGamepads[index], index));
}

void NavigatorGamepad::gamepadDisconnected(unsigned index)
{
    // If this Navigator hasn't seen any gamepads yet its Vector will still be empty.
    if (!m_gamepads.size())
        return;

    ASSERT(index < m_gamepads.size());
    ASSERT(m_gamepads[index]);

    m_gamepads[index] = nullptr;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
