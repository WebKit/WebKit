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
#import "GameControllerHapticEffect.h"

#import "GameControllerHapticEngines.h"
#import "GamepadEffectParameters.h"
#import "GamepadHapticEffectType.h"
#import "Logging.h"
#import <cmath>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>

#import "CoreHapticsSoftLink.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GameControllerHapticEffect);

static double magnitudeToIntensity(double magnitude)
{
    auto intensity = std::clamp<double>(magnitude, 0, 1);
#if HAVE(GCCONTROLLER_REQUIRING_HAPTICS_SQUARING)
    // Older versions of GameController didn't use the intensity as-is and required the values to
    // be squared. Without this, values below 0.1 would end up not triggering any gamepad vibration.
    intensity = std::sqrt(intensity);
#endif
    return intensity;
}

RefPtr<GameControllerHapticEffect> GameControllerHapticEffect::create(GameControllerHapticEngines& engines, GamepadHapticEffectType type, const GamepadEffectParameters& parameters)
{
    double delay = std::max<double>(parameters.startDelay / 1000., 0);
    double duration = std::clamp<double>(parameters.duration / 1000., 0, GamepadEffectParameters::maximumDuration.seconds());

    auto createPlayer = [&](CHHapticEngine *engine, double magnitude) -> RetainPtr<id> {
        NSDictionary* hapticDict = @{
            CHHapticPatternKeyPattern: @[
                @{ CHHapticPatternKeyEvent: @{
                    CHHapticPatternKeyEventType: CHHapticEventTypeHapticContinuous,
                    CHHapticPatternKeyTime: [NSNumber numberWithDouble:delay],
                    CHHapticPatternKeyEventDuration: [NSNumber numberWithDouble:duration],
                    CHHapticPatternKeyEventParameters: @[ @{
                        CHHapticPatternKeyParameterID: CHHapticEventParameterIDHapticIntensity,
                        CHHapticPatternKeyParameterValue: [NSNumber numberWithDouble:magnitudeToIntensity(magnitude)],
                    } ]
                }, },
            ],
        };

        NSError* error;
        auto pattern = adoptNS([allocCHHapticPatternInstance() initWithDictionary:hapticDict error:&error]);
        if (!pattern) {
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect: Failed to create a CHHapticPattern");
            return nullptr;
        }

        return retainPtr([engine createPlayerWithPattern:pattern.get() error:&error]);
    };

    RetainPtr<CHHapticEngine> leftEngine;
    RetainPtr<CHHapticEngine> rightEngine;
    double leftMagnitude = 0;
    double rightMagnitude = 0;
    switch (type) {
    case GamepadHapticEffectType::DualRumble: {
        leftEngine = engines.leftHandleEngine();
        rightEngine = engines.rightHandleEngine();
        leftMagnitude = parameters.strongMagnitude;
        rightMagnitude = parameters.weakMagnitude;
        break;
        }
    case GamepadHapticEffectType::TriggerRumble: {
        leftEngine = engines.leftTriggerEngine();
        rightEngine = engines.rightTriggerEngine();
        leftMagnitude = parameters.leftTrigger;
        rightMagnitude = parameters.rightTrigger;
        break;
        }
    }
    RetainPtr<id> leftPlayer = createPlayer(leftEngine.get(), leftMagnitude);
    RetainPtr<id> rightPlayer = createPlayer(rightEngine.get(), rightMagnitude);

    if (!leftPlayer || !rightPlayer) {
        RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect: Failed to create the haptic effect players");
        return nullptr;
    }
    return adoptRef(new GameControllerHapticEffect(WTFMove(leftEngine), WTFMove(rightEngine), WTFMove(leftPlayer), WTFMove(rightPlayer)));
}

GameControllerHapticEffect::GameControllerHapticEffect(RetainPtr<CHHapticEngine>&& leftEngine, RetainPtr<CHHapticEngine>&& rightEngine, RetainPtr<id>&& leftPlayer, RetainPtr<id>&& rightPlayer)
    : m_leftEngine(WTFMove(leftEngine))
    , m_rightEngine(WTFMove(rightEngine))
    , m_leftPlayer(WTFMove(leftPlayer))
    , m_rightPlayer(WTFMove(rightPlayer))
{
}

GameControllerHapticEffect::~GameControllerHapticEffect()
{
    stop();
}

void GameControllerHapticEffect::start(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainThread());
    ASSERT(!m_completionHandler);

    m_completionHandler = WTFMove(completionHandler);

    Ref callbackAggregator = MainRunLoopCallbackAggregator::create([weakThis = WeakPtr { *this }]() mutable {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_completionHandler)
            protectedThis->m_completionHandler(protectedThis->m_playerFinished == 2);
    });

    ensureStarted([weakThis = WeakPtr { *this }, callbackAggregator](bool success) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !success)
            return;

        NSError *error;
        Vector<RetainPtr<id>> successPlayers;
        if ([protectedThis->m_leftPlayer startAtTime:0 error:&error])
            successPlayers.append(protectedThis->m_leftPlayer);
        else
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect::start: Failed to start the strong player");

        if ([protectedThis->m_rightPlayer startAtTime:0 error:&error])
            successPlayers.append(protectedThis->m_rightPlayer);
        else
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect::start: Failed to start the weak player");

        if (successPlayers.size() != 2) {
            for (auto& player : successPlayers)
                [player stopAtTime:0.0 error:&error];
            return;
        }

        protectedThis->registerNotification(protectedThis->m_leftEngine.get(), [weakThis = WeakPtr { *protectedThis }, callbackAggregator](bool success) {
            if (weakThis && success)
                weakThis->m_playerFinished++;
        });
        protectedThis->registerNotification(protectedThis->m_rightEngine.get(), [weakThis = WeakPtr { *protectedThis }, callbackAggregator](bool success) {
            if (weakThis && success)
                weakThis->m_playerFinished++;
        });
    });
}

void GameControllerHapticEffect::ensureStarted(Function<void(bool)>&& completionHandler)
{
    Ref callbackAggregator = MainRunLoopCallbackAggregator::create([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        bool success = weakThis && weakThis->m_engineStarted == 2;
        completionHandler(success);
    });

    startEngine(m_leftEngine.get(), [weakThis = WeakPtr { *this }, callbackAggregator](bool success) {
        if (weakThis && success)
            weakThis->m_engineStarted++;
    });
    startEngine(m_rightEngine.get(), [weakThis = WeakPtr { *this }, callbackAggregator](bool success) {
        if (weakThis && success)
            weakThis->m_engineStarted++;
    });
}

void GameControllerHapticEffect::startEngine(CHHapticEngine *engine, Function<void(bool)>&& completionHandler)
{
    [engine startWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSError* error) mutable {
        ensureOnMainRunLoop([completionHandler = WTFMove(completionHandler), success = !error]() mutable {
            completionHandler(success);
        });
        if (error)
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::ensureStarted: Failed to start haptic engine");
    }).get()];
}

void GameControllerHapticEffect::registerNotification(CHHapticEngine *engine, Function<void(bool)>&& completionHandler)
{
    [engine notifyWhenPlayersFinished:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        ensureOnMainRunLoop([completionHandler = WTFMove(completionHandler), success = !error] mutable {
            completionHandler(success);
        });
        if (error)
            RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEngines::registerNotification: Failed to start haptic engine");
        return CHHapticEngineFinishedActionLeaveEngineRunning;
    }).get()];
}

void GameControllerHapticEffect::stop()
{
    ASSERT(isMainThread());

    NSError *error;
    [m_leftPlayer cancelAndReturnError:&error];
    [m_rightPlayer cancelAndReturnError:&error];

    if (m_completionHandler)
        m_completionHandler(false);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)
