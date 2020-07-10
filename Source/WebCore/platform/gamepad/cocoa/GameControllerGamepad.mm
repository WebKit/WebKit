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
#import "config.h"
#import "GameControllerGamepad.h"

#if ENABLE(GAMEPAD)
#import "GameControllerGamepadProvider.h"
#import "GameControllerSoftLink.h"
#import <GameController/GameController.h>

namespace WebCore {

GameControllerGamepad::GameControllerGamepad(GCController *controller, unsigned index)
    : PlatformGamepad(index)
    , m_gcController(controller)
{
    ASSERT(index < 4);
    controller.playerIndex = (GCControllerPlayerIndex)(GCControllerPlayerIndex1 + index);

    m_extendedGamepad = controller.extendedGamepad;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!m_extendedGamepad)
        m_gamepad = controller.gamepad;
    ALLOW_DEPRECATED_DECLARATIONS_END

    ASSERT(m_extendedGamepad || m_gamepad);

    if (m_extendedGamepad)
        setupAsExtendedGamepad();
    else
        setupAsGamepad();
}

static GCControllerButtonInput *homeButtonFromExtendedGamepad(GCExtendedGamepad *gamepad)
{
#if HAVE(GCEXTENDEDGAMEPAD_HOME_BUTTON)
    if (gamepad.buttonHome)
        return gamepad.buttonHome;
#endif

    id potentialButton = [gamepad valueForKey:@"_buttonHome"];
    if (potentialButton && [potentialButton isKindOfClass:getGCControllerButtonInputClass()])
        return potentialButton;

    return nil;
}

void GameControllerGamepad::setupAsExtendedGamepad()
{
    ASSERT(m_extendedGamepad);

    m_id = makeString(String(m_gcController.get().vendorName), " Extended Gamepad"_s);
    m_mapping = String("standard");

    auto *homeButton = homeButtonFromExtendedGamepad(m_extendedGamepad.get());
    m_buttonValues.resize(homeButton ? 17 : 16);

    m_extendedGamepad.get().valueChangedHandler = ^(GCExtendedGamepad *, GCControllerElement *) {
        m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, m_hadButtonPresses);
        m_hadButtonPresses = false;
    };

    auto bindButton = ^(GCControllerButtonInput *button, size_t idx) {
        m_buttonValues[idx] = button.value;
        button.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
            m_buttonValues[idx] = value;
            if (pressed)
                m_hadButtonPresses = true;
        };
    };

    // Button Pad
    bindButton(m_extendedGamepad.get().buttonA, 0);
    bindButton(m_extendedGamepad.get().buttonB, 1);
    bindButton(m_extendedGamepad.get().buttonX, 2);
    bindButton(m_extendedGamepad.get().buttonY, 3);

    // Shoulders, Triggers
    bindButton(m_extendedGamepad.get().leftShoulder, 4);
    bindButton(m_extendedGamepad.get().rightShoulder, 5);
    bindButton(m_extendedGamepad.get().leftTrigger, 6);
    bindButton(m_extendedGamepad.get().rightTrigger, 7);

    // D Pad
    bindButton(m_extendedGamepad.get().dpad.up, 12);
    bindButton(m_extendedGamepad.get().dpad.down, 13);
    bindButton(m_extendedGamepad.get().dpad.left, 14);
    bindButton(m_extendedGamepad.get().dpad.right, 15);

    if (homeButton)
        bindButton(homeButton, 16);

    // Select, Start
#if HAVE(GCEXTENDEDGAMEPAD_BUTTONS_OPTIONS_MENU)
    bindButton(m_extendedGamepad.get().buttonOptions, 8);
    bindButton(m_extendedGamepad.get().buttonMenu, 9);
#endif

    // L3, R3
#if HAVE(GCEXTENDEDGAMEPAD_BUTTONS_THUMBSTICK)
    // Thumbstick buttons are only in macOS 10.14.1 / iOS 12.1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
    if ([m_extendedGamepad.get() respondsToSelector:@selector(leftThumbstickButton)]) {
        bindButton(m_extendedGamepad.get().leftThumbstickButton, 10);
        bindButton(m_extendedGamepad.get().rightThumbstickButton, 11);
    }
#pragma clang diagnostic pop
#endif

    m_axisValues.resize(4);
    m_axisValues[0] = m_extendedGamepad.get().leftThumbstick.xAxis.value;
    m_axisValues[1] = -m_extendedGamepad.get().leftThumbstick.yAxis.value;
    m_axisValues[2] = m_extendedGamepad.get().rightThumbstick.xAxis.value;
    m_axisValues[3] = -m_extendedGamepad.get().rightThumbstick.yAxis.value;

    m_extendedGamepad.get().leftThumbstick.xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[0] = value;
    };
    m_extendedGamepad.get().leftThumbstick.yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[1] = -value;
    };
    m_extendedGamepad.get().rightThumbstick.xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[2] = value;
    };
    m_extendedGamepad.get().rightThumbstick.yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[3] = -value;
    };
}

void GameControllerGamepad::setupAsGamepad()
{
    ASSERT(m_gamepad);

    m_id = makeString(String(m_gcController.get().vendorName), " Gamepad"_s);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN // GCGamepad
    m_gamepad.get().valueChangedHandler = ^(GCGamepad *, GCControllerElement *) {
        m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, m_hadButtonPresses);
        m_hadButtonPresses = false;
    };
    ALLOW_DEPRECATED_DECLARATIONS_END

    m_buttonValues.resize(6);
    m_buttonValues[0] = m_extendedGamepad.get().buttonA.value;
    m_buttonValues[1] = m_extendedGamepad.get().buttonB.value;
    m_buttonValues[2] = m_extendedGamepad.get().buttonX.value;
    m_buttonValues[3] = m_extendedGamepad.get().buttonY.value;
    m_buttonValues[4] = m_extendedGamepad.get().leftShoulder.value;
    m_buttonValues[5] = m_extendedGamepad.get().rightShoulder.value;

    m_axisValues.resize(2);
    m_axisValues[0] = m_extendedGamepad.get().dpad.xAxis.value;
    m_axisValues[1] = m_extendedGamepad.get().dpad.yAxis.value;

    m_extendedGamepad.get().buttonA.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[0] = value;
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().buttonB.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[1] = value;
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().buttonX.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[2] = value;
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().buttonY.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[3] = value;
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().leftShoulder.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[4] = value;
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().rightShoulder.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[5] = value;
        if (pressed)
            m_hadButtonPresses = true;
    };

    m_extendedGamepad.get().leftThumbstick.xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[0] = value;
    };
    m_extendedGamepad.get().leftThumbstick.yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[1] = value;
    };
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
