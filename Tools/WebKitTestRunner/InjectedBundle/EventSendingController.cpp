/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "DictionaryFunctions.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSEventSendingController.h"
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
    auto wkTitle = adoptWK(WKContextMenuItemCopyTitle(privateData->m_item.get()));
    return JSValueMakeString(context, toJS(wkTitle).get());
}

static JSValueRef getMenuItemEnabledCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    auto* privateData = static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(object));
    return JSValueMakeBoolean(context, WKContextMenuItemGetEnabled(privateData->m_item.get()));
}

static JSClassRef getMenuItemClass();

static JSValueRef getMenuItemChildrenCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    MenuItemPrivateData* privateData = static_cast<MenuItemPrivateData*>(JSObjectGetPrivate(object));
    auto children = adoptWK(WKContextMenuCopySubmenuItems(privateData->m_item.get()));
    auto array = JSObjectMakeArray(context, 0, 0, 0);
    for (size_t i = 0; i < WKArrayGetSize(children.get()); ++i) {
        auto item = static_cast<WKContextMenuItemRef>(WKArrayGetItemAtIndex(children.get(), i));
        auto* privateData = new MenuItemPrivateData(InjectedBundle::singleton().page()->page(), item);
        JSObjectSetPropertyAtIndex(context, array, i, JSObjectMake(context, getMenuItemClass(), privateData), 0);
    }
    return array;
}

static const JSStaticFunction staticMenuItemFunctions[] = {
    { "click", menuItemClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static const JSStaticValue staticMenuItemValues[] = {
    { "title", getMenuItemTitleCallback, 0, kJSPropertyAttributeReadOnly },
    { "children", getMenuItemChildrenCallback, 0, kJSPropertyAttributeReadOnly },
    { "enabled", getMenuItemEnabledCallback, 0, kJSPropertyAttributeReadOnly },
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

static WKEventModifiers parseModifier(const JSRetainPtr<JSStringRef>& modifier)
{
    if (JSStringIsEqualToUTF8CString(modifier.get(), "ctrlKey"))
        return kWKEventModifiersControlKey;
    if (JSStringIsEqualToUTF8CString(modifier.get(), "shiftKey") || JSStringIsEqualToUTF8CString(modifier.get(), "rangeSelectionKey"))
        return kWKEventModifiersShiftKey;
    if (JSStringIsEqualToUTF8CString(modifier.get(), "altKey"))
        return kWKEventModifiersAltKey;
    if (JSStringIsEqualToUTF8CString(modifier.get(), "metaKey"))
        return kWKEventModifiersMetaKey;
    if (JSStringIsEqualToUTF8CString(modifier.get(), "capsLockKey"))
        return kWKEventModifiersCapsLockKey;
    if (JSStringIsEqualToUTF8CString(modifier.get(), "addSelectionKey")) {
#if OS(MACOS)
        return kWKEventModifiersMetaKey;
#else
        return kWKEventModifiersControlKey;
#endif
    }
    return 0;
}

#if ENABLE(TOUCH_EVENTS)

static uint64_t parseTouchModifier(JSStringRef modifier)
{
    if (JSStringIsEqualToUTF8CString(modifier, "ctrl"))
        return kWKEventModifiersControlKey;
    if (JSStringIsEqualToUTF8CString(modifier, "shift"))
        return kWKEventModifiersShiftKey;
    if (JSStringIsEqualToUTF8CString(modifier, "alt"))
        return kWKEventModifiersAltKey;
    if (JSStringIsEqualToUTF8CString(modifier, "metaKey"))
        return kWKEventModifiersMetaKey;
    return 0;
}

#endif

static WKEventModifiers parseModifierArray(JSContextRef context, JSValueRef arrayValue)
{
    if (!arrayValue)
        return 0;

    // The value may either be a string with a single modifier or an array of modifiers.
    if (JSValueIsString(context, arrayValue))
        return parseModifier(createJSString(context, arrayValue));

    if (!JSValueIsObject(context, arrayValue))
        return 0;
    JSObjectRef array = const_cast<JSObjectRef>(arrayValue);
    unsigned length = arrayLength(context, array);
    WKEventModifiers modifiers = 0;
    for (unsigned i = 0; i < length; i++) {
        if (auto value = JSObjectGetPropertyAtIndex(context, array, i, nullptr))
            modifiers |= parseModifier(createJSString(context, value));
    }
    return modifiers;
}

static WKEventModifiers parseModifierArray(JSValueRef arrayValue)
{
    return parseModifierArray(WKBundleFrameGetJavaScriptContext(WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page())), arrayValue);
}

Ref<EventSendingController> EventSendingController::create()
{
    return adoptRef(*new EventSendingController);
}

JSClassRef EventSendingController::wrapperClass()
{
    return JSEventSendingController::eventSendingControllerClass();
}

enum MouseState { MouseUp, MouseDown };

static WKRetainPtr<WKDictionaryRef> createMouseMessageBody(MouseState state, int button, WKEventModifiers modifiers, JSStringRef pointerType)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", state == MouseUp ? "MouseUp" : "MouseDown");
    setValue(body, "Button", adoptWK(WKUInt64Create(button)));
    setValue(body, "Modifiers", adoptWK(WKUInt64Create(modifiers)));
    if (pointerType)
        setValue(body, "PointerType", pointerType);
    return body;
}

void EventSendingController::mouseDown(int button, JSValueRef modifierArray, JSStringRef pointerType)
{
    postSynchronousPageMessage("EventSender", createMouseMessageBody(MouseDown, button, parseModifierArray(modifierArray), pointerType));
}

void EventSendingController::mouseUp(int button, JSValueRef modifierArray, JSStringRef pointerType)
{
    postSynchronousPageMessage("EventSender", createMouseMessageBody(MouseUp, button, parseModifierArray(modifierArray), pointerType));
}

void EventSendingController::mouseMoveTo(int x, int y, JSStringRef pointerType)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "MouseMoveTo");
    setValue(body, "X", adoptWK(WKDoubleCreate(x)));
    setValue(body, "Y", adoptWK(WKDoubleCreate(y)));
    if (pointerType)
        setValue(body, "PointerType", pointerType);
    m_position = WKPointMake(x, y);
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::mouseForceClick()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "MouseForceClick");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::startAndCancelMouseForceClick()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "StartAndCancelMouseForceClick");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::mouseForceDown()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "MouseForceDown");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::mouseForceUp()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "MouseForceUp");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::mouseForceChanged(double force)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "MouseForceChanged");
    setValue(body, "Force", force);
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::leapForward(int milliseconds)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "LeapForward");
    setValue(body, "TimeInMilliseconds", adoptWK(WKUInt64Create(milliseconds)));
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::scheduleAsynchronousClick()
{
    postPageMessage("EventSender", createMouseMessageBody(MouseDown, 0, 0, nullptr));
    postPageMessage("EventSender", createMouseMessageBody(MouseUp, 0, 0, nullptr));
}

static WKRetainPtr<WKMutableDictionaryRef> createKeyDownMessageBody(JSStringRef key, WKEventModifiers modifiers, int location)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "KeyDown");
    setValue(body, "Key", key);
    setValue(body, "Modifiers", adoptWK(WKUInt64Create(modifiers)));
    setValue(body, "Location", adoptWK(WKUInt64Create(location)));
    return body;
}

static WKRetainPtr<WKMutableDictionaryRef> createRawKeyDownMessageBody(JSStringRef key, WKEventModifiers modifiers, int location)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "RawKeyDown");
    setValue(body, "Key", key);
    setValue(body, "Modifiers", adoptWK(WKUInt64Create(modifiers)));
    setValue(body, "Location", adoptWK(WKUInt64Create(location)));
    return body;
}

static WKRetainPtr<WKMutableDictionaryRef> createRawKeyUpMessageBody(JSStringRef key, WKEventModifiers modifiers, int location)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "RawKeyUp");
    setValue(body, "Key", key);
    setValue(body, "Modifiers", adoptWK(WKUInt64Create(modifiers)));
    setValue(body, "Location", adoptWK(WKUInt64Create(location)));
    return body;
}

void EventSendingController::keyDown(JSStringRef key, JSValueRef modifierArray, int location)
{
    postSynchronousPageMessage("EventSender", createKeyDownMessageBody(key, parseModifierArray(modifierArray), location));
}

void EventSendingController::rawKeyDown(JSStringRef key, JSValueRef modifierArray, int location)
{
    postSynchronousPageMessage("EventSender", createRawKeyDownMessageBody(key, parseModifierArray(modifierArray), location));
}

void EventSendingController::rawKeyUp(JSStringRef key, JSValueRef modifierArray, int location)
{
    postSynchronousPageMessage("EventSender", createRawKeyUpMessageBody(key, parseModifierArray(modifierArray), location));
}

void EventSendingController::scheduleAsynchronousKeyDown(JSStringRef key)
{
    postPageMessage("EventSender", createKeyDownMessageBody(key, 0, 0));
}

void EventSendingController::mouseScrollBy(int x, int y)
{
    WKBundlePageForceRepaint(InjectedBundle::singleton().page()->page()); // Triggers a scrolling tree commit.

    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "MouseScrollBy");
    setValue(body, "X", adoptWK(WKDoubleCreate(x)));
    setValue(body, "Y", adoptWK(WKDoubleCreate(y)));
    postPageMessage("EventSender", body);
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
    uint64_t phase = cgEventPhaseFromString(phaseStr);
    uint64_t momentum = cgEventMomentumPhaseFromString(momentumStr);

    if (phase == 4 /* kCGScrollPhaseEnded */ || phase == 8 /* kCGScrollPhaseCancelled */)
        m_sentWheelPhaseEndOrCancel = true;
    if (momentum == 3 /* kCGMomentumScrollPhaseEnd */)
        m_sentWheelMomentumPhaseEnd = true;

    WKBundlePageForceRepaint(InjectedBundle::singleton().page()->page()); // Triggers a scrolling tree commit.

    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "MouseScrollByWithWheelAndMomentumPhases");
    setValue(body, "X", adoptWK(WKDoubleCreate(x)));
    setValue(body, "Y", adoptWK(WKDoubleCreate(y)));
    setValue(body, "Phase", phase);
    setValue(body, "Momentum", momentum);
    postPageMessage("EventSender", body);
}

void EventSendingController::setWheelHasPreciseDeltas(bool hasPreciseDeltas)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "SetWheelHasPreciseDeltas");
    setValue(body, "HasPreciseDeltas", hasPreciseDeltas);
    postPageMessage("EventSender", body);
}

void EventSendingController::continuousMouseScrollBy(int x, int y, bool paged)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "ContinuousMouseScrollBy");
    setValue(body, "X", adoptWK(WKDoubleCreate(x)));
    setValue(body, "Y", adoptWK(WKDoubleCreate(y)));
    setValue(body, "Paged", paged);
    // FIXME: This message should be asynchronous, as scrolling is intrinsically asynchronous.
    // See also: <https://bugs.webkit.org/show_bug.cgi?id=148256>.
    postSynchronousPageMessage("EventSender", body);
}

JSValueRef EventSendingController::contextClick()
{
    auto page = InjectedBundle::singleton().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
#if ENABLE(CONTEXT_MENUS)
    auto menuEntries = adoptWK(WKBundlePageCopyContextMenuAtPointInWindow(page, m_position));
    auto array = JSObjectMakeArray(context, 0, 0, 0);
    if (!menuEntries)
        return array;

    size_t entriesSize = WKArrayGetSize(menuEntries.get());
    for (size_t i = 0; i < entriesSize; ++i) {
        ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(menuEntries.get(), i)) == WKContextMenuItemGetTypeID());

        WKContextMenuItemRef item = static_cast<WKContextMenuItemRef>(WKArrayGetItemAtIndex(menuEntries.get(), i));
        MenuItemPrivateData* privateData = new MenuItemPrivateData(page, item);
        JSObjectSetPropertyAtIndex(context, array, i, JSObjectMake(context, getMenuItemClass(), privateData), 0);
    }

    return array;
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

MonitorWheelEventsOptions* toMonitorWheelEventsOptions(JSContextRef context, JSValueRef argument)
{
    if (!JSValueIsObject(context, argument))
        return nullptr;

    static MonitorWheelEventsOptions options;
    options.resetLatching = booleanProperty(context, (JSObjectRef)argument, "resetLatching", true);
    return &options;
}

void EventSendingController::monitorWheelEvents(MonitorWheelEventsOptions* options)
{
    auto page = InjectedBundle::singleton().page()->page();
    
    m_sentWheelPhaseEndOrCancel = false;
    m_sentWheelMomentumPhaseEnd = false;
    WKBundlePageStartMonitoringScrollOperations(page, options ? options->resetLatching : true);
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

    auto page = InjectedBundle::singleton().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    
    JSObjectRef functionCallbackObject = JSValueToObject(context, functionCallback, nullptr);
    if (!functionCallbackObject)
        return;
    
    JSValueProtect(context, functionCallbackObject);

    auto scrollCompletionCallbackData = makeUnique<ScrollCompletionCallbackData>(context, functionCallbackObject);
    auto scrollCompletionCallbackDataPtr = scrollCompletionCallbackData.release();
    bool callbackWillBeCalled = WKBundlePageRegisterScrollOperationCompletionCallback(page, executeCallback, m_sentWheelPhaseEndOrCancel, m_sentWheelMomentumPhaseEnd, scrollCompletionCallbackDataPtr);
    if (!callbackWillBeCalled) {
        // Reassign raw pointer to std::unique_ptr<> so it will not be leaked.
        scrollCompletionCallbackData.reset(scrollCompletionCallbackDataPtr);
    }
}

#if ENABLE(TOUCH_EVENTS)

void EventSendingController::addTouchPoint(int x, int y)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "AddTouchPoint");
    setValue(body, "X", static_cast<uint64_t>(x));
    setValue(body, "Y", static_cast<uint64_t>(y));
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::updateTouchPoint(int index, int x, int y)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "UpdateTouchPoint");
    setValue(body, "Index", static_cast<uint64_t>(index));
    setValue(body, "X", static_cast<uint64_t>(x));
    setValue(body, "Y", static_cast<uint64_t>(y));
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::setTouchModifier(JSStringRef modifier, bool enable)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "SetTouchModifier");
    setValue(body, "Modifier", parseTouchModifier(modifier));
    setValue(body, "Enable", enable);
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::setTouchPointRadius(int radiusX, int radiusY)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "SetTouchPointRadius");
    setValue(body, "RadiusX", static_cast<uint64_t>(radiusX));
    setValue(body, "RadiusY", static_cast<uint64_t>(radiusY));
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::touchStart()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "TouchStart");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::touchMove()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "TouchMove");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::touchEnd()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "TouchEnd");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::touchCancel()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "TouchCancel");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::clearTouchPoints()
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "ClearTouchPoints");
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::releaseTouchPoint(int index)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "ReleaseTouchPoint");
    setValue(body, "Index", static_cast<uint64_t>(index));
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::cancelTouchPoint(int index)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "CancelTouchPoint");
    setValue(body, "Index", static_cast<uint64_t>(index));
    postSynchronousPageMessage("EventSender", body);
}

#endif

#if ENABLE(MAC_GESTURE_EVENTS)

void EventSendingController::scaleGestureStart(double scale)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "ScaleGestureStart");
    setValue(body, "Scale", scale);
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::scaleGestureChange(double scale)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "ScaleGestureChange");
    setValue(body, "Scale", scale);
    postSynchronousPageMessage("EventSender", body);
}

void EventSendingController::scaleGestureEnd(double scale)
{
    auto body = adoptWK(WKMutableDictionaryCreate());
    setValue(body, "SubMessage", "ScaleGestureEnd");
    setValue(body, "Scale", scale);
    postSynchronousPageMessage("EventSender", body);
}

#endif // ENABLE(MAC_GESTURE_EVENTS)

// Object Creation

void EventSendingController::makeWindowObject(JSContextRef context)
{
    setGlobalObjectProperty(context, "eventSender", this);
}

} // namespace WTR
