/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "PlatformGamepad.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS GCController;
OBJC_CLASS GCControllerAxisInput;
OBJC_CLASS GCControllerButtonInput;
OBJC_CLASS GCControllerElement;
OBJC_CLASS GCExtendedGamepad;
OBJC_CLASS GCGamepad;

namespace WebCore {

class GameControllerGamepad : public PlatformGamepad {
    WTF_MAKE_NONCOPYABLE(GameControllerGamepad);
public:
    GameControllerGamepad(GCController *, unsigned index);

    const Vector<double>& axisValues() const final { return m_axisValues; }
    const Vector<double>& buttonValues() const final { return m_buttonValues; }

    const char* source() const final { return "GameController"_s; }

private:
    void setupAsExtendedGamepad();
    void setupAsGamepad();

    RetainPtr<GCController> m_gcController;

    Vector<double> m_axisValues;
    Vector<double> m_buttonValues;

    RetainPtr<GCGamepad> m_gamepad;
    RetainPtr<GCExtendedGamepad> m_extendedGamepad;

    bool m_hadButtonPresses { false };
};



} // namespace WebCore

#endif // ENABLE(GAMEPAD)
