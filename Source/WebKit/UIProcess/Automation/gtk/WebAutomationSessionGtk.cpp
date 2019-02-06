/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebAutomationSession.h"

#include "WebAutomationSessionMacros.h"
#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <gtk/gtk.h>

namespace WebKit {
using namespace WebCore;

static unsigned modifiersToEventState(OptionSet<WebEvent::Modifier> modifiers)
{
    unsigned state = 0;
    if (modifiers.contains(WebEvent::Modifier::ControlKey))
        state |= GDK_CONTROL_MASK;
    if (modifiers.contains(WebEvent::Modifier::ShiftKey))
        state |= GDK_SHIFT_MASK;
    if (modifiers.contains(WebEvent::Modifier::AltKey))
        state |= GDK_META_MASK;
    if (modifiers.contains(WebEvent::Modifier::CapsLockKey))
        state |= GDK_LOCK_MASK;
    return state;
}

static unsigned mouseButtonToGdkButton(WebMouseEvent::Button button)
{
    switch (button) {
    case WebMouseEvent::NoButton:
    case WebMouseEvent::LeftButton:
        return GDK_BUTTON_PRIMARY;
    case WebMouseEvent::MiddleButton:
        return GDK_BUTTON_MIDDLE;
    case WebMouseEvent::RightButton:
        return GDK_BUTTON_SECONDARY;
    }
    return GDK_BUTTON_PRIMARY;
}

static void doMouseEvent(GdkEventType type, GtkWidget* widget, const WebCore::IntPoint& location, unsigned button, unsigned state)
{
    ASSERT(type == GDK_BUTTON_PRESS || type == GDK_BUTTON_RELEASE);

    GUniquePtr<GdkEvent> event(gdk_event_new(type));
    event->button.window = gtk_widget_get_window(widget);
    g_object_ref(event->button.window);
    event->button.time = GDK_CURRENT_TIME;
    event->button.x = location.x();
    event->button.y = location.y();
    event->button.axes = 0;
    event->button.state = state;
    event->button.button = button;
    event->button.device = gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gtk_widget_get_display(widget)));
    int xRoot, yRoot;
    gdk_window_get_root_coords(gtk_widget_get_window(widget), location.x(), location.y(), &xRoot, &yRoot);
    event->button.x_root = xRoot;
    event->button.y_root = yRoot;
    gtk_main_do_event(event.get());
}

static void doMotionEvent(GtkWidget* widget, const WebCore::IntPoint& location, unsigned state)
{
    GUniquePtr<GdkEvent> event(gdk_event_new(GDK_MOTION_NOTIFY));
    event->motion.window = gtk_widget_get_window(widget);
    g_object_ref(event->motion.window);
    event->motion.time = GDK_CURRENT_TIME;
    event->motion.x = location.x();
    event->motion.y = location.y();
    event->motion.axes = 0;
    event->motion.state = state;
    event->motion.device = gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gtk_widget_get_display(widget)));
    int xRoot, yRoot;
    gdk_window_get_root_coords(gtk_widget_get_window(widget), location.x(), location.y(), &xRoot, &yRoot);
    event->motion.x_root = xRoot;
    event->motion.y_root = yRoot;
    gtk_main_do_event(event.get());
}

void WebAutomationSession::platformSimulateMouseInteraction(WebPageProxy& page, MouseInteraction interaction, WebMouseEvent::Button button, const WebCore::IntPoint& locationInView, OptionSet<WebEvent::Modifier> keyModifiers)
{
    unsigned gdkButton = mouseButtonToGdkButton(button);
    auto modifier = stateModifierForGdkButton(gdkButton);
    unsigned state = modifiersToEventState(keyModifiers) | m_currentModifiers;

    switch (interaction) {
    case MouseInteraction::Move:
        doMotionEvent(page.viewWidget(), locationInView, state);
        break;
    case MouseInteraction::Down:
        doMouseEvent(GDK_BUTTON_PRESS, page.viewWidget(), locationInView, gdkButton, state);
        m_currentModifiers |= modifier;
        break;
    case MouseInteraction::Up:
        doMouseEvent(GDK_BUTTON_RELEASE, page.viewWidget(), locationInView, gdkButton, state);
        m_currentModifiers &= ~modifier;
        break;
    case MouseInteraction::SingleClick:
        doMouseEvent(GDK_BUTTON_PRESS, page.viewWidget(), locationInView, gdkButton, state);
        doMouseEvent(GDK_BUTTON_RELEASE, page.viewWidget(), locationInView, gdkButton, state | modifier);
        break;
    case MouseInteraction::DoubleClick:
        doMouseEvent(GDK_BUTTON_PRESS, page.viewWidget(), locationInView, gdkButton, state);
        doMouseEvent(GDK_BUTTON_RELEASE, page.viewWidget(), locationInView, gdkButton, state | modifier);
        doMouseEvent(GDK_BUTTON_PRESS, page.viewWidget(), locationInView, gdkButton, state);
        doMouseEvent(GDK_BUTTON_RELEASE, page.viewWidget(), locationInView, gdkButton, state | modifier);
        break;
    }
}

static void doKeyStrokeEvent(GdkEventType type, GtkWidget* widget, unsigned keyVal, unsigned state, bool doReleaseAfterPress = false)
{
    ASSERT(type == GDK_KEY_PRESS || type == GDK_KEY_RELEASE);

    GUniquePtr<GdkEvent> event(gdk_event_new(type));
    event->key.keyval = keyVal;

    event->key.time = GDK_CURRENT_TIME;
    event->key.window = gtk_widget_get_window(widget);
    g_object_ref(event->key.window);
    gdk_event_set_device(event.get(), gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gtk_widget_get_display(widget))));
    event->key.state = state;

    // When synthesizing an event, an invalid hardware_keycode value can cause it to be badly processed by GTK+.
    GUniqueOutPtr<GdkKeymapKey> keys;
    int keysCount;
    if (gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(), keyVal, &keys.outPtr(), &keysCount) && keysCount)
        event->key.hardware_keycode = keys.get()[0].keycode;

    gtk_main_do_event(event.get());
    if (doReleaseAfterPress) {
        ASSERT(type == GDK_KEY_PRESS);
        event->key.type = GDK_KEY_RELEASE;
        gtk_main_do_event(event.get());
    }
}

static int keyCodeForVirtualKey(Inspector::Protocol::Automation::VirtualKey key)
{
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
        return GDK_KEY_Shift_R;
    case Inspector::Protocol::Automation::VirtualKey::Control:
        return GDK_KEY_Control_R;
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
        return GDK_KEY_Alt_L;
    case Inspector::Protocol::Automation::VirtualKey::Meta:
        return GDK_KEY_Meta_R;
    case Inspector::Protocol::Automation::VirtualKey::Command:
        return GDK_KEY_Execute;
    case Inspector::Protocol::Automation::VirtualKey::Help:
        return GDK_KEY_Help;
    case Inspector::Protocol::Automation::VirtualKey::Backspace:
        return GDK_KEY_BackSpace;
    case Inspector::Protocol::Automation::VirtualKey::Tab:
        return GDK_KEY_Tab;
    case Inspector::Protocol::Automation::VirtualKey::Clear:
        return GDK_KEY_Clear;
    case Inspector::Protocol::Automation::VirtualKey::Enter:
        return GDK_KEY_Return;
    case Inspector::Protocol::Automation::VirtualKey::Pause:
        return GDK_KEY_Pause;
    case Inspector::Protocol::Automation::VirtualKey::Cancel:
        return GDK_KEY_Cancel;
    case Inspector::Protocol::Automation::VirtualKey::Escape:
        return GDK_KEY_Escape;
    case Inspector::Protocol::Automation::VirtualKey::PageUp:
        return GDK_KEY_Page_Up;
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
        return GDK_KEY_Page_Down;
    case Inspector::Protocol::Automation::VirtualKey::End:
        return GDK_KEY_End;
    case Inspector::Protocol::Automation::VirtualKey::Home:
        return GDK_KEY_Home;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
        return GDK_KEY_Left;
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
        return GDK_KEY_Up;
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
        return GDK_KEY_Right;
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        return GDK_KEY_Down;
    case Inspector::Protocol::Automation::VirtualKey::Insert:
        return GDK_KEY_Insert;
    case Inspector::Protocol::Automation::VirtualKey::Delete:
        return GDK_KEY_Delete;
    case Inspector::Protocol::Automation::VirtualKey::Space:
        return GDK_KEY_space;
    case Inspector::Protocol::Automation::VirtualKey::Semicolon:
        return GDK_KEY_semicolon;
    case Inspector::Protocol::Automation::VirtualKey::Equals:
        return GDK_KEY_equal;
    case Inspector::Protocol::Automation::VirtualKey::Return:
        return GDK_KEY_Return;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad0:
        return GDK_KEY_KP_0;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad1:
        return GDK_KEY_KP_1;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad2:
        return GDK_KEY_KP_2;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad3:
        return GDK_KEY_KP_3;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad4:
        return GDK_KEY_KP_4;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad5:
        return GDK_KEY_KP_5;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad6:
        return GDK_KEY_KP_6;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad7:
        return GDK_KEY_KP_7;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad8:
        return GDK_KEY_KP_8;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad9:
        return GDK_KEY_KP_9;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
        return GDK_KEY_KP_Multiply;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
        return GDK_KEY_KP_Add;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSubtract:
        return GDK_KEY_KP_Subtract;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSeparator:
        return GDK_KEY_KP_Separator;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDecimal:
        return GDK_KEY_KP_Decimal;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDivide:
        return GDK_KEY_KP_Divide;
    case Inspector::Protocol::Automation::VirtualKey::Function1:
        return GDK_KEY_F1;
    case Inspector::Protocol::Automation::VirtualKey::Function2:
        return GDK_KEY_F2;
    case Inspector::Protocol::Automation::VirtualKey::Function3:
        return GDK_KEY_F3;
    case Inspector::Protocol::Automation::VirtualKey::Function4:
        return GDK_KEY_F4;
    case Inspector::Protocol::Automation::VirtualKey::Function5:
        return GDK_KEY_F5;
    case Inspector::Protocol::Automation::VirtualKey::Function6:
        return GDK_KEY_F6;
    case Inspector::Protocol::Automation::VirtualKey::Function7:
        return GDK_KEY_F7;
    case Inspector::Protocol::Automation::VirtualKey::Function8:
        return GDK_KEY_F8;
    case Inspector::Protocol::Automation::VirtualKey::Function9:
        return GDK_KEY_F9;
    case Inspector::Protocol::Automation::VirtualKey::Function10:
        return GDK_KEY_F10;
    case Inspector::Protocol::Automation::VirtualKey::Function11:
        return GDK_KEY_F11;
    case Inspector::Protocol::Automation::VirtualKey::Function12:
        return GDK_KEY_F12;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static unsigned modifiersForKeyCode(unsigned keyCode)
{
    switch (keyCode) {
    case GDK_KEY_Shift_R:
        return GDK_SHIFT_MASK;
    case GDK_KEY_Control_R:
        return GDK_CONTROL_MASK;
    case GDK_KEY_Alt_L:
        return GDK_MOD1_MASK;
    case GDK_KEY_Meta_R:
        return GDK_META_MASK;
    }
    return 0;
}

void WebAutomationSession::platformSimulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, WTF::Variant<VirtualKey, CharKey>&& key)
{
    unsigned keyCode;
    WTF::switchOn(key,
        [&] (VirtualKey virtualKey) {
            keyCode = keyCodeForVirtualKey(virtualKey);
        },
        [&] (CharKey charKey) {
            keyCode = gdk_unicode_to_keyval(g_utf8_get_char(&charKey));
        }
    );
    unsigned modifiers = modifiersForKeyCode(keyCode);

    switch (interaction) {
    case KeyboardInteraction::KeyPress:
        m_currentModifiers |= modifiers;
        doKeyStrokeEvent(GDK_KEY_PRESS, page.viewWidget(), keyCode, m_currentModifiers);
        break;
    case KeyboardInteraction::KeyRelease:
        m_currentModifiers &= ~modifiers;
        doKeyStrokeEvent(GDK_KEY_RELEASE, page.viewWidget(), keyCode, m_currentModifiers);
        break;
    case KeyboardInteraction::InsertByKey:
        doKeyStrokeEvent(GDK_KEY_PRESS, page.viewWidget(), keyCode, m_currentModifiers, true);
        break;
    }
}

void WebAutomationSession::platformSimulateKeySequence(WebPageProxy& page, const String& keySequence)
{
    CString keySequenceUTF8 = keySequence.utf8();
    const char* p = keySequenceUTF8.data();
    do {
        doKeyStrokeEvent(GDK_KEY_PRESS, page.viewWidget(), gdk_unicode_to_keyval(g_utf8_get_char(p)), m_currentModifiers, true);
        p = g_utf8_next_char(p);
    } while (*p);
}

} // namespace WebKit

