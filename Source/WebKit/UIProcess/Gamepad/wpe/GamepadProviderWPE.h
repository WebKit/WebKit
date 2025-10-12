/*
 * Copyright (C) 2025 Igalia S.L.
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

#if ENABLE(GAMEPAD) && ENABLE(WPE_PLATFORM)
#include <WebCore/GamepadProvider.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _WPEGamepad WPEGamepad;
typedef struct _WPEGamepadManager WPEGamepadManager;

namespace WebKit {
class PlatformGamepadWPE;

class GamepadProviderWPE final : public WebCore::GamepadProvider {
    WTF_MAKE_NONCOPYABLE(GamepadProviderWPE);
    friend class NeverDestroyed<GamepadProviderWPE>;
public:
    static GamepadProviderWPE& singleton();
    virtual ~GamepadProviderWPE();

    // Do nothing since this is a singleton.
    void ref() const { }
    void deref() const { }

    enum class ShouldMakeGamepadsVisible : bool { No, Yes };
    void notifyInput(PlatformGamepadWPE&, ShouldMakeGamepadsVisible);

    void startMonitoringInput();
    void stopMonitoringInput();

private:
    GamepadProviderWPE();

    enum class IsInitialDevice : bool { No, Yes };
    void gamepadConnected(WPEGamepad*, IsInitialDevice);
    void gamepadDisconnected(WPEGamepad*);

    void startMonitoringGamepads(WebCore::GamepadProviderClient&) final;
    void stopMonitoringGamepads(WebCore::GamepadProviderClient&) final;
    const Vector<WeakPtr<WebCore::PlatformGamepad>>& platformGamepads() final { return m_gamepadVector; }
    void playEffect(unsigned, const String&, WebCore::GamepadHapticEffectType, const WebCore::GamepadEffectParameters&, CompletionHandler<void(bool)>&&) final;
    void stopEffects(unsigned, const String&, CompletionHandler<void()>&&) final;

    void inputNotificationTimerFired();

    GRefPtr<WPEGamepadManager> m_manager;
    Vector<WeakPtr<WebCore::PlatformGamepad>> m_gamepadVector;
    HashMap<WPEGamepad*, std::unique_ptr<PlatformGamepadWPE>> m_gamepadMap;
    bool m_isMonitoringInput { false };
    RunLoop::Timer m_inputNotificationTimer;
};

} // namespace WebKit

#endif // ENABLE(GAMEPAD) && ENABLE(WPE_PLATFORM)
