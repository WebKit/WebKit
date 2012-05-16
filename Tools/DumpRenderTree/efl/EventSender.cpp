/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Zan Dobersek <zandobersek@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2011, 2012 Samsung Electronics
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "EventSender.h"

#include "DumpRenderTree.h"
#include "DumpRenderTreeChrome.h"
#include "JSStringUtils.h"
#include "NotImplemented.h"
#include "WebCoreSupport/DumpRenderTreeSupportEfl.h"
#include "ewk_private.h"
#include <EWebKit.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <wtf/ASCIICType.h>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>

static bool gDragMode;
static int gTimeOffset = 0;

static int gLastMousePositionX;
static int gLastMousePositionY;
static int gLastClickPositionX;
static int gLastClickPositionY;
static int gLastClickTimeOffset;
static int gLastClickButton;
static int gButtonCurrentlyDown;
static int gClickCount;

static const float zoomMultiplierRatio = 1.2f;

// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DomKeyLocationStandard,
    DomKeyLocationLeft,
    DomKeyLocationRight,
    DomKeyLocationNumpad
};

enum EvasKeyModifier {
    EvasKeyModifierNone    = 0,
    EvasKeyModifierControl = 1 << 0,
    EvasKeyModifierShift   = 1 << 1,
    EvasKeyModifierAlt     = 1 << 2,
    EvasKeyModifierMeta    = 1 << 3
};

enum EvasMouseButton {
    EvasMouseButtonNone,
    EvasMouseButtonLeft,
    EvasMouseButtonMiddle,
    EvasMouseButtonRight
};

enum EvasMouseEvent {
    EvasMouseEventNone        = 0,
    EvasMouseEventDown        = 1 << 0,
    EvasMouseEventUp          = 1 << 1,
    EvasMouseEventMove        = 1 << 2,
    EvasMouseEventScrollUp    = 1 << 3,
    EvasMouseEventScrollDown  = 1 << 4,
    EvasMouseEventScrollLeft  = 1 << 5,
    EvasMouseEventScrollRight = 1 << 6,
    EvasMouseEventClick       = EvasMouseEventMove | EvasMouseEventDown | EvasMouseEventUp,
};

enum ZoomEvent {
    ZoomIn,
    ZoomOut
};

enum EventQueueStrategy {
    FeedQueuedEvents,
    DoNotFeedQueuedEvents
};

struct KeyEventInfo {
    KeyEventInfo(const CString& keyName, const CString& keyString, EvasKeyModifier modifiers)
        : keyName(keyName)
        , keyString(keyString)
        , modifiers(modifiers)
    {
    }

    const CString keyName;
    const CString keyString;
    EvasKeyModifier modifiers;
};

struct MouseEventInfo {
    MouseEventInfo(EvasMouseEvent event, EvasKeyModifier modifiers = EvasKeyModifierNone, EvasMouseButton button = EvasMouseButtonNone, int horizontalDelta = 0, int verticalDelta = 0)
        : event(event)
        , modifiers(modifiers)
        , button(button)
        , horizontalDelta(horizontalDelta)
        , verticalDelta(verticalDelta)
    {
    }

    EvasMouseEvent event;
    EvasKeyModifier modifiers;
    EvasMouseButton button;
    int horizontalDelta;
    int verticalDelta;
};

struct DelayedEvent {
    DelayedEvent(MouseEventInfo* eventInfo, unsigned long delay = 0)
        : eventInfo(eventInfo)
        , delay(delay)
    {
    }

    MouseEventInfo* eventInfo;
    unsigned long delay;
};

WTF::Vector<DelayedEvent>& delayedEventQueue()
{
    DEFINE_STATIC_LOCAL(WTF::Vector<DelayedEvent>, staticDelayedEventQueue, ());
    return staticDelayedEventQueue;
}


static void feedOrQueueMouseEvent(MouseEventInfo*, EventQueueStrategy);
static void feedMouseEvent(MouseEventInfo*);
static void feedQueuedMouseEvents();

static void setEvasModifiers(Evas* evas, EvasKeyModifier modifiers)
{
    static const char* modifierNames[] = { "Control", "Shift", "Alt", "Super" };
    for (unsigned modifier = 0; modifier < 4; ++modifier) {
        if (modifiers & (1 << modifier))
            evas_key_modifier_on(evas, modifierNames[modifier]);
        else
            evas_key_modifier_off(evas, modifierNames[modifier]);
    }
}

static EvasMouseButton translateMouseButtonNumber(int eventSenderButtonNumber)
{
    static const EvasMouseButton translationTable[] = {
        EvasMouseButtonLeft,
        EvasMouseButtonMiddle,
        EvasMouseButtonRight,
        EvasMouseButtonMiddle // fast/events/mouse-click-events expects the 4th button to be treated as the middle button
    };
    static const unsigned translationTableSize = sizeof(translationTable) / sizeof(translationTable[0]);

    if (eventSenderButtonNumber < translationTableSize)
        return translationTable[eventSenderButtonNumber];

    return EvasMouseButtonLeft;
}

static Eina_Bool sendClick(void*)
{
    MouseEventInfo* eventInfo = new MouseEventInfo(EvasMouseEventClick, EvasKeyModifierNone, EvasMouseButtonLeft);
    feedOrQueueMouseEvent(eventInfo, FeedQueuedEvents);
    return ECORE_CALLBACK_CANCEL;
}

static JSValueRef scheduleAsynchronousClickCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ecore_idler_add(sendClick, 0);
    return JSValueMakeUndefined(context);
}

static void updateClickCount(int button)
{
    if (gLastClickPositionX != gLastMousePositionX
        || gLastClickPositionY != gLastMousePositionY
        || gLastClickButton != button
        || gTimeOffset - gLastClickTimeOffset >= 1)
        gClickCount = 1;
    else
        gClickCount++;
}

static EvasKeyModifier modifierFromJSValue(JSContextRef context, const JSValueRef value)
{
    JSRetainPtr<JSStringRef> jsKeyValue(Adopt, JSValueToStringCopy(context, value, 0));

    if (equals(jsKeyValue, "ctrlKey") || equals(jsKeyValue, "addSelectionKey"))
        return EvasKeyModifierControl;
    if (equals(jsKeyValue, "shiftKey") || equals(jsKeyValue, "rangeSelectionKey"))
        return EvasKeyModifierShift;
    if (equals(jsKeyValue, "altKey"))
        return EvasKeyModifierAlt;
    if (equals(jsKeyValue, "metaKey"))
        return EvasKeyModifierMeta;

    return EvasKeyModifierNone;
}

static EvasKeyModifier modifiersFromJSValue(JSContextRef context, const JSValueRef modifiers)
{
    // The value may either be a string with a single modifier or an array of modifiers.
    if (JSValueIsString(context, modifiers))
        return modifierFromJSValue(context, modifiers);

    JSObjectRef modifiersArray = JSValueToObject(context, modifiers, 0);
    if (!modifiersArray)
        return EvasKeyModifierNone;

    unsigned modifier = 0;
    JSRetainPtr<JSStringRef> lengthProperty(Adopt, JSStringCreateWithUTF8CString("length"));
    int modifiersCount = JSValueToNumber(context, JSObjectGetProperty(context, modifiersArray, lengthProperty.get(), 0), 0);
    for (int i = 0; i < modifiersCount; ++i)
        modifier |= static_cast<unsigned>(modifierFromJSValue(context, JSObjectGetPropertyAtIndex(context, modifiersArray, i, 0)));
    return static_cast<EvasKeyModifier>(modifier);
}

static JSValueRef mouseDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int button = 0;
    if (argumentCount == 1) {
        button = static_cast<int>(JSValueToNumber(context, arguments[0], exception));

        if (exception && *exception)
            return JSValueMakeUndefined(context);
    }

    button = translateMouseButtonNumber(button);
    // If the same mouse button is already in the down position don't send another event as it may confuse Xvfb.
    if (gButtonCurrentlyDown == button)
        return JSValueMakeUndefined(context);

    updateClickCount(button);

    EvasKeyModifier modifiers = argumentCount >= 2 ? modifiersFromJSValue(context, arguments[1]) : EvasKeyModifierNone;
    MouseEventInfo* eventInfo = new MouseEventInfo(EvasMouseEventDown, modifiers, static_cast<EvasMouseButton>(button));
    feedOrQueueMouseEvent(eventInfo, FeedQueuedEvents);
    gButtonCurrentlyDown = button;
    return JSValueMakeUndefined(context);
}

static JSValueRef mouseUpCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int button = 0;
    if (argumentCount == 1) {
        button = static_cast<int>(JSValueToNumber(context, arguments[0], exception));
        if (exception && *exception)
            return JSValueMakeUndefined(context);
    }

    gLastClickPositionX = gLastMousePositionX;
    gLastClickPositionY = gLastMousePositionY;
    gLastClickButton = gButtonCurrentlyDown;
    gLastClickTimeOffset = gTimeOffset;
    gButtonCurrentlyDown = 0;

    EvasKeyModifier modifiers = argumentCount >= 2 ? modifiersFromJSValue(context, arguments[1]) : EvasKeyModifierNone;
    MouseEventInfo* eventInfo = new MouseEventInfo(EvasMouseEventUp, modifiers, translateMouseButtonNumber(button));
    feedOrQueueMouseEvent(eventInfo, FeedQueuedEvents);
    return JSValueMakeUndefined(context);
}

static JSValueRef mouseMoveToCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    gLastMousePositionX = static_cast<int>(JSValueToNumber(context, arguments[0], exception));
    if (exception && *exception)
        return JSValueMakeUndefined(context);
    gLastMousePositionY = static_cast<int>(JSValueToNumber(context, arguments[1], exception));
    if (exception && *exception)
        return JSValueMakeUndefined(context);

    MouseEventInfo* eventInfo = new MouseEventInfo(EvasMouseEventMove);
    feedOrQueueMouseEvent(eventInfo, DoNotFeedQueuedEvents);
    return JSValueMakeUndefined(context);
}

static JSValueRef leapForwardCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount > 0) {
        const unsigned long leapForwardDelay = JSValueToNumber(context, arguments[0], exception);
        if (delayedEventQueue().isEmpty())
            delayedEventQueue().append(DelayedEvent(0, leapForwardDelay));
        else
            delayedEventQueue().last().delay = leapForwardDelay;
        gTimeOffset += leapForwardDelay;
    }

    return JSValueMakeUndefined(context);
}

static EvasMouseEvent evasMouseEventFromHorizontalAndVerticalOffsets(int horizontalOffset, int verticalOffset)
{
    unsigned mouseEvent = 0;

    if (verticalOffset > 0)
        mouseEvent |= EvasMouseEventScrollUp;
    else if (verticalOffset < 0)
        mouseEvent |= EvasMouseEventScrollDown;

    if (horizontalOffset > 0)
        mouseEvent |= EvasMouseEventScrollRight;
    else if (horizontalOffset < 0)
        mouseEvent |= EvasMouseEventScrollLeft;

    return static_cast<EvasMouseEvent>(mouseEvent);
}

static JSValueRef mouseScrollByCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    // We need to invert scrolling values since in EFL negative z value means that
    // canvas is scrolling down
    const int horizontal = -(static_cast<int>(JSValueToNumber(context, arguments[0], exception)));
    if (exception && *exception)
        return JSValueMakeUndefined(context);
    const int vertical = -(static_cast<int>(JSValueToNumber(context, arguments[1], exception)));
    if (exception && *exception)
        return JSValueMakeUndefined(context);

    MouseEventInfo* eventInfo = new MouseEventInfo(evasMouseEventFromHorizontalAndVerticalOffsets(horizontal, vertical), EvasKeyModifierNone, EvasMouseButtonNone, horizontal, vertical);
    feedOrQueueMouseEvent(eventInfo, FeedQueuedEvents);
    return JSValueMakeUndefined(context);
}

static JSValueRef continuousMouseScrollByCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeUndefined(context);
}

static KeyEventInfo* keyPadNameFromJSValue(JSStringRef character, EvasKeyModifier modifiers)
{
    if (equals(character, "leftArrow"))
        return new KeyEventInfo("KP_Left", "", modifiers);
    if (equals(character, "rightArrow"))
        return new KeyEventInfo("KP_Right", "", modifiers);
    if (equals(character, "upArrow"))
        return new KeyEventInfo("KP_Up", "", modifiers);
    if (equals(character, "downArrow"))
        return new KeyEventInfo("KP_Down", "", modifiers);
    if (equals(character, "pageUp"))
        return new KeyEventInfo("KP_Prior", "", modifiers);
    if (equals(character, "pageDown"))
        return new KeyEventInfo("KP_Next", "", modifiers);
    if (equals(character, "home"))
        return new KeyEventInfo("KP_Home", "", modifiers);
    if (equals(character, "end"))
        return new KeyEventInfo("KP_End", "", modifiers);
    if (equals(character, "insert"))
        return new KeyEventInfo("KP_Insert", "", modifiers);
    if (equals(character, "delete"))
        return new KeyEventInfo("KP_Delete", "", modifiers);

    return new KeyEventInfo(character->ustring().utf8(), character->ustring().utf8(), modifiers);
}

static KeyEventInfo* keyNameFromJSValue(JSStringRef character, EvasKeyModifier modifiers)
{
    if (equals(character, "leftArrow"))
        return new KeyEventInfo("Left", "", modifiers);
    if (equals(character, "rightArrow"))
        return new KeyEventInfo("Right", "", modifiers);
    if (equals(character, "upArrow"))
        return new KeyEventInfo("Up", "", modifiers);
    if (equals(character, "downArrow"))
        return new KeyEventInfo("Down", "", modifiers);
    if (equals(character, "pageUp"))
        return new KeyEventInfo("Prior", "", modifiers);
    if (equals(character, "pageDown"))
        return new KeyEventInfo("Next", "", modifiers);
    if (equals(character, "home"))
        return new KeyEventInfo("Home", "", modifiers);
    if (equals(character, "end"))
        return new KeyEventInfo("End", "", modifiers);
    if (equals(character, "insert"))
        return new KeyEventInfo("Insert", "", modifiers);
    if (equals(character, "delete"))
        return new KeyEventInfo("Delete", "", modifiers);
    if (equals(character, "printScreen"))
        return new KeyEventInfo("Print", "", modifiers);
    if (equals(character, "menu"))
        return new KeyEventInfo("Menu", "", modifiers);
    if (equals(character, "F1"))
        return new KeyEventInfo("F1", "", modifiers);
    if (equals(character, "F2"))
        return new KeyEventInfo("F2", "", modifiers);
    if (equals(character, "F3"))
        return new KeyEventInfo("F3", "", modifiers);
    if (equals(character, "F4"))
        return new KeyEventInfo("F4", "", modifiers);
    if (equals(character, "F5"))
        return new KeyEventInfo("F5", "", modifiers);
    if (equals(character, "F6"))
        return new KeyEventInfo("F6", "", modifiers);
    if (equals(character, "F7"))
        return new KeyEventInfo("F7", "", modifiers);
    if (equals(character, "F8"))
        return new KeyEventInfo("F8", "", modifiers);
    if (equals(character, "F9"))
        return new KeyEventInfo("F9", "", modifiers);
    if (equals(character, "F10"))
        return new KeyEventInfo("F10", "", modifiers);
    if (equals(character, "F11"))
        return new KeyEventInfo("F11", "", modifiers);
    if (equals(character, "F12"))
        return new KeyEventInfo("F12", "", modifiers);

    int charCode = JSStringGetCharactersPtr(character)[0];
    if (charCode == '\n' || charCode == '\r')
        return new KeyEventInfo("Return", "Return", modifiers);
    if (charCode == '\t')
        return new KeyEventInfo("Tab", "Tab", modifiers);
    if (charCode == '\x8')
        return new KeyEventInfo("BackSpace", "BackSpace", modifiers);
    if (charCode == ' ')
        return new KeyEventInfo("space", " ", modifiers);

    return new KeyEventInfo(character->ustring().utf8(), character->ustring().utf8(), modifiers);
}

static KeyEventInfo* createKeyEventInfo(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (!ewk_frame_view_get(browser->mainFrame()))
        return 0;

    if (argumentCount < 1)
        return 0;

    // handle location argument.
    int location = DomKeyLocationStandard;
    if (argumentCount > 2)
        location = static_cast<int>(JSValueToNumber(context, arguments[2], exception));

    JSRetainPtr<JSStringRef> character(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    if (exception && *exception)
        return 0;

    EvasKeyModifier modifiers = EvasKeyModifierNone;
    if (argumentCount >= 2)
        modifiers = modifiersFromJSValue(context, arguments[1]);

    return (location == DomKeyLocationNumpad) ? keyPadNameFromJSValue(character.get(), modifiers) : keyNameFromJSValue(character.get(), modifiers);
}

static void sendKeyDown(Evas* evas, KeyEventInfo* keyEventInfo)
{
    if (!keyEventInfo)
        return;

    const char* keyName = keyEventInfo->keyName.data();
    const char* keyString = keyEventInfo->keyString.data();
    EvasKeyModifier modifiers = keyEventInfo->modifiers;

    DumpRenderTreeSupportEfl::layoutFrame(browser->mainFrame());

    ASSERT(evas);
    setEvasModifiers(evas, modifiers);
    evas_event_feed_key_down(evas, keyName, keyName, keyString, 0, 0, 0);
    evas_event_feed_key_up(evas, keyName, keyName, keyString, 0, 1, 0);
    setEvasModifiers(evas, EvasKeyModifierNone);

    DumpRenderTreeSupportEfl::deliverAllMutationsIfNecessary();
}

static JSValueRef keyDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    OwnPtr<KeyEventInfo> keyEventInfo = adoptPtr(createKeyEventInfo(context, argumentCount, arguments, exception));
    sendKeyDown(evas_object_evas_get(browser->mainFrame()), keyEventInfo.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef scalePageByCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 3)
        return JSValueMakeUndefined(context);

    Evas_Object* view = ewk_frame_view_get(browser->mainFrame());
    if (!view)
        return JSValueMakeUndefined(context);

    float scaleFactor = JSValueToNumber(context, arguments[0], exception);
    float x = JSValueToNumber(context, arguments[1], exception);
    float y = JSValueToNumber(context, arguments[2], exception);
    ewk_view_scale_set(view, scaleFactor, x, y);

    return JSValueMakeUndefined(context);
}

static void textZoom(ZoomEvent zoomEvent)
{
    Evas_Object* view = ewk_frame_view_get(browser->mainFrame());
    if (!view)
        return;

    float zoomFactor = ewk_view_text_zoom_get(view);
    if (zoomEvent == ZoomIn)
        zoomFactor *= zoomMultiplierRatio;
    else
        zoomFactor /= zoomMultiplierRatio;

    ewk_view_text_zoom_set(view, zoomFactor);
}

static void pageZoom(ZoomEvent zoomEvent)
{
    Evas_Object* view = ewk_frame_view_get(browser->mainFrame());
    if (!view)
        return;

    float zoomFactor = ewk_view_page_zoom_get(view);
    if (zoomEvent == ZoomIn)
        zoomFactor *= zoomMultiplierRatio;
    else
        zoomFactor /= zoomMultiplierRatio;

    ewk_view_page_zoom_set(view, zoomFactor);
}

static JSValueRef textZoomInCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    textZoom(ZoomIn);
    return JSValueMakeUndefined(context);
}

static JSValueRef textZoomOutCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    textZoom(ZoomOut);
    return JSValueMakeUndefined(context);
}

static JSValueRef zoomPageInCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    pageZoom(ZoomIn);
    return JSValueMakeUndefined(context);
}

static JSValueRef zoomPageOutCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    pageZoom(ZoomOut);
    return JSValueMakeUndefined(context);
}

static Eina_Bool sendAsynchronousKeyDown(void* userData)
{
    OwnPtr<KeyEventInfo> keyEventInfo = adoptPtr(static_cast<KeyEventInfo*>(userData));
    sendKeyDown(evas_object_evas_get(browser->mainFrame()), keyEventInfo.get());
    return ECORE_CALLBACK_CANCEL;
}

static JSValueRef scheduleAsynchronousKeyDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    KeyEventInfo* keyEventInfo = createKeyEventInfo(context, argumentCount, arguments, exception);
    ecore_idler_add(sendAsynchronousKeyDown, static_cast<void*>(keyEventInfo));
    return JSValueMakeUndefined(context);
}

static JSStaticFunction staticFunctions[] = {
    { "mouseScrollBy", mouseScrollByCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "continuousMouseScrollBy", continuousMouseScrollByCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseDown", mouseDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseUp", mouseUpCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseMoveTo", mouseMoveToCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "leapForward", leapForwardCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "keyDown", keyDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "scheduleAsynchronousClick", scheduleAsynchronousClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "scheduleAsynchronousKeyDown", scheduleAsynchronousKeyDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "scalePageBy", scalePageByCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "textZoomIn", textZoomInCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "textZoomOut", textZoomOutCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "zoomPageIn", zoomPageInCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "zoomPageOut", zoomPageOutCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSStaticValue staticValues[] = {
    { 0, 0, 0, 0 }
};

static JSClassRef getClass(JSContextRef context)
{
    static JSClassRef eventSenderClass = 0;

    if (!eventSenderClass) {
        JSClassDefinition classDefinition = {
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        classDefinition.staticFunctions = staticFunctions;
        classDefinition.staticValues = staticValues;

        eventSenderClass = JSClassCreate(&classDefinition);
    }

    return eventSenderClass;
}

JSObjectRef makeEventSender(JSContextRef context, bool isTopFrame)
{
    if (isTopFrame) {
        gDragMode = true;

        // Fly forward in time one second when the main frame loads. This will
        // ensure that when a test begins clicking in the same location as
        // a previous test, those clicks won't be interpreted as continuations
        // of the previous test's click sequences.
        gTimeOffset += 1000;

        gLastMousePositionX = gLastMousePositionY = 0;
        gLastClickPositionX = gLastClickPositionY = 0;
        gLastClickTimeOffset = 0;
        gLastClickButton = 0;
        gButtonCurrentlyDown = 0;
        gClickCount = 0;
    }

    return JSObjectMake(context, getClass(context), 0);
}

static void feedOrQueueMouseEvent(MouseEventInfo* eventInfo, EventQueueStrategy strategy)
{
    if (!delayedEventQueue().isEmpty()) {
        if (delayedEventQueue().last().eventInfo)
            delayedEventQueue().append(DelayedEvent(eventInfo));
        else
            delayedEventQueue().last().eventInfo = eventInfo;

        if (strategy == FeedQueuedEvents)
            feedQueuedMouseEvents();
    } else
        feedMouseEvent(eventInfo);
}

static void feedMouseEvent(MouseEventInfo* eventInfo)
{
    if (!eventInfo)
        return;

    unsigned timeStamp = 0;
    Evas_Object* mainFrame = browser->mainFrame();
    Evas* evas = evas_object_evas_get(mainFrame);
    DumpRenderTreeSupportEfl::layoutFrame(mainFrame);
    EvasMouseEvent event = eventInfo->event;

    setEvasModifiers(evas, eventInfo->modifiers);

    Evas_Button_Flags flags = EVAS_BUTTON_NONE;

    // FIXME: We need to pass additional information with our events, so that
    // we could construct correct PlatformWheelEvent. At the moment, max number
    // of clicks is 3
    if (gClickCount == 3)
        flags = EVAS_BUTTON_TRIPLE_CLICK;
    else if (gClickCount == 2)
        flags = EVAS_BUTTON_DOUBLE_CLICK;

    if (event & EvasMouseEventMove)
        evas_event_feed_mouse_move(evas, gLastMousePositionX, gLastMousePositionY, timeStamp++, 0);
    if (event & EvasMouseEventDown)
        evas_event_feed_mouse_down(evas, eventInfo->button, flags, timeStamp++, 0);
    if (event & EvasMouseEventUp)
        evas_event_feed_mouse_up(evas, eventInfo->button, flags, timeStamp++, 0);
    if (event & EvasMouseEventScrollLeft | event & EvasMouseEventScrollRight)
        evas_event_feed_mouse_wheel(evas, 1, eventInfo->horizontalDelta, timeStamp, 0);
    if (event & EvasMouseEventScrollUp | event & EvasMouseEventScrollDown)
        evas_event_feed_mouse_wheel(evas, 0, eventInfo->verticalDelta, timeStamp, 0);

    setEvasModifiers(evas, EvasKeyModifierNone);

    delete eventInfo;
}

static void feedQueuedMouseEvents()
{
    WTF::Vector<DelayedEvent>::const_iterator it = delayedEventQueue().begin();
    for (; it != delayedEventQueue().end(); it++) {
        DelayedEvent delayedEvent = *it;
        if (delayedEvent.delay)
            usleep(delayedEvent.delay * 1000);
        feedMouseEvent(delayedEvent.eventInfo);
    }
    delayedEventQueue().clear();
}
