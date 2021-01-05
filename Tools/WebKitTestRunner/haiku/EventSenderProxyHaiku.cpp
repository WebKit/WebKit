/*
  Copyright (C) 2014 Haiku, inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EventSenderProxy.h"

#include "NotImplemented.h"

namespace WTR {

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    , m_time(0)
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(kWKEventMouseButtonNoButton)
#if ENABLE(TOUCH_EVENTS)
    , m_touchPoints(0)
#endif
{
}

EventSenderProxy::~EventSenderProxy()
{
#if ENABLE(TOUCH_EVENTS)
    clearTouchPoints();
#endif
}

void EventSenderProxy::updateClickCountForButton(int button)
{
    if (m_time - m_clickTime < 1 && m_position == m_clickPosition && button == m_clickButton) {
        ++m_clickCount;
        m_clickTime = m_time;
        return;
    }

    m_clickCount = 1;
    m_clickTime = m_time;
    m_clickPosition = m_position;
    m_clickButton = button;
}

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers)
{
    notImplemented();
    updateClickCountForButton(button);
}

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers)
{
    notImplemented();

    m_clickPosition = m_position;
    m_clickTime = currentEventTime();
}

void EventSenderProxy::mouseMoveTo(double x, double y)
{
    m_position.x = x;
    m_position.y = y;

    notImplemented();
}

void EventSenderProxy::mouseScrollBy(int horizontal, int vertical)
{
    notImplemented();
}

void EventSenderProxy::continuousMouseScrollBy(int horizontal, int vertical, bool paged)
{
    notImplemented();
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int x, int y, int /*phase*/, int /*momentum*/)
{
    // EFL does not have the concept of wheel gesture phases or momentum. Just relay to
    // the mouse wheel handler.
    mouseScrollBy(x, y);
}

void EventSenderProxy::leapForward(int milliseconds)
{
    notImplemented();

    m_time += milliseconds / 1000.0;
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    notImplemented();
}

}
