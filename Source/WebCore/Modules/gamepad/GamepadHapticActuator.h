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

#if ENABLE(GAMEPAD)

#include "ActiveDOMObject.h"
#include "GamepadHapticEffectType.h"
#include "VisibilityChangeClient.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;
class Document;
class Gamepad;
struct GamepadEffectParameters;

class GamepadHapticActuator : public RefCounted<GamepadHapticActuator>, public ActiveDOMObject, public VisibilityChangeClient {
public:
    using EffectType = GamepadHapticEffectType;
    enum class Type : uint8_t { Vibration, DualRumble };
    enum class Result : uint8_t { Complete, Preempted };

    static Ref<GamepadHapticActuator> create(Document*, Type, Gamepad&);
    ~GamepadHapticActuator();

    Type type() const { return m_type; }
    bool canPlayEffectType(EffectType) const;
    void playEffect(EffectType, GamepadEffectParameters&&, Ref<DeferredPromise>&&);
    void reset(Ref<DeferredPromise>&&);

private:
    GamepadHapticActuator(Document*, Type, Gamepad&);

    Document* document();
    const Document* document() const;

    void stopEffects(CompletionHandler<void()>&&);
    RefPtr<DeferredPromise>& promiseForEffectType(EffectType);

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    void suspend(ReasonForSuspension) final;
    void stop() final;

    // VisibilityChangeClient.
    void visibilityStateChanged() final;

    Type m_type;
    WeakPtr<Gamepad> m_gamepad;
    RefPtr<DeferredPromise> m_dualRumbleEffectPromise;
    RefPtr<DeferredPromise> m_triggerRumbleEffectPromise;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
