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
#import "Logging.h"

#import "CoreHapticsSoftLink.h"

namespace WebCore {

std::unique_ptr<GameControllerHapticEffect> GameControllerHapticEffect::create(GameControllerHapticEngines& engines, const GamepadEffectParameters& parameters, CompletionHandler<void(bool)>&& completionHandler)
{
    auto createPlayer = [&](CHHapticEngine *engine, double intensity) -> RetainPtr<id> {
        NSDictionary* hapticDict = @{
            CHHapticPatternKeyPattern: @[
                @{ CHHapticPatternKeyEvent: @{
                    CHHapticPatternKeyEventType: CHHapticEventTypeHapticContinuous,
                    CHHapticPatternKeyTime: [NSNumber numberWithDouble:std::max<double>(parameters.startDelay / 1000., 0)],
                    CHHapticPatternKeyEventDuration: [NSNumber numberWithDouble:std::max<double>(parameters.duration / 1000., 0)],
                    CHHapticPatternKeyEventParameters: @[ @{
                        CHHapticPatternKeyParameterID: CHHapticEventParameterIDHapticIntensity,
                        CHHapticPatternKeyParameterValue: [NSNumber numberWithDouble:std::clamp<double>(intensity, 0, 1)],
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
    auto strongPlayer = createPlayer(engines.strongEngine(), parameters.strongMagnitude);
    auto weakPlayer = createPlayer(engines.weakEngine(), parameters.weakMagnitude);
    if (!strongPlayer || !weakPlayer) {
        RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect: Failed to create the haptic effect players");
        completionHandler(false);
        return nullptr;
    }
    return std::unique_ptr<GameControllerHapticEffect>(new GameControllerHapticEffect(WTFMove(strongPlayer), WTFMove(weakPlayer), WTFMove(completionHandler)));
}

GameControllerHapticEffect::GameControllerHapticEffect(RetainPtr<id>&& strongPlayer, RetainPtr<id>&& weakPlayer, CompletionHandler<void(bool)>&& completionHandler)
    : m_strongPlayer(WTFMove(strongPlayer))
    , m_weakPlayer(WTFMove(weakPlayer))
    , m_completionHandler(WTFMove(completionHandler))
{
}

GameControllerHapticEffect::~GameControllerHapticEffect()
{
    if (m_completionHandler)
        m_completionHandler(false);
}

bool GameControllerHapticEffect::start()
{
    NSError *error;
    if (m_strongPlayer && ![m_strongPlayer startAtTime:0 error:&error]) {
        RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect::start: Failed to start the strong player");
        m_strongPlayer = nullptr;
    }
    if (m_weakPlayer && ![m_weakPlayer startAtTime:0 error:&error]) {
        RELEASE_LOG_ERROR(Gamepad, "GameControllerHapticEffect::start: Failed to start the weak player");
        m_weakPlayer = nullptr;
    }
    return m_strongPlayer || m_weakPlayer;
}

void GameControllerHapticEffect::stop()
{
    NSError *error;
    if (auto player = std::exchange(m_strongPlayer, nullptr))
        [player stopAtTime:0.0 error:&error];
    if (auto player = std::exchange(m_weakPlayer, nullptr))
        [player stopAtTime:0.0 error:&error];
}

void GameControllerHapticEffect::strongEffectFinishedPlaying()
{
    m_strongPlayer = nullptr;
    if (!m_weakPlayer && m_completionHandler)
        m_completionHandler(true);
}

void GameControllerHapticEffect::weakEffectFinishedPlaying()
{
    m_weakPlayer = nullptr;
    if (!m_strongPlayer && m_completionHandler)
        m_completionHandler(true);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)
