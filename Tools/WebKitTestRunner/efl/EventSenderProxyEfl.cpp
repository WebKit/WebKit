/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Zan Dobersek <zandobersek@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2012 Samsung Electronics
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
#include "PlatformWebView.h"
#include "TestController.h"

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <unistd.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTR {

enum WTREventType {
    WTREventTypeNone = 0,
    WTREventTypeMouseDown,
    WTREventTypeMouseUp,
    WTREventTypeMouseMove,
    WTREventTypeMouseScrollBy,
    WTREventTypeLeapForward
};

enum WTRMouseButton {
    WTRMouseButtonNone = -1,
    WTRMouseButtonLeft,
    WTRMouseButtonMiddle,
    WTRMouseButtonRight,
    WTRMouseButtonMiddleDuplicated,
};

enum EvasMouseButton {
    EvasMouseButtonNone = 0,
    EvasMouseButtonLeft,
    EvasMouseButtonMiddle,
    EvasMouseButtonRight
};

// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DOMKeyLocationStandard      = 0x00,
    DOMKeyLocationLeft          = 0x01,
    DOMKeyLocationRight         = 0x02,
    DOMKeyLocationNumpad        = 0x03
};

struct WTREvent {
    WTREventType eventType;
    unsigned delay;
    WKEventModifiers modifiers;
    int button;
    int horizontal;
    int vertical;

    WTREvent()
        : eventType(WTREventTypeNone)
        , delay(0)
        , modifiers(0)
        , button(-1)
        , horizontal(-1)
        , vertical(-1)
    {
    }

    WTREvent(WTREventType eventType, unsigned delay, WKEventModifiers modifiers, int button)
        : eventType(eventType)
        , delay(delay)
        , modifiers(modifiers)
        , button(button)
        , horizontal(-1)
        , vertical(-1)
    {
    }
};

struct KeyEventInfo : public RefCounted<KeyEventInfo> {
    KeyEventInfo(const CString& keyName, const CString& keyString)
        : keyName(keyName)
        , keyString(keyString)
    {
    }

    const CString keyName;
    const CString keyString;
};

static unsigned evasMouseButton(unsigned button)
{
    if (button == WTRMouseButtonLeft)
        return EvasMouseButtonLeft;
    if (button == WTRMouseButtonMiddle)
        return EvasMouseButtonMiddle;
    if (button == WTRMouseButtonRight)
        return EvasMouseButtonRight;
    if (button == WTRMouseButtonMiddleDuplicated)
        return EvasMouseButtonMiddle;

    return EvasMouseButtonNone;
}

static void setEvasModifiers(Evas* evas, WKEventModifiers wkModifiers)
{
    static const char* modifierNames[] = { "Shift", "Control", "Alt", "Meta" };
    for (unsigned modifier = 0; modifier < (sizeof(modifierNames) / sizeof(char*)); ++modifier) {
        if (wkModifiers & (1 << modifier))
            evas_key_modifier_on(evas, modifierNames[modifier]);
        else
            evas_key_modifier_off(evas, modifierNames[modifier]);
    }
}

static void dispatchMouseDownEvent(Evas* evas, unsigned button, WKEventModifiers wkModifiers, int clickCount)
{
    Evas_Button_Flags buttonFlags = EVAS_BUTTON_NONE;
    if (clickCount == 3)
        buttonFlags = EVAS_BUTTON_TRIPLE_CLICK;
    else if (clickCount == 2)
        buttonFlags = EVAS_BUTTON_DOUBLE_CLICK;

    setEvasModifiers(evas, wkModifiers);
    evas_event_feed_mouse_down(evas, button, buttonFlags, 0, 0);
    setEvasModifiers(evas, 0);
}

static void dispatchMouseUpEvent(Evas* evas, unsigned button, WKEventModifiers wkModifiers)
{
    setEvasModifiers(evas, wkModifiers);
    evas_event_feed_mouse_up(evas, button, EVAS_BUTTON_NONE, 0, 0);
    setEvasModifiers(evas, 0);
}

static void dispatchMouseMoveEvent(Evas* evas, int x, int y)
{
    evas_event_feed_mouse_move(evas, x, y, 0, 0);
}

static void dispatchMouseScrollByEvent(Evas* evas, int horizontal, int vertical)
{
    const int SCROLLLEFT = -10;
    const int SCROLLRIGHT = 10;
    const int SCROLLDOWN = -10;
    const int SCROLLUP = 10;

    if (horizontal > 0)
        evas_event_feed_mouse_wheel(evas, 1, SCROLLLEFT, 0, 0);
    else if (horizontal < 0)
        evas_event_feed_mouse_wheel(evas, 1, SCROLLRIGHT, 0, 0);

    if (vertical > 0)
        evas_event_feed_mouse_wheel(evas, 0, SCROLLUP, 0, 0);
    else if (vertical < 0)
        evas_event_feed_mouse_wheel(evas, 0, SCROLLDOWN, 0, 0);
}

static const PassRefPtr<KeyEventInfo> keyPadName(WKStringRef keyRef)
{
    if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
        return adoptRef(new KeyEventInfo("KP_Left", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "rightArrow"))
        return adoptRef(new KeyEventInfo("KP_Right", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
        return adoptRef(new KeyEventInfo("KP_Up", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
        return adoptRef(new KeyEventInfo("KP_Down", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
        return adoptRef(new KeyEventInfo("KP_Prior", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
        return adoptRef(new KeyEventInfo("KP_Next", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "home"))
        return adoptRef(new KeyEventInfo("KP_Home", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "end"))
        return adoptRef(new KeyEventInfo("KP_End", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
        return adoptRef(new KeyEventInfo("KP_Insert", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
        return adoptRef(new KeyEventInfo("KP_Delete", ""));

    size_t bufferSize = WKStringGetMaximumUTF8CStringSize(keyRef);
    OwnArrayPtr<char> buffer = adoptArrayPtr(new char[bufferSize]);
    WKStringGetUTF8CString(keyRef, buffer.get(), bufferSize);
    return adoptRef(new KeyEventInfo(buffer.get(), buffer.get()));
}

static const PassRefPtr<KeyEventInfo> keyName(WKStringRef keyRef)
{
    if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
        return adoptRef(new KeyEventInfo("Left", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "rightArrow"))
        return adoptRef(new KeyEventInfo("Right", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
        return adoptRef(new KeyEventInfo("Up", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
        return adoptRef(new KeyEventInfo("Down", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
        return adoptRef(new KeyEventInfo("Prior", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
        return adoptRef(new KeyEventInfo("Next", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "home"))
        return adoptRef(new KeyEventInfo("Home", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "end"))
        return adoptRef(new KeyEventInfo("End", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
        return adoptRef(new KeyEventInfo("Insert", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
        return adoptRef(new KeyEventInfo("Delete", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "printScreen"))
        return adoptRef(new KeyEventInfo("Print", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "menu"))
        return adoptRef(new KeyEventInfo("Menu", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F1"))
        return adoptRef(new KeyEventInfo("F1", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F2"))
        return adoptRef(new KeyEventInfo("F2", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F3"))
        return adoptRef(new KeyEventInfo("F3", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F4"))
        return adoptRef(new KeyEventInfo("F4", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F5"))
        return adoptRef(new KeyEventInfo("F5", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F6"))
        return adoptRef(new KeyEventInfo("F6", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F7"))
        return adoptRef(new KeyEventInfo("F7", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F8"))
        return adoptRef(new KeyEventInfo("F8", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F9"))
        return adoptRef(new KeyEventInfo("F9", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F10"))
        return adoptRef(new KeyEventInfo("F10", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F11"))
        return adoptRef(new KeyEventInfo("F11", ""));
    if (WKStringIsEqualToUTF8CString(keyRef, "F12"))
        return adoptRef(new KeyEventInfo("F12", ""));

    size_t bufferSize = WKStringGetMaximumUTF8CStringSize(keyRef);
    OwnArrayPtr<char> buffer = adoptArrayPtr(new char[bufferSize]);
    WKStringGetUTF8CString(keyRef, buffer.get(), bufferSize);
    char charCode = buffer.get()[0];

    if (charCode == '\n' || charCode == '\r')
        return adoptRef(new KeyEventInfo("Return", "\r"));
    if (charCode == '\t')
        return adoptRef(new KeyEventInfo("Tab", "\t"));
    if (charCode == '\x8')
        return adoptRef(new KeyEventInfo("BackSpace", "\x8"));
    if (charCode == ' ')
        return adoptRef(new KeyEventInfo("space", " "));

    return adoptRef(new KeyEventInfo(buffer.get(), buffer.get()));
}

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    , m_time(0)
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(WTRMouseButtonNone)
    , m_mouseButton(WTRMouseButtonNone)
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

void EventSenderProxy::dispatchEvent(const WTREvent& event)
{
    if (event.eventType == WTREventTypeMouseDown)
        dispatchMouseDownEvent(ecore_evas_get(m_testController->mainWebView()->platformWindow()), event.button, event.modifiers, m_clickCount);
    else if (event.eventType == WTREventTypeMouseUp)
        dispatchMouseUpEvent(ecore_evas_get(m_testController->mainWebView()->platformWindow()), event.button, event.modifiers);
    else if (event.eventType == WTREventTypeMouseMove)
        dispatchMouseMoveEvent(ecore_evas_get(m_testController->mainWebView()->platformWindow()), static_cast<int>(m_position.x), static_cast<int>(m_position.y));
    else if (event.eventType == WTREventTypeMouseScrollBy)
        dispatchMouseScrollByEvent(ecore_evas_get(m_testController->mainWebView()->platformWindow()), event.horizontal, event.vertical);
}

void EventSenderProxy::replaySavedEvents()
{
    while (!m_eventQueue.isEmpty()) {
        WTREvent event = m_eventQueue.takeFirst();
        if (event.delay)
            usleep(event.delay * 1000);

        dispatchEvent(event);
    }
}

void EventSenderProxy::sendOrQueueEvent(const WTREvent& event)
{
    if (m_eventQueue.isEmpty() || !m_eventQueue.last().delay) {
        dispatchEvent(event);
        return;
    }

    m_eventQueue.append(event);
    replaySavedEvents();
}

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers)
{
    if (m_mouseButton == button)
        return;

    m_mouseButton = button;
    updateClickCountForButton(button);

    sendOrQueueEvent(WTREvent(WTREventTypeMouseDown, 0, wkModifiers, evasMouseButton(button)));
}

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers)
{
    sendOrQueueEvent(WTREvent(WTREventTypeMouseUp, 0, wkModifiers, evasMouseButton(button)));

    if (m_mouseButton == button)
        m_mouseButton = WTRMouseButtonNone;

    m_clickPosition = m_position;
    m_clickTime = ecore_time_get();
}

void EventSenderProxy::mouseMoveTo(double x, double y)
{
    m_position.x = x;
    m_position.y = y;

    sendOrQueueEvent(WTREvent(WTREventTypeMouseMove, 0, 0, WTRMouseButtonNone));
}

void EventSenderProxy::mouseScrollBy(int horizontal, int vertical)
{
    WTREvent event(WTREventTypeMouseScrollBy, 0, 0, WTRMouseButtonNone);
    event.horizontal = horizontal;
    event.vertical = vertical;
    sendOrQueueEvent(event);
}

void EventSenderProxy::leapForward(int milliseconds)
{
    if (m_eventQueue.isEmpty())
        m_eventQueue.append(WTREvent(WTREventTypeLeapForward, milliseconds, 0, WTRMouseButtonNone));

    m_time += milliseconds / 1000.0;
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    const RefPtr<KeyEventInfo> keyEventInfo = (location == DOMKeyLocationNumpad) ? keyPadName(keyRef) : keyName(keyRef);
    if (!keyEventInfo)
        return;

    const char* keyName = keyEventInfo->keyName.data();
    const char* keyString = keyEventInfo->keyString.data();

    Evas* evas = ecore_evas_get(m_testController->mainWebView()->platformWindow());
    setEvasModifiers(evas, wkModifiers);
    evas_event_feed_key_down(evas, keyName, keyName, keyString, 0, 0, 0);
    evas_event_feed_key_up(evas, keyName, keyName, keyString, 0, 1, 0);
    setEvasModifiers(evas, 0);
}

#if ENABLE(TOUCH_EVENTS)
void EventSenderProxy::addTouchPoint(int x, int y)
{
    notImplemented();
}

void EventSenderProxy::updateTouchPoint(int index, int x, int y)
{
    notImplemented();
}

void EventSenderProxy::setTouchModifier(WKEventModifiers modifier, bool enable)
{
    notImplemented();
}

void EventSenderProxy::touchStart()
{
    notImplemented();
}

void EventSenderProxy::touchMove()
{
    notImplemented();
}

void EventSenderProxy::touchEnd()
{
    notImplemented();
}

void EventSenderProxy::touchCancel()
{
    notImplemented();
}

void EventSenderProxy::clearTouchPoints()
{
    notImplemented();
}

void EventSenderProxy::releaseTouchPoint(int index)
{
    notImplemented();
}

void EventSenderProxy::cancelTouchPoint(int index)
{
    notImplemented();
}

void EventSenderProxy::setTouchPointRadius(int radiusX, int radiusY)
{
    notImplemented();
}

#endif

}
