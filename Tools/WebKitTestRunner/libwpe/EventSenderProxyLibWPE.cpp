/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "EventSenderProxy.h"

#include "EventSenderProxyClientLibWPE.h"
#include "TestController.h"
#include <WebCore/NotImplemented.h>
#include <wtf/MonotonicTime.h>

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
#include "EventSenderProxyClientWPE.h"
#endif

namespace WTR {

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    // WPE event timestamps are just MonotonicTime, not actual WallTime, so we can
    // use any point of origin, as long as we are consistent.
    , m_time(MonotonicTime::now().secondsSinceEpoch().value())
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(kWKEventMouseButtonNoButton)
{
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    if (testController->useWPELegacyAPI())
        m_client = makeUnique<EventSenderProxyClientLibWPE>(*testController);
    else
        m_client = makeUnique<EventSenderProxyClientWPE>(*testController);
#else
    m_client = makeUnique<EventSenderProxyClientLibWPE>(*testController);
#endif
}

EventSenderProxy::~EventSenderProxy()
{
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

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers, WKStringRef pointerType)
{
    updateClickCountForButton(button);
    m_client->mouseDown(button, m_time, wkModifiers, m_position.x, m_position.y, m_clickCount, m_mouseButtonsCurrentlyDown);
}

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers, WKStringRef pointerType)
{
    m_client->mouseUp(button, m_time, wkModifiers, m_position.x, m_position.y, m_mouseButtonsCurrentlyDown);
    m_clickPosition = m_position;
    m_clickTime = m_time;
}

void EventSenderProxy::mouseMoveTo(double x, double y, WKStringRef pointerType)
{
    m_position.x = x;
    m_position.y = y;
    m_client->mouseMoveTo(x, y, m_time, m_clickButton, m_mouseButtonsCurrentlyDown);
}

void EventSenderProxy::mouseScrollBy(int horizontal, int vertical)
{
    // Copy behaviour of GTK - just return in case of (0,0) mouse scroll.
    if (!horizontal && !vertical)
        return;

    m_client->mouseScrollBy(horizontal, vertical, m_time, m_position.x, m_position.y);
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int horizontal, int vertical, int, int)
{
    mouseScrollBy(horizontal, vertical);
}

void EventSenderProxy::continuousMouseScrollBy(int, int, bool)
{
}

void EventSenderProxy::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    m_client->keyDown(keyRef, m_time, wkModifiers, location);
}

void EventSenderProxy::rawKeyDown(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
}

void EventSenderProxy::rawKeyUp(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
}

#if ENABLE(TOUCH_EVENTS)

void EventSenderProxy::addTouchPoint(int x, int y)
{
    m_client->addTouchPoint(x, y, m_time);
}

void EventSenderProxy::updateTouchPoint(int index, int x, int y)
{
    m_client->updateTouchPoint(index, x, y, m_time);
}

void EventSenderProxy::setTouchModifier(WKEventModifiers, bool)
{
    notImplemented();
}

void EventSenderProxy::setTouchPointRadius(int, int)
{
    notImplemented();
}

void EventSenderProxy::touchStart()
{
    m_client->touchStart(m_time);
}

void EventSenderProxy::touchMove()
{
    m_client->touchMove(m_time);
}

void EventSenderProxy::touchEnd()
{
    m_client->touchEnd(m_time);
}

void EventSenderProxy::touchCancel()
{
    m_client->touchCancel(m_time);
}

void EventSenderProxy::clearTouchPoints()
{
    m_client->clearTouchPoints();
}

void EventSenderProxy::releaseTouchPoint(int index)
{
    m_client->releaseTouchPoint(index, m_time);
}

void EventSenderProxy::cancelTouchPoint(int index)
{
    m_client->cancelTouchPoint(index, m_time);
}

#endif // ENABLE(TOUCH_EVENTS)

void EventSenderProxy::waitForPendingMouseEvents()
{
}

} // namespace WTR
