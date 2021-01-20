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
            "relativeTimeMS" : 0,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGEventGestureHIDType" : 6,
            "kCGSEventTypeField" : 29,
            "kCGEventGesturePhase" : 128,
            "windowLocation" : "{400, 300}"
        },
        {
            "relativeTimeMS" : 8,
            "kCGEventGestureHIDType" : 61,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureStartEndSeriesType" : 6,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 8,
            "windowLocation" : "{400, 300}",
            "kCGEventGesturePhase" : 1,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 2
        },
        {
            "relativeTimeMS" : 8,
            "windowLocation" : "{400, 300}",
            "kCGScrollWheelEventScrollPhase" : 1,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis2" : 1,
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 17,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : 1,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 12
        },
        {
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "relativeTimeMS" : 17,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventPointDeltaAxis2" : 4,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 25,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : 2,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 23
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 1,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "relativeTimeMS" : 25,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 11,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 33,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : 2,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 28
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 2,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "relativeTimeMS" : 33,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 16,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 41,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : 1,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 35
        },
        {
            "kCGScrollWheelEventDeltaAxis2" : 2,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "relativeTimeMS" : 41,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 24,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 49,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : -1,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 45
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 4,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "relativeTimeMS" : 49,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 37,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 57,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : -3,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 57
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 5,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "relativeTimeMS" : 57,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 54,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 65,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : -5,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 81
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 9,
            "kCGScrollWheelEventPointDeltaAxis1" : -2,
            "relativeTimeMS" : 65,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 92,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
    ]);
}

NSString *completeSwipeBackEventStream()
{
    return QUOTE([
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 73,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : -3,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 75
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 11,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "relativeTimeMS" : 73,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 106,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 81,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : -3,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 85
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 13,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "relativeTimeMS" : 81,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 127,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 89,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : -1,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 91
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 14,
            "kCGScrollWheelEventPointDeltaAxis1" : -1,
            "relativeTimeMS" : 89,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 139,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 97,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : 2,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 136
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 20,
            "kCGScrollWheelEventPointDeltaAxis1" : 1,
            "relativeTimeMS" : 97,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 204,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 105,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : 5,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 92
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 14,
            "kCGScrollWheelEventPointDeltaAxis1" : 2,
            "relativeTimeMS" : 105,
            "kCGScrollWheelEventIsContinuous" : 1,
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 136,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22
        },
        {
            "kCGEventGestureHIDType" : 6,
            "relativeTimeMS" : 113,
            "windowLocation" : "{400, 300}",
            "kCGEventGestureScrollY" : 14,
            "kCGEventGesturePhase" : 2,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureScrollX" : 133
        },
        {
            "kCGScrollWheelEventScrollPhase" : 2,
            "kCGScrollWheelEventPointDeltaAxis2" : 192,
            "kCGSEventTypeField" : 22,
            "kCGScrollWheelEventDeltaAxis1" : 1,
            "kCGScrollWheelEventIsContinuous" : 1,
            "relativeTimeMS" : 113,
            "kCGScrollWheelEventScrollCount" : 1,
            "kCGScrollWheelEventDeltaAxis2" : 19,
            "windowLocation" : "{400, 300}",
            "kCGScrollWheelEventPointDeltaAxis1" : 7
        },
        {
            "relativeTimeMS" : 121,
            "kCGEventScrollGestureFlagBits" : 1,
            "kCGEventGestureHIDType" : 6,
            "kCGSEventTypeField" : 29,
            "kCGEventGesturePhase" : 4,
            "windowLocation" : "{400, 300}"
        },
        {
            "relativeTimeMS" : 121,
            "kCGEventGestureHIDType" : 62,
            "kCGSEventTypeField" : 29,
            "kCGEventGestureStartEndSeriesType" : 6,
            "windowLocation" : "{400, 300}"
        },
        {
            "kCGScrollWheelEventScrollCount" : 1,
            "relativeTimeMS" : 139,
            "windowLocation" : "{400, 300}",
            "kCGSEventTypeField" : 22,
            "kCGScrollWheelEventScrollPhase" : 4,
            "kCGScrollWheelEventIsContinuous" : 1
        }
    ]);
}

#endif
