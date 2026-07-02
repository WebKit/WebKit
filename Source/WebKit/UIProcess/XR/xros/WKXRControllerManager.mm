/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEBXR) && USE(COMPOSITORXR) && HAVE(SPATIAL_CONTROLLERS)
#import "WKXRControllerManager.h"

#import "Logging.h"

#import "ARKitSoftLink.h"
#import <WebCore/GameControllerSoftLink.h>

// https://www.w3.org/TR/webxr-gamepads-module-1/#xr-standard-heading
typedef NS_ENUM(NSInteger, WKXRControllerButtonType) {
    WKXRControllerButtonTypeTrigger = 0,
    WKXRControllerButtonTypeSqueeze,
    WKXRControllerButtonTypeTouchpadPress,
    WKXRControllerButtonTypeThumbstickPress,
    WKXRControllerButtonTypeButtonA,
    WKXRControllerButtonTypeButtonB,
    WKXRControllerButtonTypeButtonsSize // MUST be the last enumerator
};

// https://www.w3.org/TR/webxr-gamepads-module-1/#xr-standard-heading
typedef NS_ENUM(NSInteger, WKXRControllerAxisType) {
    WKXRControllerAxisTypeTouchpadX = 0,
    WKXRControllerAxisTypeTouchpadY,
    WKXRControllerAxisTypeThumbstickX,
    WKXRControllerAxisTypeThumbstickY,
    WKXRControllerAxisTypeAxesSize // MUST be the last enumerator
};

@interface WKXRControllerManager ()

- (void)_notificationControllerDidConnect:(NSNotification *)notification;
- (void)_notificationControllerDidDisconnect:(NSNotification *)notification;
- (void)_controllerDidConnect:(GCController *)controller;

- (void)_startMonitoringGamepads;
- (void)_stopMonitoringGamepads;

- (void)_initInputSourceButtonWithGCButtonInput:(PlatformXR::FrameData::InputSourceButton&)inputSourceButton gcButtonInput:(GCControllerButtonInput *)gcButtonInput;

@end

@implementation WKXRControllerManager {
    RetainPtr<GCController> _leftController;
    RetainPtr<GCController> _rightController;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    [self _startMonitoringGamepads];

    return self;
}

- (void)dealloc
{
    [self _stopMonitoringGamepads];

    [super dealloc];
}

- (void)_notificationControllerDidConnect:(NSNotification *)notification
{
    GCController *controller = [notification object];
    [self _controllerDidConnect:controller];
}

- (void)_notificationControllerDidDisconnect:(NSNotification *)notification
{
    GCController *controller = [notification object];
    if (_leftController.get() == controller)
        _leftController = nullptr;
    else if (_rightController.get() == controller)
        _rightController = nullptr;
}

- (void)_controllerDidConnect:(GCController *)controller
{
    if (_leftController.get() == controller || _rightController.get() == controller)
        return;

    if (WebCore::canLoad_GameController_GCProductCategoryLeftSpatialController() && [controller.productCategory isEqualToString:GCProductCategoryLeftSpatialController]) {
        _leftController = controller;
        RELEASE_LOG_INFO(XR, "Left spatial controller connected");
    } else if (WebCore::canLoad_GameController_GCProductCategoryRightSpatialController() && [controller.productCategory isEqualToString:GCProductCategoryRightSpatialController]) {
        _rightController = controller;
        RELEASE_LOG_INFO(XR, "Right spatial controller connected");
    }
}

- (void)_initInputSourceButtonWithGCButtonInput:(PlatformXR::FrameData::InputSourceButton&)inputSourceButton gcButtonInput:(GCControllerButtonInput *)gcButtonInput
{
    if (gcButtonInput && (gcButtonInput.pressed || gcButtonInput.touched)) {
        inputSourceButton.pressed = gcButtonInput.pressed;
        inputSourceButton.touched = gcButtonInput.touched;
        inputSourceButton.pressedValue = gcButtonInput.value;
    }
}

- (void)_startMonitoringGamepads
{
    if (!WebCore::canLoad_GameController_GCInputButtonA()
        || !WebCore::canLoad_GameController_GCInputButtonB()
        || !WebCore::canLoad_GameController_GCInputButtonX()
        || !WebCore::canLoad_GameController_GCInputButtonY()

        || !WebCore::canLoad_GameController_GCInputLeftTrigger()
        || !WebCore::canLoad_GameController_GCInputLeftShoulder()
        || !WebCore::canLoad_GameController_GCInputLeftThumbstickButton()
        || !WebCore::canLoad_GameController_GCInputLeftThumbstick()

        || !WebCore::canLoad_GameController_GCInputRightTrigger()
        || !WebCore::canLoad_GameController_GCInputRightShoulder()
        || !WebCore::canLoad_GameController_GCInputRightThumbstickButton()
        || !WebCore::canLoad_GameController_GCInputRightThumbstick()) {
        RELEASE_LOG_ERROR(XR, "Unable to load required GCController strings");
        return;
    }

    if (WebCore::canLoad_GameController_GCControllerDidConnectNotification())
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_notificationControllerDidConnect:) name:WebCore::get_GameController_GCControllerDidConnectNotification() object:nil];

    if (WebCore::canLoad_GameController_GCControllerDidDisconnectNotification())
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_notificationControllerDidDisconnect:) name:WebCore::get_GameController_GCControllerDidDisconnectNotification() object:nil];

    auto *controllers = [WebCore::getGCControllerClass() controllers];
    if (!controllers || !controllers.count)
        RELEASE_LOG_INFO(XR, "WKXRControllerManager has no initial GCControllers attached");

    for (GCController *controller in controllers) {
        RELEASE_LOG_INFO(XR, "WKXRControllerManager has initial GCController %p", controller);
        [self _controllerDidConnect:controller];
    }
}

- (void)_stopMonitoringGamepads
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (Vector<PlatformXR::FrameData::InputSourceButton>)getButtonsState:(BOOL)isLeftController
{
    Vector<PlatformXR::FrameData::InputSourceButton> buttons(WKXRControllerButtonTypeButtonsSize);

    auto controller = isLeftController ? _leftController : _rightController;
    if (!controller.get() || !controller.get().physicalInputProfile)
        return buttons;
    auto *profile = [controller physicalInputProfile];

    if (isLeftController) {
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeTrigger] gcButtonInput:profile.buttons[GCInputLeftTrigger]];
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeSqueeze] gcButtonInput:profile.buttons[GCInputLeftShoulder]];
        // buttons[WKXRControllerButtonTypeTouchpadPress]: placeholder
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeThumbstickPress] gcButtonInput:profile.buttons[GCInputLeftThumbstickButton]];
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeButtonA] gcButtonInput:profile.buttons[GCInputButtonX]];
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeButtonB] gcButtonInput:profile.buttons[GCInputButtonY]];
    } else {
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeTrigger] gcButtonInput:profile.buttons[GCInputRightTrigger]];
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeSqueeze] gcButtonInput:profile.buttons[GCInputRightShoulder]];
        // buttons[WKXRControllerButtonTypeTouchpadPress]: placeholder
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeThumbstickPress] gcButtonInput:profile.buttons[GCInputRightThumbstickButton]];
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeButtonA] gcButtonInput:profile.buttons[GCInputButtonA]];
        [self _initInputSourceButtonWithGCButtonInput:buttons[WKXRControllerButtonTypeButtonB] gcButtonInput:profile.buttons[GCInputButtonB]];
    }

    return buttons;
}

- (Vector<float>)getAxesState:(BOOL)isLeftController
{
    Vector<float> axes(WKXRControllerAxisTypeAxesSize);

    auto controller = isLeftController ? _leftController : _rightController;
    if (!controller.get() || !controller.get().physicalInputProfile)
        return axes;
    auto *profile = [controller physicalInputProfile];

    if (isLeftController) {
        // axes[WKXRControllerAxisTypeTouchpadX]: placeholder
        // axes[WKXRControllerAxisTypeTouchpadY]: placeholder
        axes[WKXRControllerAxisTypeThumbstickX] = profile.dpads[GCInputLeftThumbstick].xAxis.value;
        // Apple's natural scrolling translates to inverted values in WebXR content hence added minus.
        axes[WKXRControllerAxisTypeThumbstickY] = -profile.dpads[GCInputLeftThumbstick].yAxis.value;
    } else {
        // axes[WKXRControllerAxisTypeTouchpadX]: placeholder
        // axes[WKXRControllerAxisTypeTouchpadY]: placeholder
        axes[WKXRControllerAxisTypeThumbstickX] = profile.dpads[GCInputRightThumbstick].xAxis.value;
        // Apple's natural scrolling translates to inverted values in WebXR content hence added minus.
        axes[WKXRControllerAxisTypeThumbstickY] = -profile.dpads[GCInputRightThumbstick].yAxis.value;
    }

    return axes;
}

@end

#endif // ENABLE(WEBXR) && USE(COMPOSITORXR) && HAVE(SPATIAL_CONTROLLERS)
