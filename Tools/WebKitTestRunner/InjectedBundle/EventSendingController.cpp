/*
 * Copyright (C) 2010, 2011, 2014-2015 Apple Inc. All rights reserved.
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
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePagePrivate.h>
#include <WebKit/WKBundlePrivate.h>
#include <WebKit/WKContextMenuItem.h>
#include <WebKit/WKMutableDictionary.h>
#include <WebKit/WKNumber.h>
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
static JSValueRef menuItemClickCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*)
{
    MenuItemPrivateData* privateData = static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(thisObject));
    WKBundlePageClickMenuItem(privateData->m_page, privateData->m_item.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef getMenuItemTitleCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    MenuItemPrivateData* privateData = static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(object));
    WKRetainPtr<WKStringRef> wkTitle = adoptWK(WKContextMenuItemCopyTitle(privateData->m_item.get()));
    return JSValueMakeString(context, toJS(wkTitle).get());
}

static JSClassRef getMenuItemClass();

static JSValueRef getMenuItemChildrenCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    MenuItemPrivateData* privateData = static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(object));
    auto children = adoptWK(WKContextMenuCopySubmenuItems(privateData->m_item.get()));

    JSValueRef arrayResult = JSObjectMakeArray(context, 0, 0, 0);
    if (!WKArrayGetSize(children.get()))
        return arrayResult;

    JSObjectRef arrayObject = JSValueToObject(context, arrayResult, 0);
    for (size_t i = 0; i < WKArrayGetSize(children.get()); ++i) {
        auto item = static_cast<WKContextMenuItemRef>(WKArrayGetItemAtIndex(children.get(), i));
        auto* privateData = new MenuItemPrivateData(InjectedBundle::singleton().page()->page(), item);
        JSObjectSetPropertyAtIndex(context, arrayObject, i, JSObjectMake(context, getMenuItemClass(), privateData), 0);
    }

    return arrayResult;
}

static JSStaticFunction staticMenuItemFunctions[] = {
    { "click", menuItemClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSStaticValue staticMenuItemValues[] = {
    { "title", getMenuItemTitleCallback, 0, kJSPropertyAttributeReadOnly },
    { "children", getMenuItemChildrenCallback, 0, kJSPropertyAttributeReadOnly },
    { 0, 0, 0, 0 }
};

static void staticMenuItemFinalize(JSObjectRef object)
{
    delete static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(object));
}

static JSValueRef staticConvertMenuItemToType(JSContextRef context, JSObjectRef object, JSType type, JSValueRef*)
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
    if (JSStringIsEqualToUTF8CString(modifier, "capsLockKey"))
        return kWKEventModifiersCapsLockKey;
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
    auto lengthString = adopt(JSStringCreateWithUTF8CString("length"));
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
        auto string = adopt(JSValueToStringCopy(context, arrayValue, 0));
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
        auto string = adopt(JSValueToStringCopy(context, value, &exception));
        if (exception)
            continue;
        modifiers |= parseModifier(string.get());
    }
    return modifiers;
}

Ref<EventSendingController> EventSendingController::create()
{
    return adoptRef(*new EventSendingController);
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

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString(state == MouseUp ? "MouseUp" : "MouseDown"));
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
    auto& injectedBundle = InjectedBundle::singleton();
    WKBundlePageRef page = injectedBundle.page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);

    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(createMouseMessageBody(MouseDown, button, modifiers));

    WKBundlePagePostSynchronousMessageForTesting(page, EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseUp(int button, JSValueRef modifierArray)
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);

    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(createMouseMessageBody(MouseUp, button, modifiers));

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseMoveTo(int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("MouseMoveTo"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef = adoptWK(WKDoubleCreate(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef = adoptWK(WKDoubleCreate(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    m_position = WKPointMake(x, y);
    
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseForceClick()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("MouseForceClick"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::startAndCancelMouseForceClick()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("StartAndCancelMouseForceClick"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseForceDown()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("MouseForceDown"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseForceUp()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("MouseForceUp"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::mouseForceChanged(double force)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("MouseForceChanged"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> forceKey = adoptWK(WKStringCreateWithUTF8CString("Force"));
    WKRetainPtr<WKDoubleRef> forceRef = adoptWK(WKDoubleCreate(force));
    WKDictionarySetItem(EventSenderMessageBody.get(), forceKey.get(), forceRef.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::leapForward(int milliseconds)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("LeapForward"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> timeKey = adoptWK(WKStringCreateWithUTF8CString("TimeInMilliseconds"));
    WKRetainPtr<WKUInt64Ref> timeRef = adoptWK(WKUInt64Create(milliseconds));
    WKDictionarySetItem(EventSenderMessageBody.get(), timeKey.get(), timeRef.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::scheduleAsynchronousClick()
{
    WKEventModifiers modifiers = 0;
    int button = 0;

    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));

    // Asynchronous mouse down.
    WKRetainPtr<WKMutableDictionaryRef> mouseDownMessageBody = adoptWK(createMouseMessageBody(MouseDown, button, modifiers));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), mouseDownMessageBody.get());

    // Asynchronous mouse up.
    WKRetainPtr<WKMutableDictionaryRef> mouseUpMessageBody = adoptWK(createMouseMessageBody(MouseUp, button, modifiers));
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), mouseUpMessageBody.get());
}

static WKRetainPtr<WKMutableDictionaryRef> createKeyDownMessageBody(JSStringRef key, WKEventModifiers modifiers, int location)
{
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("KeyDown"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> keyKey = adoptWK(WKStringCreateWithUTF8CString("Key"));
    WKDictionarySetItem(EventSenderMessageBody.get(), keyKey.get(), toWK(key).get());

    WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
    WKRetainPtr<WKUInt64Ref> modifiersRef = adoptWK(WKUInt64Create(modifiers));
    WKDictionarySetItem(EventSenderMessageBody.get(), modifiersKey.get(), modifiersRef.get());

    WKRetainPtr<WKStringRef> locationKey = adoptWK(WKStringCreateWithUTF8CString("Location"));
    WKRetainPtr<WKUInt64Ref> locationRef = adoptWK(WKUInt64Create(location));
    WKDictionarySetItem(EventSenderMessageBody.get(), locationKey.get(), locationRef.get());

    return EventSenderMessageBody;
}

void EventSendingController::keyDown(JSStringRef key, JSValueRef modifierArray, int location)
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    WKEventModifiers modifiers = parseModifierArray(context, modifierArray);

    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> keyDownMessageBody = createKeyDownMessageBody(key, modifiers, location);

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), keyDownMessageBody.get(), 0);
}

void EventSendingController::scheduleAsynchronousKeyDown(JSStringRef key)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> keyDownMessageBody = createKeyDownMessageBody(key, 0 /* modifiers */, 0 /* location */);

    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), keyDownMessageBody.get());
}

void EventSendingController::mouseScrollBy(int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("MouseScrollBy"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef = adoptWK(WKDoubleCreate(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef = adoptWK(WKDoubleCreate(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePageForceRepaint(InjectedBundle::singleton().page()->page()); // Triggers a scrolling tree commit.
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get());
}

static uint64_t cgEventPhaseFromString(JSStringRef phaseStr)
{
    if (JSStringIsEqualToUTF8CString(phaseStr, "none"))
        return 0;
    if (JSStringIsEqualToUTF8CString(phaseStr, "began"))
        return 1; // kCGScrollPhaseBegan
    if (JSStringIsEqualToUTF8CString(phaseStr, "changed"))
        return 2; // kCGScrollPhaseChanged
    if (JSStringIsEqualToUTF8CString(phaseStr, "ended"))
        return 4; // kCGScrollPhaseEnded
    if (JSStringIsEqualToUTF8CString(phaseStr, "cancelled"))
        return 8; // kCGScrollPhaseCancelled
    if (JSStringIsEqualToUTF8CString(phaseStr, "maybegin"))
        return 128; // kCGScrollPhaseMayBegin

    ASSERT_NOT_REACHED();
    return 0;
}

static uint64_t cgEventMomentumPhaseFromString(JSStringRef phaseStr)
{
    if (JSStringIsEqualToUTF8CString(phaseStr, "none"))
        return 0; // kCGMomentumScrollPhaseNone
    if (JSStringIsEqualToUTF8CString(phaseStr, "begin"))
        return 1; // kCGMomentumScrollPhaseBegin
    if (JSStringIsEqualToUTF8CString(phaseStr, "continue"))
        return 2; // kCGMomentumScrollPhaseContinue
    if (JSStringIsEqualToUTF8CString(phaseStr, "end"))
        return 3; // kCGMomentumScrollPhaseEnd

    ASSERT_NOT_REACHED();
    return 0;
}

void EventSendingController::mouseScrollByWithWheelAndMomentumPhases(int x, int y, JSStringRef phaseStr, JSStringRef momentumStr)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());
    
    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("MouseScrollByWithWheelAndMomentumPhases"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());
    
    WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef = adoptWK(WKDoubleCreate(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());
    
    WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef = adoptWK(WKDoubleCreate(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    uint64_t phase = cgEventPhaseFromString(phaseStr);
    uint64_t momentum = cgEventMomentumPhaseFromString(momentumStr);

    WKRetainPtr<WKStringRef> phaseKey = adoptWK(WKStringCreateWithUTF8CString("Phase"));
    WKRetainPtr<WKUInt64Ref> phaseRef = adoptWK(WKUInt64Create(phase));
    WKDictionarySetItem(EventSenderMessageBody.get(), phaseKey.get(), phaseRef.get());

    WKRetainPtr<WKStringRef> momentumKey = adoptWK(WKStringCreateWithUTF8CString("Momentum"));
    WKRetainPtr<WKUInt64Ref> momentumRef = adoptWK(WKUInt64Create(momentum));
    WKDictionarySetItem(EventSenderMessageBody.get(), momentumKey.get(), momentumRef.get());

    WKBundlePageForceRepaint(InjectedBundle::singleton().page()->page()); // Triggers a scrolling tree commit.
    WKBundlePagePostMessage(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get());
}

void EventSendingController::continuousMouseScrollBy(int x, int y, bool paged)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("ContinuousMouseScrollBy"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKDoubleRef> xRef = adoptWK(WKDoubleCreate(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKDoubleRef> yRef = adoptWK(WKDoubleCreate(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKRetainPtr<WKStringRef> pagedKey = adoptWK(WKStringCreateWithUTF8CString("Paged"));
    WKRetainPtr<WKUInt64Ref> pagedRef = adoptWK(WKUInt64Create(paged));
    WKDictionarySetItem(EventSenderMessageBody.get(), pagedKey.get(), pagedRef.get());

    // FIXME: This message should be asynchronous, as scrolling is intrinsically asynchronous.
    // See also: <https://bugs.webkit.org/show_bug.cgi?id=148256>.
    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

JSValueRef EventSendingController::contextClick()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
#if ENABLE(CONTEXT_MENUS)
    WKRetainPtr<WKArrayRef> menuEntries = adoptWK(WKBundlePageCopyContextMenuAtPointInWindow(page, m_position));
    JSValueRef arrayResult = JSObjectMakeArray(context, 0, 0, 0);
    if (!menuEntries)
        return arrayResult;

    JSObjectRef arrayObj = JSValueToObject(context, arrayResult, 0);
    size_t entriesSize = WKArrayGetSize(menuEntries.get());
    for (size_t i = 0; i < entriesSize; ++i) {
        ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(menuEntries.get(), i)) == WKContextMenuItemGetTypeID());

        WKContextMenuItemRef item = static_cast<WKContextMenuItemRef>(WKArrayGetItemAtIndex(menuEntries.get(), i));
        MenuItemPrivateData* privateData = new MenuItemPrivateData(page, item);
        JSObjectSetPropertyAtIndex(context, arrayObj, i, JSObjectMake(context, getMenuItemClass(), privateData), 0);
    }

    return arrayResult;
#else
    return JSValueMakeUndefined(context);
#endif
}

void EventSendingController::textZoomIn()
{
    auto& injectedBundle = InjectedBundle::singleton();
    // Ensure page zoom is reset.
    WKBundlePageSetPageZoomFactor(injectedBundle.page()->page(), 1);

    double zoomFactor = WKBundlePageGetTextZoomFactor(injectedBundle.page()->page());
    WKBundlePageSetTextZoomFactor(injectedBundle.page()->page(), zoomFactor * ZoomMultiplierRatio);
}

void EventSendingController::textZoomOut()
{
    auto& injectedBundle = InjectedBundle::singleton();
    // Ensure page zoom is reset.
    WKBundlePageSetPageZoomFactor(injectedBundle.page()->page(), 1);

    double zoomFactor = WKBundlePageGetTextZoomFactor(injectedBundle.page()->page());
    WKBundlePageSetTextZoomFactor(injectedBundle.page()->page(), zoomFactor / ZoomMultiplierRatio);
}

void EventSendingController::zoomPageIn()
{
    auto& injectedBundle = InjectedBundle::singleton();
    // Ensure text zoom is reset.
    WKBundlePageSetTextZoomFactor(injectedBundle.page()->page(), 1);

    double zoomFactor = WKBundlePageGetPageZoomFactor(injectedBundle.page()->page());
    WKBundlePageSetPageZoomFactor(injectedBundle.page()->page(), zoomFactor * ZoomMultiplierRatio);
}

void EventSendingController::zoomPageOut()
{
    auto& injectedBundle = InjectedBundle::singleton();
    // Ensure text zoom is reset.
    WKBundlePageSetTextZoomFactor(injectedBundle.page()->page(), 1);

    double zoomFactor = WKBundlePageGetPageZoomFactor(injectedBundle.page()->page());
    WKBundlePageSetPageZoomFactor(injectedBundle.page()->page(), zoomFactor / ZoomMultiplierRatio);
}

void EventSendingController::scalePageBy(double scale, double x, double y)
{
    WKPoint origin = { x, y };
    WKBundlePageSetScaleAtOrigin(InjectedBundle::singleton().page()->page(), scale, origin);
}

void EventSendingController::monitorWheelEvents()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    
    WKBundlePageStartMonitoringScrollOperations(page);
}

struct ScrollCompletionCallbackData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    JSContextRef m_context;
    JSObjectRef m_function;

    ScrollCompletionCallbackData(JSContextRef context, JSObjectRef function)
        : m_context(context), m_function(function)
    {
    }
};

static void executeCallback(void* context)
{
    if (!context)
        return;

    std::unique_ptr<ScrollCompletionCallbackData> callbackData(reinterpret_cast<ScrollCompletionCallbackData*>(context));

    JSObjectCallAsFunction(callbackData->m_context, callbackData->m_function, nullptr, 0, nullptr, nullptr);
    JSValueUnprotect(callbackData->m_context, callbackData->m_function);
}

void EventSendingController::callAfterScrollingCompletes(JSValueRef functionCallback)
{
    if (!functionCallback)
        return;

    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    
    JSObjectRef functionCallbackObject = JSValueToObject(context, functionCallback, nullptr);
    if (!functionCallbackObject)
        return;
    
    JSValueProtect(context, functionCallbackObject);

    auto scrollCompletionCallbackData = makeUnique<ScrollCompletionCallbackData>(context, functionCallbackObject);
    auto scrollCompletionCallbackDataPtr = scrollCompletionCallbackData.release();
    bool callbackWillBeCalled = WKBundlePageRegisterScrollOperationCompletionCallback(page, executeCallback, scrollCompletionCallbackDataPtr);
    if (!callbackWillBeCalled) {
        // Reassign raw pointer to std::unique_ptr<> so it will not be leaked.
        scrollCompletionCallbackData.reset(scrollCompletionCallbackDataPtr);
    }
}

#if ENABLE(TOUCH_EVENTS)
void EventSendingController::addTouchPoint(int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("AddTouchPoint"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKUInt64Ref> xRef = adoptWK(WKUInt64Create(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKUInt64Ref> yRef = adoptWK(WKUInt64Create(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::updateTouchPoint(int index, int x, int y)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("UpdateTouchPoint"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef = adoptWK(WKUInt64Create(index));
    WKDictionarySetItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
    WKRetainPtr<WKUInt64Ref> xRef = adoptWK(WKUInt64Create(x));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
    WKRetainPtr<WKUInt64Ref> yRef = adoptWK(WKUInt64Create(y));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::setTouchModifier(const JSStringRef &modifier, bool enable)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("SetTouchModifier"));
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

    WKRetainPtr<WKStringRef> modifierKey = adoptWK(WKStringCreateWithUTF8CString("Modifier"));
    WKRetainPtr<WKUInt64Ref> modifierRef = adoptWK(WKUInt64Create(mod));
    WKDictionarySetItem(EventSenderMessageBody.get(), modifierKey.get(), modifierRef.get());

    WKRetainPtr<WKStringRef> enableKey = adoptWK(WKStringCreateWithUTF8CString("Enable"));
    WKRetainPtr<WKUInt64Ref> enableRef = adoptWK(WKUInt64Create(enable));
    WKDictionarySetItem(EventSenderMessageBody.get(), enableKey.get(), enableRef.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}


void EventSendingController::setTouchPointRadius(int radiusX, int radiusY)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("SetTouchPointRadius"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("RadiusX"));
    WKRetainPtr<WKUInt64Ref> xRef = adoptWK(WKUInt64Create(radiusX));
    WKDictionarySetItem(EventSenderMessageBody.get(), xKey.get(), xRef.get());

    WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("RadiusY"));
    WKRetainPtr<WKUInt64Ref> yRef = adoptWK(WKUInt64Create(radiusY));
    WKDictionarySetItem(EventSenderMessageBody.get(), yKey.get(), yRef.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchStart()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("TouchStart"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchMove()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("TouchMove"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchEnd()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("TouchEnd"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::touchCancel()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("TouchCancel"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::clearTouchPoints()
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("ClearTouchPoints"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::releaseTouchPoint(int index)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("ReleaseTouchPoint"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef = adoptWK(WKUInt64Create(index));
    WKDictionarySetItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}

void EventSendingController::cancelTouchPoint(int index)
{
    WKRetainPtr<WKStringRef> EventSenderMessageName = adoptWK(WKStringCreateWithUTF8CString("EventSender"));
    WKRetainPtr<WKMutableDictionaryRef> EventSenderMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
    WKRetainPtr<WKStringRef> subMessageName = adoptWK(WKStringCreateWithUTF8CString("CancelTouchPoint"));
    WKDictionarySetItem(EventSenderMessageBody.get(), subMessageKey.get(), subMessageName.get());

    WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
    WKRetainPtr<WKUInt64Ref> indexRef = adoptWK(WKUInt64Create(index));
    WKDictionarySetItem(EventSenderMessageBody.get(), indexKey.get(), indexRef.get());

    WKBundlePagePostSynchronousMessageForTesting(InjectedBundle::singleton().page()->page(), EventSenderMessageName.get(), EventSenderMessageBody.get(), 0);
}
#endif

// Object Creation

void EventSendingController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "eventSender", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

} // namespace WTR
