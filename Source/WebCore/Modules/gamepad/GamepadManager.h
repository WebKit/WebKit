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

#pragma once

#if ENABLE(GAMEPAD)

#include "GamepadProviderClient.h"
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class Gamepad;
class LocalDOMWindow;
class NavigatorGamepad;
class WeakPtrImplWithEventTargetData;

class GamepadManager : public GamepadProviderClient {
    WTF_MAKE_NONCOPYABLE(GamepadManager);
    friend class NeverDestroyed<GamepadManager>;
public:
    static GamepadManager& singleton();

    void platformGamepadConnected(PlatformGamepad&, EventMakesGamepadsVisible) final;
    void platformGamepadDisconnected(PlatformGamepad&) final;
    void platformGamepadInputActivity(EventMakesGamepadsVisible) final;

    void registerNavigator(NavigatorGamepad&);
    void unregisterNavigator(NavigatorGamepad&);
    void registerDOMWindow(LocalDOMWindow&);
    void unregisterDOMWindow(LocalDOMWindow&);

private:
    GamepadManager();

    void makeGamepadVisible(PlatformGamepad&, WeakHashSet<NavigatorGamepad>&, WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData>&);
    void dispatchGamepadEvent(const AtomString& eventName, PlatformGamepad&);

    void maybeStartMonitoringGamepads();
    void maybeStopMonitoringGamepads();

    bool m_isMonitoringGamepads;

    WeakHashSet<NavigatorGamepad> m_navigators;
    WeakHashSet<NavigatorGamepad> m_gamepadBlindNavigators;
    WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData> m_domWindows;
    WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData> m_gamepadBlindDOMWindows;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
