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

#if ENABLE(WPE_PLATFORM)
#include "EventSenderProxyClient.h"
#include <wtf/Vector.h>

namespace WTR {

class TestController;

class EventSenderProxyClientWPE final : public EventSenderProxyClient {
public:
    explicit EventSenderProxyClientWPE(TestController&);
    ~EventSenderProxyClientWPE();

private:
    void mouseDown(unsigned, double, WKEventModifiers, double, double, int /*clickCount*/, unsigned&) override;
    void mouseUp(unsigned, double, WKEventModifiers, double, double, unsigned&) override;
    void mouseMoveTo(double, double, double, WKEventMouseButton, unsigned) override;
    void mouseScrollBy(int, int, double, double, double) override;

    void keyDown(WKStringRef, double, WKEventModifiers, unsigned) override;
    void rawKeyDown(WKStringRef, WKEventModifiers, unsigned) override;
    void rawKeyUp(WKStringRef, WKEventModifiers, unsigned) override;

#if ENABLE(TOUCH_EVENTS)
    void addTouchPoint(int, int, double) override;
    void updateTouchPoint(int, int, int, double) override;
    void releaseTouchPoint(int, double) override;
    void cancelTouchPoint(int, double) override;
    void clearTouchPoints() override;
    void touchStart(double) override;
    void touchMove(double) override;
    void touchEnd(double) override;
    void touchCancel(double) override;
    void setTouchModifier(WKEventModifiers, bool);

    struct TouchPoint {
        enum class State { Down, Up, Move, Cancel, Stationary };

        uint32_t id { 0 };
        State state { State::Stationary };
        int x { 0 };
        int y { 0 };
    };

    struct TouchPointContext;

    std::function<bool(TouchPoint&)> pointProcessor(const TouchPointContext&);

    Vector<TouchPoint> m_touchPoints;
    unsigned m_touchModifiers { 0 };
#endif // ENABLE(TOUCH_EVENTS)
};

} // namespace WTR

#endif // ENABLE(WPE_PLATFORM)
