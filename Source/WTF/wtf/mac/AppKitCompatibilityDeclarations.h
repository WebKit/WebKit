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

#ifndef AppKitCompatibilityDeclarations_h
#define AppKitCompatibilityDeclarations_h

#import <Availability.h>

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101300
#import <AppKit/AppKit.h>
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101200

static const NSAlertStyle NSAlertStyleCritical = NSCriticalAlertStyle;
static const NSAlertStyle NSAlertStyleInformational = NSInformationalAlertStyle;
static const NSAlertStyle NSAlertStyleWarning = NSWarningAlertStyle;

static const NSBezelStyle NSBezelStyleShadowlessSquare = NSShadowlessSquareBezelStyle;
static const NSBezelStyle NSBezelStyleRounded = NSRoundedBezelStyle;
static const NSBezelStyle NSBezelStyleRoundedDisclosure = NSRoundedDisclosureBezelStyle;

static const NSButtonType NSButtonTypeMomentaryLight = NSMomentaryLightButton;
static const NSButtonType NSButtonTypePushOnPushOff = NSPushOnPushOffButton;
static const NSButtonType NSButtonTypeToggle = NSToggleButton;
static const NSButtonType NSButtonTypeSwitch = NSSwitchButton;
static const NSButtonType NSButtonTypeRadio = NSRadioButton;
static const NSButtonType NSButtonTypeMomentaryChange = NSMomentaryChangeButton;
static const NSButtonType NSButtonTypeOnOff = NSOnOffButton;
static const NSButtonType NSButtonTypeMomentaryPushIn = NSMomentaryPushInButton;
static const NSButtonType NSButtonTypeAccelerator = NSAcceleratorButton;
static const NSButtonType NSButtonTypeMultiLevelAccelerator = NSMultiLevelAcceleratorButton;

static const NSCompositingOperation NSCompositingOperationCopy = NSCompositeCopy;
static const NSCompositingOperation NSCompositingOperationDestinationIn = NSCompositeDestinationIn;
static const NSCompositingOperation NSCompositingOperationDestinationOut =  NSCompositeDestinationOut;
static const NSCompositingOperation NSCompositingOperationDestinationOver = NSCompositeDestinationOver;
static const NSCompositingOperation NSCompositingOperationSourceAtop = NSCompositeSourceAtop;
static const NSCompositingOperation NSCompositingOperationSourceIn = NSCompositeSourceIn;
static const NSCompositingOperation NSCompositingOperationSourceOver = NSCompositeSourceOver;

static const NSControlSize NSControlSizeRegular = NSRegularControlSize;
static const NSControlSize NSControlSizeSmall = NSSmallControlSize;
static const NSControlSize NSControlSizeMini = NSMiniControlSize;

static const NSSliderType NSSliderTypeLinear = NSLinearSlider;
static const NSSliderType NSSliderTypeCircular = NSCircularSlider;

static const NSEventMask NSEventMaskAny = NSAnyEventMask;
static const NSEventMask NSEventMaskFlagsChanged = NSFlagsChangedMask;
static const NSEventMask NSEventMaskKeyDown = NSKeyDownMask;
static const NSEventMask NSEventMaskKeyUp = NSKeyUpMask;
static const NSEventMask NSEventMaskLeftMouseDown = NSLeftMouseDownMask;
static const NSEventMask NSEventMaskLeftMouseDragged = NSLeftMouseDraggedMask;
static const NSEventMask NSEventMaskLeftMouseUp = NSLeftMouseUpMask;
static const NSEventMask NSEventMaskOtherMouseDown = NSOtherMouseDownMask;
static const NSEventMask NSEventMaskOtherMouseUp = NSOtherMouseUpMask;
static const NSEventMask NSEventMaskPeriodic = NSPeriodicMask;
static const NSEventMask NSEventMaskRightMouseDown = NSRightMouseDownMask;
static const NSEventMask NSEventMaskRightMouseUp = NSRightMouseUpMask;
static const NSEventMask NSEventMaskMouseMoved = NSMouseMovedMask;
static const NSEventMask NSEventMaskScrollWheel = NSScrollWheelMask;

static const NSEventModifierFlags NSEventModifierFlagOption = NSAlternateKeyMask;
static const NSEventModifierFlags NSEventModifierFlagControl = NSControlKeyMask;
static const NSEventModifierFlags NSEventModifierFlagCapsLock = NSAlphaShiftKeyMask;
static const NSEventModifierFlags NSEventModifierFlagCommand =  NSCommandKeyMask;
static const NSEventModifierFlags NSEventModifierFlagDeviceIndependentFlagsMask =  NSDeviceIndependentModifierFlagsMask;
static const NSEventModifierFlags NSEventModifierFlagFunction = NSFunctionKeyMask;
static const NSEventModifierFlags NSEventModifierFlagNumericPad = NSNumericPadKeyMask;
static const NSEventModifierFlags NSEventModifierFlagShift = NSShiftKeyMask;
static const NSEventModifierFlags NSEventModifierFlagHelp = NSHelpKeyMask;

static const NSEventType NSEventTypeApplicationDefined = NSApplicationDefined;
static const NSEventType NSEventTypeFlagsChanged = NSFlagsChanged;
static const NSEventType NSEventTypeKeyDown = NSKeyDown;
static const NSEventType NSEventTypeKeyUp = NSKeyUp;
static const NSEventType NSEventTypeLeftMouseDown = NSLeftMouseDown;
static const NSEventType NSEventTypeLeftMouseDragged = NSLeftMouseDragged;
static const NSEventType NSEventTypeLeftMouseUp = NSLeftMouseUp;
static const NSEventType NSEventTypeMouseEntered = NSMouseEntered;
static const NSEventType NSEventTypeMouseExited = NSMouseExited;
static const NSEventType NSEventTypeMouseMoved = NSMouseMoved;
static const NSEventType NSEventTypeOtherMouseDown = NSOtherMouseDown;
static const NSEventType NSEventTypeOtherMouseDragged = NSOtherMouseDragged;
static const NSEventType NSEventTypeOtherMouseUp = NSOtherMouseUp;
static const NSEventType NSEventTypePeriodic = NSPeriodic;
static const NSEventType NSEventTypeRightMouseDown = NSRightMouseDown;
static const NSEventType NSEventTypeRightMouseDragged = NSRightMouseDragged;
static const NSEventType NSEventTypeRightMouseUp = NSRightMouseUp;
static const NSEventType NSEventTypeScrollWheel = NSScrollWheel;
static const NSEventType NSEventTypeSystemDefined = NSSystemDefined;

typedef NSUInteger NSWindowStyleMask;
static const NSWindowStyleMask NSWindowStyleMaskBorderless = NSBorderlessWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskClosable = NSClosableWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskFullScreen = NSFullScreenWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskFullSizeContentView = NSFullSizeContentViewWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskHUDWindow = NSHUDWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskMiniaturizable = NSMiniaturizableWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskNonactivatingPanel = NSNonactivatingPanelMask;
static const NSWindowStyleMask NSWindowStyleMaskResizable = NSResizableWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskTexturedBackground = NSTexturedBackgroundWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskTitled = NSTitledWindowMask;
static const NSWindowStyleMask NSWindowStyleMaskUtilityWindow = NSUtilityWindowMask;

#endif // __MAC_OS_X_VERSION_MAX_ALLOWED < 101200

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101300

typedef NSInteger NSControlStateValue;
static const NSControlStateValue NSControlStateValueMixed = NSMixedState;
static const NSControlStateValue NSControlStateValueOff = NSOffState;
static const NSControlStateValue NSControlStateValueOn = NSOnState;

static const NSLevelIndicatorStyle NSLevelIndicatorStyleRelevancy = NSRelevancyLevelIndicatorStyle;
static const NSLevelIndicatorStyle NSLevelIndicatorStyleContinuousCapacity = NSContinuousCapacityLevelIndicatorStyle;
static const NSLevelIndicatorStyle NSLevelIndicatorStyleDiscreteCapacity = NSDiscreteCapacityLevelIndicatorStyle;
static const NSLevelIndicatorStyle NSLevelIndicatorStyleRating = NSRatingLevelIndicatorStyle;

#endif // __MAC_OS_X_VERSION_MAX_ALLOWED < 101300

#endif // AppKitCompatibilityDeclarations_h
