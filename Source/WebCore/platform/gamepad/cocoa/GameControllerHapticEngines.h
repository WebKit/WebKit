/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)

#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS CHHapticEngine;
OBJC_CLASS GCController;

namespace WebCore {
class GameControllerHapticEngines;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::GameControllerHapticEngines> : std::true_type { };
}

namespace WebCore {

class GameControllerHapticEffect;
struct GamepadEffectParameters;
enum class GamepadHapticEffectType : uint8_t;

class GameControllerHapticEngines : public CanMakeWeakPtr<GameControllerHapticEngines> {
    WTF_MAKE_TZONE_ALLOCATED(GameControllerHapticEngines);
public:
    static std::unique_ptr<GameControllerHapticEngines> create(GCController *);
    ~GameControllerHapticEngines();

    void playEffect(GamepadHapticEffectType, const GamepadEffectParameters&, CompletionHandler<void(bool)>&&);
    void stopEffects();

    void stop(CompletionHandler<void()>&&);

    CHHapticEngine *leftHandleEngine() { return m_leftHandleEngine.get(); }
    CHHapticEngine *rightHandleEngine() { return m_rightHandleEngine.get(); }
    CHHapticEngine *leftTriggerEngine() { return m_leftTriggerEngine.get(); }
    CHHapticEngine *rightTriggerEngine() { return m_rightTriggerEngine.get(); }

private:
    explicit GameControllerHapticEngines(GCController *);

    void ensureStarted(GamepadHapticEffectType, CompletionHandler<void(bool)>&&);
    std::unique_ptr<GameControllerHapticEffect>& currentEffectForType(GamepadHapticEffectType);

    RetainPtr<CHHapticEngine> m_leftHandleEngine;
    RetainPtr<CHHapticEngine> m_rightHandleEngine;
    RetainPtr<CHHapticEngine> m_leftTriggerEngine;
    RetainPtr<CHHapticEngine> m_rightTriggerEngine;
    bool m_failedToStartLeftHandleEngine { false };
    bool m_failedToStartRightHandleEngine { false };
    bool m_failedToStartLeftTriggerEngine { false };
    bool m_failedToStartRightTriggerEngine { false };
    std::unique_ptr<GameControllerHapticEffect> m_currentDualRumbleEffect;
    std::unique_ptr<GameControllerHapticEffect> m_currentTriggerRumbleEffect;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)
