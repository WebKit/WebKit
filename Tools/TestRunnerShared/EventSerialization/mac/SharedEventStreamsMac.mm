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

#import "config.h"
#import "SharedEventStreamsMac.h"

#if PLATFORM(MAC)

#define QUOTE(...) @(#__VA_ARGS__)

NSString *beginSwipeBackEventStream()
{
    return QUOTE([
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 128,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 0,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 61,
            "kCGEventGestureStartEndSeriesType" : 6,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 8,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 1,
            "kCGEventGestureScrollX" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 8,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis2" : 1,
            "kCGScrollWheelEventScrollPhase" : 1,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 8,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 12,
            "kCGEventGestureScrollY" : 1,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 17,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "kCGScrollWheelEventPointDeltaAxis2" : 4,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 17,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 23,
            "kCGEventGestureScrollY" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 25,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 1,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "kCGScrollWheelEventPointDeltaAxis2" : 11,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 25,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 28,
            "kCGEventGestureScrollY" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 33,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 2,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "kCGScrollWheelEventPointDeltaAxis2" : 16,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 33,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 35,
            "kCGEventGestureScrollY" : 1,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 41,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 2,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "kCGScrollWheelEventPointDeltaAxis2" : 24,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 41,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 45,
            "kCGEventGestureScrollY" : -1,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 49,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 4,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "kCGScrollWheelEventPointDeltaAxis2" : 37,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 49,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 57,
            "kCGEventGestureScrollY" : -3,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 57,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 5,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "kCGScrollWheelEventPointDeltaAxis2" : 54,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 57,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 81,
            "kCGEventGestureScrollY" : -5,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 65,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 9,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : -2,
            "kCGScrollWheelEventPointDeltaAxis2" : 92,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 65,
            "windowLocation" : "{400, 300}"
        },
    ]);
}

NSString *completeSwipeBackEventStream()
{
    return QUOTE([
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 75,
            "kCGEventGestureScrollY" : -3,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 73,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 11,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "kCGScrollWheelEventPointDeltaAxis2" : 106,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 73,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 85,
            "kCGEventGestureScrollY" : -3,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 81,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 13,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "kCGScrollWheelEventPointDeltaAxis2" : 127,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 81,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 91,
            "kCGEventGestureScrollY" : -1,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 89,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 14,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "kCGScrollWheelEventPointDeltaAxis2" : 139,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 89,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 136,
            "kCGEventGestureScrollY" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 97,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 20,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "kCGScrollWheelEventPointDeltaAxis2" : 204,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 97,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 92,
            "kCGEventGestureScrollY" : 5,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 105,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 14,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 136,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 105,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 2,
            "kCGEventGestureScrollX" : 133,
            "kCGEventGestureScrollY" : 14,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 113,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventDeltaAxis1" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 19,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : 7,
            "kCGScrollWheelEventPointDeltaAxis2" : 192,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 113,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "kCGEventGesturePhase" : 4,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 121,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 62,
            "kCGEventGestureStartEndSeriesType" : 6,
            "kCGSEventTypeField" : 29,
            "relativeTimeMS" : 121,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventScrollPhase" : 4,
            "kCGSEventTypeField" : 22,
            "relativeTimeMS" : 139,
            "windowLocation" : "{400, 300}"
        }
    ]);
}

#endif
