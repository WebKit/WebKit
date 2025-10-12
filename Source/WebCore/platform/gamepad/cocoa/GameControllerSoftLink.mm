/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GAMEPAD) && PLATFORM(COCOA)

#import "GameControllerSPI.h"
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebCore, GameController)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(WebCore, GameController, GCController, WEBCORE_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(WebCore, GameController, GCControllerButtonInput, WEBCORE_EXPORT)

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputButtonA, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputButtonB, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputButtonX, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputButtonY, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputButtonHome, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputButtonMenu, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputButtonOptions, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputDirectionPad, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputLeftShoulder, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputLeftTrigger, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputLeftThumbstick, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputLeftThumbstickButton, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputRightShoulder, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputRightTrigger, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputRightThumbstick, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCInputRightThumbstickButton, NSString *, WEBCORE_EXPORT)

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCControllerDidConnectNotification, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCControllerDidDisconnectNotification, NSString *, WEBCORE_EXPORT)

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCHapticsLocalityLeftHandle, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCHapticsLocalityRightHandle, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCHapticsLocalityLeftTrigger, NSString *, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCHapticsLocalityRightTrigger, NSString *, WEBCORE_EXPORT)

#if HAVE(MULTIGAMEPADPROVIDER_SUPPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, GameController, ControllerClassForService, Class, (IOHIDServiceClientRef service), (service))
#endif

#if PLATFORM(VISION) && __has_include(<GameController/GCEventInteraction.h>)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(WebCore, GameController, GCEventInteraction, WEBCORE_EXPORT)
#endif

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT) && ENABLE(POINTER_LOCK)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCMouse, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(WebCore, GameController, GCMouseDidStopBeingCurrentNotification, NSString *, WEBCORE_EXPORT)
#endif

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/GameControllerSoftLinkAdditions.mm>)
#import <WebKitAdditions/GameControllerSoftLinkAdditions.mm>
#endif

#endif // ENABLE(GAMEPAD) && PLATFORM(COCOA)
