/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#include "config.h"
#include "WKEventEfl.h"

#include "EwkTouchEvent.h"
#include "EwkTouchPoint.h"
#include "ImmutableArray.h"
#include "WKAPICast.h"
#include "WebEvent.h"

using namespace WebKit;

WKTouchPointRef WKTouchPointCreate(int id, WKPoint position, WKPoint screenPosition, WKTouchPointState state, WKSize radius, float rotationAngle, float forceFactor)
{
#if ENABLE(TOUCH_EVENTS)
    return toAPI(EwkTouchPoint::create(id, state, screenPosition, position, radius, rotationAngle, forceFactor).leakRef());
#else
    UNUSED_PARAM(id);
    UNUSED_PARAM(position);
    UNUSED_PARAM(screenPosition);
    UNUSED_PARAM(state);
    UNUSED_PARAM(radius);
    UNUSED_PARAM(rotationAngle);
    UNUSED_PARAM(forceFactor);
    return 0;
#endif
}

WKTouchEventRef WKTouchEventCreate(WKEventType type, WKArrayRef wkTouchPoints, WKEventModifiers modifiers, double timestamp)
{
#if ENABLE(TOUCH_EVENTS)
    return toAPI(EwkTouchEvent::create(type, wkTouchPoints, modifiers, timestamp).leakRef());
#else
    UNUSED_PARAM(type);
    UNUSED_PARAM(wkTouchPoints);
    UNUSED_PARAM(modifiers);
    UNUSED_PARAM(timestamp);
    return 0;
#endif
}
