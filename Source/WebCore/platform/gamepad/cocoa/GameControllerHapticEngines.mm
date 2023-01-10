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

#import "GameControllerSoftLink.h"
#import "CoreHapticsSoftLink.h"

namespace WebCore {

std::unique_ptr<GameControllerHapticEngines> GameControllerHapticEngines::create(GCController *gamepad)
{
    return std::unique_ptr<GameControllerHapticEngines>(new GameControllerHapticEngines(gamepad));
}

GameControllerHapticEngines::GameControllerHapticEngines(GCController *gamepad)
    : m_strongEngine([gamepad.haptics createEngineWithLocality:get_GameController_GCHapticsLocalityLeftHandle()])
    , m_weakEngine([gamepad.haptics createEngineWithLocality:get_GameController_GCHapticsLocalityRightHandle()])
{
}

GameControllerHapticEngines::~GameControllerHapticEngines() = default;

void GameControllerHapticEngines::playEffect(GamepadHapticEffectType type, const GamepadEffectParameters& parameters, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT_UNUSED(type, type == GamepadHapticEffectType::DualRumble);

    // Trying to create pattern players with a 0 duration will fail. However, Games on XBox seem to use such
    // requests to stop vibrating.
    if (!parameters.duration) {
        if (auto currentEffect = std::exchange(m_currentEffect, nullptr))
            currentEffect->stop();
        return completionHandler(true);
    }

    auto currentEffect = GameControllerHapticEffect::create(*this, parameters, WTFMove(completionHandler));
    if (!currentEffect)
        return;

    if (m_currentEffect)
        m_currentEffect->stop();

    m_currentEffect = WTFMove(currentEffect);
    ensureStarted([weakThis = WeakPtr { *this }](bool success) mutable {
        if (!success || !weakThis || !weakThis->m_currentEffect)
            return;

        if (!weakThis->m_currentEffect->start())
            weakThis->m_currentEffect = nullptr;
    });
}

void GameControllerHapticEngines::stopEffects()
{
    if (auto currentEffect = std::exchange(m_currentEffect, nullptr))
        currentEffect->stop();
}

void GameControllerHapticEngines::ensureStarted(CompletionHandler<void(bool)>&& completionHandler)
{
    auto callbackAggregator = MainRunLoopCallbackAggregator::create([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(weakThis && !weakThis->m_failedToStartStrongEngine && !weakThis->m_failedToStartWeakEngine);
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
    startEngine(m_strongEngine.get(), [weakThis = WeakPtr { *this }, callbackAggregator](bool success) {
        if (weakThis)
            weakThis->m_failedToStartStrongEngine = !success;
    }, [weakThis = WeakPtr { *this }] {
        if (weakThis && weakThis->m_currentEffect)
            weakThis->m_currentEffect->strongEffectFinishedPlaying();
    });
    startEngine(m_weakEngine.get(), [weakThis = WeakPtr { *this }, callbackAggregator](bool success) {
        if (weakThis)
            weakThis->m_failedToStartWeakEngine = !success;
    }, [weakThis = WeakPtr { *this }] {
        if (weakThis && weakThis->m_currentEffect)
            weakThis->m_currentEffect->weakEffectFinishedPlaying();
    });
}

void GameControllerHapticEngines::stop(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = MainRunLoopCallbackAggregator::create(WTFMove(completionHandler));
    [m_strongEngine stopWithCompletionHandler:makeBlockPtr([callbackAggregator](NSError *error) {
        if (error)
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::stop: Failed to stop the strong haptic engine");
    }).get()];
    [m_weakEngine stopWithCompletionHandler:makeBlockPtr([callbackAggregator](NSError *error) {
        if (error)
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::stop: Failed to stop the weak haptic engine");
    }).get()];
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)
