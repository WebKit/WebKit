/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

// Buttons in the "standard" gamepad layout in the Web Gamepad spec
// https://www.w3.org/TR/gamepad/#dfn-standard-gamepad-layout
enum class GamepadButtons : uint8_t {
    RightClusterBottom = 0,
    RightClusterRight = 1,
    RightClusterLeft = 2,
    RightClusterTop = 3,
    LeftShoulderFront = 4,
    RightShoulderFront = 5,
    LeftShoulderBack = 6,
    RightShoulderBack = 7,
    CenterClusterLeft = 8,
    CenterClusterRight = 9,
    LeftStick = 10,
    RightStick = 11,
    LeftClusterTop = 12,
    LeftClusterBottom = 13,
    LeftClusterLeft = 14,
    LeftClusterRight = 15,
    CenterClusterCenter = 16,
};

static constexpr auto numGamepadButtonsWithoutHomeButton = static_cast<size_t>(GamepadButtons::CenterClusterCenter);
static constexpr auto numGamepadButtonsWithHomeButton = static_cast<size_t>(GamepadButtons::CenterClusterCenter) + 1;

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
    m_buttonValues.resize(homeButton ? numGamepadButtonsWithHomeButton : numGamepadButtonsWithoutHomeButton);

    m_extendedGamepad.get().valueChangedHandler = ^(GCExtendedGamepad *, GCControllerElement *) {
        m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, m_hadButtonPresses);
        m_hadButtonPresses = false;
    };

    auto bindButton = ^(GCControllerButtonInput *button, GamepadButtons index) {
        m_buttonValues[(size_t)index].setValue(button.value);
        button.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
            m_buttonValues[(size_t)index].setValue(value);
            if (pressed)
                m_hadButtonPresses = true;
        };
    };

    // Button Pad
    bindButton(m_extendedGamepad.get().buttonA, GamepadButtons::RightClusterBottom);
    bindButton(m_extendedGamepad.get().buttonB, GamepadButtons::RightClusterRight);
    bindButton(m_extendedGamepad.get().buttonX, GamepadButtons::RightClusterLeft);
    bindButton(m_extendedGamepad.get().buttonY, GamepadButtons::RightClusterTop);

    // Shoulders, Triggers
    bindButton(m_extendedGamepad.get().leftShoulder, GamepadButtons::LeftShoulderFront);
    bindButton(m_extendedGamepad.get().rightShoulder, GamepadButtons::RightShoulderFront);
    bindButton(m_extendedGamepad.get().leftTrigger, GamepadButtons::LeftShoulderBack);
    bindButton(m_extendedGamepad.get().rightTrigger, GamepadButtons::RightShoulderBack);

    // D Pad
    bindButton(m_extendedGamepad.get().dpad.up, GamepadButtons::LeftClusterTop);
    bindButton(m_extendedGamepad.get().dpad.down, GamepadButtons::LeftClusterBottom);
    bindButton(m_extendedGamepad.get().dpad.left, GamepadButtons::LeftClusterLeft);
    bindButton(m_extendedGamepad.get().dpad.right, GamepadButtons::LeftClusterRight);

    if (homeButton)
        bindButton(homeButton, GamepadButtons::CenterClusterCenter);

    // Select, Start
#if HAVE(GCEXTENDEDGAMEPAD_BUTTONS_OPTIONS_MENU)
    bindButton(m_extendedGamepad.get().buttonOptions, GamepadButtons::CenterClusterLeft);
    bindButton(m_extendedGamepad.get().buttonMenu, GamepadButtons::CenterClusterRight);
#endif

    // L3, R3
#if HAVE(GCEXTENDEDGAMEPAD_BUTTONS_THUMBSTICK)
    // Thumbstick buttons are only in macOS 10.14.1 / iOS 12.1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
    if ([m_extendedGamepad.get() respondsToSelector:@selector(leftThumbstickButton)]) {
        bindButton(m_extendedGamepad.get().leftThumbstickButton, GamepadButtons::LeftStick);
        bindButton(m_extendedGamepad.get().rightThumbstickButton, GamepadButtons::RightStick);
    }
#pragma clang diagnostic pop
#endif

    m_axisValues.resize(4);
    m_axisValues[0].setValue(m_extendedGamepad.get().leftThumbstick.xAxis.value);
    m_axisValues[1].setValue(-m_extendedGamepad.get().leftThumbstick.yAxis.value);
    m_axisValues[2].setValue(m_extendedGamepad.get().rightThumbstick.xAxis.value);
    m_axisValues[3].setValue(-m_extendedGamepad.get().rightThumbstick.yAxis.value);

    m_extendedGamepad.get().leftThumbstick.xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[0].setValue(value);
    };
    m_extendedGamepad.get().leftThumbstick.yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[1].setValue(-value);
    };
    m_extendedGamepad.get().rightThumbstick.xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[2].setValue(value);
    };
    m_extendedGamepad.get().rightThumbstick.yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[3].setValue(-value);
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
    m_buttonValues[0].setValue(m_extendedGamepad.get().buttonA.value);
    m_buttonValues[1].setValue(m_extendedGamepad.get().buttonB.value);
    m_buttonValues[2].setValue(m_extendedGamepad.get().buttonX.value);
    m_buttonValues[3].setValue(m_extendedGamepad.get().buttonY.value);
    m_buttonValues[4].setValue(m_extendedGamepad.get().leftShoulder.value);
    m_buttonValues[5].setValue(m_extendedGamepad.get().rightShoulder.value);

    m_axisValues.resize(2);
    m_axisValues[0].setValue(m_extendedGamepad.get().dpad.xAxis.value);
    m_axisValues[1].setValue(m_extendedGamepad.get().dpad.yAxis.value);

    m_extendedGamepad.get().buttonA.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[0].setValue(value);
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().buttonB.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[1].setValue(value);
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().buttonX.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[2].setValue(value);
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().buttonY.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[3].setValue(value);
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().leftShoulder.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[4].setValue(value);
        if (pressed)
            m_hadButtonPresses = true;
    };
    m_extendedGamepad.get().rightShoulder.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
        m_buttonValues[5].setValue(value);
        if (pressed)
            m_hadButtonPresses = true;
    };

    m_extendedGamepad.get().leftThumbstick.xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[0].setValue(value);
    };
    m_extendedGamepad.get().leftThumbstick.yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[1].setValue(value);
    };
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
