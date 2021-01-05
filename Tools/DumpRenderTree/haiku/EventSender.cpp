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
#include "IntPoint.h"
#include "JSStringUtils.h"
#include "NotImplemented.h"
#include "PlatformEvent.h"

#include <WebView.h>
#include <DumpRenderTreeClient.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebPage.h>
#include <WebView.h>
#include <wtf/ASCIICType.h>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>

#include <InterfaceDefs.h>
#include <View.h>
#include <Window.h>

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

extern BWebView* webView;

// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DomKeyLocationStandard,
    DomKeyLocationLeft,
    DomKeyLocationRight,
    DomKeyLocationNumpad
};

enum EventQueueStrategy {
    FeedQueuedEvents,
    DoNotFeedQueuedEvents
};

static unsigned touchModifiers;

WTF::Vector<BMessage*>& delayedEventQueue()
{
    static NeverDestroyed<WTF::Vector<BMessage*>> staticDelayedEventQueue;
    return staticDelayedEventQueue;
}


static void feedOrQueueMouseEvent(BMessage*, EventQueueStrategy);
static void feedQueuedMouseEvents();

static int32 translateMouseButtonNumber(int eventSenderButtonNumber)
{
    static const int32 translationTable[] = {
        B_PRIMARY_MOUSE_BUTTON,
        B_TERTIARY_MOUSE_BUTTON,
        B_SECONDARY_MOUSE_BUTTON,
        B_TERTIARY_MOUSE_BUTTON // fast/events/mouse-click-events expects the 4th button to be treated as the middle button
    };
    static const unsigned translationTableSize = sizeof(translationTable) / sizeof(translationTable[0]);

    if (eventSenderButtonNumber < translationTableSize)
        return translationTable[eventSenderButtonNumber];

    return 0;
}

static JSValueRef scheduleAsynchronousClickCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    notImplemented();
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

static int32 modifierFromJSValue(JSContextRef context, const JSValueRef value)
{
    JSRetainPtr<JSStringRef> jsKeyValue(Adopt, JSValueToStringCopy(context, value, 0));

    if (equals(jsKeyValue, "ctrlKey") || equals(jsKeyValue, "addSelectionKey"))
        return B_CONTROL_KEY;
    if (equals(jsKeyValue, "shiftKey") || equals(jsKeyValue, "rangeSelectionKey"))
        return B_SHIFT_KEY;
    if (equals(jsKeyValue, "altKey"))
        return B_COMMAND_KEY;
    if (equals(jsKeyValue, "metaKey"))
        return B_OPTION_KEY;

    return 0;
}

static unsigned modifiersFromJSValue(JSContextRef context, const JSValueRef modifiers)
{
    // The value may either be a string with a single modifier or an array of modifiers.
    if (JSValueIsString(context, modifiers))
        return modifierFromJSValue(context, modifiers);

    JSObjectRef modifiersArray = JSValueToObject(context, modifiers, 0);
    if (!modifiersArray)
        return 0;

    unsigned modifier = 0;
    JSRetainPtr<JSStringRef> lengthProperty(Adopt, JSStringCreateWithUTF8CString("length"));
    int modifiersCount = JSValueToNumber(context, JSObjectGetProperty(context, modifiersArray, lengthProperty.get(), 0), 0);
    for (int i = 0; i < modifiersCount; ++i)
        modifier |= modifierFromJSValue(context, JSObjectGetPropertyAtIndex(context, modifiersArray, i, 0));
    return modifier;
}

static JSValueRef getMenuItemTitleCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    notImplemented();
    return JSValueMakeString(context, JSStringCreateWithUTF8CString(""));
}

static bool setMenuItemTitleCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    return true;
}

static JSValueRef menuItemClickCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    notImplemented();
    return JSValueMakeUndefined(context);
}

static JSStaticFunction staticMenuItemFunctions[] = {
    { "click", menuItemClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSStaticValue staticMenuItemValues[] = {
    { "title", getMenuItemTitleCallback, setMenuItemTitleCallback, kJSPropertyAttributeNone },
    { 0, 0, 0, 0 }
};

static JSClassRef getMenuItemClass()
{
    static JSClassRef menuItemClass = 0;

    if (!menuItemClass) {
        JSClassDefinition classDefinition = {
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        classDefinition.staticFunctions = staticMenuItemFunctions;
        classDefinition.staticValues = staticMenuItemValues;

        menuItemClass = JSClassCreate(&classDefinition);
    }

    return menuItemClass;
}

static JSValueRef contextClickCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Should invoke a context menu, and return its contents
    notImplemented();
    return JSValueMakeUndefined(context);
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

    unsigned modifiers = argumentCount >= 2 ? modifiersFromJSValue(context, arguments[1]) : 0;
    BMessage* eventInfo = new BMessage(B_MOUSE_DOWN);
    eventInfo->AddInt32("modifiers", modifiers);
    eventInfo->AddInt32("buttons", button);
    eventInfo->AddInt32("clicks", gClickCount);
    eventInfo->AddPoint("where", BPoint(gLastMousePositionX, gLastMousePositionY));
    eventInfo->AddPoint("be:view_where", BPoint(gLastMousePositionX, gLastMousePositionY));
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

    button = translateMouseButtonNumber(button);

    gLastClickPositionX = gLastMousePositionX;
    gLastClickPositionY = gLastMousePositionY;
    gLastClickButton = gButtonCurrentlyDown;
    gLastClickTimeOffset = gTimeOffset;
    gButtonCurrentlyDown = 0;

    unsigned modifiers = argumentCount >= 2 ? modifiersFromJSValue(context, arguments[1]) : 0;
    BMessage* eventInfo = new BMessage(B_MOUSE_UP);
    eventInfo->AddInt32("modifiers", modifiers);
    eventInfo->AddInt32("previous buttons", button);
    eventInfo->AddPoint("where", BPoint(gLastMousePositionX, gLastMousePositionY));
    eventInfo->AddPoint("be:view_where", BPoint(gLastMousePositionX, gLastMousePositionY));
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

    BMessage* eventInfo = new BMessage(B_MOUSE_MOVED);
    eventInfo->AddPoint("where", BPoint(gLastMousePositionX, gLastMousePositionY));
    eventInfo->AddPoint("be:view_where", BPoint(gLastMousePositionX, gLastMousePositionY));
    feedOrQueueMouseEvent(eventInfo, DoNotFeedQueuedEvents);
    eventInfo->AddInt32("buttons", gButtonCurrentlyDown);
    return JSValueMakeUndefined(context);
}

static JSValueRef leapForwardCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount > 0) {
        const unsigned long leapForwardDelay = JSValueToNumber(context, arguments[0], exception);
        if (delayedEventQueue().isEmpty())
            delayedEventQueue().append(new BMessage((uint32)0));
        delayedEventQueue().last()->AddInt32("delay", leapForwardDelay);
        gTimeOffset += leapForwardDelay;
    }

    return JSValueMakeUndefined(context);
}

static BMessage* evasMouseEventFromHorizontalAndVerticalOffsets(int horizontalOffset, int verticalOffset)
{
    BMessage* message = new BMessage(B_MOUSE_WHEEL_CHANGED);
    message->AddFloat("be:wheel_delta_x", horizontalOffset);
    message->AddFloat("be:wheel_delta_y", verticalOffset);
    return message;
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

    BMessage* eventInfo = evasMouseEventFromHorizontalAndVerticalOffsets(horizontal, vertical);
    feedOrQueueMouseEvent(eventInfo, FeedQueuedEvents);
    return JSValueMakeUndefined(context);
}

static JSValueRef continuousMouseScrollByCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeUndefined(context);
}

struct mapping {
    const char* name;
    uint32_t code;
    char byte;
};

static BMessage* keyPadNameFromJSValue(JSStringRef character, unsigned modifiers)
{
    BMessage* message = new BMessage(B_KEY_DOWN);
    message->AddInt32("modifiers", modifiers);

    static const mapping keys [] = {
        {"leftArrow",  0x48, B_LEFT_ARROW},
        {"rightArrow", 0x4a, B_RIGHT_ARROW},
        {"upArrow",    0x38, B_UP_ARROW},
        {"downArrow",  0x59, B_DOWN_ARROW},
        {"pageUp",     0x39, B_PAGE_UP},
        {"pageDown",   0x5a, B_PAGE_DOWN},
        {"home",       0x37, B_HOME},
        {"end",        0x58, B_END},
        {"insert",     0x64, B_INSERT},
        {"delete",     0x65, B_DELETE},
    };

    bool special = false;
    for(int i = 0; i < sizeof(keys)/sizeof(mapping); i++)
    {
        if (equals(character, keys[i].name)) {
            message->AddInt32("key", keys[i].code);
            message->AddData("bytes", B_STRING_TYPE, &keys[i].byte, 1);
            special = true;
        }
    }

    if(!special) {
        BString bytes(character->string());
        message->AddString("bytes", bytes);
        if ((character->length() == 1) && (bytes[0] >= 'A' && bytes[0] <= 'Z'))
            modifiers |= B_SHIFT_KEY;
    }
    return message;
}

static BMessage* keyNameFromJSValue(JSStringRef character, unsigned modifiers)
{
    BMessage* message = new BMessage(B_KEY_DOWN);

    message->AddInt32("modifiers", modifiers);

    static const mapping keys [] = {
        {"leftArrow",  0x61, B_LEFT_ARROW},
        {"rightArrow", 0x63, B_RIGHT_ARROW},
        {"upArrow",    0x57, B_UP_ARROW},
        {"downArrow",  0x62, B_DOWN_ARROW},
        {"pageUp",     0x21, B_PAGE_UP},
        {"pageDown",   0x36, B_PAGE_DOWN},
        {"home",       0x20, B_HOME},
        {"end",        0x35, B_END},
        {"insert",     0x1f, B_INSERT},
        {"delete",     0x34, B_DELETE},

        {"printScreen",B_PRINT_KEY, B_FUNCTION_KEY},
        {"F1",         B_F1_KEY, B_FUNCTION_KEY},
        {"F2",         B_F2_KEY, B_FUNCTION_KEY},
        {"F3",         B_F3_KEY, B_FUNCTION_KEY},
        {"F4",         B_F4_KEY, B_FUNCTION_KEY},
        {"F5",         B_F5_KEY, B_FUNCTION_KEY},
        {"F6",         B_F6_KEY, B_FUNCTION_KEY},
        {"F7",         B_F7_KEY, B_FUNCTION_KEY},
        {"F8",         B_F8_KEY, B_FUNCTION_KEY},
        {"F9",         B_F9_KEY, B_FUNCTION_KEY},
        {"F10",        B_F10_KEY, B_FUNCTION_KEY},
        {"F11",        B_F11_KEY, B_FUNCTION_KEY},
        {"F12",        B_F12_KEY, B_FUNCTION_KEY}
    };

    bool special = false;
    for(int i = 0; i < sizeof(keys)/sizeof(mapping); i++)
    {
        if (equals(character, keys[i].name)) {
            message->AddInt32("key", keys[i].code);
            message->AddData("bytes", B_STRING_TYPE, &keys[i].byte, 1);
            special = true;
        }
    }

#if 0
    // Modifiers
    if (equals(character, "menu"))
        return new KeyEventInfo("Menu", modifiers);
    if (equals(character, "leftControl"))
        return new KeyEventInfo("Control_L", modifiers);
    if (equals(character, "rightControl"))
        return new KeyEventInfo("Control_R", modifiers);
    if (equals(character, "leftShift"))
        return new KeyEventInfo("Shift_L", modifiers);
    if (equals(character, "rightShift"))
        return new KeyEventInfo("Shift_R", modifiers);
    if (equals(character, "leftAlt"))
        return new KeyEventInfo("Alt_L", modifiers);
    if (equals(character, "rightAlt"))
        return new KeyEventInfo("Alt_R", modifiers);
#endif

    if(!special) {
        BString bytes(character->string());
        message->AddString("bytes", bytes);
        if ((character->length() == 1) && (bytes[0] >= 'A' && bytes[0] <= 'Z'))
            modifiers |= B_SHIFT_KEY;
    }

    return message;
}

static BMessage* createKeyEventInfo(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return 0;

    // handle location argument.
    int location = DomKeyLocationStandard;
    if (argumentCount > 2)
        location = static_cast<int>(JSValueToNumber(context, arguments[2], exception));

    JSRetainPtr<JSStringRef> character(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    if (exception && *exception)
        return 0;

    unsigned modifiers = 0;
    if (argumentCount >= 2)
        modifiers = modifiersFromJSValue(context, arguments[1]);

    return (location == DomKeyLocationNumpad) ? keyPadNameFromJSValue(character.get(), modifiers) : keyNameFromJSValue(character.get(), modifiers);
}

static JSValueRef keyDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    BMessage* event = createKeyEventInfo(context, argumentCount, arguments, exception);
    WebCore::DumpRenderTreeClient::injectKeyEvent(webView->WebPage(), event);
    return JSValueMakeUndefined(context);
}

static JSValueRef scalePageByCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    notImplemented();
    return JSValueMakeUndefined(context);
}

static JSValueRef textZoomInCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    webView->IncreaseZoomFactor(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef textZoomOutCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    webView->DecreaseZoomFactor(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef zoomPageInCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    webView->IncreaseZoomFactor(false);
    return JSValueMakeUndefined(context);
}

static JSValueRef zoomPageOutCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    webView->DecreaseZoomFactor(false);
    return JSValueMakeUndefined(context);
}

static JSValueRef scheduleAsynchronousKeyDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    notImplemented();
    return JSValueMakeUndefined(context);
}

static JSStaticFunction staticFunctions[] = {
    { "mouseDown", mouseDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseUp", mouseUpCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseMoveTo", mouseMoveToCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseScrollBy", mouseScrollByCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "leapForward", leapForwardCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "keyDown", keyDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#if 0
    { "contextClick", contextClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "continuousMouseScrollBy", continuousMouseScrollByCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "beginDragWithFiles", beginDragWithFilesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "scheduleAsynchronousClick", scheduleAsynchronousClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "scheduleAsynchronousKeyDown", scheduleAsynchronousKeyDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "scalePageBy", scalePageByCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#endif
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

JSObjectRef makeEventSender(JSContextRef context)
{
    return JSObjectMake(context, getClass(context), 0);
}

static void feedOrQueueMouseEvent(BMessage* eventInfo, EventQueueStrategy strategy)
{
    if (!delayedEventQueue().isEmpty()) {
        if (delayedEventQueue().last() != NULL)
            delayedEventQueue().append(eventInfo);
        else
            delayedEventQueue().last() = eventInfo;

        if (strategy == FeedQueuedEvents)
            feedQueuedMouseEvents();
    } else
        WebCore::DumpRenderTreeClient::injectMouseEvent(webView->WebPage(), eventInfo);
}

namespace WebCore {

void DumpRenderTreeClient::injectMouseEvent(BWebPage* target, BMessage* event)
{
    // We are short-circuiting the normal message delivery path, because tests
    // expect this to be synchronous (the event must be processed when the
    // method returns)
    target->handleMouseEvent(event);
    delete event;
}

void DumpRenderTreeClient::injectKeyEvent(BWebPage* target, BMessage* event)
{
    // We are short-circuiting the normal message delivery path, because tests
    // expect this to be synchronous (the event must be processed when the
    // method returns)
    target->handleKeyEvent(event);
    event->what = B_KEY_UP;
    target->handleKeyEvent(event);
    delete event;
}
}

static void feedQueuedMouseEvents()
{
    WTF::Vector<BMessage*>::const_iterator it = delayedEventQueue().begin();
    for (; it != delayedEventQueue().end(); it++) {
        BMessage* delayedEvent = *it;
        int32 delay = delayedEvent->FindInt32("delay");
        if (delay)
            usleep(delay * 1000);
        WebCore::DumpRenderTreeClient::injectMouseEvent(webView->WebPage(), delayedEvent);
    }
    delayedEventQueue().clear();
}
