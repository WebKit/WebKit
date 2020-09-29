/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "EventSenderProxy.h"

#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestController.h"
#import <wtf/RetainPtr.h>
#import <WebKit/WKString.h>

namespace WTR {

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
{
    UNUSED_PARAM(m_testController);
    UNUSED_PARAM(m_leftMouseButtonDown);
    UNUSED_PARAM(eventNumber);
}

EventSenderProxy::~EventSenderProxy() = default;

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

void EventSenderProxy::mouseDown(unsigned buttonNumber, WKEventModifiers modifiers)
{
}

void EventSenderProxy::mouseUp(unsigned buttonNumber, WKEventModifiers modifiers)
{
}

void EventSenderProxy::mouseMoveTo(double x, double y)
{
}

void EventSenderProxy::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

void EventSenderProxy::keyDown(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
}

void EventSenderProxy::mouseScrollBy(int x, int y)
{
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int x, int y, int phase, int momentum)
{
}

void EventSenderProxy::continuousMouseScrollBy(int x, int y, bool paged)
{
}

#if ENABLE(TOUCH_EVENTS)

void EventSenderProxy::addTouchPoint(int x, int y)
{
}

void EventSenderProxy::updateTouchPoint(int index, int x, int y)
{
}

void EventSenderProxy::setTouchModifier(WKEventModifiers, bool enable)
{
}

void EventSenderProxy::setTouchPointRadius(int radiusX, int radiusY)
{
}

void EventSenderProxy::touchStart()
{
}

void EventSenderProxy::touchMove()
{
}

void EventSenderProxy::touchEnd()
{
}

void EventSenderProxy::touchCancel()
{
}

void EventSenderProxy::clearTouchPoints()
{
}

void EventSenderProxy::releaseTouchPoint(int index)
{
}

void EventSenderProxy::cancelTouchPoint(int index)
{
}

#endif

} // namespace WTR
