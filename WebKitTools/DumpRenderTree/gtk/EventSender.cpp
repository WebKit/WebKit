/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Zan Dobersek <zandobersek@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
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

#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <webkit/webkitwebframe.h>
#include <webkit/webkitwebview.h>
#include <wtf/ASCIICType.h>
#include <wtf/Platform.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

// TODO: Currently drag and drop related code is left out and
// should be merged once we have drag and drop support in WebCore.

extern "C" {
    extern void webkit_web_frame_layout(WebKitWebFrame* frame);
}

static bool down = false;
static bool currentEventButton = 1;
static bool dragMode = true;
static bool replayingSavedEvents = false;
static int lastMousePositionX;
static int lastMousePositionY;

static int lastClickPositionX;
static int lastClickPositionY;
static int clickCount = 0;

struct DelayedMessage {
    GdkEvent event;
    gulong delay;
    gboolean isDragEvent;
};

static DelayedMessage msgQueue[1024];

static unsigned endOfQueue;
static unsigned startOfQueue;

static const float zoomMultiplierRatio = 1.2f;

static JSValueRef getDragModeCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeBoolean(context, dragMode);
}

static bool setDragModeCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    dragMode = JSValueToBoolean(context, value);
    return true;
}

static JSValueRef leapForwardCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // FIXME: Add proper support for forward leaps
    return JSValueMakeUndefined(context);
}

static JSValueRef contextClickCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    webkit_web_frame_layout(mainFrame);

    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    GdkEvent event;
    memset(&event, 0, sizeof(event));
    event.button.button = 3;
    event.button.x = lastMousePositionX;
    event.button.y = lastMousePositionY;
    event.button.window = GTK_WIDGET(view)->window;

    gboolean return_val;
    down = true;
    event.type = GDK_BUTTON_PRESS;
    g_signal_emit_by_name(view, "button_press_event", &event, &return_val);

    down = false;
    event.type = GDK_BUTTON_RELEASE;
    g_signal_emit_by_name(view, "button_release_event", &event, &return_val);

    return JSValueMakeUndefined(context);
}

static void updateClickCount(int /* button */)
{
    // FIXME: take the last clicked button number and the time of last click into account.
    if (lastClickPositionX != lastMousePositionX && lastClickPositionY != lastMousePositionY)
        clickCount = 1;
    else
        clickCount++;
}

static JSValueRef mouseDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    down = true;

    GdkEvent event;
    memset(&event, 0, sizeof(event));
    event.type = GDK_BUTTON_PRESS;
    event.button.button = 1;

    if (argumentCount == 1) {
        event.button.button = (int)JSValueToNumber(context, arguments[0], exception) + 1;
        g_return_val_if_fail((!exception || !*exception), JSValueMakeUndefined(context));
    }

    currentEventButton = event.button.button;

    event.button.x = lastMousePositionX;
    event.button.y = lastMousePositionY;
    event.button.window = GTK_WIDGET(view)->window;
    event.button.time = GDK_CURRENT_TIME;
    event.button.device = gdk_device_get_core_pointer();

    int x_root, y_root;
    gdk_window_get_root_coords(GTK_WIDGET(view)->window, lastMousePositionX, lastMousePositionY, &x_root, &y_root);

    event.button.x_root = x_root;
    event.button.y_root = y_root;

    updateClickCount(1);

    if (!msgQueue[endOfQueue].delay) {
        webkit_web_frame_layout(mainFrame);

        gboolean return_val;
        g_signal_emit_by_name(view, "button_press_event", &event, &return_val);
        if (clickCount == 2) {
            event.type = GDK_2BUTTON_PRESS;
            g_signal_emit_by_name(view, "button_press_event", &event, &return_val);
        }
    } else {
        // replaySavedEvents should have the required logic to make leapForward delays work
        msgQueue[endOfQueue++].event = event;
        replaySavedEvents();
    }

    return JSValueMakeUndefined(context);
}

static JSValueRef mouseUpCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{

    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    down = false;

    GdkEvent event;
    memset(&event, 0, sizeof(event));
    event.type = GDK_BUTTON_RELEASE;
    event.button.button = 1;

    if (argumentCount == 1) {
        event.button.button = (int)JSValueToNumber(context, arguments[0], exception) + 1;
        g_return_val_if_fail((!exception || !*exception), JSValueMakeUndefined(context));
    }

    currentEventButton = event.button.button;

    event.button.x = lastMousePositionX;
    event.button.y = lastMousePositionY;
    event.button.window = GTK_WIDGET(view)->window;
    event.button.time = GDK_CURRENT_TIME;
    event.button.device = gdk_device_get_core_pointer();

    int x_root, y_root;
    gdk_window_get_root_coords(GTK_WIDGET(view)->window, lastMousePositionX, lastMousePositionY, &x_root, &y_root);

    event.button.x_root = x_root;
    event.button.y_root = y_root;

    if ((dragMode && !replayingSavedEvents) || msgQueue[endOfQueue].delay) {
        msgQueue[endOfQueue].event = event;
        msgQueue[endOfQueue++].isDragEvent = true;
        replaySavedEvents();
    } else {
        webkit_web_frame_layout(mainFrame);

        gboolean return_val;
        g_signal_emit_by_name(view, "button_release_event", &event, &return_val);
    }

    lastClickPositionX = lastMousePositionX;
    lastClickPositionY = lastMousePositionY;

    return JSValueMakeUndefined(context);
}

static JSValueRef mouseMoveToCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    lastMousePositionX = (int)JSValueToNumber(context, arguments[0], exception);
    g_return_val_if_fail((!exception || !*exception), JSValueMakeUndefined(context));
    lastMousePositionY = (int)JSValueToNumber(context, arguments[1], exception);
    g_return_val_if_fail((!exception || !*exception), JSValueMakeUndefined(context));

    GdkEvent event;
    memset(&event, 0, sizeof(event));
    event.type = GDK_MOTION_NOTIFY;
    event.motion.x = lastMousePositionX;
    event.motion.y = lastMousePositionY;
    event.motion.time = GDK_CURRENT_TIME;
    event.motion.window = GTK_WIDGET(view)->window;
    event.motion.device = gdk_device_get_core_pointer();

    int x_root, y_root;
    gdk_window_get_root_coords(GTK_WIDGET(view)->window, lastMousePositionX, lastMousePositionY, &x_root, &y_root);

    event.motion.x_root = x_root;
    event.motion.y_root = y_root;

    if (down) {
        if (currentEventButton == 1)
            event.motion.state = GDK_BUTTON1_MASK;
        else if (currentEventButton == 2)
            event.motion.state = GDK_BUTTON2_MASK;
        else if (currentEventButton == 3)
            event.motion.state = GDK_BUTTON3_MASK;
    } else
        event.motion.state = 0;

    if (dragMode && down && !replayingSavedEvents) {
        msgQueue[endOfQueue].event = event;
        msgQueue[endOfQueue++].isDragEvent = true;
    } else {
        webkit_web_frame_layout(mainFrame);

        gboolean return_val;
        g_signal_emit_by_name(view, "motion_notify_event", &event, &return_val);
    }

    return JSValueMakeUndefined(context);
}

static JSValueRef mouseWheelToCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    int horizontal = (int)JSValueToNumber(context, arguments[0], exception);
    g_return_val_if_fail((!exception || !*exception), JSValueMakeUndefined(context));
    int vertical = (int)JSValueToNumber(context, arguments[1], exception);
    g_return_val_if_fail((!exception || !*exception), JSValueMakeUndefined(context));

    // GTK+ doesn't support multiple direction scrolls in the same event!
    g_return_val_if_fail((!vertical || !horizontal), JSValueMakeUndefined(context));

    GdkEvent event;
    event.type = GDK_SCROLL;
    event.scroll.x = lastMousePositionX;
    event.scroll.y = lastMousePositionY;
    event.scroll.time = GDK_CURRENT_TIME;
    event.scroll.window = GTK_WIDGET(view)->window;

    if (horizontal < 0)
        event.scroll.direction = GDK_SCROLL_LEFT;
    else if (horizontal > 0)
        event.scroll.direction = GDK_SCROLL_RIGHT;
    else if (vertical < 0)
        event.scroll.direction = GDK_SCROLL_UP;
    else if (vertical > 0)
        event.scroll.direction = GDK_SCROLL_DOWN;
    else
        g_assert_not_reached();

    if (dragMode && down && !replayingSavedEvents) {
        msgQueue[endOfQueue].event = event;
        msgQueue[endOfQueue++].isDragEvent = true;
    } else {
        webkit_web_frame_layout(mainFrame);
        gtk_main_do_event(&event);
    }

    return JSValueMakeUndefined(context);
}

static JSValueRef beginDragWithFilesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    // FIXME: Implement this completely once WebCore has complete drag and drop support
    return JSValueMakeUndefined(context);
}

void replaySavedEvents()
{
    // FIXME: This doesn't deal with forward leaps, but it should.

    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return;

    replayingSavedEvents = true;

    for (unsigned queuePos = 0; queuePos < endOfQueue; queuePos++) {
        GdkEvent event = msgQueue[queuePos].event;
        gboolean return_val;

        switch (event.type) {
        case GDK_BUTTON_RELEASE:
            g_signal_emit_by_name(view, "button_release_event", &event, &return_val);
            break;
        case GDK_BUTTON_PRESS:
            g_signal_emit_by_name(view, "button_press_event", &event, &return_val);
            break;
        case GDK_MOTION_NOTIFY:
            g_signal_emit_by_name(view, "motion_notify_event", &event, &return_val);
            break;
        default:
            continue;
        }

        startOfQueue++;
    }

    int numQueuedMessages = endOfQueue - startOfQueue;
    if (!numQueuedMessages) {
        startOfQueue = 0;
        endOfQueue = 0;
        replayingSavedEvents = false;
        return;
    }

    startOfQueue = 0;
    endOfQueue = 0;

    replayingSavedEvents = false;
}

static JSValueRef keyDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    static const JSStringRef lengthProperty = JSStringCreateWithUTF8CString("length");

    webkit_web_frame_layout(mainFrame);

    // handle modifier keys.
    int state = 0;
    if (argumentCount > 1) {
        JSObjectRef modifiersArray = JSValueToObject(context, arguments[1], exception);
        if (modifiersArray) {
            for (int i = 0; i < JSValueToNumber(context, JSObjectGetProperty(context, modifiersArray, lengthProperty, 0), 0); ++i) {
                JSValueRef value = JSObjectGetPropertyAtIndex(context, modifiersArray, i, 0);
                JSStringRef string = JSValueToStringCopy(context, value, 0);
                if (JSStringIsEqualToUTF8CString(string, "ctrlKey"))
                    state |= GDK_CONTROL_MASK;
                else if (JSStringIsEqualToUTF8CString(string, "shiftKey"))
                    state |= GDK_SHIFT_MASK;
                else if (JSStringIsEqualToUTF8CString(string, "altKey"))
                    state |= GDK_MOD1_MASK;

                JSStringRelease(string);
            }
        }
    }

    JSStringRef character = JSValueToStringCopy(context, arguments[0], exception);
    g_return_val_if_fail((!exception || !*exception), JSValueMakeUndefined(context));
    int gdkKeySym;
    if (JSStringIsEqualToUTF8CString(character, "leftArrow"))
        gdkKeySym = GDK_Left;
    else if (JSStringIsEqualToUTF8CString(character, "rightArrow"))
        gdkKeySym = GDK_Right;
    else if (JSStringIsEqualToUTF8CString(character, "upArrow"))
        gdkKeySym = GDK_Up;
    else if (JSStringIsEqualToUTF8CString(character, "downArrow"))
        gdkKeySym = GDK_Down;
    else if (JSStringIsEqualToUTF8CString(character, "pageUp"))
        gdkKeySym = GDK_Page_Up;
    else if (JSStringIsEqualToUTF8CString(character, "pageDown"))
        gdkKeySym = GDK_Page_Down;
    else if (JSStringIsEqualToUTF8CString(character, "home"))
        gdkKeySym = GDK_Home;
    else if (JSStringIsEqualToUTF8CString(character, "end"))
        gdkKeySym = GDK_End;
    else if (JSStringIsEqualToUTF8CString(character, "delete"))
        gdkKeySym = GDK_BackSpace;
    else if (JSStringIsEqualToUTF8CString(character, "F1"))
        gdkKeySym = GDK_F1;
    else if (JSStringIsEqualToUTF8CString(character, "F2"))
        gdkKeySym = GDK_F2;
    else if (JSStringIsEqualToUTF8CString(character, "F3"))
        gdkKeySym = GDK_F3;
    else if (JSStringIsEqualToUTF8CString(character, "F4"))
        gdkKeySym = GDK_F4;
    else if (JSStringIsEqualToUTF8CString(character, "F5"))
        gdkKeySym = GDK_F5;
    else if (JSStringIsEqualToUTF8CString(character, "F6"))
        gdkKeySym = GDK_F6;
    else if (JSStringIsEqualToUTF8CString(character, "F7"))
        gdkKeySym = GDK_F7;
    else if (JSStringIsEqualToUTF8CString(character, "F8"))
        gdkKeySym = GDK_F8;
    else if (JSStringIsEqualToUTF8CString(character, "F9"))
        gdkKeySym = GDK_F9;
    else if (JSStringIsEqualToUTF8CString(character, "F10"))
        gdkKeySym = GDK_F10;
    else if (JSStringIsEqualToUTF8CString(character, "F11"))
        gdkKeySym = GDK_F11;
    else if (JSStringIsEqualToUTF8CString(character, "F12"))
        gdkKeySym = GDK_F12;
    else {
        int charCode = JSStringGetCharactersPtr(character)[0];
        if (charCode == '\n' || charCode == '\r')
            gdkKeySym = GDK_Return;
        else if (charCode == '\t')
            gdkKeySym = GDK_Tab;
        else if (charCode == '\x8')
            gdkKeySym = GDK_BackSpace;
        else {
            gdkKeySym = gdk_unicode_to_keyval(charCode);
            if (WTF::isASCIIUpper(charCode))
                state |= GDK_SHIFT_MASK;
        }
    }

    JSStringRelease(character);

    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    // create and send the event
    GdkEvent event;
    memset(&event, 0, sizeof(event));
    event.key.keyval = gdkKeySym;
    event.key.state = state;
    event.key.window = GTK_WIDGET(view)->window;

    gboolean return_val;
    event.key.type = GDK_KEY_PRESS;
    g_signal_emit_by_name(view, "key-press-event", &event.key, &return_val);

    event.key.type = GDK_KEY_RELEASE;
    g_signal_emit_by_name(view, "key-release-event", &event.key, &return_val);

    return JSValueMakeUndefined(context);
}

static JSValueRef textZoomInCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    gfloat currentZoom = webkit_web_view_get_zoom_level(view);
    webkit_web_view_set_zoom_level(view, currentZoom * zoomMultiplierRatio);

    return JSValueMakeUndefined(context);
}

static JSValueRef textZoomOutCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    gfloat currentZoom = webkit_web_view_get_zoom_level(view);
    webkit_web_view_set_zoom_level(view, currentZoom / zoomMultiplierRatio);

    return JSValueMakeUndefined(context);
}

static JSValueRef zoomPageInCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    webkit_web_view_zoom_in(view);
    return JSValueMakeUndefined(context);
}

static JSValueRef zoomPageOutCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    if (!view)
        return JSValueMakeUndefined(context);

    webkit_web_view_zoom_out(view);
    return JSValueMakeUndefined(context);
}

static JSStaticFunction staticFunctions[] = {
    { "mouseWheelTo", mouseWheelToCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "contextClick", contextClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseDown", mouseDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseUp", mouseUpCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseMoveTo", mouseMoveToCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "beginDragWithFiles", beginDragWithFilesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "leapForward", leapForwardCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "keyDown", keyDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "textZoomIn", textZoomInCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "textZoomOut", textZoomOutCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "zoomPageIn", zoomPageInCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "zoomPageOut", zoomPageOutCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSStaticValue staticValues[] = {
    { "dragMode", getDragModeCallback, setDragModeCallback, kJSPropertyAttributeNone },
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
    down = false;
    dragMode = true;
    lastMousePositionX = lastMousePositionY = 0;
    lastClickPositionX = lastClickPositionY = 0;

    if (!replayingSavedEvents) {
        // This function can be called in the middle of a test, even
        // while replaying saved events. Resetting these while doing that
        // can break things.
        endOfQueue = 0;
        startOfQueue = 0;
    }

    return JSObjectMake(context, getClass(context), 0);
}
