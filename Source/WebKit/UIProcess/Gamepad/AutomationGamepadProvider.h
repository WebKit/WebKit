/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(GAMEPAD) && PLATFORM(GTK)

#include "AutomationGamepad.h"
#include <WebCore/GamepadProvider.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/WeakHashMap.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct AutomationGamepadInput {
    unsigned gamepadIndex;
    unsigned controlIndex;
    double value;
    bool isButton;
};

class AutomationGamepadProvider : public WebCore::GamepadProvider {
    WTF_MAKE_NONCOPYABLE(AutomationGamepadProvider);
    friend class NeverDestroyed<AutomationGamepadProvider>;
public:
    static AutomationGamepadProvider& LEGACY_NODELETE singleton();

    void startMonitoringGamepads(WebCore::GamepadProviderClient&) final;
    void stopMonitoringGamepads(WebCore::GamepadProviderClient&) final;
    const Vector<WeakPtr<WebCore::PlatformGamepad>>& platformGamepads() LIFETIME_BOUND final { return m_connectedGamepadVector; }
    bool isMockGamepadProvider() const final { return true; }
    void playEffect(unsigned, const String&, WebCore::GamepadHapticEffectType, const WebCore::GamepadEffectParameters&, CompletionHandler<void(bool)>&&) final;
    void stopEffects(unsigned, const String&, CompletionHandler<void()>&&) final;
    void clearGamepadsForTesting() final;

    void setAutomationGamepadDetails(unsigned index, const String& gamepadID, const String& mapping, unsigned axisCount, unsigned buttonCount, bool supportsDualRumble, bool wasConnected);
    bool connectAutomationGamepad(unsigned index);
    bool disconnectAutomationGamepad(unsigned index);
    bool applyInputFrame(const Vector<AutomationGamepadInput>&);

private:
    AutomationGamepadProvider();

    Vector<WeakPtr<WebCore::PlatformGamepad>> m_connectedGamepadVector;
    Vector<std::unique_ptr<AutomationGamepad>> m_mockGamepadVector;

};

}

#endif // ENABLE(GAMEPAD) && PLATFORM(GTK)
