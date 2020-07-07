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

#include <WebCore/GamepadProvider.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {
enum class EventMakesGamepadsVisible : bool;
}

namespace WebKit {

class SharedMemory;
class WebGamepad;

class GamepadData;

class WebGamepadProvider : public WebCore::GamepadProvider {
public:
    static WebGamepadProvider& singleton();

    void gamepadConnected(const GamepadData&, WebCore::EventMakesGamepadsVisible);
    void gamepadDisconnected(unsigned index);
    void gamepadActivity(const Vector<GamepadData>&, WebCore::EventMakesGamepadsVisible);

    void setInitialGamepads(const Vector<GamepadData>&);

private:
    friend NeverDestroyed<WebGamepadProvider>;
    WebGamepadProvider();
    ~WebGamepadProvider() final;
    
    void startMonitoringGamepads(WebCore::GamepadProviderClient&) final;
    void stopMonitoringGamepads(WebCore::GamepadProviderClient&) final;
    const Vector<WebCore::PlatformGamepad*>& platformGamepads() final;

    HashSet<WebCore::GamepadProviderClient*> m_clients;

    Vector<std::unique_ptr<WebGamepad>> m_gamepads;
    Vector<WebCore::PlatformGamepad*> m_rawGamepads;
};

} // namespace WebKit

#endif // ENABLE(GAMEPAD)
