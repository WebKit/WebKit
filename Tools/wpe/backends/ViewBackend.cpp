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

#include <epoxy/egl.h>
#include <glib.h>
#include <wpe/fdo-egl.h>

#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY
#include "WebKitAccessibleApplication.h"
#include <atk-bridge.h>
#include <atk/atk.h>
#endif

namespace WPEToolingBackends {

#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY
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

static void notifyAccessibilityKeyEventListeners(struct wpe_input_keyboard_event* event)
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
#endif

ViewBackend::ViewBackend(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
{
    wpe_loader_init("libWPEBackend-fdo-1.0.so");
}

ViewBackend::~ViewBackend()
{
    if (m_exportable)
        wpe_view_backend_exportable_fdo_destroy(m_exportable);

    if (m_eglContext)
        eglDestroyContext(m_eglDisplay, m_eglContext);
}

bool ViewBackend::initialize()
{
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return false;

    eglInitialize(m_eglDisplay, nullptr, nullptr);

    if (!eglBindAPI(EGL_OPENGL_ES_API))
        return false;

    wpe_fdo_initialize_for_egl_display(m_eglDisplay);

    static const EGLint configAttributes[13] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    {
        EGLint count = 0;
        if (!eglGetConfigs(m_eglDisplay, nullptr, 0, &count) || count < 1)
            return false;

        EGLConfig* configs = g_new0(EGLConfig, count);
        EGLint matched = 0;
        if (eglChooseConfig(m_eglDisplay, configAttributes, configs, count, &matched) && !!matched)
            m_eglConfig = configs[0];
        g_free(configs);
    }

    static const EGLint contextAttributes[3] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (!m_eglContext)
        return false;

    static struct wpe_view_backend_exportable_fdo_egl_client exportableClient = {
        // export_egl_image
        nullptr,
        // export_fdo_egl_image
        [](void* data, struct wpe_fdo_egl_exported_image* image)
        {
            static_cast<ViewBackend*>(data)->displayBuffer(image);
        },
        // padding
        nullptr, nullptr, nullptr
    };
    m_exportable = wpe_view_backend_exportable_fdo_egl_create(&exportableClient, this, m_width, m_height);

    initializeAccessibility();

    return true;
}

void ViewBackend::initializeAccessibility()
{
#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY
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
#endif
}

void ViewBackend::updateAccessibilityState(uint32_t previousFlags)
{
#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY
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
#endif
}

void ViewBackend::setInputClient(std::unique_ptr<InputClient>&& client)
{
    m_inputClient = std::move(client);
}

#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY
void ViewBackend::setAccessibleChild(AtkObject* child)
{
    auto* accessible = atk_get_root();
    if (!WEBKIT_IS_ACCESSIBLE_APPLICATION(accessible))
        return;

    webkitAccessibleApplicationSetChild(WEBKIT_ACCESSIBLE_APPLICATION(accessible), child);
}
#endif

struct wpe_view_backend* ViewBackend::backend() const
{
    return m_exportable ? wpe_view_backend_exportable_fdo_get_view_backend(m_exportable) : nullptr;
}

void ViewBackend::addActivityState(uint32_t flags)
{
    uint32_t previousFlags = wpe_view_backend_get_activity_state(backend());
    wpe_view_backend_add_activity_state(backend(), flags);
    updateAccessibilityState(previousFlags);
}

void ViewBackend::removeActivityState(uint32_t flags)
{
    uint32_t previousFlags = wpe_view_backend_get_activity_state(backend());
    wpe_view_backend_remove_activity_state(backend(), flags);
    updateAccessibilityState(previousFlags);
}

void ViewBackend::dispatchInputPointerEvent(struct wpe_input_pointer_event* event)
{
    if (m_inputClient && m_inputClient->dispatchPointerEvent(event))
        return;
    wpe_view_backend_dispatch_pointer_event(backend(), event);
}

void ViewBackend::dispatchInputAxisEvent(struct wpe_input_axis_event* event)
{
    if (m_inputClient && m_inputClient->dispatchAxisEvent(event))
        return;
    wpe_view_backend_dispatch_axis_event(backend(), event);
}

void ViewBackend::dispatchInputKeyboardEvent(struct wpe_input_keyboard_event* event)
{
#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY
    notifyAccessibilityKeyEventListeners(event);
#endif

    if (m_inputClient && m_inputClient->dispatchKeyboardEvent(event))
        return;
    wpe_view_backend_dispatch_keyboard_event(backend(), event);
}

void ViewBackend::dispatchInputTouchEvent(struct wpe_input_touch_event* event)
{
    if (m_inputClient && m_inputClient->dispatchTouchEvent(event))
        return;
    wpe_view_backend_dispatch_touch_event(backend(), event);
}

} // namespace WPEToolingBackends
