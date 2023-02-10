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
#import "GameControllerHapticEngines.h"
#import "GamepadConstants.h"
#import "RuntimeApplicationChecks.h"
#import <GameController/GCControllerElement.h>
#import <GameController/GameController.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#import "GameControllerSoftLink.h"

namespace WebCore {

GameControllerGamepad::GameControllerGamepad(GCController *controller, unsigned index)
    : PlatformGamepad(index)
    , m_gcController(controller)
{
    ASSERT(index < 4);
    controller.playerIndex = (GCControllerPlayerIndex)(GCControllerPlayerIndex1 + index);

    setupElements();
}

static void disableDefaultSystemAction(GCControllerButtonInput *button)
{
    if ([button respondsToSelector:@selector(preferredSystemGestureState)])
        button.preferredSystemGestureState = GCSystemGestureStateDisabled;
}

void GameControllerGamepad::setupElements()
{
#if PLATFORM(IOS_FAMILY)
    // rdar://103093747 - Backbone controller not recognized by Backbone app
    if (IOSApplication::isBackboneApp() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::UsesGameControllerPhysicalInputProfile))
        m_gcController.get().extendedGamepad.valueChangedHandler = ^(GCExtendedGamepad *, GCControllerElement *) { };
#endif

    auto *profile = m_gcController.get().physicalInputProfile;

    // The user can expose an already-connected game controller to a web page by expressing explicit intent.
    // Examples include pressing a button, or wiggling the joystick with intent.
    if ([profile respondsToSelector:@selector(setThumbstickUserIntentHandler:)]) {
        [profile setThumbstickUserIntentHandler:^(__kindof GCPhysicalInputProfile*, GCControllerElement*) {
            m_lastUpdateTime = MonotonicTime::now();
            GameControllerGamepadProvider::singleton().gamepadHadInput(*this, true);
        }];
    }

    auto *homeButton = profile.buttons[GCInputButtonHome];
    m_buttonValues.resize(homeButton ? numberOfStandardGamepadButtonsWithHomeButton : numberOfStandardGamepadButtonsWithoutHomeButton);

    m_id = makeString(String(m_gcController.get().vendorName), m_gcController.get().extendedGamepad ? " Extended Gamepad"_s : " Gamepad"_s);

#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    if (auto *haptics = [m_gcController haptics]) {
        if (canLoad_GameController_GCHapticsLocalityLeftHandle() && canLoad_GameController_GCHapticsLocalityRightHandle()) {
            if ([haptics.supportedLocalities containsObject:get_GameController_GCHapticsLocalityLeftHandle()] && [haptics.supportedLocalities containsObject:get_GameController_GCHapticsLocalityRightHandle()])
                m_supportedEffectTypes.add(GamepadHapticEffectType::DualRumble);
        }
        if (canLoad_GameController_GCHapticsLocalityLeftTrigger() && canLoad_GameController_GCHapticsLocalityRightTrigger()) {
            if ([haptics.supportedLocalities containsObject:get_GameController_GCHapticsLocalityLeftTrigger()] && [haptics.supportedLocalities containsObject:get_GameController_GCHapticsLocalityRightTrigger()])
                m_supportedEffectTypes.add(GamepadHapticEffectType::TriggerRumble);
        }
    }
#endif

    if (m_gcController.get().extendedGamepad)
        m_mapping = standardGamepadMappingString();

    auto bindButton = ^(GCControllerButtonInput *button, GamepadButtonRole index) {
        m_buttonValues[(size_t)index].setValue(button.value);
        if (!button)
            return;

        button.valueChangedHandler = ^(GCControllerButtonInput *, float value, BOOL pressed) {
            // GameController framework will materialize missing values from a HID report as NaN.
            // This shouldn't happen with physical hardware, but does happen with virtual devices
            // with imperfect reports (e.g. virtual HID devices in API tests)
            // Ignoring them is preferable to surfacing NaN to javascript.
            if (std::isnan(value))
                return;
            m_buttonValues[(size_t)index].setValue(value);
            m_lastUpdateTime = MonotonicTime::now();
            GameControllerGamepadProvider::singleton().gamepadHadInput(*this, pressed);
        };
    };

    // Button Pad
    bindButton(profile.buttons[GCInputButtonA], GamepadButtonRole::RightClusterBottom);
    bindButton(profile.buttons[GCInputButtonB], GamepadButtonRole::RightClusterRight);
    bindButton(profile.buttons[GCInputButtonX], GamepadButtonRole::RightClusterLeft);
    bindButton(profile.buttons[GCInputButtonY], GamepadButtonRole::RightClusterTop);

    // Shoulders, Triggers
    bindButton(profile.buttons[GCInputLeftShoulder], GamepadButtonRole::LeftShoulderFront);
    bindButton(profile.buttons[GCInputRightShoulder], GamepadButtonRole::RightShoulderFront);
    bindButton(profile.buttons[GCInputLeftTrigger], GamepadButtonRole::LeftShoulderBack);
    bindButton(profile.buttons[GCInputRightTrigger], GamepadButtonRole::RightShoulderBack);

    // D Pad
    bindButton(profile.dpads[GCInputDirectionPad].up, GamepadButtonRole::LeftClusterTop);
    bindButton(profile.dpads[GCInputDirectionPad].down, GamepadButtonRole::LeftClusterBottom);
    bindButton(profile.dpads[GCInputDirectionPad].left, GamepadButtonRole::LeftClusterLeft);
    bindButton(profile.dpads[GCInputDirectionPad].right, GamepadButtonRole::LeftClusterRight);
    
    // Home, Select, Start
    if (homeButton) {
        bindButton(homeButton, GamepadButtonRole::CenterClusterCenter);
        disableDefaultSystemAction(homeButton);
    }
    bindButton(profile.buttons[GCInputButtonOptions], GamepadButtonRole::CenterClusterLeft);
    disableDefaultSystemAction(profile.buttons[GCInputButtonOptions]);
    bindButton(profile.buttons[GCInputButtonMenu], GamepadButtonRole::CenterClusterRight);
    disableDefaultSystemAction(profile.buttons[GCInputButtonMenu]);

    // L3, R3
    bindButton(profile.buttons[GCInputLeftThumbstickButton], GamepadButtonRole::LeftStick);
    bindButton(profile.buttons[GCInputRightThumbstickButton], GamepadButtonRole::RightStick);

    m_axisValues.resize(4);
    m_axisValues[0].setValue(profile.dpads[GCInputLeftThumbstick].xAxis.value);
    m_axisValues[1].setValue(-profile.dpads[GCInputLeftThumbstick].yAxis.value);
    m_axisValues[2].setValue(profile.dpads[GCInputRightThumbstick].xAxis.value);
    m_axisValues[3].setValue(-profile.dpads[GCInputRightThumbstick].yAxis.value);

    profile.dpads[GCInputLeftThumbstick].xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[0].setValue(value);
        m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, false);
    };
    profile.dpads[GCInputLeftThumbstick].yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[1].setValue(-value);
        m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, false);
    };
    profile.dpads[GCInputRightThumbstick].xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[2].setValue(value);
        m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, false);
    };
    profile.dpads[GCInputRightThumbstick].yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        m_axisValues[3].setValue(-value);
        m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, false);
    };
}

#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
GameControllerHapticEngines& GameControllerGamepad::ensureHapticEngines()
{
    if (!m_hapticEngines)
        m_hapticEngines = GameControllerHapticEngines::create(m_gcController.get());
    return *m_hapticEngines;
}
#endif

void GameControllerGamepad::playEffect(GamepadHapticEffectType type, const GamepadEffectParameters& parameters, CompletionHandler<void(bool)>&& completionHandler)
{
#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    ensureHapticEngines().playEffect(type, parameters, WTFMove(completionHandler));
#else
    UNUSED_PARAM(type);
    UNUSED_PARAM(parameters);
    completionHandler(false);
#endif
}

void GameControllerGamepad::stopEffects(CompletionHandler<void()>&& completionHandler)
{
#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    if (m_hapticEngines)
        m_hapticEngines->stopEffects();
#endif
    completionHandler();
}

void GameControllerGamepad::noLongerHasAnyClient()
{
#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    // Stop the haptics engine if it is running.
    if (m_hapticEngines)
        m_hapticEngines->stop([] { });
#endif
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
