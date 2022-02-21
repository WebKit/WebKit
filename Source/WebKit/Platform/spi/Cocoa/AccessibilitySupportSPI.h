/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(APPLE_INTERNAL_SDK)

#include <AccessibilitySupport.h>

#else

WTF_EXTERN_C_BEGIN

extern void _AXSSetReduceMotionEnabled(Boolean enabled);
extern void _AXSSetDarkenSystemColors(Boolean enabled);
extern Boolean _AXSKeyRepeatEnabled();
extern Boolean _AXSApplicationAccessibilityEnabled();
extern CFStringRef kAXSApplicationAccessibilityEnabledNotification;

#if PLATFORM(IOS_FAMILY)
extern CFStringRef kAXSReduceMotionPreference;

extern CFStringRef kAXSReduceMotionChangedNotification;
extern CFStringRef kAXSIncreaseButtonLegibilityNotification;
extern CFStringRef kAXSEnhanceTextLegibilityChangedNotification;
extern CFStringRef kAXSDarkenSystemColorsEnabledNotification;
extern CFStringRef kAXSInvertColorsEnabledNotification;

typedef enum {
    AXValueStateInvalid = -2,
    AXValueStateEmpty = -1,
    AXValueStateOff,
    AXValueStateOn
} AXValueState;

extern AXValueState _AXSReduceMotionEnabledApp(CFStringRef appID);
extern AXValueState _AXSIncreaseButtonLegibilityApp(CFStringRef appID);
extern AXValueState _AXSEnhanceTextLegibilityEnabledApp(CFStringRef appID);
extern AXValueState _AXDarkenSystemColorsApp(CFStringRef appID);
extern AXValueState _AXSInvertColorsEnabledApp(CFStringRef appID);

extern void _AXSSetReduceMotionEnabledApp(AXValueState enabled, CFStringRef appID);
extern void _AXSSetIncreaseButtonLegibilityApp(AXValueState enabled, CFStringRef appID);
extern void _AXSSetEnhanceTextLegibilityEnabledApp(AXValueState enabled, CFStringRef appID);
extern void _AXSSetDarkenSystemColorsApp(AXValueState enabled, CFStringRef appID);
extern void _AXSInvertColorsSetEnabledApp(AXValueState enabled, CFStringRef appID);
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(FULL_KEYBOARD_ACCESS)
extern CFStringRef kAXSFullKeyboardAccessEnabledNotification;
extern Boolean _AXSFullKeyboardAccessEnabled();
#endif

extern CFStringRef kAXSAccessibilityPreferenceDomain;

WTF_EXTERN_C_END

#endif
