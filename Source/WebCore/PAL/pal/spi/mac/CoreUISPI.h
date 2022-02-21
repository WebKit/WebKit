/*
 * Copyright (C) 2018 Apple Inc.  All rights reserved.
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

#include <CoreUI/CoreUI.h>

#else

extern const CFStringRef kCUIAnimationStartTimeKey;

extern const CFStringRef kCUIAnimationTimeKey;

extern const CFStringRef kCUIIsFlippedKey;

extern const CFStringRef kCUIMaskOnlyKey;

extern const CFStringRef kCUIOrientationKey;
extern const CFStringRef kCUIOrientHorizontal;

extern const CFStringRef kCUIPresentationStateKey;
extern const CFStringRef kCUIPresentationStateActiveKey;
extern const CFStringRef kCUIPresentationStateInactive;

extern const CFStringRef kCUIScaleKey;

extern const CFStringRef kCUISizeKey;
extern const CFStringRef kCUISizeMini;
extern const CFStringRef kCUISizeSmall;
extern const CFStringRef kCUISizeRegular;

extern const CFStringRef kCUIStateKey;
extern const CFStringRef kCUIStateActive;
extern const CFStringRef kCUIStateDisabled;
extern const CFStringRef kCUIStatePressed;

extern const CFStringRef kCUIUserInterfaceLayoutDirectionKey;
extern const CFStringRef kCUIUserInterfaceLayoutDirectionLeftToRight;

extern const CFStringRef kCUIValueKey;

extern const CFStringRef kCUIWidgetKey;
extern const CFStringRef kCUIWidgetButtonLittleArrows;
extern const CFStringRef kCUIWidgetProgressIndeterminateBar;
extern const CFStringRef kCUIWidgetProgressBar;
extern const CFStringRef kCUIWidgetScrollBarTrackCorner;
#if HAVE(LARGE_CONTROL_SIZE)
extern const CFStringRef kCUIWidgetButtonComboBox;
#endif

#endif
