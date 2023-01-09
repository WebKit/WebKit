/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(GAMEPAD) && PLATFORM(COCOA)

#import <CoreHaptics/CoreHaptics.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(WebCore, CoreHaptics)

SOFT_LINK_CLASS_FOR_HEADER(WebCore, CHHapticEngine)
SOFT_LINK_CLASS_FOR_HEADER(WebCore, CHHapticPattern)

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticPatternKeyEvent, NSString *);
#define CHHapticPatternKeyEvent WebCore::get_CoreHaptics_CHHapticPatternKeyEvent()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticPatternKeyEventType, NSString *);
#define CHHapticPatternKeyEventType WebCore::get_CoreHaptics_CHHapticPatternKeyEventType()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticEventTypeHapticTransient, NSString *);
#define CHHapticEventTypeHapticTransient WebCore::get_CoreHaptics_CHHapticEventTypeHapticTransient()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticEventTypeHapticContinuous, NSString *);
#define CHHapticEventTypeHapticContinuous WebCore::get_CoreHaptics_CHHapticEventTypeHapticContinuous()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticPatternKeyTime, NSString *);
#define CHHapticPatternKeyTime WebCore::get_CoreHaptics_CHHapticPatternKeyTime()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticPatternKeyEventParameters, NSString *);
#define CHHapticPatternKeyEventParameters WebCore::get_CoreHaptics_CHHapticPatternKeyEventParameters()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticPatternKeyParameterID, NSString *);
#define CHHapticPatternKeyParameterID WebCore::get_CoreHaptics_CHHapticPatternKeyParameterID()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticPatternKeyParameterValue, NSString *);
#define CHHapticPatternKeyParameterValue WebCore::get_CoreHaptics_CHHapticPatternKeyParameterValue()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticEventParameterIDHapticIntensity, NSString *);
#define CHHapticEventParameterIDHapticIntensity WebCore::get_CoreHaptics_CHHapticEventParameterIDHapticIntensity()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticPatternKeyEventDuration, NSString *);
#define CHHapticPatternKeyEventDuration WebCore::get_CoreHaptics_CHHapticPatternKeyEventDuration()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreHaptics, CHHapticPatternKeyPattern, NSString *);
#define CHHapticPatternKeyPattern WebCore::get_CoreHaptics_CHHapticPatternKeyPattern()

#endif // ENABLE(GAMEPAD) && PLATFORM(COCOA)
