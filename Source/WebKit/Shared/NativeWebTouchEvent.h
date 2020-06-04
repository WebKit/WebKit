/*
 * Copyright (C) 2011 Benjamin Poulain <benjamin@webkit.org>
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

#include "WebEvent.h"

#if ENABLE(TOUCH_EVENTS)

#if PLATFORM(IOS_FAMILY)
#if defined(__OBJC__)
#include <UIKit/UIKit.h>
struct _UIWebTouchEvent;
#endif
#elif PLATFORM(GTK)
#include <WebCore/GUniquePtrGtk.h>
#elif USE(LIBWPE)
#include <wpe/wpe.h>
#endif

#endif // ENABLE(TOUCH_EVENTS)

namespace WebKit {

#if ENABLE(TOUCH_EVENTS)

class NativeWebTouchEvent : public WebTouchEvent {
public:
#if PLATFORM(IOS_FAMILY)
#if defined(__OBJC__)
    explicit NativeWebTouchEvent(const _UIWebTouchEvent*, UIKeyModifierFlags);
#endif
#elif PLATFORM(GTK)
    NativeWebTouchEvent(GdkEvent*, Vector<WebPlatformTouchPoint>&&);
    NativeWebTouchEvent(const NativeWebTouchEvent&);
    const GdkEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif USE(LIBWPE)
    NativeWebTouchEvent(struct wpe_input_touch_event*, float deviceScaleFactor);
    const struct wpe_input_touch_event_raw* nativeFallbackTouchPoint() const { return &m_fallbackTouchPoint; }
#elif PLATFORM(WIN)
    NativeWebTouchEvent();
#endif

private:
#if PLATFORM(IOS_FAMILY) && defined(__OBJC__)
    Vector<WebPlatformTouchPoint> extractWebTouchPoint(const _UIWebTouchEvent*);
#endif

#if PLATFORM(GTK)
    GUniquePtr<GdkEvent> m_nativeEvent;
#elif USE(LIBWPE)
    struct wpe_input_touch_event_raw m_fallbackTouchPoint;
#endif
};

#endif // ENABLE(TOUCH_EVENTS)

#if PLATFORM(IOS_FAMILY) && defined(__OBJC__)
OptionSet<WebEvent::Modifier> webEventModifierFlags(UIKeyModifierFlags);
#endif

} // namespace WebKit
