/*
 * Copyright (C) 2020 RDK Management  All rights reserved.
 * Copyright (C) 2022 Igalia S.L.
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

#if ENABLE(GAMEPAD) && USE(LIBWPE)

#include "GamepadProvider.h"
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>

struct wpe_gamepad;
struct wpe_gamepad_provider;
struct wpe_view_backend;

namespace WebCore {

class GamepadLibWPE;

class GamepadProviderLibWPE final : public GamepadProvider {
    WTF_MAKE_NONCOPYABLE(GamepadProviderLibWPE);
    friend class NeverDestroyed<GamepadProviderLibWPE>;

public:
    static GamepadProviderLibWPE& singleton();

    virtual ~GamepadProviderLibWPE();

    void startMonitoringGamepads(GamepadProviderClient&) final;
    void stopMonitoringGamepads(GamepadProviderClient&) final;
    const Vector<PlatformGamepad*>& platformGamepads() final { return m_gamepadVector; }

    enum class ShouldMakeGamepadsVisible : bool { No,
        Yes };
    void scheduleInputNotification(GamepadLibWPE&, ShouldMakeGamepadsVisible);

    struct wpe_view_backend* inputView();

private:
    GamepadProviderLibWPE();

    void gamepadConnected(unsigned);
    void gamepadDisconnected(unsigned);
    std::unique_ptr<GamepadLibWPE> removeGamepadForId(unsigned);

    unsigned indexForNewlyConnectedDevice();
    void initialGamepadsConnectedTimerFired();
    void inputNotificationTimerFired();

    Vector<PlatformGamepad*> m_gamepadVector;
    HashMap<unsigned, std::unique_ptr<GamepadLibWPE>> m_gamepadMap;
    bool m_initialGamepadsConnected { false };

    std::unique_ptr<struct wpe_gamepad_provider, void (*)(struct wpe_gamepad_provider*)> m_provider;
    struct wpe_gamepad* m_lastActiveGamepad { nullptr };

    RunLoop::Timer<GamepadProviderLibWPE> m_initialGamepadsConnectedTimer;
    RunLoop::Timer<GamepadProviderLibWPE> m_inputNotificationTimer;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && USE(LIBWPE)
