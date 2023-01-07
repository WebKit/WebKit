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

#include "config.h"

#if ENABLE(GAMEPAD)
#include "GamepadHapticActuator.h"

#include "Document.h"
#include "EventLoop.h"
#include "Gamepad.h"
#include "GamepadEffectParameters.h"
#include "GamepadProvider.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

Ref<GamepadHapticActuator> GamepadHapticActuator::create(Gamepad& gamepad)
{
    return adoptRef(*new GamepadHapticActuator(gamepad));
}

GamepadHapticActuator::GamepadHapticActuator(Gamepad& gamepad)
    : m_type { Type::Vibration }
    , m_gamepad { gamepad }
{
}

GamepadHapticActuator::~GamepadHapticActuator() = default;

bool GamepadHapticActuator::canPlayEffectType(EffectType effectType) const
{
    return m_gamepad && m_gamepad->supportedEffectTypes().contains(effectType);
}

void GamepadHapticActuator::playEffect(Document& document, EffectType effectType, GamepadEffectParameters&& effectParameters, Ref<DeferredPromise>&& promise)
{
    if (!document.isFullyActive() || document.hidden() || !m_gamepad) {
        promise->resolve<IDLEnumeration<Result>>(Result::Preempted);
        return;
    }
    if (auto playingEffectPromise = std::exchange(m_playingEffectPromise, nullptr)) {
        document.eventLoop().queueTask(TaskSource::Gamepad, [playingEffectPromise = WTFMove(playingEffectPromise)] {
            playingEffectPromise->resolve<IDLEnumeration<Result>>(Result::Preempted);
        });
    }
    if (!canPlayEffectType(effectType)) {
        promise->reject(Exception { NotSupportedError, "This gamepad doesn't support playing such effect"_s });
        return;
    }
    m_playingEffectPromise = WTFMove(promise);
    GamepadProvider::singleton().playEffect(m_gamepad->index(), m_gamepad->id(), effectType, effectParameters, [this, protectedThis = Ref { *this }, document = Ref { document }, playingEffectPromise = m_playingEffectPromise](bool success) {
        if (m_playingEffectPromise != playingEffectPromise)
            return; // Was already pre-empted.
        document->eventLoop().queueTask(TaskSource::Gamepad, [playingEffectPromise = std::exchange(m_playingEffectPromise, nullptr), success] {
            playingEffectPromise->resolve<IDLEnumeration<Result>>(success ? Result::Complete : Result::Preempted);
        });
    });
}

void GamepadHapticActuator::reset(Document& document, Ref<DeferredPromise>&& promise)
{
    if (!document.isFullyActive() || document.hidden() || !m_gamepad) {
        promise->resolve<IDLEnumeration<Result>>(Result::Preempted);
        return;
    }
    if (auto playingEffectPromise = std::exchange(m_playingEffectPromise, nullptr)) {
        document.eventLoop().queueTask(TaskSource::Gamepad, [playingEffectPromise = WTFMove(playingEffectPromise)] {
            playingEffectPromise->resolve<IDLEnumeration<Result>>(Result::Preempted);
        });
    }
    GamepadProvider::singleton().stopEffects(m_gamepad->index(), m_gamepad->id(), [document = Ref { document }, promise = WTFMove(promise)]() mutable {
        document->eventLoop().queueTask(TaskSource::Gamepad, [promise = WTFMove(promise)] {
            promise->resolve<IDLEnumeration<Result>>(Result::Complete);
        });
    });
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
