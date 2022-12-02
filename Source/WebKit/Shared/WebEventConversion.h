/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#ifndef WebEventConversion_h
#define WebEventConversion_h

#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformWheelEvent.h>

#if ENABLE(IOS_TOUCH_EVENTS)
#include <WebKitAdditions/PlatformTouchEventIOS.h>
#elif ENABLE(TOUCH_EVENTS)
#include <WebCore/PlatformTouchEvent.h>
#include <WebCore/PlatformTouchPoint.h>
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
#include <WebKitAdditions/PlatformGestureEventMac.h>
#endif

namespace WebKit {

class WebMouseEvent;
class WebWheelEvent;
class WebKeyboardEvent;

#if ENABLE(TOUCH_EVENTS)
class WebTouchEvent;
class WebTouchPoint;
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
class WebGestureEvent;
#endif

OptionSet<WebCore::PlatformEvent::Modifier> platform(OptionSet<WebEventModifier>);

WebCore::PlatformMouseEvent platform(const WebMouseEvent&);
WebCore::PlatformWheelEvent platform(const WebWheelEvent&);
WebCore::PlatformKeyboardEvent platform(const WebKeyboardEvent&);

#if ENABLE(TOUCH_EVENTS)
WebCore::PlatformTouchEvent platform(const WebTouchEvent&);
#if !ENABLE(IOS_TOUCH_EVENTS)
WebCore::PlatformTouchPoint platform(const WebTouchPoint&);
#endif
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
WebCore::PlatformGestureEvent platform(const WebGestureEvent&);
#endif

} // namespace WebKit

#endif // WebEventConversion_h
