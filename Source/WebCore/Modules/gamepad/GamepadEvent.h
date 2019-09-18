/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "Event.h"
#include "Gamepad.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class GamepadEvent : public Event {
public:
    ~GamepadEvent() = default;

    static Ref<GamepadEvent> create(const AtomString& eventType, Gamepad& gamepad)
    {
        return adoptRef(*new GamepadEvent(eventType, gamepad));
    }

    struct Init : EventInit {
        RefPtr<Gamepad> gamepad;
    };

    static Ref<GamepadEvent> create(const AtomString& eventType, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new GamepadEvent(eventType, initializer, isTrusted));
    }

    Gamepad* gamepad() const { return m_gamepad.get(); }

    EventInterface eventInterface() const override { return GamepadEventInterfaceType; }

private:
    explicit GamepadEvent(const AtomString& eventType, Gamepad&);
    GamepadEvent(const AtomString& eventType, const Init&, IsTrusted);

    RefPtr<Gamepad> m_gamepad;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
