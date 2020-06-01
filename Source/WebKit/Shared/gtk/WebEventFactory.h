/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#if USE(GTK4)
typedef struct _GdkEvent GdkEvent;
#else
typedef union _GdkEvent GdkEvent;
#endif

namespace WebKit {

class WebEventFactory {
public:
    static WebMouseEvent createWebMouseEvent(const GdkEvent*, int, Optional<WebCore::FloatSize>);
    static WebMouseEvent createWebMouseEvent(const GdkEvent*, const WebCore::IntPoint&, const WebCore::IntPoint&, int, Optional<WebCore::FloatSize>);
    static WebMouseEvent createWebMouseEvent(const WebCore::IntPoint&);
    static WebWheelEvent createWebWheelEvent(const GdkEvent*);
    static WebWheelEvent createWebWheelEvent(const GdkEvent*, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase);
    static WebWheelEvent createWebWheelEvent(const GdkEvent*, const WebCore::IntPoint&, const WebCore::IntPoint&, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase);
    static WebWheelEvent createWebWheelEvent(const GdkEvent*, const WebCore::IntPoint&, const WebCore::IntPoint&, const WebCore::FloatSize& wheelTicks, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase);
    static WebKeyboardEvent createWebKeyboardEvent(const GdkEvent*, const String&, bool handledByInputMethod, Optional<Vector<WebCore::CompositionUnderline>>&&, Optional<EditingRange>&&, Vector<String>&& commands);
#if ENABLE(TOUCH_EVENTS)
    static WebTouchEvent createWebTouchEvent(const GdkEvent*, Vector<WebPlatformTouchPoint>&&);
#endif
};

} // namespace WebKit
