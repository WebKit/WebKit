/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include "PlatformWebView.h"
#include "TestController.h"
#include <WebCore/NotImplemented.h>

namespace WTR {

// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DOMKeyLocationStandard      = 0x00,
    DOMKeyLocationLeft          = 0x01,
    DOMKeyLocationRight         = 0x02,
    DOMKeyLocationNumpad        = 0x03
};

enum ButtonState {
    ButtonReleased = 0,
    ButtonPressed = 1
};

enum PointerAxis {
    VerticalScroll = 0,
    HorizontalScroll = 1
};

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    , m_time(0)
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(kWKEventMouseButtonNoButton)
    , m_buttonState(ButtonReleased)
{
}

EventSenderProxy::~EventSenderProxy()
{
}

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers)
{
    notImplemented();
}

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers)
{
    notImplemented();
}

void EventSenderProxy::mouseMoveTo(double x, double y)
{
    notImplemented();
}

void EventSenderProxy::mouseScrollBy(int horizontal, int vertical)
{
    notImplemented();
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int horizontal, int vertical, int, int)
{
    notImplemented();
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
    notImplemented();
}

} // namespace WTR
