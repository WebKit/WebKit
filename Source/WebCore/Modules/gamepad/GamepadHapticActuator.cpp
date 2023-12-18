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

static bool areEffectParametersValid(GamepadHapticEffectType effectType, const GamepadEffectParameters& parameters)
{
    if (parameters.duration < 0 || parameters.startDelay < 0)
        return false;

    if (effectType == GamepadHapticEffectType::DualRumble) {
        if (parameters.weakMagnitude < 0 || parameters.strongMagnitude < 0 || parameters.weakMagnitude > 1 || parameters.strongMagnitude > 1)
            return false;
    }
    if (effectType == GamepadHapticEffectType::TriggerRumble) {
        if (parameters.leftTrigger < 0 || parameters.rightTrigger < 0 || parameters.leftTrigger > 1 || parameters.rightTrigger > 1)
            return false;
    }
    return true;
}

Ref<GamepadHapticActuator> GamepadHapticActuator::create(Document* document, Type type, Gamepad& gamepad)
{
    auto actuator = adoptRef(*new GamepadHapticActuator(document, type, gamepad));
    actuator->suspendIfNeeded();
    return actuator;
}

GamepadHapticActuator::GamepadHapticActuator(Document* document, Type type, Gamepad& gamepad)
    : ActiveDOMObject(document)
    , m_type { type }
    , m_gamepad { gamepad }
{
    if (document)
        document->registerForVisibilityStateChangedCallbacks(*this);
}

GamepadHapticActuator::~GamepadHapticActuator() = default;

bool GamepadHapticActuator::canPlayEffectType(EffectType effectType) const
{
    if (effectType == EffectType::TriggerRumble && (!document() || !document()->settings().gamepadTriggerRumbleEnabled()))
        return false;

    return m_gamepad && m_gamepad->supportedEffectTypes().contains(effectType);
}

void GamepadHapticActuator::playEffect(EffectType effectType, GamepadEffectParameters&& effectParameters, Ref<DeferredPromise>&& promise)
{
    if (!areEffectParametersValid(effectType, effectParameters)) {
        promise->reject(Exception { ExceptionCode::TypeError, "Invalid effect parameter"_s });
        return;
    }

    auto document = this->document();
    if (!document || !document->isFullyActive() || document->hidden() || !m_gamepad) {
        promise->resolve<IDLEnumeration<Result>>(Result::Preempted);
        return;
    }
    auto& currentEffectPromise = promiseForEffectType(effectType);
    if (auto playingEffectPromise = std::exchange(currentEffectPromise, nullptr)) {
        queueTaskKeepingObjectAlive(*this, TaskSource::Gamepad, [playingEffectPromise = WTFMove(playingEffectPromise)] {
            playingEffectPromise->resolve<IDLEnumeration<Result>>(Result::Preempted);
        });
    }
    if (!canPlayEffectType(effectType)) {
        promise->reject(Exception { ExceptionCode::NotSupportedError, "This gamepad doesn't support playing such effect"_s });
        return;
    }

    effectParameters.duration = std::min(effectParameters.duration, GamepadEffectParameters::maximumDuration.milliseconds());

    currentEffectPromise = WTFMove(promise);
    GamepadProvider::singleton().playEffect(m_gamepad->index(), m_gamepad->id(), effectType, effectParameters, [this, protectedThis = makePendingActivity(*this), playingEffectPromise = currentEffectPromise, effectType](bool success) mutable {
        auto& currentEffectPromise = promiseForEffectType(effectType);
        if (playingEffectPromise != currentEffectPromise)
            return; // Was already pre-empted.
        queueTaskKeepingObjectAlive(*this, TaskSource::Gamepad, [playingEffectPromise = std::exchange(currentEffectPromise, nullptr), success] {
            playingEffectPromise->resolve<IDLEnumeration<Result>>(success ? Result::Complete : Result::Preempted);
        });
    });
}

void GamepadHapticActuator::reset(Ref<DeferredPromise>&& promise)
{
    auto document = this->document();
    if (!document || !document->isFullyActive() || document->hidden() || !m_gamepad) {
        promise->resolve<IDLEnumeration<Result>>(Result::Preempted);
        return;
    }
    stopEffects([this, protectedThis = makePendingActivity(*this), promise = WTFMove(promise)]() mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::Gamepad, [promise = WTFMove(promise)] {
            promise->resolve<IDLEnumeration<Result>>(Result::Complete);
        });
    });
}

void GamepadHapticActuator::stopEffects(CompletionHandler<void()>&& completionHandler)
{
    if (!m_triggerRumbleEffectPromise && !m_dualRumbleEffectPromise)
        return completionHandler();

    auto dualRumbleEffectPromise = std::exchange(m_dualRumbleEffectPromise, nullptr);
    auto triggerRumbleEffectPromise = std::exchange(m_triggerRumbleEffectPromise, nullptr);
    queueTaskKeepingObjectAlive(*this, TaskSource::Gamepad, [dualRumbleEffectPromise = WTFMove(dualRumbleEffectPromise), triggerRumbleEffectPromise = WTFMove(triggerRumbleEffectPromise)] {
        if (dualRumbleEffectPromise)
            dualRumbleEffectPromise->resolve<IDLEnumeration<Result>>(Result::Preempted);
        if (triggerRumbleEffectPromise)
            triggerRumbleEffectPromise->resolve<IDLEnumeration<Result>>(Result::Preempted);
    });
    GamepadProvider::singleton().stopEffects(m_gamepad->index(), m_gamepad->id(), WTFMove(completionHandler));
}

Document* GamepadHapticActuator::document()
{
    return downcast<Document>(scriptExecutionContext());
}

const Document* GamepadHapticActuator::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

const char* GamepadHapticActuator::activeDOMObjectName() const
{
    return "GamepadHapticActuator";
}

void GamepadHapticActuator::suspend(ReasonForSuspension)
{
    stopEffects([] { });
}

void GamepadHapticActuator::stop()
{
    stopEffects([] { });
}

void GamepadHapticActuator::visibilityStateChanged()
{
    RefPtr document = this->document();
    if (!document || !document->hidden())
        return;
    stopEffects([] { });
}

RefPtr<DeferredPromise>& GamepadHapticActuator::promiseForEffectType(EffectType effectType)
{
    switch (effectType) {
    case EffectType::TriggerRumble:
        return m_triggerRumbleEffectPromise;
    case EffectType::DualRumble:
        break;
    }
    return m_dualRumbleEffectPromise;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
