/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "EventSendingController.h"

#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSEventSendingController.h"
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundlePrivate.h>

namespace WTR {

static const float ZoomMultiplierRatio = 1.2f;

static bool operator==(const WKPoint& a, const WKPoint& b)
{
    return a.x == b.x && a.y == b.y;
}

static WKEventModifiers parseModifier(JSStringRef modifier)
{
    if (JSStringIsEqualToUTF8CString(modifier, "ctrlKey"))
        return kWKEventModifiersControlKey;
    if (JSStringIsEqualToUTF8CString(modifier, "shiftKey") || JSStringIsEqualToUTF8CString(modifier, "rangeSelectionKey"))
        return kWKEventModifiersShiftKey;
    if (JSStringIsEqualToUTF8CString(modifier, "altKey"))
        return kWKEventModifiersAltKey;
    if (JSStringIsEqualToUTF8CString(modifier, "metaKey") || JSStringIsEqualToUTF8CString(modifier, "addSelectionKey"))
        return kWKEventModifiersMetaKey;
    return 0;
}

static unsigned arrayLength(JSContextRef context, JSObjectRef array)
{
    JSRetainPtr<JSStringRef> lengthString(Adopt, JSStringCreateWithUTF8CString("length"));
    JSValueRef lengthValue = JSObjectGetProperty(context, array, lengthString.get(), 0);
    if (!lengthValue)
        return 0;
    return static_cast<unsigned>(JSValueToNumber(context, lengthValue, 0));
}

static WKEventModifiers parseModifierArray(JSContextRef context, JSValueRef arrayValue)
{
    if (!arrayValue)
        return 0;
    if (!JSValueIsObject(context, arrayValue))
        return 0;
    JSObjectRef array = const_cast<JSObjectRef>(arrayValue);
    unsigned length = arrayLength(context, array);
    WKEventModifiers modifiers = 0;
    for (unsigned i = 0; i < length; i++) {
        JSValueRef exception = 0;
        JSValueRef value = JSObjectGetPropertyAtIndex(context, array, i, &exception);
        if (exception)
            continue;
        JSRetainPtr<JSStringRef> string(Adopt, JSValueToStringCopy(context, value, &exception));
        if (exception)
            continue;
        modifiers |= parseModifier(string.get());
    }
    return modifiers;
}

PassRefPtr<EventSendingController> EventSendingController::create()
{
    return adoptRef(new EventSendingController);
}

EventSendingController::EventSendingController()
    : m_time(0)
    , m_position()
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickPosition()
    , m_clickButton(kWKEventMouseButtonNoButton)
{
}

EventSendingController::~EventSendingController()
{
}

JSClassRef EventSendingController::wrapperClass()
{
    return JSEventSendingController::eventSendingControllerClass();
}

void EventSendingController::mouseDown(int button, JSValueRef modifierArray)
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);
    updateClickCount(button);
    WKBundlePageSimulateMouseDown(page, button, m_position, m_clickCount, modifiers, m_time);
}

void EventSendingController::mouseUp(int button, JSValueRef modifierArray)
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);
    updateClickCount(button);
    WKBundlePageSimulateMouseUp(page, button, m_position, m_clickCount, modifiers, m_time);
}

void EventSendingController::mouseMoveTo(int x, int y)
{
    m_position.x = x;
    m_position.y = y;
    WKBundlePageSimulateMouseMotion(InjectedBundle::shared().page()->page(), m_position, m_time);
}

void EventSendingController::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

void EventSendingController::updateClickCount(WKEventMouseButton button)
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

void EventSendingController::textZoomIn()
{
    // Ensure page zoom is reset.
    WKBundlePageSetPageZoomFactor(InjectedBundle::shared().page()->page(), 1);

    double zoomFactor = WKBundlePageGetTextZoomFactor(InjectedBundle::shared().page()->page());
    WKBundlePageSetTextZoomFactor(InjectedBundle::shared().page()->page(), zoomFactor * ZoomMultiplierRatio);
}

void EventSendingController::textZoomOut()
{
    // Ensure page zoom is reset.
    WKBundlePageSetPageZoomFactor(InjectedBundle::shared().page()->page(), 1);

    double zoomFactor = WKBundlePageGetTextZoomFactor(InjectedBundle::shared().page()->page());
    WKBundlePageSetTextZoomFactor(InjectedBundle::shared().page()->page(), zoomFactor / ZoomMultiplierRatio);
}

void EventSendingController::zoomPageIn()
{
    // Ensure text zoom is reset.
    WKBundlePageSetTextZoomFactor(InjectedBundle::shared().page()->page(), 1);

    double zoomFactor = WKBundlePageGetPageZoomFactor(InjectedBundle::shared().page()->page());
    WKBundlePageSetPageZoomFactor(InjectedBundle::shared().page()->page(), zoomFactor * ZoomMultiplierRatio);
}

void EventSendingController::zoomPageOut()
{
    // Ensure text zoom is reset.
    WKBundlePageSetTextZoomFactor(InjectedBundle::shared().page()->page(), 1);

    double zoomFactor = WKBundlePageGetPageZoomFactor(InjectedBundle::shared().page()->page());
    WKBundlePageSetPageZoomFactor(InjectedBundle::shared().page()->page(), zoomFactor / ZoomMultiplierRatio);
}

void EventSendingController::scalePageBy(double scale, double x, double y)
{
    WKPoint origin = { x, y };
    WKBundlePageSetScaleAtOrigin(InjectedBundle::shared().page()->page(), scale, origin);
}

// Object Creation

void EventSendingController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "eventSender", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

} // namespace WTR
