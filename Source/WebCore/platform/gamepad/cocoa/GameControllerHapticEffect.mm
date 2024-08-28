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

std::unique_ptr<GameControllerHapticEffect> GameControllerHapticEffect::create(GameControllerHapticEngines& engines, GamepadHapticEffectType type, const GamepadEffectParameters& parameters)
{
    auto createPlayer = [&](CHHapticEngine *engine, double magnitude) -> RetainPtr<id> {
        NSDictionary* hapticDict = @{
            CHHapticPatternKeyPattern: @[
                @{ CHHapticPatternKeyEvent: @{
                    CHHapticPatternKeyEventType: CHHapticEventTypeHapticContinuous,
                    CHHapticPatternKeyTime: [NSNumber numberWithDouble:std::max<double>(parameters.startDelay / 1000., 0)],
                    CHHapticPatternKeyEventDuration: [NSNumber numberWithDouble:std::clamp<double>(parameters.duration / 1000., 0, GamepadEffectParameters::maximumDuration.seconds())],
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

    RetainPtr<id> leftPlayer;
    RetainPtr<id> rightPlayer;
    switch (type) {
    case GamepadHapticEffectType::DualRumble: {
        leftPlayer = createPlayer(engines.leftHandleEngine(), parameters.strongMagnitude);
        rightPlayer = createPlayer(engines.rightHandleEngine(), parameters.weakMagnitude);
        break;
        }
    case GamepadHapticEffectType::TriggerRumble: {
        leftPlayer = createPlayer(engines.leftTriggerEngine(), parameters.leftTrigger);
        rightPlayer = createPlayer(engines.rightTriggerEngine(), parameters.rightTrigger);
        break;
        }
    }

    if (!leftPlayer || !rightPlayer) {
        RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect: Failed to create the haptic effect players");
        return nullptr;
    }
    return std::unique_ptr<GameControllerHapticEffect>(new GameControllerHapticEffect(WTFMove(leftPlayer), WTFMove(rightPlayer)));
}

GameControllerHapticEffect::GameControllerHapticEffect(RetainPtr<id>&& leftPlayer, RetainPtr<id>&& rightPlayer)
    : m_leftPlayer(WTFMove(leftPlayer))
    , m_rightPlayer(WTFMove(rightPlayer))
{
}

GameControllerHapticEffect::~GameControllerHapticEffect()
{
    if (m_completionHandler)
        m_completionHandler(false);
}

void GameControllerHapticEffect::start(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!m_completionHandler);
    m_completionHandler = WTFMove(completionHandler);

    NSError *error;
    if (m_leftPlayer && ![m_leftPlayer startAtTime:0 error:&error]) {
        RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect::start: Failed to start the strong player");
        m_leftPlayer = nullptr;
    }
    if (m_rightPlayer && ![m_rightPlayer startAtTime:0 error:&error]) {
        RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect::start: Failed to start the weak player");
        m_rightPlayer = nullptr;
    }
    if (!m_leftPlayer && !m_rightPlayer)
        m_completionHandler(false);
}

void GameControllerHapticEffect::stop()
{
    NSError *error;
    if (auto player = std::exchange(m_leftPlayer, nullptr))
        [player stopAtTime:0.0 error:&error];
    if (auto player = std::exchange(m_rightPlayer, nullptr))
        [player stopAtTime:0.0 error:&error];
}

void GameControllerHapticEffect::leftEffectFinishedPlaying()
{
    m_leftPlayer = nullptr;
    if (!m_rightPlayer && m_completionHandler)
        m_completionHandler(true);
}

void GameControllerHapticEffect::rightEffectFinishedPlaying()
{
    m_rightPlayer = nullptr;
    if (!m_leftPlayer && m_completionHandler)
        m_completionHandler(true);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)
