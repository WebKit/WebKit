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

#pragma once

#import <wtf/Platform.h>

#if USE(APPLE_INTERNAL_SDK)

#import <CoreGraphics/CGEventPrivate.h>
#import <CoreGraphics/CGSEvent.h>

#else

WTF_EXTERN_C_BEGIN

static const CGEventField kCGSEventWindowIDField = (CGEventField)51;
static const CGEventField kCGSEventTypeField = (CGEventField)55;
static const CGEventField kCGEventGestureHIDType = (CGEventField)110;
static const CGEventField kCGEventGestureIsPreflight = (CGEventField)111;
static const CGEventField kCGEventGestureZoomValue = (CGEventField)113;
static const CGEventField kCGEventGestureRotationValue = (CGEventField)114;
static const CGEventField kCGEventGestureSwipeValue = (CGEventField)115;
static const CGEventField kCGEventGesturePreflightProgress = (CGEventField)116;
static const CGEventField kCGEventGestureStartEndSeriesType = (CGEventField)117;
static const CGEventField kCGEventGestureScrollX = (CGEventField)118;
static const CGEventField kCGEventGestureScrollY = (CGEventField)119;
static const CGEventField kCGEventGestureScrollZ = (CGEventField)120;
static const CGEventField kCGEventGestureSwipeMotion = (CGEventField)123;
static const CGEventField kCGEventGestureSwipeProgress = (CGEventField)124;
static const CGEventField kCGEventGestureSwipePositionX = (CGEventField)125;
static const CGEventField kCGEventGestureSwipePositionY = (CGEventField)126;
static const CGEventField kCGEventGestureSwipeVelocityX = (CGEventField)129;
static const CGEventField kCGEventGestureSwipeVelocityY = (CGEventField)130;
static const CGEventField kCGEventGestureSwipeVelocityZ = (CGEventField)131;
static const CGEventField kCGEventGesturePhase = (CGEventField)132;
static const CGEventField kCGEventGestureMask = (CGEventField)133;
static const CGEventField kCGEventGestureSwipeMask = (CGEventField)134;
static const CGEventField kCGEventScrollGestureFlagBits = (CGEventField)135;
static const CGEventField kCGEventSwipeGestureFlagBits = (CGEventField)136;
static const CGEventField kCGEventGestureFlavor = (CGEventField)138;
static const CGEventField kCGEventGestureZoomDeltaX = (CGEventField)139;
static const CGEventField kCGEventGestureZoomDeltaY = (CGEventField)140;
static const CGEventField kCGEventGestureProgress = (CGEventField)142;
static const CGEventField kCGEventGestureStage = (CGEventField)143;
static const CGEventField kCGEventGestureBehavior = (CGEventField)144;
static const CGEventField kCGEventTransitionProgress = (CGEventField)147;
static const CGEventField kCGEventStagePressure = (CGEventField)148;

enum {
    kCGSEventScrollWheel = 22,
    kCGSEventZoom = 28,
    kCGSEventGesture = 29,
    kCGSEventDockControl = 30,
    kCGSEventFluidTouchGesture = 31,
};
typedef uint32_t CGSEventType;

enum {
    kCGHIDEventTypeGestureStarted = 61,
    kCGHIDEventTypeGestureEnded = 62,
};
typedef uint32_t CGSHIDEventType;

typedef CF_ENUM(uint8_t, CGSGesturePhase)
{
    kCGSGesturePhaseNone = 0,
    kCGSGesturePhaseBegan = 1,
    kCGSGesturePhaseChanged = 2,
    kCGSGesturePhaseEnded = 4,
    kCGSGesturePhaseCancelled = 8,
    kCGSGesturePhaseMayBegin = 128
};

typedef CF_ENUM(uint8_t, CGSGestureBehavior)
{
    kCGSGestureBehaviorDeepPress = 5,
};

CGPoint CGEventGetWindowLocation(CGEventRef);
void CGEventSetWindowLocation(CGEventRef, CGPoint);

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)
