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

#import <wtf/CompletionHandler.h>
#import <wtf/RefCountedAndCanMakeWeakPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/WeakPtr.h>

OBJC_CLASS CHHapticEngine;

namespace WebCore {

class GameControllerHapticEngines;
struct GamepadEffectParameters;
enum class GamepadHapticEffectType : uint8_t;

class GameControllerHapticEffect final : public RefCountedAndCanMakeWeakPtr<GameControllerHapticEffect> {
    WTF_MAKE_TZONE_ALLOCATED(GameControllerHapticEffect);
public:
    static RefPtr<GameControllerHapticEffect> create(GameControllerHapticEngines&, GamepadHapticEffectType, const GamepadEffectParameters&);
    ~GameControllerHapticEffect();

    void start(CompletionHandler<void(bool)>&&);
    void stop();

private:
    GameControllerHapticEffect(RetainPtr<CHHapticEngine>&& leftEngine, RetainPtr<CHHapticEngine>&& rightEngine, RetainPtr<id>&& leftPlayer, RetainPtr<id>&& rightPlayer);

    void ensureStarted(Function<void(bool)>&&);
    void startEngine(CHHapticEngine *, Function<void(bool)>&&);
    void registerNotification(CHHapticEngine *, Function<void(bool)>&&);

    RetainPtr<CHHapticEngine> m_leftEngine;
    RetainPtr<CHHapticEngine> m_rightEngine;
    RetainPtr<id> m_leftPlayer;
    RetainPtr<id> m_rightPlayer;
    unsigned m_engineStarted { 0 };
    unsigned m_playerFinished { 0 };
    CompletionHandler<void(bool)> m_completionHandler;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && HAVE(WIDE_GAMECONTROLLER_SUPPORT)
