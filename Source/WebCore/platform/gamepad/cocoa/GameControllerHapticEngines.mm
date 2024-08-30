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

#import "config.h"

#if ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)
#import "GameControllerHapticEngines.h"

#import "GameControllerHapticEffect.h"
#import "GamepadEffectParameters.h"
#import "GamepadHapticEffectType.h"
#import "Logging.h"
#import <GameController/GameController.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/CompletionHandler.h>
#import <wtf/TZoneMallocInlines.h>

#import "GameControllerSoftLink.h"
#import "CoreHapticsSoftLink.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GameControllerHapticEngines);

std::unique_ptr<GameControllerHapticEngines> GameControllerHapticEngines::create(GCController *gamepad)
{
    return std::unique_ptr<GameControllerHapticEngines>(new GameControllerHapticEngines(gamepad));
}

GameControllerHapticEngines::GameControllerHapticEngines(GCController *gamepad)
    : m_leftHandleEngine([gamepad.haptics createEngineWithLocality:get_GameController_GCHapticsLocalityLeftHandle()])
    , m_rightHandleEngine([gamepad.haptics createEngineWithLocality:get_GameController_GCHapticsLocalityRightHandle()])
    , m_leftTriggerEngine([gamepad.haptics createEngineWithLocality:get_GameController_GCHapticsLocalityLeftTrigger()])
    , m_rightTriggerEngine([gamepad.haptics createEngineWithLocality:get_GameController_GCHapticsLocalityRightTrigger()])
{
}

GameControllerHapticEngines::~GameControllerHapticEngines() = default;

std::unique_ptr<GameControllerHapticEffect>& GameControllerHapticEngines::currentEffectForType(GamepadHapticEffectType type)
{
    switch (type) {
    case GamepadHapticEffectType::DualRumble:
        return m_currentDualRumbleEffect;
    case GamepadHapticEffectType::TriggerRumble:
        return m_currentTriggerRumbleEffect;
    }
    ASSERT_NOT_REACHED();
    return m_currentDualRumbleEffect;
}

void GameControllerHapticEngines::playEffect(GamepadHapticEffectType type, const GamepadEffectParameters& parameters, CompletionHandler<void(bool)>&& completionHandler)
{
    auto& currentEffect = currentEffectForType(type);

    // Trying to create pattern players with a 0 duration will fail. However, Games on XBox seem to use such
    // requests to stop vibrating.
    if (!parameters.duration) {
        if (auto effect = std::exchange(currentEffect, nullptr))
            effect->stop();
        return completionHandler(true);
    }

    auto newEffect = GameControllerHapticEffect::create(*this, type, parameters);
    if (!newEffect)
        return completionHandler(false);

    if (currentEffect)
        currentEffect->stop();

    currentEffect = WTFMove(newEffect);
    ensureStarted(type, [weakThis = WeakPtr { *this }, type, effect = WeakPtr { *currentEffect }, completionHandler = WTFMove(completionHandler)](bool success) mutable {
        if (!weakThis)
            return completionHandler(false);

        auto& currentEffect = weakThis->currentEffectForType(type);
        if (!currentEffect || currentEffect.get() != effect.get())
            return completionHandler(false);

        if (!success) {
            currentEffect = nullptr;
            return completionHandler(false);
        }

        currentEffect->start([weakThis = WTFMove(weakThis), effect = WTFMove(effect), type, completionHandler = WTFMove(completionHandler)](bool success) mutable {
            completionHandler(success);
            if (!weakThis)
                return;
            auto& currentEffect = weakThis->currentEffectForType(type);
            if (currentEffect.get() == effect.get())
                currentEffect = nullptr;
        });
    });
}

void GameControllerHapticEngines::stopEffects()
{
    if (auto currentEffect = std::exchange(m_currentDualRumbleEffect, nullptr))
        currentEffect->stop();
    if (auto currentEffect = std::exchange(m_currentTriggerRumbleEffect, nullptr))
        currentEffect->stop();
}

void GameControllerHapticEngines::ensureStarted(GamepadHapticEffectType effectType, CompletionHandler<void(bool)>&& completionHandler)
{
    auto callbackAggregator = MainRunLoopCallbackAggregator::create([weakThis = WeakPtr { *this }, effectType, completionHandler = WTFMove(completionHandler)]() mutable {
        bool success = false;
        switch (effectType) {
        case GamepadHapticEffectType::DualRumble:
            success = weakThis && !weakThis->m_failedToStartLeftHandleEngine && !weakThis->m_failedToStartRightHandleEngine;
            break;
        case GamepadHapticEffectType::TriggerRumble:
            success = weakThis && !weakThis->m_failedToStartLeftTriggerEngine && !weakThis->m_failedToStartRightTriggerEngine;
            break;
        }
        completionHandler(success);
    });
    auto startEngine = [weakThis = WeakPtr { *this }](CHHapticEngine *engine, CompletionHandler<void(bool)>&& completionHandler, std::function<void()>&& playersFinished) mutable {
        [engine startWithCompletionHandler:makeBlockPtr([weakThis = WTFMove(weakThis), engine, completionHandler = WTFMove(completionHandler), playersFinished](NSError* error) mutable {
            ensureOnMainRunLoop([completionHandler = WTFMove(completionHandler), success = !error]() mutable {
                completionHandler(success);
            });
            if (error)
                RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::ensureStarted: Failed to start haptic engine");
            [engine notifyWhenPlayersFinished:makeBlockPtr([playersFinished](NSError *) mutable -> CHHapticEngineFinishedAction {
                ensureOnMainRunLoop([playersFinished] {
                    playersFinished();
                });
                return CHHapticEngineFinishedActionLeaveEngineRunning;
            }).get()];
        }).get()];
    };
    switch (effectType) {
    case GamepadHapticEffectType::DualRumble:
        startEngine(m_leftHandleEngine.get(), [weakThis = WeakPtr { *this }, callbackAggregator](bool success) {
            if (weakThis)
                weakThis->m_failedToStartLeftHandleEngine = !success;
        }, [weakThis = WeakPtr { *this }] {
            if (weakThis && weakThis->m_currentDualRumbleEffect)
                weakThis->m_currentDualRumbleEffect->leftEffectFinishedPlaying();
        });
        startEngine(m_rightHandleEngine.get(), [weakThis = WeakPtr { *this }, callbackAggregator](bool success) {
            if (weakThis)
                weakThis->m_failedToStartRightHandleEngine = !success;
        }, [weakThis = WeakPtr { *this }] {
            if (weakThis && weakThis->m_currentDualRumbleEffect)
                weakThis->m_currentDualRumbleEffect->rightEffectFinishedPlaying();
        });
        break;
    case GamepadHapticEffectType::TriggerRumble:
        startEngine(m_leftTriggerEngine.get(), [weakThis = WeakPtr { *this }, callbackAggregator](bool success) {
            if (weakThis)
                weakThis->m_failedToStartLeftTriggerEngine = !success;
        }, [weakThis = WeakPtr { *this }] {
            if (weakThis && weakThis->m_currentTriggerRumbleEffect)
                weakThis->m_currentTriggerRumbleEffect->leftEffectFinishedPlaying();
        });
        startEngine(m_rightTriggerEngine.get(), [weakThis = WeakPtr { *this }, callbackAggregator](bool success) {
            if (weakThis)
                weakThis->m_failedToStartRightTriggerEngine = !success;
        }, [weakThis = WeakPtr { *this }] {
            if (weakThis && weakThis->m_currentTriggerRumbleEffect)
                weakThis->m_currentTriggerRumbleEffect->rightEffectFinishedPlaying();
        });
        break;
    }
}

void GameControllerHapticEngines::stop(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = MainRunLoopCallbackAggregator::create(WTFMove(completionHandler));
    [m_leftHandleEngine stopWithCompletionHandler:makeBlockPtr([callbackAggregator](NSError *error) {
        if (error)
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::stop: Failed to stop the left handle haptic engine");
    }).get()];
    [m_rightHandleEngine stopWithCompletionHandler:makeBlockPtr([callbackAggregator](NSError *error) {
        if (error)
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::stop: Failed to stop the right handle haptic engine");
    }).get()];
    [m_leftTriggerEngine stopWithCompletionHandler:makeBlockPtr([callbackAggregator](NSError *error) {
        if (error)
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::stop: Failed to stop the left trigger haptic engine");
    }).get()];
    [m_rightTriggerEngine stopWithCompletionHandler:makeBlockPtr([callbackAggregator](NSError *error) {
        if (error)
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::stop: Failed to stop the right trigger haptic engine");
    }).get()];
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)
