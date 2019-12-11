/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/GamepadProviderClient.h>
#include <wtf/HashSet.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

namespace WebKit {

class UIGamepad;
class WebPageProxy;
class WebProcessPool;
class GamepadData;

class UIGamepadProvider : public WebCore::GamepadProviderClient {
public:
    static UIGamepadProvider& singleton();

    void processPoolStartedUsingGamepads(WebProcessPool&);
    void processPoolStoppedUsingGamepads(WebProcessPool&);

    void viewBecameActive(WebPageProxy&);
    void viewBecameInactive(WebPageProxy&);

    Vector<GamepadData> gamepadStates() const;

#if PLATFORM(COCOA)
    static void setUsesGameControllerFramework();
#endif

    Vector<GamepadData> snapshotGamepads();

private:
    friend NeverDestroyed<UIGamepadProvider>;
    UIGamepadProvider();
    ~UIGamepadProvider() final;

    void startMonitoringGamepads();
    void stopMonitoringGamepads();

    void platformSetDefaultGamepadProvider();
    WebPageProxy* platformWebPageProxyForGamepadInput();
    void platformStopMonitoringInput();
    void platformStartMonitoringInput();

    void setInitialConnectedGamepads(const Vector<WebCore::PlatformGamepad*>&) final;
    void platformGamepadConnected(WebCore::PlatformGamepad&) final;
    void platformGamepadDisconnected(WebCore::PlatformGamepad&) final;
    void platformGamepadInputActivity(bool shouldMakeGamepadsVisible) final;

    void scheduleGamepadStateSync();
    void gamepadSyncTimerFired();

    HashSet<WebProcessPool*> m_processPoolsUsingGamepads;

    Vector<std::unique_ptr<UIGamepad>> m_gamepads;

    RunLoop::Timer<UIGamepadProvider> m_gamepadSyncTimer;

    bool m_isMonitoringGamepads { false };
    bool m_hasInitialGamepads { false };
    bool m_shouldMakeGamepadsVisibleOnSync { false };
};

}

#endif // ENABLE(GAMEPAD)
