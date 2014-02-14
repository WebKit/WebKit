/*
 * Copyright (C) 2010, 2011, 2014 Apple Inc. All rights reserved.
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
#include <WebKit2/WKContextMenuItem.h>
#include <WebKit2/WKMutableDictionary.h>
#include <WebKit2/WKNumber.h>
#include <wtf/StdLibExtras.h>

namespace WTR {

static const float ZoomMultiplierRatio = 1.2f;

struct MenuItemPrivateData {
    MenuItemPrivateData(WKBundlePageRef page, WKContextMenuItemRef item) :
        m_page(page),
        m_item(item) { }
    WKBundlePageRef m_page;
    WKRetainPtr<WKContextMenuItemRef> m_item;
};

#if ENABLE(CONTEXT_MENUS)
static JSValueRef menuItemClickCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    MenuItemPrivateData* privateData = static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(thisObject));
    WKBundlePageClickMenuItem(privateData->m_page, privateData->m_item.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef getMenuItemTitleCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    MenuItemPrivateData* privateData = static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(object));
    WKRetainPtr<WKStringRef> wkTitle(AdoptWK, WKContextMenuItemCopyTitle(privateData->m_item.get()));
    return JSValueMakeString(context, toJS(wkTitle).get());
}

static JSStaticFunction staticMenuItemFunctions[] = {
    { "click", menuItemClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSStaticValue staticMenuItemValues[] = {
    { "title", getMenuItemTitleCallback, 0, kJSPropertyAttributeReadOnly },
    { 0, 0, 0, 0 }
};

static void staticMenuItemFinalize(JSObjectRef object)
{
    delete static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(object));
}

static JSValueRef staticConvertMenuItemToType(JSContextRef context, JSObjectRef object, JSType type, JSValueRef* exception)
{
    if (kJSTypeString == type)
        return getMenuItemTitleCallback(context, object, 0, 0);
    return 0;
}

static JSClassRef getMenuItemClass()
{
    static JSClassRef menuItemClass = 0;

    if (!menuItemClass) {
        JSClassDefinition classDefinition = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        classDefinition.staticFunctions = staticMenuItemFunctions;
        classDefinition.staticValues = staticMenuItemValues;
        classDefinition.finalize = staticMenuItemFinalize;
        classDefinition.convertToType = staticConvertMenuItemToType;

        menuItemClass = JSClassCreate(&classDefinition);
    }

    return menuItemClass;
}
#endif

static WKEventModifiers parseModifier(JSStringRef modifier)
{
    if (JSStringIsEqualToUTF8CString(modifier, "ctrlKey"))
        return kWKEventModifiersControlKey;
    if (JSStringIsEqualToUTF8CString(modifier, "shiftKey") || JSStringIsEqualToUTF8CString(modifier, "rangeSelectionKey"))
        return kWKEventModifiersShiftKey;
    if (JSStringIsEqualToUTF8CString(modifier, "altKey"))
        return kWKEventModifiersAltKey;
    if (JSStringIsEqualToUTF8CString(modifier, "metaKey"))
        return kWKEventModifiersMetaKey;
    if (JSStringIsEqualToUTF8CString(modifier, "addSelectionKey")) {
#if OS(MAC_OS_X)
        return kWKEventModifiersMetaKey;
#else
        return kWKEventModifiersControlKey;
#endif
    }
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

    // The value may either be a string with a single modifier or an array of modifiers.
    if (JSValueIsString(context, arrayValue)) {
        JSRetainPtr<JSStringRef> string(Adopt, JSValueToStringCopy(context, arrayValue, 0));
        return parseModifier(string.get());
    }

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
{
}

EventSendingController::~EventSendingController()
{
}

JSClassRef EventSendingController::wrapperClass()
{
    return JSEventSendingController::eventSendingControllerClass();
}

enum MouseState {
    MouseUp,
    MouseDown
};

static WKMutableDictionaryRef createMouseMessageBody(MouseState state, int button, WKEventModifiers modifiers)
{
    WKMutableDictionaryRef EventSenderMessageBody = WKMutableDictionaryCreate();

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString(state == MouseUp ? "MouseUp" : "MouseDown"));
    WKDictionarySetItem(EventSenderMessageBody, subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> buttonKey = adoptWK(WKStringCreateWithUTF8CString("Button"));
    WKRetainPtr<WKUInt64Ref> buttonRef = adoptWK(WKUInt64Create(button));
    WKDictionarySetItem(EventSenderMessageBody, buttonKey.get(), buttonRef.get());

    WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
    WKRetainPtr<WKUInt64Ref> modifiersRef = adoptWK(WKUInt64Create(modifiers));
    WKDictionarySetItem(EventSenderMessageBody, modifiersKey.get(), modifiersRef.get());

    return EventSenderMessageBody;
}

void EventSendingController::mouseDown(int button, JSValueRef modifierArray) 
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);

    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, createMouseMessageBody(MouseDown, button, modifiers));

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseUp(int button, JSValueRef modifierArray)
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);

    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, createMouseMessageBody(MouseUp, button, modifiers));

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseMoveTo(int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("MouseMoveTo"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef(AdoptWK, WKDoubleCreate(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef(AdoptWK, WKDoubleCreate(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::leapForward(int milliseconds)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("LeapForward"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> timeKey(AdoptWK, WKStringCreateWithUTF8CString("TimeInMilliseconds"));
    WKRetainPtr<WKUInt64Ref> timeRef(AdoptWK, WKUInt64Create(milliseconds));
    WKDictionarySetItem(EventSenderMessageBody.get(), timeKey.get(), timeRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::scheduleAsynchronousClick()
{
    WKEventModifiers modifiers = 0;
    int button = 0;

    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));

    // Asynchronous mouse down.
    WKRetainPtr<WKMutableDictionaryRef> mouseDownMessageBody(AdoptWK, createMouseMessageBody(MouseDown, button, modifiers));
    WKBundlePostMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), mouseDownMessageBody.get());

    // Asynchronous mouse up.
    WKRetainPtr<WKMutableDictionaryRef> mouseUpMessageBody(AdoptWK, createMouseMessageBody(MouseUp, button, modifiers));
    WKBundlePostMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), mouseUpMessageBody.get());
}

static WKRetainPtr<WKMutableDictionaryRef> createKeyDownMessageBody(JSStringRef key, WKEventModifiers modifiers, int location)
{
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("KeyDown"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> keyKey(AdoptWK, WKStringCreateWithUTF8CString("Key"));
    WKDictionarySetItem(EventSenderMessageBody.get(), keyKey.get(), toWK(key).get());

    WKRetainPtr<WKStringRef> modifiersKey(AdoptWK, WKStringCreateWithUTF8CString("Modifiers"));
    WKRetainPtr<WKUInt64Ref> modifiersRef(AdoptWK, WKUInt64Create(modifiers));
    WKDictionarySetItem(EventSenderMessageBody.get(), modifiersKey.get(), modifiersRef.get());

    WKRetainPtr<WKStringRef> locationKey(AdoptWK, WKStringCreateWithUTF8CString("Location"));
    WKRetainPtr<WKUInt64Ref> locationRef(AdoptWK, WKUInt64Create(location));
    WKDictionarySetItem(EventSenderMessageBody.get(), locationKey.get(), locationRef.get());

    return EventSenderMessageBody;
}

void EventSendingController::keyDown(JSStringRef key, JSValueRef modifierArray, int location)
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);

    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> keyDownMessageBody = createKeyDownMessageBody(key, modifiers, location);

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), keyDownMessageBody.get(), 0);
}

void EventSendingController::scheduleAsynchronousKeyDown(JSStringRef key)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> keyDownMessageBody = createKeyDownMessageBody(key, 0 /* modifiers */, 0 /* location */);

    WKBundlePostMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), keyDownMessageBody.get());
}

void EventSendingController::mouseScrollBy(int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("MouseScrollBy"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef(AdoptWK, WKDoubleCreate(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef(AdoptWK, WKDoubleCreate(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseScrollByWithWheelAndMomentumPhases(int x, int y, JSStringRef phaseStr, JSStringRef momentumStr, bool asyncScrolling)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());
    
    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("MouseScrollByWithWheelAndMomentumPhases"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());
    
    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef(AdoptWK, WKDoubleCreate(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());
    
    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef(AdoptWK, WKDoubleCreate(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    uint64_t phase = 0;
    if (JSStringIsEqualToUTF8CString(phaseStr, "none"))
        phase = 0;
    else if (JSStringIsEqualToUTF8CString(phaseStr, "began"))
        phase = 1; // kCGScrollPhaseBegan
    else if (JSStringIsEqualToUTF8CString(phaseStr, "changed"))
        phase = 2; // kCGScrollPhaseChanged
    else if (JSStringIsEqualToUTF8CString(phaseStr, "ended"))
        phase = 4; // kCGScrollPhaseEnded
    else if (JSStringIsEqualToUTF8CString(phaseStr, "cancelled"))
        phase = 8; // kCGScrollPhaseCancelled
    else if (JSStringIsEqualToUTF8CString(phaseStr, "maybegin"))
        phase = 128; // kCGScrollPhaseMayBegin

    WKRetainPtr<WKStringRef> phaseKey(AdoptWK, WKStringCreateWithUTF8CString("Phase"));
    WKRetainPtr<WKUInt64Ref> phaseRef(AdoptWK, WKUInt64Create(phase));
    WKDictionarySetItem(EventSenderMessageBody.get(), phaseKey.get(), phaseRef.get());

    uint64_t momentum = 0;
    if (JSStringIsEqualToUTF8CString(momentumStr, "none"))
        momentum = 0; // kCGMomentumScrollPhaseNone
    else if (JSStringIsEqualToUTF8CString(momentumStr, "begin"))
        momentum = 1; // kCGMomentumScrollPhaseBegin
    else if (JSStringIsEqualToUTF8CString(momentumStr, "continue"))
        momentum = 2; // kCGMomentumScrollPhaseContinue
    else if (JSStringIsEqualToUTF8CString(momentumStr, "end"))
        momentum = 3; // kCGMomentumScrollPhaseEnd

    WKRetainPtr<WKStringRef> momentumKey(AdoptWK, WKStringCreateWithUTF8CString("Momentum"));
    WKRetainPtr<WKUInt64Ref> momentumRef(AdoptWK, WKUInt64Create(momentum));
    WKDictionarySetItem(EventSenderMessageBody.get(), momentumKey.get(), momentumRef.get());

    if (asyncScrolling)
        WKBundlePostMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get());
    else
        WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::continuousMouseScrollBy(int x, int y, bool paged)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("ContinuousMouseScrollBy"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef(AdoptWK, WKDoubleCreate(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef(AdoptWK, WKDoubleCreate(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKRetainPtr<WKStringRef> pagedKey(AdoptWK, WKStringCreateWithUTF8CString("Paged"));
    WKRetainPtr<WKUInt64Ref> pagedRef(AdoptWK, WKUInt64Create(paged));
    WKDictionarySetItem(EventSenderMessageBody.get(), pagedKey.get(), pagedRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

JSValueRef EventSendingController::contextClick()
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
#if ENABLE(CONTEXT_MENUS)
    // Do mouse context click.
    mouseDown(2, 0);
    mouseUp(2, 0);

    WKRetainPtr<WKArrayRef> menuEntries = adoptWK(WKBundlePageCopyContextMenuItems(page));
    size_t entriesSize = WKArrayGetSize(menuEntries.get());
    auto jsValuesArray = std::make_unique<JSValueRef[]>(entriesSize);
    for (size_t i = 0; i < entriesSize; ++i) {
        ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(menuEntries.get(), i)) == WKContextMenuItemGetTypeID());

        WKContextMenuItemRef item = static_cast<WKContextMenuItemRef>(WKArrayGetItemAtIndex(menuEntries.get(), i));
        MenuItemPrivateData* privateData = new MenuItemPrivateData(page, item);
        jsValuesArray[i] = JSObjectMake(context, getMenuItemClass(), privateData);
    }

    return JSObjectMakeArray(context, entriesSize, jsValuesArray.get(), 0);
#else
    return JSValueMakeUndefined(context);
#endif
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

#if ENABLE(TOUCH_EVENTS)
void EventSendingController::addTouchPoint(int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("AddTouchPoint"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKUInt64Ref> xRef(AdoptWK, WKUInt64Create(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKUInt64Ref> yRef(AdoptWK, WKUInt64Create(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::updateTouchPoint(int index, int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("UpdateTouchPoint"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey(AdoptWK, WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef(AdoptWK, WKUInt64Create(index));
    WKDictionarySetItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKUInt64Ref> xRef(AdoptWK, WKUInt64Create(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKUInt64Ref> yRef(AdoptWK, WKUInt64Create(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::setTouchModifier(const JSStringRef &modifier, bool enable)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("SetTouchModifier"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

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
    WKDictionarySetItem(EventSenderMessageBody.get(), modifierKey.get(), modifierRef.get());

    WKRetainPtr<WKStringRef> enableKey(AdoptWK, WKStringCreateWithUTF8CString("Enable"));
    WKRetainPtr<WKUInt64Ref> enableRef(AdoptWK, WKUInt64Create(enable));
    WKDictionarySetItem(EventSenderMessageBody.get(), enableKey.get(), enableRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}


void EventSendingController::setTouchPointRadius(int radiusX, int radiusY)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("SetTouchPointRadius"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey(AdoptWK, WKStringCreateWithUTF8CString("RadiusX"));
    WKRetainPtr<WKUInt64Ref> xRef(AdoptWK, WKUInt64Create(radiusX));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey(AdoptWK, WKStringCreateWithUTF8CString("RadiusY"));
    WKRetainPtr<WKUInt64Ref> yRef(AdoptWK, WKUInt64Create(radiusY));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchStart()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("TouchStart"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchMove()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("TouchMove"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchEnd()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("TouchEnd"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchCancel()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("TouchCancel"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::clearTouchPoints()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("ClearTouchPoints"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::releaseTouchPoint(int index)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("ReleaseTouchPoint"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey(AdoptWK, WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef(AdoptWK, WKUInt64Create(index));
    WKDictionarySetItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::cancelTouchPoint(int index)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName(AdoptWK, WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName(AdoptWK, WKStringCreateWithUTF8CString("CancelTouchPoint"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey(AdoptWK, WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef(AdoptWK, WKUInt64Create(index));
    WKDictionarySetItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKBundlePostSynchronousMessage(InjectedBundle::shared().bundle(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}
#endif

// Object Creation

void EventSendingController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "eventSender", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

} // namespace WTR
