/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "WTFStringUtilities.h"
#include <WebKit/InputMethodFilter.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;

namespace TestWebKitAPI {

class TestInputMethodFilter : public InputMethodFilter {
public:
    TestInputMethodFilter()
        : m_testWindow(gtk_window_new(GTK_WINDOW_POPUP))
    {
        setTestingMode(true);

        gtk_widget_show(m_testWindow);
        gtk_im_context_set_client_window(context(), gtk_widget_get_window(m_testWindow));

        setEnabled(true);
    }

    ~TestInputMethodFilter()
    {
        gtk_widget_destroy(m_testWindow);
    }

    void sendKeyEventToFilter(unsigned gdkKeyValue, GdkEventType type, unsigned modifiers = 0)
    {
        GdkEvent* event = gdk_event_new(type);
        event->key.keyval = gdkKeyValue;
        event->key.state = modifiers;
        event->key.window = gtk_widget_get_window(m_testWindow);
        event->key.time = GDK_CURRENT_TIME;
        g_object_ref(event->key.window);
        gdk_event_set_device(event, gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gdk_display_get_default())));

        GUniqueOutPtr<GdkKeymapKey> keys;
        gint nKeys;
        if (gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(), gdkKeyValue, &keys.outPtr(), &nKeys) && nKeys)
            event->key.hardware_keycode = keys.get()[0].keycode;

        filterKeyEvent(&event->key);
        gdk_event_free(event);
    }

    void sendPressAndReleaseKeyEventPairToFilter(unsigned gdkKeyValue, unsigned modifiers = 0)
    {
        sendKeyEventToFilter(gdkKeyValue, GDK_KEY_PRESS, modifiers);
        sendKeyEventToFilter(gdkKeyValue, GDK_KEY_RELEASE, modifiers);
    }

private:
    GtkWidget* m_testWindow;
};

TEST(WebKit2, InputMethodFilterSimple)
{
    TestInputMethodFilter inputMethodFilter;
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_g);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_t);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_k);

    const Vector<String>& events = inputMethodFilter.events();

    ASSERT_EQ(6, events.size());
    ASSERT_EQ(String("sendSimpleKeyEvent type=press keycode=67 text='g'"), events[0]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=67"), events[1]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=press keycode=74 text='t'"), events[2]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=74"), events[3]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=press keycode=6B text='k'"), events[4]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=6B"), events[5]);
}

TEST(WebKit2, InputMethodFilterUnicodeSequence)
{
    TestInputMethodFilter inputMethodFilter;

    // This is simple unicode hex entry of the characters, u, 0, 0, f, 4 pressed with
    // the shift and controls keys held down. In reality, these values are not typical
    // of an actual hex entry, because they'd be transformed by the shift modifier according
    // to the keyboard layout. For instance, on a US keyboard a 0 with the shift key pressed
    // is a right parenthesis. Using these values prevents having to work out what the
    // transformed characters are based on the current keyboard layout.
    inputMethodFilter.sendKeyEventToFilter(GDK_KEY_Control_L, GDK_KEY_PRESS);
    inputMethodFilter.sendKeyEventToFilter(GDK_KEY_Shift_L, GDK_KEY_PRESS, GDK_CONTROL_MASK);

    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_U, GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_0, GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_0, GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_F, GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_4, GDK_SHIFT_MASK | GDK_CONTROL_MASK);

    inputMethodFilter.sendKeyEventToFilter(GDK_KEY_Shift_L, GDK_KEY_RELEASE, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    inputMethodFilter.sendKeyEventToFilter(GDK_KEY_Control_L, GDK_KEY_RELEASE, GDK_CONTROL_MASK);

    const Vector<String>& events = inputMethodFilter.events();
    ASSERT_EQ(21, events.size());
    ASSERT_EQ(String("sendSimpleKeyEvent type=press keycode=FFE3"), events[0]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=press keycode=FFE1"), events[1]);
    ASSERT_EQ(String("sendKeyEventWithCompositionResults type=press keycode=55"), events[2]);
    ASSERT_EQ(String("setPreedit text='u' cursorOffset=1"), events[3]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=55"), events[4]);
    ASSERT_EQ(String("sendKeyEventWithCompositionResults type=press keycode=30"), events[5]);
    ASSERT_EQ(String("setPreedit text='u0' cursorOffset=2"), events[6]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=30"), events[7]);
    ASSERT_EQ(String("sendKeyEventWithCompositionResults type=press keycode=30"), events[8]);
    ASSERT_EQ(String("setPreedit text='u00' cursorOffset=3"), events[9]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=30"), events[10]);
    ASSERT_EQ(String("sendKeyEventWithCompositionResults type=press keycode=46"), events[11]);
    ASSERT_EQ(String("setPreedit text='u00F' cursorOffset=4"), events[12]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=46"), events[13]);
    ASSERT_EQ(String("sendKeyEventWithCompositionResults type=press keycode=34"), events[14]);
    ASSERT_EQ(String("setPreedit text='u00F4' cursorOffset=5"), events[15]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=34"), events[16]);
    ASSERT_EQ(String::fromUTF8("confirmComposition 'ô'"), events[17]);
    ASSERT_EQ(String("setPreedit text='' cursorOffset=0"), events[18]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=FFE1"), events[19]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=FFE3"), events[20]);
}

TEST(WebKit2, InputMethodFilterComposeKey)
{
    TestInputMethodFilter inputMethodFilter;

    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_Multi_key);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_apostrophe);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_o);

    const Vector<String>& events = inputMethodFilter.events();
    ASSERT_EQ(3, events.size());
    ASSERT_EQ(String("sendKeyEventWithCompositionResults type=press keycode=6F"), events[0]);
    ASSERT_EQ(String::fromUTF8("confirmComposition 'ó'"), events[1]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=6F"), events[2]);
}

typedef void (*GetPreeditStringCallback) (GtkIMContext*, gchar**, PangoAttrList**, int*);
static void temporaryGetPreeditStringOverride(GtkIMContext*, char** string, PangoAttrList** attrs, int* cursorPosition)
{
    *string = g_strdup("preedit of doom, bringer of cheese");
    *cursorPosition = 3;
}

TEST(WebKit2, InputMethodFilterContextEventsWithoutKeyEvents)
{
    TestInputMethodFilter inputMethodFilter;

    // This is a bit of a hack to avoid mocking out the entire InputMethodContext, by
    // simply replacing the get_preedit_string virtual method for the length of this test.
    GtkIMContext* context = inputMethodFilter.context();
    GtkIMContextClass* contextClass = GTK_IM_CONTEXT_GET_CLASS(context);
    GetPreeditStringCallback previousCallback = contextClass->get_preedit_string;
    contextClass->get_preedit_string = temporaryGetPreeditStringOverride;

    g_signal_emit_by_name(context, "preedit-changed");
    g_signal_emit_by_name(context, "commit", "commit text");

    contextClass->get_preedit_string = previousCallback;

    const Vector<String>& events = inputMethodFilter.events();
    ASSERT_EQ(6, events.size());
    ASSERT_EQ(String("sendKeyEventWithCompositionResults type=press keycode=FFFFFF (faked)"), events[0]);
    ASSERT_EQ(String("setPreedit text='preedit of doom, bringer of cheese' cursorOffset=3"), events[1]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=FFFFFF (faked)"), events[2]);
    ASSERT_EQ(String("sendKeyEventWithCompositionResults type=press keycode=FFFFFF (faked)"), events[3]);
    ASSERT_EQ(String("confirmComposition 'commit text'"), events[4]);
    ASSERT_EQ(String("sendSimpleKeyEvent type=release keycode=FFFFFF (faked)"), events[5]);
}

static bool gSawContextReset = false;
typedef void (*ResetCallback) (GtkIMContext*);
static void temporaryResetOverride(GtkIMContext*)
{
    gSawContextReset = true;
}

TEST(WebKit2, InputMethodFilterContextFocusOutDuringOngoingComposition)
{
    TestInputMethodFilter inputMethodFilter;

    // See comment above about this technique.
    GtkIMContext* context = inputMethodFilter.context();
    GtkIMContextClass* contextClass = GTK_IM_CONTEXT_GET_CLASS(context);
    ResetCallback previousCallback = contextClass->reset;
    contextClass->reset = temporaryResetOverride;

    gSawContextReset = false;
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_Multi_key);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_apostrophe);
    inputMethodFilter.notifyFocusedOut();
    ASSERT_TRUE(gSawContextReset);

    contextClass->reset = previousCallback;
}

TEST(WebKit2, InputMethodFilterContextMouseClickDuringOngoingComposition)
{
    TestInputMethodFilter inputMethodFilter;

    // See comment above about this technique.
    GtkIMContext* context = inputMethodFilter.context();
    GtkIMContextClass* contextClass = GTK_IM_CONTEXT_GET_CLASS(context);
    ResetCallback previousCallback = contextClass->reset;
    contextClass->reset = temporaryResetOverride;

    gSawContextReset = false;
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_Multi_key);
    inputMethodFilter.sendPressAndReleaseKeyEventPairToFilter(GDK_KEY_apostrophe);
    inputMethodFilter.notifyMouseButtonPress();
    ASSERT_TRUE(gSawContextReset);

    contextClass->reset = previousCallback;
}

} // namespace TestWebKitAPI
