/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#if ENABLE(GAMEPAD) && OS(LINUX)

#include "GamepadProvider.h"
#include <libmanette.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>

namespace WebCore {

class ManetteGamepad;
class GamepadProviderClient;

class ManetteGamepadProvider final : public GamepadProvider {
    WTF_MAKE_NONCOPYABLE(ManetteGamepadProvider);
    friend class NeverDestroyed<ManetteGamepadProvider>;
public:
    static ManetteGamepadProvider& singleton();

    virtual ~ManetteGamepadProvider();

    void startMonitoringGamepads(GamepadProviderClient&) final;
    void stopMonitoringGamepads(GamepadProviderClient&) final;
    const Vector<PlatformGamepad*>& platformGamepads() final { return m_gamepadVector; }

    void deviceConnected(ManetteDevice*);
    void deviceDisconnected(ManetteDevice*);

    enum class ShouldMakeGamepadsVisible : bool { No, Yes };
    void gamepadHadInput(ManetteGamepad&, ShouldMakeGamepadsVisible);

private:
    ManetteGamepadProvider();

    std::unique_ptr<ManetteGamepad> removeGamepadForDevice(ManetteDevice*);

    unsigned indexForNewlyConnectedDevice();
    void initialGamepadsConnectedTimerFired();
    void inputNotificationTimerFired();

    Vector<PlatformGamepad*> m_gamepadVector;
    HashMap<ManetteDevice*, std::unique_ptr<ManetteGamepad>> m_gamepadMap;
    bool m_initialGamepadsConnected { false };

    GRefPtr<ManetteMonitor> m_monitor;
    RunLoop::Timer m_initialGamepadsConnectedTimer;
    RunLoop::Timer m_inputNotificationTimer;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && OS(LINUX)
