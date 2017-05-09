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

namespace WTR {

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    , m_time(0)
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(kWKEventMouseButtonNoButton)
{
}

EventSenderProxy::~EventSenderProxy()
{
}

void EventSenderProxy::mouseDown(unsigned, WKEventModifiers)
{
}

void EventSenderProxy::mouseUp(unsigned, WKEventModifiers)
{
}

void EventSenderProxy::mouseMoveTo(double, double)
{
}

void EventSenderProxy::mouseScrollBy(int, int)
{
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int, int, int, int)
{
}

void EventSenderProxy::continuousMouseScrollBy(int, int, bool)
{
}

void EventSenderProxy::leapForward(int)
{
}

void EventSenderProxy::keyDown(WKStringRef, WKEventModifiers, unsigned)
{
}

void EventSenderProxy::addTouchPoint(int, int)
{
}

void EventSenderProxy::updateTouchPoint(int, int, int)
{
}

void EventSenderProxy::setTouchModifier(WKEventModifiers, bool)
{
}

void EventSenderProxy::setTouchPointRadius(int, int)
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

void EventSenderProxy::releaseTouchPoint(int)
{
}

void EventSenderProxy::cancelTouchPoint(int)
{
}

} // namespace WTR
