/*
 * Copyright (C) 2023 Igalia S.L.
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

#include <wtf/FastMalloc.h>

namespace WTR {

class TestController;

class EventSenderProxyClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(EventSenderProxyClient);
public:
    virtual ~EventSenderProxyClient() = default;

    virtual void mouseDown(unsigned, double, WKEventModifiers, double, double, int /*clickCount*/, unsigned&) = 0;
    virtual void mouseUp(unsigned, double, WKEventModifiers, double, double, unsigned&) = 0;
    virtual void mouseMoveTo(double, double, double, WKEventMouseButton, unsigned) = 0;
    virtual void mouseScrollBy(int, int, double, double, double) = 0;

    virtual void keyDown(WKStringRef, double, WKEventModifiers, unsigned) = 0;
    virtual void rawKeyDown(WKStringRef, WKEventModifiers, unsigned) = 0;
    virtual void rawKeyUp(WKStringRef, WKEventModifiers, unsigned) = 0;

#if ENABLE(TOUCH_EVENTS)
    virtual void addTouchPoint(int, int, double) { };
    virtual void updateTouchPoint(int, int, int, double) { };
    virtual void touchStart(double) { };
    virtual void touchMove(double) { };
    virtual void touchEnd(double) { };
    virtual void touchCancel(double) { };
    virtual void clearTouchPoints() { };
    virtual void releaseTouchPoint(int, double) { };
    virtual void cancelTouchPoint(int, double) { };
#endif // ENABLE(TOUCH_EVENTS)

protected:
    EventSenderProxyClient(TestController& controller)
        : m_testController(controller)
    {
    }

    TestController& m_testController;
};

} // namespace WTR
