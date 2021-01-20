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

#include "DOMWindow.h"
#include "Gamepad.h"
#include "GamepadManager.h"
#include "GamepadProvider.h"
#include "Navigator.h"
#include "PlatformGamepad.h"

namespace WebCore {

NavigatorGamepad::NavigatorGamepad()
{
    GamepadManager::singleton().registerNavigator(this);
}

NavigatorGamepad::~NavigatorGamepad()
{
    GamepadManager::singleton().unregisterNavigator(this);
}

const char* NavigatorGamepad::supplementName()
{
    return "NavigatorGamepad";
}

NavigatorGamepad* NavigatorGamepad::from(Navigator* navigator)
{
    NavigatorGamepad* supplement = static_cast<NavigatorGamepad*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorGamepad>();
        supplement = newSupplement.get();
        provideTo(navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

Ref<Gamepad> NavigatorGamepad::gamepadFromPlatformGamepad(PlatformGamepad& platformGamepad)
{
    unsigned index = platformGamepad.index();
    if (index >= m_gamepads.size() || !m_gamepads[index])
        return Gamepad::create(platformGamepad);

    return *m_gamepads[index];
}

const Vector<RefPtr<Gamepad>>& NavigatorGamepad::getGamepads(Navigator& navigator)
{
    auto* domWindow = navigator.window();
    Document* document = domWindow ? domWindow->document() : nullptr;
    if (!document) {
        static NeverDestroyed<Vector<RefPtr<Gamepad>>> emptyGamepads;
        return emptyGamepads;
    }

    if (!document->isSecureContext()) {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [document] {
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Navigator.getGamepads() will be removed from insecure contexts in a future release"_s);
        });

    }

    return NavigatorGamepad::from(&navigator)->gamepads();
}

const Vector<RefPtr<Gamepad>>& NavigatorGamepad::gamepads()
{
    if (m_gamepads.isEmpty())
        return m_gamepads;

    const Vector<PlatformGamepad*>& platformGamepads = GamepadProvider::singleton().platformGamepads();

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
    const Vector<PlatformGamepad*>& platformGamepads = GamepadProvider::singleton().platformGamepads();
    m_gamepads.resize(platformGamepads.size());

    for (unsigned i = 0; i < platformGamepads.size(); ++i) {
        if (!platformGamepads[i])
            continue;

        m_gamepads[i] = Gamepad::create(*platformGamepads[i]);
    }
}

void NavigatorGamepad::gamepadConnected(PlatformGamepad& platformGamepad)
{
    // If this is the first gamepad this Navigator object has seen, then all gamepads just became visible.
    if (m_gamepads.isEmpty()) {
        gamepadsBecameVisible();
        return;
    }

    unsigned index = platformGamepad.index();
    ASSERT(GamepadProvider::singleton().platformGamepads()[index] == &platformGamepad);

    // The new index should already fit in the existing array, or should be exactly one past-the-end of the existing array.
    ASSERT(index <= m_gamepads.size());

    if (index < m_gamepads.size())
        m_gamepads[index] = Gamepad::create(platformGamepad);
    else if (index == m_gamepads.size())
        m_gamepads.append(Gamepad::create(platformGamepad));
}

void NavigatorGamepad::gamepadDisconnected(PlatformGamepad& platformGamepad)
{
    // If this Navigator hasn't seen any gamepads yet its Vector will still be empty.
    if (!m_gamepads.size())
        return;

    ASSERT(platformGamepad.index() < m_gamepads.size());
    ASSERT(m_gamepads[platformGamepad.index()]);

    m_gamepads[platformGamepad.index()] = nullptr;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
