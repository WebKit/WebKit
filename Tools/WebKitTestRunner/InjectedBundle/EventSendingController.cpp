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
#include "StringFunctions.h"
#include <WebKit2/WKBundle.h>
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundlePrivate.h>
#include <WebKit2/WKMutableDictionary.h>
#include <WebKit2/WKNumber.h>

namespace WTR {

static const float ZoomMultiplierRatio = 1.2f;

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
#ifdef USE_WEBPROCESS_EVENT_SIMULATION
    : m_time(0)
    , m_position()
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickPosition()
    , m_clickButton(kWKEventMouseButtonNoButton)
#endif
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

#ifdef USE_WEBPROCESS_EVENT_SIMULATION
    updateClickCount(button);
    WKBundlePageSimulateMouseDown(page, button, m_position, m_clickCount, modifiers, m_time);
#else
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("MouseDown"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> buttonKey = adoptWK(WKStringCreateWithUTF8CString("Button"));
    WKRetainPtr<WKUInt64Ref> buttonRef = adoptWK(WKUInt64Create(button));
    WKDictionaryAddItem(EventSenderMessageBody.get(), buttonKey.get(), buttonRef.get());

    WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
    WKRetainPtr<WKUInt64Ref> modifiersRef = adoptWK(WKUInt64Create(modifiers));
    WKDictionaryAddItem(EventSenderMessageBody.get(), modifiersKey.get(), modifiersRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
#endif
}

void EventSendingController::mouseUp(int button, JSValueRef modifierArray)
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);

#ifdef USE_WEBPROCESS_EVENT_SIMULATION
    updateClickCount(button);
    WKBundlePageSimulateMouseUp(page, button, m_position, m_clickCount, modifiers, m_time);
#else
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("MouseUp"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> buttonKey(AdoptWK, WKStringCreateWithUTF8CString("Button"));
    WKRetainPtr<WKUInt64Ref> buttonRef(AdoptWK, WKUInt64Create(button));
    WKDictionaryAddItem(EventSenderMessageBody.get(), buttonKey.get(), buttonRef.get());

    WKRetainPtr<WKStringRef> modifiersKey(AdoptWK, WKStringCreateWithUTF8CString("Modifiers"));
    WKRetainPtr<WKUInt64Ref> modifiersRef(AdoptWK, WKUInt64Create(modifiers));
    WKDictionaryAddItem(EventSenderMessageBody.get(), modifiersKey.get(), modifiersRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
#endif
}

void EventSendingController::mouseMoveTo(int x, int y)
{
#ifdef USE_WEBPROCESS_EVENT_SIMULATION
    m_position.x = x;
    m_position.y = y;
    WKBundlePageSimulateMouseMotion(InjectedBundle::shared().page()->page(), m_position, m_time);
#else
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("MouseMoveTo"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef(AdoptWK, WKDoubleCreate(x));
    WKDictionaryAddItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef(AdoptWK, WKDoubleCreate(y));
    WKDictionaryAddItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
#endif
}

void EventSendingController::leapForward(int milliseconds)
{
#ifdef USE_WEBPROCESS_EVENT_SIMULATION
    m_time += milliseconds / 1000.0;
#else
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("LeapForward"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> timeKey(AdoptWK, WKStringCreateWithUTF8CString("TimeInMilliseconds"));
    WKRetainPtr<WKUInt64Ref> timeRef(AdoptWK, WKUInt64Create(milliseconds));
    WKDictionaryAddItem(EventSenderMessageBody.get(), timeKey.get(), timeRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
#endif
}

void EventSendingController::keyDown(JSStringRef key, JSValueRef modifierArray, int location)
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);

    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("KeyDown"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> keyKey(AdoptWK, WKStringCreateWithUTF8CString("Key"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), keyKey.get(), toWK(key).get());

    WKRetainPtr<WKStringRef> modifiersKey(AdoptWK, WKStringCreateWithUTF8CString("Modifiers"));
    WKRetainPtr<WKUInt64Ref> modifiersRef(AdoptWK, WKUInt64Create(modifiers));
    WKDictionaryAddItem(EventSenderMessageBody.get(), modifiersKey.get(), modifiersRef.get());

    WKRetainPtr<WKStringRef> locationKey(AdoptWK, WKStringCreateWithUTF8CString("Location"));
    WKRetainPtr<WKUInt64Ref> locationRef(AdoptWK, WKUInt64Create(location));
    WKDictionaryAddItem(EventSenderMessageBody.get(), locationKey.get(), locationRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseScrollBy(int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("MouseScrollBy"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef(AdoptWK, WKDoubleCreate(x));
    WKDictionaryAddItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef(AdoptWK, WKDoubleCreate(y));
    WKDictionaryAddItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::continuousMouseScrollBy(int x, int y, bool paged)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("ContinuousMouseScrollBy"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef(AdoptWK, WKDoubleCreate(x));
    WKDictionaryAddItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef(AdoptWK, WKDoubleCreate(y));
    WKDictionaryAddItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKRetainPtr<WKStringRef> pagedKey(AdoptWK, WKStringCreateWithUTF8CString("Paged"));
    WKRetainPtr<WKUInt64Ref> pagedRef(AdoptWK, WKUInt64Create(paged));
    WKDictionaryAddItem(EventSenderMessageBody.get(), pagedKey.get(), pagedRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

#ifdef USE_WEBPROCESS_EVENT_SIMULATION
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
#endif

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

#if ENABLE(TOUCH_EVENTS)
void EventSendingController::addTouchPoint(int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("AddTouchPoint"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKUInt64Ref> xRef(AdoptWK, WKUInt64Create(x));
    WKDictionaryAddItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKUInt64Ref> yRef(AdoptWK, WKUInt64Create(y));
    WKDictionaryAddItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::updateTouchPoint(int index, int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("UpdateTouchPoint"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey(AdoptWK, WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef(AdoptWK, WKUInt64Create(index));
    WKDictionaryAddItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKUInt64Ref> xRef(AdoptWK, WKUInt64Create(x));
    WKDictionaryAddItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKUInt64Ref> yRef(AdoptWK, WKUInt64Create(y));
    WKDictionaryAddItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::setTouchModifier(const JSStringRef &modifier, bool enable)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("SetTouchModifier"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKEventModifiers mod = 0;
    if (JSStringIsEqualToUTF8CString(modifier, "ctrl"))
        mod = kWKEventModifiersControlKey;
    if (JSStringIsEqualToUTF8CString(modifier, "shift"))
        mod = kWKEventModifiersShiftKey;
    if (JSStringIsEqualToUTF8CString(modifier, "alt"))
        mod = kWKEventModifiersAltKey;
    if (JSStringIsEqualToUTF8CString(modifier, "meta"))
        mod = kWKEventModifiersMetaKey;

    WKRetainPtr<WKStringRef> modifierKey(AdoptWK, WKStringCreateWithUTF8CString("Modifier"));
    WKRetainPtr<WKUInt64Ref> modifierRef(AdoptWK, WKUInt64Create(mod));
    WKDictionaryAddItem(EventSenderMessageBody.get(), modifierKey.get(), modifierRef.get());

    WKRetainPtr<WKStringRef> enableKey(AdoptWK, WKStringCreateWithUTF8CString("Enable"));
    WKRetainPtr<WKUInt64Ref> enableRef(AdoptWK, WKUInt64Create(enable));
    WKDictionaryAddItem(EventSenderMessageBody.get(), enableKey.get(), enableRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}


void EventSendingController::setTouchPointRadius(int radiusX, int radiusY)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("SetTouchPointRadius"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("RadiusX"));
    WKRetainPtr<WKUInt64Ref> xRef(AdoptWK, WKUInt64Create(radiusX));
    WKDictionaryAddItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("RadiusY"));
    WKRetainPtr<WKUInt64Ref> yRef(AdoptWK, WKUInt64Create(radiusY));
    WKDictionaryAddItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchStart()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("TouchStart"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchMove()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("TouchMove"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchEnd()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("TouchEnd"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchCancel()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("TouchCancel"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::clearTouchPoints()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("ClearTouchPoints"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::releaseTouchPoint(int index)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("ReleaseTouchPoint"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey(AdoptWK, WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef(AdoptWK, WKUInt64Create(index));
    WKDictionaryAddItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::cancelTouchPoint(int index)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("CancelTouchPoint"));
    WKDictionaryAddItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey(AdoptWK, WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef(AdoptWK, WKUInt64Create(index));
    WKDictionaryAddItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}
#endif

// Object Creation

void EventSendingController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "eventSender", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

} // namespace WTR
