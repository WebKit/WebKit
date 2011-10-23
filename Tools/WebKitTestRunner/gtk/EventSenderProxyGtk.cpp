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
#include "EventSenderProxy.h"

#include "PlatformWebView.h"
#include "TestController.h"
#include <GOwnPtrGtk.h>
#include <GtkVersioning.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <wtf/text/WTFString.h>

namespace WTR {
// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DOMKeyLocationStandard      = 0x00,
    DOMKeyLocationLeft          = 0x01,
    DOMKeyLocationRight         = 0x02,
    DOMKeyLocationNumpad        = 0x03
};

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
{
}

static void dispatchEvent(GdkEvent* event)
{
    gtk_main_do_event(event);
    gdk_event_free(event);
}


static guint getModifiers(WKEventModifiers modifiersRef)
{
    guint modifiers = 0;

    if (modifiersRef & kWKEventModifiersControlKey)
        modifiers |= kWKEventModifiersControlKey;
    if (modifiersRef & kWKEventModifiersShiftKey)
        modifiers |= kWKEventModifiersShiftKey;
    if (modifiersRef & kWKEventModifiersAltKey)
        modifiers |= kWKEventModifiersAltKey;
    if (modifiersRef & kWKEventModifiersMetaKey)
        modifiers |= kWKEventModifiersMetaKey;

    return modifiers;
}

int getGDKKeySymForKeyRef(WKStringRef keyRef, unsigned location, guint* modifiers)
{
    if (location == DOMKeyLocationNumpad) {
        if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
            return GDK_KEY_KP_Left;
        if (WKStringIsEqualToUTF8CString(keyRef, "rightArror"))
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

    if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
        return GDK_KEY_Left;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightArror"))
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

    size_t stringSize = WKStringGetLength(keyRef);
    char* buffer = new char[stringSize];
    WKStringGetUTF8CString(keyRef, buffer, stringSize);
    int charCode = buffer[0];

    if (charCode == '\n' || charCode == '\r')
        return GDK_KEY_Return;
    if (charCode == '\t')
        return GDK_KEY_Tab;
    if (charCode == '\x8')
        return GDK_KEY_BackSpace;

    if (WTF::isASCIIUpper(charCode))
        *modifiers |= GDK_SHIFT_MASK;

    return gdk_unicode_to_keyval(charCode);
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers modifiersRef, unsigned location)
{
    guint modifiers = getModifiers(modifiersRef);
    int gdkKeySym = getGDKKeySymForKeyRef(keyRef, location, &modifiers);

    GdkEvent* pressEvent = gdk_event_new(GDK_KEY_PRESS);
    pressEvent->key.keyval = gdkKeySym;
    pressEvent->key.state = modifiers;
    pressEvent->key.window = gtk_widget_get_window(GTK_WIDGET(m_testController->mainWebView()->platformView()));
    g_object_ref(pressEvent->key.window);
    gdk_event_set_device(pressEvent, getDefaultGDKPointerDevice(pressEvent->key.window));

    GOwnPtr<GdkKeymapKey> keys;
    gint nKeys;
    if (gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(), gdkKeySym, &keys.outPtr(), &nKeys))
        pressEvent->key.hardware_keycode = keys.outPtr()[0].keycode;

    GdkEvent* releaseEvent = gdk_event_copy(pressEvent);
    dispatchEvent(pressEvent);
    releaseEvent->key.type = GDK_KEY_RELEASE;
    dispatchEvent(releaseEvent);
}

} // namespace WTR
