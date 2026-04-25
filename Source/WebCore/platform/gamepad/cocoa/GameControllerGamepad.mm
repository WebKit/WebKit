/*
 * Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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
#import <GameController/GCControllerElement.h>
#import <GameController/GameController.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/MakeString.h>

#import "GameControllerSoftLink.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GameControllerGamepad);

GameControllerGamepad::GameControllerGamepad(GCController *controller, unsigned index)
    : PlatformGamepad(index)
    , m_gcController(controller)
{
    ASSERT(index < 4);
    controller.playerIndex = (GCControllerPlayerIndex)(GCControllerPlayerIndex1 + index);

    setupElements();
}

GameControllerGamepad::~GameControllerGamepad()
{
    teardownElements();
}

static void disableDefaultSystemAction(GCControllerButtonInput *button)
{
    if ([button respondsToSelector:@selector(preferredSystemGestureState)])
        button.preferredSystemGestureState = GCSystemGestureStateDisabled;
}

void GameControllerGamepad::setupElements()
{
    RetainPtr<GCPhysicalInputProfile> profile = m_gcController.get().physicalInputProfile;
    WeakPtr weakThis { *this };

    // The user can expose an already-connected game controller to a web page by expressing explicit intent.
    // Examples include pressing a button, or wiggling the joystick with intent.
    if ([profile respondsToSelector:@selector(setThumbstickUserIntentHandler:)]) {
        [profile setThumbstickUserIntentHandler:^(__kindof GCPhysicalInputProfile*, GCControllerElement*) {
            if (!weakThis)
                return;
            weakThis->m_lastUpdateTime = MonotonicTime::now();
            GameControllerGamepadProvider::singleton().gamepadHadInput(*weakThis, true);
        }];
    }

    RetainPtr<GCControllerButtonInput> homeButton = profile.get().buttons[GCInputButtonHome];
    m_buttonValues.resize(homeButton ? numberOfStandardGamepadButtonsWithHomeButton : numberOfStandardGamepadButtonsWithoutHomeButton);

    m_id = makeString(String(m_gcController.get().vendorName), m_gcController.get().extendedGamepad ? " Extended Gamepad"_s : " Gamepad"_s);

#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    if (RetainPtr haptics = [m_gcController haptics]) {
        RetainPtr<NSSet<NSString *>> supportedLocalities = haptics.get().supportedLocalities;
        if (canLoad_GameController_GCHapticsLocalityLeftHandle() && canLoad_GameController_GCHapticsLocalityRightHandle()) {
            if ([supportedLocalities containsObject:get_GameController_GCHapticsLocalityLeftHandleSingleton()] && [supportedLocalities containsObject:get_GameController_GCHapticsLocalityRightHandleSingleton()])
                m_supportedEffectTypes.add(GamepadHapticEffectType::DualRumble);
        }
        if (canLoad_GameController_GCHapticsLocalityLeftTrigger() && canLoad_GameController_GCHapticsLocalityRightTrigger()) {
            if ([supportedLocalities containsObject:get_GameController_GCHapticsLocalityLeftTriggerSingleton()] && [supportedLocalities containsObject:get_GameController_GCHapticsLocalityRightTriggerSingleton()])
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
            if (!weakThis)
                return;
            weakThis->m_buttonValues[(size_t)index].setValue(value);
            weakThis->m_lastUpdateTime = MonotonicTime::now();
            GameControllerGamepadProvider::singleton().gamepadHadInput(*weakThis, pressed);
        };
    };

    // Button Pad
    bindButton(profile.get().buttons[GCInputButtonA], GamepadButtonRole::RightClusterBottom);
    bindButton(profile.get().buttons[GCInputButtonB], GamepadButtonRole::RightClusterRight);
    bindButton(profile.get().buttons[GCInputButtonX], GamepadButtonRole::RightClusterLeft);
    bindButton(profile.get().buttons[GCInputButtonY], GamepadButtonRole::RightClusterTop);

    // Shoulders, Triggers
    bindButton(profile.get().buttons[GCInputLeftShoulder], GamepadButtonRole::LeftShoulderFront);
    bindButton(profile.get().buttons[GCInputRightShoulder], GamepadButtonRole::RightShoulderFront);
    bindButton(profile.get().buttons[GCInputLeftTrigger], GamepadButtonRole::LeftShoulderBack);
    bindButton(profile.get().buttons[GCInputRightTrigger], GamepadButtonRole::RightShoulderBack);

    // D Pad
    bindButton(profile.get().dpads[GCInputDirectionPad].up, GamepadButtonRole::LeftClusterTop);
    bindButton(profile.get().dpads[GCInputDirectionPad].down, GamepadButtonRole::LeftClusterBottom);
    bindButton(profile.get().dpads[GCInputDirectionPad].left, GamepadButtonRole::LeftClusterLeft);
    bindButton(profile.get().dpads[GCInputDirectionPad].right, GamepadButtonRole::LeftClusterRight);
    
    // Home, Select, Start
    if (homeButton) {
        bindButton(homeButton.get(), GamepadButtonRole::CenterClusterCenter);
        disableDefaultSystemAction(homeButton.get());
    }
    RetainPtr<GCControllerButtonInput> optionButton = profile.get().buttons[GCInputButtonOptions];
    bindButton(optionButton.get(), GamepadButtonRole::CenterClusterLeft);
    disableDefaultSystemAction(optionButton.get());
    RetainPtr<GCControllerButtonInput> menuButton = profile.get().buttons[GCInputButtonMenu];
    bindButton(menuButton.get(), GamepadButtonRole::CenterClusterRight);
    disableDefaultSystemAction(menuButton.get());

    // L3, R3
    bindButton(profile.get().buttons[GCInputLeftThumbstickButton], GamepadButtonRole::LeftStick);
    bindButton(profile.get().buttons[GCInputRightThumbstickButton], GamepadButtonRole::RightStick);

    m_axisValues.resize(4);
    m_axisValues[0].setValue(profile.get().dpads[GCInputLeftThumbstick].xAxis.value);
    m_axisValues[1].setValue(-profile.get().dpads[GCInputLeftThumbstick].yAxis.value);
    m_axisValues[2].setValue(profile.get().dpads[GCInputRightThumbstick].xAxis.value);
    m_axisValues[3].setValue(-profile.get().dpads[GCInputRightThumbstick].yAxis.value);

    profile.get().dpads[GCInputLeftThumbstick].xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        if (!weakThis)
            return;
        weakThis->m_axisValues[0].setValue(value);
        weakThis->m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, false);
    };
    profile.get().dpads[GCInputLeftThumbstick].yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        if (!weakThis)
            return;
        weakThis->m_axisValues[1].setValue(-value);
        weakThis->m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, false);
    };
    profile.get().dpads[GCInputRightThumbstick].xAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        if (!weakThis)
            return;
        weakThis->m_axisValues[2].setValue(value);
        weakThis->m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, false);
    };
    profile.get().dpads[GCInputRightThumbstick].yAxis.valueChangedHandler = ^(GCControllerAxisInput *, float value) {
        if (!weakThis)
            return;
        weakThis->m_axisValues[3].setValue(-value);
        weakThis->m_lastUpdateTime = MonotonicTime::now();
        GameControllerGamepadProvider::singleton().gamepadHadInput(*this, false);
    };
}

void GameControllerGamepad::teardownElements()
{
    auto profile = RetainPtr { m_gcController.get().physicalInputProfile };
    if (!profile)
        return;

    // Clear thumbstick user intent handler.
    if ([profile respondsToSelector:@selector(setThumbstickUserIntentHandler:)])
        [profile setThumbstickUserIntentHandler:nil];

    // Clear all button handlers.
    for (GCControllerButtonInput *button in [profile allButtons])
        button.valueChangedHandler = nil;

    // Clear axis handlers for thumbsticks.
    profile.get().dpads[GCInputLeftThumbstick].xAxis.valueChangedHandler = nil;
    profile.get().dpads[GCInputLeftThumbstick].yAxis.valueChangedHandler = nil;
    profile.get().dpads[GCInputRightThumbstick].xAxis.valueChangedHandler = nil;
    profile.get().dpads[GCInputRightThumbstick].yAxis.valueChangedHandler = nil;
}

#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
GameControllerHapticEngines& GameControllerGamepad::ensureHapticEngines()
{
    if (!m_hapticEngines)
        lazyInitialize(m_hapticEngines, GameControllerHapticEngines::create(m_gcController.get()));
    return *m_hapticEngines;
}
#endif

void GameControllerGamepad::playEffect(GamepadHapticEffectType type, const GamepadEffectParameters& parameters, CompletionHandler<void(bool)>&& completionHandler)
{
#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    protect(ensureHapticEngines())->playEffect(type, parameters, WTF::move(completionHandler));
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
