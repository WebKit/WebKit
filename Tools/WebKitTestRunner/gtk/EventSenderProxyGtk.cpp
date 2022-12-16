/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Zan Dobersek <zandobersek@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EventSenderProxy.h"

#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestController.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/WebKitWebViewBaseInternal.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueArray.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/WTFString.h>

namespace WTR {

// WebCore and layout tests assume this value
static const float pixelsPerScrollTick = 40;

// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DOMKeyLocationStandard      = 0x00,
    DOMKeyLocationLeft          = 0x01,
    DOMKeyLocationRight         = 0x02,
    DOMKeyLocationNumpad        = 0x03
};


EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    , m_time(0)
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(kWKEventMouseButtonNoButton)
{
}

static inline WebKitWebViewBase* toWebKitGLibAPI(PlatformWKView view)
{
    return const_cast<WebKitWebViewBase*>(reinterpret_cast<const WebKitWebViewBase*>(view));
}

EventSenderProxy::~EventSenderProxy()
{
}

static unsigned eventSenderButtonToGDKButton(unsigned button)
{
    int mouseButton = 3;
    if (button <= 2)
        mouseButton = button + 1;
    // fast/events/mouse-click-events expects the 4th button to be treated as the middle button.
    else if (button == 3)
        mouseButton = 2;

    return mouseButton;
}

static guint webkitModifiersToGDKModifiers(WKEventModifiers wkModifiers)
{
    guint modifiers = 0;

    if (wkModifiers & kWKEventModifiersControlKey)
        modifiers |= GDK_CONTROL_MASK;
    if (wkModifiers & kWKEventModifiersShiftKey)
        modifiers |= GDK_SHIFT_MASK;
    if (wkModifiers & kWKEventModifiersAltKey)
        modifiers |= GDK_MOD1_MASK;
    if (wkModifiers & kWKEventModifiersMetaKey)
        modifiers |= GDK_META_MASK;
    if (wkModifiers & kWKEventModifiersCapsLockKey)
        modifiers |= GDK_LOCK_MASK;

    return modifiers;
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

static int getGDKKeySymForKeyRef(WKStringRef keyRef, unsigned location, guint* modifiers)
{
    if (location == DOMKeyLocationNumpad) {
        if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
            return GDK_KEY_KP_Left;
        if (WKStringIsEqualToUTF8CString(keyRef, "rightArrow"))
            return GDK_KEY_KP_Right;
        if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
            return GDK_KEY_KP_Up;
        if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
            return GDK_KEY_KP_Down;
        if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
            return GDK_KEY_KP_Page_Up;
        if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
            return GDK_KEY_KP_Page_Down;
        if (WKStringIsEqualToUTF8CString(keyRef, "home"))
            return GDK_KEY_KP_Home;
        if (WKStringIsEqualToUTF8CString(keyRef, "end"))
            return GDK_KEY_KP_End;
        if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
            return GDK_KEY_KP_Insert;
        if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
            return GDK_KEY_KP_Delete;

        return GDK_KEY_VoidSymbol;
    }

    if (WKStringIsEqualToUTF8CString(keyRef, "leftControl"))
        return GDK_KEY_Control_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightControl"))
        return GDK_KEY_Control_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftShift"))
        return GDK_KEY_Shift_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightShift"))
        return GDK_KEY_Shift_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftAlt"))
        return GDK_KEY_Alt_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightAlt"))
        return GDK_KEY_Alt_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftMeta"))
        return GDK_KEY_Meta_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightMeta"))
        return GDK_KEY_Meta_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
        return GDK_KEY_Left;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightArrow"))
        return GDK_KEY_Right;
    if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
        return GDK_KEY_Up;
    if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
        return GDK_KEY_Down;
    if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
        return GDK_KEY_Page_Up;
    if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
        return GDK_KEY_Page_Down;
    if (WKStringIsEqualToUTF8CString(keyRef, "home"))
        return GDK_KEY_Home;
    if (WKStringIsEqualToUTF8CString(keyRef, "end"))
        return GDK_KEY_End;
    if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
        return GDK_KEY_Insert;
    if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
        return GDK_KEY_Delete;
    if (WKStringIsEqualToUTF8CString(keyRef, "printScreen"))
        return GDK_KEY_Print;
    if (WKStringIsEqualToUTF8CString(keyRef, "menu"))
        return GDK_KEY_Menu;
    if (WKStringIsEqualToUTF8CString(keyRef, "F1"))
        return GDK_KEY_F1;
    if (WKStringIsEqualToUTF8CString(keyRef, "F2"))
        return GDK_KEY_F2;
    if (WKStringIsEqualToUTF8CString(keyRef, "F3"))
        return GDK_KEY_F3;
    if (WKStringIsEqualToUTF8CString(keyRef, "F4"))
        return GDK_KEY_F4;
    if (WKStringIsEqualToUTF8CString(keyRef, "F5"))
        return GDK_KEY_F5;
    if (WKStringIsEqualToUTF8CString(keyRef, "F6"))
        return GDK_KEY_F6;
    if (WKStringIsEqualToUTF8CString(keyRef, "F7"))
        return GDK_KEY_F7;
    if (WKStringIsEqualToUTF8CString(keyRef, "F8"))
        return GDK_KEY_F8;
    if (WKStringIsEqualToUTF8CString(keyRef, "F9"))
        return GDK_KEY_F9;
    if (WKStringIsEqualToUTF8CString(keyRef, "F10"))
        return GDK_KEY_F10;
    if (WKStringIsEqualToUTF8CString(keyRef, "F11"))
        return GDK_KEY_F11;
    if (WKStringIsEqualToUTF8CString(keyRef, "F12"))
        return GDK_KEY_F12;
    if (WKStringIsEqualToUTF8CString(keyRef, "escape"))
        return GDK_KEY_Escape;

    size_t bufferSize = WKStringGetMaximumUTF8CStringSize(keyRef);
    auto buffer = makeUniqueArray<char>(bufferSize);
    WKStringGetUTF8CString(keyRef, buffer.get(), bufferSize);
    char charCode = buffer.get()[0];

    if (charCode == '\n' || charCode == '\r')
        return GDK_KEY_Return;
    if (charCode == '\t')
        return GDK_KEY_Tab;
    if (charCode == '\x8')
        return GDK_KEY_BackSpace;
    if (charCode == 0x001B)
        return GDK_KEY_Escape;

    if (WTF::isASCIIUpper(charCode))
        *modifiers |= GDK_SHIFT_MASK;

    return gdk_unicode_to_keyval(static_cast<guint32>(buffer.get()[0]));
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    guint modifiers = webkitModifiersToGDKModifiers(wkModifiers);
    int gdkKeySym = getGDKKeySymForKeyRef(keyRef, location, &modifiers);
    webkitWebViewBaseSynthesizeKeyEvent(toWebKitGLibAPI(m_testController->mainWebView()->platformView()), KeyEventType::Insert, gdkKeySym, modifiers, ShouldTranslateKeyboardState::No);
}

void EventSenderProxy::rawKeyDown(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
}

void EventSenderProxy::rawKeyUp(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
}

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers, WKStringRef pointerType)
{
    unsigned gdkButton = eventSenderButtonToGDKButton(button);
    auto modifier = WebCore::stateModifierForGdkButton(gdkButton);

    // If the same mouse button is already in the down position don't
    // send another event as it may confuse Xvfb.
    if (m_mouseButtonsCurrentlyDown & modifier)
        return;

    updateClickCountForButton(button);
    m_mouseButtonsCurrentlyDown |= modifier;

    webkitWebViewBaseSynthesizeMouseEvent(toWebKitGLibAPI(m_testController->mainWebView()->platformView()),
        MouseEventType::Press, gdkButton, m_mouseButtonsCurrentlyDown, m_position.x, m_position.y, webkitModifiersToGDKModifiers(wkModifiers), m_clickCount, toWTFString(pointerType));
}

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers, WKStringRef pointerType)
{
    unsigned gdkButton = eventSenderButtonToGDKButton(button);
    auto modifier = WebCore::stateModifierForGdkButton(gdkButton);
    m_mouseButtonsCurrentlyDown &= ~modifier;
    webkitWebViewBaseSynthesizeMouseEvent(toWebKitGLibAPI(m_testController->mainWebView()->platformView()),
        MouseEventType::Release, gdkButton, m_mouseButtonsCurrentlyDown, m_position.x, m_position.y, webkitModifiersToGDKModifiers(wkModifiers), 0, toWTFString(pointerType));

    m_clickPosition = m_position;
    m_clickTime = m_time;
}

void EventSenderProxy::mouseMoveTo(double x, double y, WKStringRef pointerType)
{
    m_position.x = x;
    m_position.y = y;

    webkitWebViewBaseSynthesizeMouseEvent(toWebKitGLibAPI(m_testController->mainWebView()->platformView()),
        MouseEventType::Motion, 0, m_mouseButtonsCurrentlyDown, m_position.x, m_position.y, 0, 0, toWTFString(pointerType));
}

void EventSenderProxy::mouseScrollBy(int horizontal, int vertical)
{
    // Copy behaviour of Qt and EFL - just return in case of (0,0) mouse scroll
    if (!horizontal && !vertical)
        return;

    webkitWebViewBaseSynthesizeWheelEvent(toWebKitGLibAPI(m_testController->mainWebView()->platformView()),
        horizontal, vertical, m_position.x, m_position.y, WheelEventPhase::NoPhase, WheelEventPhase::NoPhase, m_hasPreciseDeltas);
}

void EventSenderProxy::continuousMouseScrollBy(int horizontal, int vertical, bool paged)
{
    // Gtk+ does not support paged scroll events.
    if (paged) {
        WTFLogAlways("EventSenderProxy::continuousMouseScrollBy not implemented for paged scroll events");
        return;
    }

    webkitWebViewBaseSynthesizeWheelEvent(toWebKitGLibAPI(m_testController->mainWebView()->platformView()),
        horizontal / pixelsPerScrollTick, vertical / pixelsPerScrollTick, m_position.x, m_position.y, WheelEventPhase::NoPhase, WheelEventPhase::NoPhase, m_hasPreciseDeltas);
}

void EventSenderProxy::monitorWheelEvents()
{
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int horizontal, int vertical, int phase, int momentum)
{
    WheelEventPhase eventPhase = WheelEventPhase::NoPhase;
    switch (phase) {
    case 0:
        eventPhase = WheelEventPhase::NoPhase;
        break;
    case 1:
        eventPhase = WheelEventPhase::Began;
        break;
    case 2:
        eventPhase = WheelEventPhase::Changed;
        break;
    case 4:
        eventPhase = WheelEventPhase::Ended;
        break;
    case 8:
        eventPhase = WheelEventPhase::Cancelled;
        break;
    case 128:
        eventPhase = WheelEventPhase::MayBegin;
        break;
    }

    WheelEventPhase eventMomentumPhase = WheelEventPhase::NoPhase;
    switch (momentum) {
    case 0:
        eventMomentumPhase = WheelEventPhase::NoPhase;
        break;
    case 1:
        eventMomentumPhase = WheelEventPhase::Began;
        break;
    case 2:
        eventMomentumPhase = WheelEventPhase::Changed;
        break;
    case 3:
        eventMomentumPhase = WheelEventPhase::Ended;
        break;
    }

    webkitWebViewBaseSynthesizeWheelEvent(toWebKitGLibAPI(m_testController->mainWebView()->platformView()),
        horizontal, vertical, m_position.x, m_position.y, eventPhase, eventMomentumPhase, m_hasPreciseDeltas);
}

void EventSenderProxy::setWheelHasPreciseDeltas(bool hasPreciseDeltas)
{
    m_hasPreciseDeltas = hasPreciseDeltas;
}

void EventSenderProxy::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

#if ENABLE(TOUCH_EVENTS)
void EventSenderProxy::addTouchPoint(int, int)
{
}

void EventSenderProxy::updateTouchPoint(int, int, int)
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

void EventSenderProxy::setTouchPointRadius(int, int)
{
}

void EventSenderProxy::setTouchModifier(WKEventModifiers, bool)
{
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WTR
