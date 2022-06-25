/*
 * Copyright (C) 2018 Igalia S.L.
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

#include "ViewBackend.h"

#if defined(ENABLE_ACCESSIBILITY) && ENABLE_ACCESSIBILITY

#include "WebKitAccessibleApplication.h"
#include <atk-bridge.h>
#include <atk/atk.h>
#include <glib.h>

namespace WPEToolingBackends {

static GHashTable* keyEventListeners;

struct KeyEventListener {
    AtkKeySnoopFunc function;
    gpointer userData;
};

static unsigned addKeyEventListener(AtkKeySnoopFunc function, gpointer userData)
{
    if (!keyEventListeners) {
        keyEventListeners = g_hash_table_new_full(g_direct_hash, g_direct_equal, nullptr, [](gpointer data) {
            delete static_cast<KeyEventListener*>(data);
        });
    }

    static unsigned key = 0;
    key++;
    g_hash_table_insert(keyEventListeners, GUINT_TO_POINTER(key), new KeyEventListener { function, userData });

    return key;
}

static void removeKeyEventListener(unsigned key)
{
    if (!keyEventListeners)
        return;

    g_hash_table_remove(keyEventListeners, GUINT_TO_POINTER(key));
}

void ViewBackend::notifyAccessibilityKeyEventListeners(struct wpe_input_keyboard_event* event)
{
    if (!keyEventListeners)
        return;

    AtkKeyEventStruct atkEvent;
    atkEvent.type = event->pressed ? ATK_KEY_EVENT_PRESS : ATK_KEY_EVENT_RELEASE;
    atkEvent.state = event->modifiers;
    atkEvent.keyval = event->key_code;
    atkEvent.keycode = event->hardware_key_code;
    atkEvent.timestamp = event->time;

    atkEvent.string = nullptr;
    switch (atkEvent.keyval) {
    case WPE_KEY_ISO_Enter:
    case WPE_KEY_KP_Enter:
    case WPE_KEY_Return:
        atkEvent.string = g_strdup("\r");
        atkEvent.length = 1;
        break;
    case WPE_KEY_BackSpace:
        atkEvent.string = g_strdup("\x8");
        atkEvent.length = 1;
        break;
    case WPE_KEY_Tab:
        atkEvent.string = g_strdup("\t");
        atkEvent.length = 1;
        break;
    default:
        break;
    }

    if (!atkEvent.string) {
        auto unicodeCharacter = wpe_key_code_to_unicode(atkEvent.keyval);
        if (unicodeCharacter) {
            char buffer[7];
            int length = g_unichar_to_utf8(unicodeCharacter, buffer);
            buffer[length] = '\0';
            size_t bytesWritten;
            atkEvent.string = g_locale_from_utf8(buffer, length, nullptr, &bytesWritten, nullptr);
            atkEvent.length = bytesWritten;
        }
    }

    if (!atkEvent.string) {
        atkEvent.length = 0;
        atkEvent.string = g_strdup("");
    }

    GHashTableIter iter;
    gpointer value;
    g_hash_table_iter_init(&iter, keyEventListeners);
    while (g_hash_table_iter_next(&iter, nullptr, &value)) {
        auto* listener = static_cast<KeyEventListener*>(value);
        listener->function(&atkEvent, listener->userData);
    }

    g_free(atkEvent.string);
}

void ViewBackend::initializeAccessibility()
{
    auto* atkUtilClass = ATK_UTIL_CLASS(g_type_class_ref(ATK_TYPE_UTIL));
    atkUtilClass->add_key_event_listener = [](AtkKeySnoopFunc listener, gpointer userData) -> guint {
        return addKeyEventListener(listener, userData);
    };

    atkUtilClass->remove_key_event_listener = [](guint key) {
        removeKeyEventListener(key);
    };

    atkUtilClass->get_root = []() -> AtkObject* {
        static AtkObject* accessible = nullptr;
        if (!accessible)
            accessible = ATK_OBJECT(webkitAccessibleApplicationNew());
        return accessible;
    };

    atkUtilClass->get_toolkit_name = []() -> const gchar* {
        return "WPEWebKit";
    };

    atkUtilClass->get_toolkit_version = []() -> const gchar* {
        return "";
    };

    atk_bridge_adaptor_init(nullptr, nullptr);
}

void ViewBackend::updateAccessibilityState(uint32_t previousFlags)
{
    auto* accessible = atk_get_root();
    if (!WEBKIT_IS_ACCESSIBLE_APPLICATION(accessible))
        return;

    uint32_t flags = wpe_view_backend_get_activity_state(backend());
    uint32_t changedFlags = previousFlags ^ flags;
    if (changedFlags & wpe_view_activity_state_in_window)
        atk_object_notify_state_change(accessible, ATK_STATE_ACTIVE, flags & wpe_view_activity_state_in_window);
    if (changedFlags & wpe_view_activity_state_visible)
        atk_object_notify_state_change(accessible, ATK_STATE_VISIBLE, flags & wpe_view_activity_state_visible);
    if (changedFlags & wpe_view_activity_state_focused) {
        atk_object_notify_state_change(accessible, ATK_STATE_FOCUSED, flags & wpe_view_activity_state_focused);
        if ((flags & wpe_view_activity_state_in_window) && (flags & wpe_view_activity_state_focused))
            g_signal_emit_by_name(accessible, "activate");
        else
            g_signal_emit_by_name(accessible, "deactivate");
    }
}

void ViewBackend::setAccessibleChild(AtkObject* child)
{
    auto* accessible = atk_get_root();
    if (!WEBKIT_IS_ACCESSIBLE_APPLICATION(accessible))
        return;

    webkitAccessibleApplicationSetChild(WEBKIT_ACCESSIBLE_APPLICATION(accessible), child);
}

} // namespace WPEToolingBackends

#endif // ENABLE(ACCESSIBILITY)
