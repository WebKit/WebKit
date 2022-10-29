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

#include "WindowViewBackend.h"

#include <cstdio>
#include <cstring>
#include <linux/input.h>
#include <memory>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>

// This include order is necessary to enforce the Wayland EGL platform.
#include <wayland-egl.h>
#include <epoxy/egl.h>
#include <wpe/fdo-egl.h>
#include <xkbcommon/xkbcommon.h>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WL_bind_wayland_display 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, struct wl_resource* buffer, EGLint attribute, EGLint* value);

#define EGL_WAYLAND_BUFFER_WL 0x31D5 // eglCreateImageKHR target
#define EGL_WAYLAND_PLANE_WL  0x31D6 // eglCreateImageKHR target
#endif

namespace WPEToolingBackends {

struct WaylandEGLConnection {
    struct wl_display* display { nullptr };
    EGLDisplay eglDisplay { EGL_NO_DISPLAY };

    static const WaylandEGLConnection& singleton()
    {
        static std::once_flag s_onceFlag;
        static WaylandEGLConnection s_connection;
        std::call_once(s_onceFlag,
            [] {
                s_connection.display = wl_display_connect(nullptr);
                if (!s_connection.display) {
                    g_warning("WaylandEGLConnection: Could not connect to Wayland Display");
                    return;
                }

                EGLDisplay eglDisplay = eglGetDisplay(s_connection.display);
                if (eglDisplay == EGL_NO_DISPLAY) {
                    g_warning("WaylandEGLConnection: No EGL Display available in this connection");
                    return;
                }

                if (!eglInitialize(eglDisplay, nullptr, nullptr) || !eglBindAPI(EGL_OPENGL_ES_API)) {
                    g_warning("WaylandEGLConnection: Failed to initialize and bind the EGL Display");
                    return;
                }

                s_connection.eglDisplay = eglDisplay;
                wpe_fdo_initialize_for_egl_display(s_connection.eglDisplay);
            });

        return s_connection;
    }
};

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC imageTargetTexture2DOES;

struct EventSource {
    static GSourceFuncs sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct wl_display* display;
};

GSourceFuncs EventSource::sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        struct wl_display* display = source->display;

        *timeout = -1;

        wl_display_dispatch_pending(display);
        wl_display_flush(display);

        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        return !!source->pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        struct wl_display* display = source->display;

        if (source->pfd.revents & G_IO_IN)
            wl_display_dispatch(display);

        if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        source->pfd.revents = 0;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

const struct wl_registry_listener WindowViewBackend::s_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto* window = static_cast<WindowViewBackend*>(data);

        if (!std::strcmp(interface, "wl_compositor"))
            window->m_compositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));

        if (!std::strcmp(interface, "xdg_wm_base"))
            window->m_xdg.wm = static_cast<struct xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));

        if (!std::strcmp(interface, "zxdg_shell_v6"))
            window->m_zxdg.shell = static_cast<struct zxdg_shell_v6*>(wl_registry_bind(registry, name, &zxdg_shell_v6_interface, 1));

        if (!std::strcmp(interface, "wl_seat"))
            window->m_seat = static_cast<struct wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 5));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t) { },
};

const struct xdg_wm_base_listener WindowViewBackend::XDGStable::s_wmListener = {
    // ping
    [](void*, struct xdg_wm_base* xdgWM, uint32_t serial)
    {
        xdg_wm_base_pong(xdgWM, serial);
    },
};

const struct zxdg_shell_v6_listener WindowViewBackend::XDGUnstable::s_shellListener = {
    // ping
    [](void*, struct zxdg_shell_v6* shell, uint32_t serial)
    {
        zxdg_shell_v6_pong(shell, serial);
    },
};

const struct wl_pointer_listener WindowViewBackend::s_pointerListener = {
    // enter
    [](void* data, struct wl_pointer*, uint32_t /*serial*/, struct wl_surface* surface, wl_fixed_t, wl_fixed_t)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        if (window.m_surface == surface) {
            window.m_seatData.pointer.target = surface;
            window.m_seatData.pointer.modifiers = 0;
        }
    },
    // leave
    [](void* data, struct wl_pointer*, uint32_t /*serial*/, struct wl_surface* surface)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        if (window.m_surface == surface && window.m_seatData.pointer.target == surface)
            window.m_seatData.pointer.target = nullptr;
    },
    // motion
    [](void* data, struct wl_pointer*, uint32_t time, wl_fixed_t fixedX, wl_fixed_t fixedY)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        int x = wl_fixed_to_int(fixedX);
        int y = wl_fixed_to_int(fixedY);
        window.m_seatData.pointer.coords = { x, y };

        if (window.m_seatData.pointer.target) {
            struct wpe_input_pointer_event event = { wpe_input_pointer_event_type_motion,
                time, x, y, window.m_seatData.pointer.button, window.m_seatData.pointer.state, window.modifiers() };
            window.dispatchInputPointerEvent(&event);
        }
    },
    // button
    [](void* data, struct wl_pointer*, uint32_t /*serial*/, uint32_t time, uint32_t button, uint32_t state)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        if (button >= BTN_MOUSE)
            button = button - BTN_MOUSE + 1;
        else
            button = 0;

        window.m_seatData.pointer.button = !!state ? button : 0;
        window.m_seatData.pointer.state = state;

        uint32_t modifier = 0;
        switch (button) {
        case 1:
            modifier = wpe_input_pointer_modifier_button1;
            break;
        case 2:
            modifier = wpe_input_pointer_modifier_button2;
            break;
        case 3:
            modifier = wpe_input_pointer_modifier_button3;
            break;
        case 4:
            modifier = wpe_input_pointer_modifier_button4;
            break;
        case 5:
            modifier = wpe_input_pointer_modifier_button5;
            break;
        default:
            break;
        }

        if (state)
            window.m_seatData.pointer.modifiers |= modifier;
        else
            window.m_seatData.pointer.modifiers &= ~modifier;

        if (window.m_seatData.pointer.target) {
            struct wpe_input_pointer_event event = { wpe_input_pointer_event_type_button,
                time, window.m_seatData.pointer.coords.first, window.m_seatData.pointer.coords.second, button, state, window.modifiers() };
            window.dispatchInputPointerEvent(&event);
        }
    },
    // axis
    [](void* data, struct wl_pointer*, uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        if (axis != WL_POINTER_AXIS_HORIZONTAL_SCROLL && axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
            return;

        auto& window = *static_cast<WindowViewBackend*>(data);
        if (window.m_seatData.pointer.target) {
            struct wpe_input_axis_event event = { wpe_input_axis_event_type_motion,
                time, window.m_seatData.pointer.coords.first, window.m_seatData.pointer.coords.second, axis, -wl_fixed_to_int(value), window.modifiers() };
            if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL && window.m_seatData.axis_discrete.horizontal)
                event.value = window.m_seatData.axis_discrete.horizontal;
            else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL && window.m_seatData.axis_discrete.vertical)
                event.value = window.m_seatData.axis_discrete.vertical;
#if WPE_CHECK_VERSION(1, 5, 0)
            else {
                struct wpe_input_axis_2d_event event2d = { event, 0, 0 };
                event2d.base.type = static_cast<wpe_input_axis_event_type>(wpe_input_axis_event_type_mask_2d | wpe_input_axis_event_type_motion_smooth);

                if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
                    event2d.x_axis = wl_fixed_to_double(value);
                else
                    event2d.y_axis = -wl_fixed_to_double(value);

                window.dispatchInputAxisEvent(&event2d.base);
                return;
            }
#endif
            window.dispatchInputAxisEvent(&event);
            window.m_seatData.axis_discrete.horizontal = window.m_seatData.axis_discrete.vertical = 0;
        }
    },
    // frame
    [](void*, struct wl_pointer*) { },
    // axis_source
    [](void*, struct wl_pointer*, uint32_t) { },
    // axis_stop
    [](void* data, struct wl_pointer*, uint32_t time, uint32_t axis)
    {
        if (axis != WL_POINTER_AXIS_HORIZONTAL_SCROLL && axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
            return;

        auto& window = *static_cast<WindowViewBackend*>(data);
        if (window.m_seatData.pointer.target) {
            struct wpe_input_axis_event event = { wpe_input_axis_event_type_motion,
                time, window.m_seatData.pointer.coords.first, window.m_seatData.pointer.coords.second, axis, 0, window.modifiers() };
            window.dispatchInputAxisEvent(&event);
        }
    },
    // axis_discrete
    [](void* data, struct wl_pointer*, uint32_t axis, int32_t discrete) {
        auto& window = *static_cast<WindowViewBackend*>(data);
        switch (axis) {
        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            window.m_seatData.axis_discrete.horizontal = discrete;
            break;
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
            window.m_seatData.axis_discrete.vertical = -discrete;
            break;
        }
    },
#ifdef WL_POINTER_AXIS_VALUE120_SINCE_VERSION
    // axis_value120
    [](void*, struct wl_pointer*, uint32_t, int32_t) { }
#endif
};

const struct wl_keyboard_listener WindowViewBackend::s_keyboardListener = {
    // keymap
    [](void*, struct wl_keyboard*, uint32_t format, int fd, uint32_t size)
    {
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
            close(fd);
            return;
        }

        void* mapping = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
        if (mapping == MAP_FAILED) {
            close(fd);
            return;
        }

        auto* xkb = wpe_input_xkb_context_get_default();
        auto* keymap = xkb_keymap_new_from_string(wpe_input_xkb_context_get_context(xkb), static_cast<char*>(mapping),
            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(mapping, size);
        close(fd);

        wpe_input_xkb_context_set_keymap(xkb, keymap);
        xkb_keymap_unref(keymap);
    },
    // enter
    [](void* data, struct wl_keyboard*, uint32_t /*serial*/, struct wl_surface* surface, struct wl_array*)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        if (window.m_surface == surface)
            window.m_seatData.keyboard.target = surface;
    },
    // leave
    [](void* data, struct wl_keyboard*, uint32_t /*serial*/, struct wl_surface* surface)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        if (window.m_surface == surface && window.m_seatData.keyboard.target == surface)
            window.m_seatData.keyboard.target = nullptr;
    },
    // key
    [](void* data, struct wl_keyboard*, uint32_t /*serial*/, uint32_t time, uint32_t key, uint32_t state)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);

        // IDK.
        key += 8;

        window.handleKeyEvent(key, state, time);

        auto& seatData = window.m_seatData;
        if (!seatData.repeatInfo.rate)
            return;

        auto* keymap = wpe_input_xkb_context_get_keymap(wpe_input_xkb_context_get_default());

        if (state == WL_KEYBOARD_KEY_STATE_RELEASED
            && seatData.repeatData.key == key) {
            if (seatData.repeatData.eventSource)
                g_source_remove(seatData.repeatData.eventSource);
            seatData.repeatData = { 0, 0, 0, 0 };
        } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED
            && keymap && xkb_keymap_key_repeats(keymap, key)) {

            if (seatData.repeatData.eventSource)
                g_source_remove(seatData.repeatData.eventSource);

            auto sourceID = g_timeout_add(seatData.repeatInfo.delay, [](void* data) -> gboolean {
                auto& window = *static_cast<WindowViewBackend*>(data);
                auto& seatData = window.m_seatData;
                window.handleKeyEvent(seatData.repeatData.key, seatData.repeatData.state, seatData.repeatData.time);
                seatData.repeatData.eventSource = g_timeout_add(seatData.repeatInfo.rate, [](void* data) -> gboolean {
                    auto& window = *static_cast<WindowViewBackend*>(data);
                    auto& seatData = window.m_seatData;
                    window.handleKeyEvent(seatData.repeatData.key, seatData.repeatData.state, seatData.repeatData.time);
                    return G_SOURCE_CONTINUE;
                }, data);
                return G_SOURCE_REMOVE;
            }, data);
            seatData.repeatData = { key, time, state, sourceID };
        }
    },
    // modifiers
    [](void* data, struct wl_keyboard*, uint32_t /*serial*/, uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group)
    {
        auto& keyboard = static_cast<WindowViewBackend*>(data)->m_seatData.keyboard;
        keyboard.modifiers = wpe_input_xkb_context_get_modifiers(wpe_input_xkb_context_get_default(), depressedMods, latchedMods, lockedMods, group);
    },
    // repeat_info
    [](void* data, struct wl_keyboard*, int32_t rate, int32_t delay)
    {
        auto& seatData = static_cast<WindowViewBackend*>(data)->m_seatData;

        auto& repeatInfo = seatData.repeatInfo;
        repeatInfo = { rate, delay };

        // A rate of zero disables any repeating.
        if (!rate) {
            auto& repeatData = seatData.repeatData;
            if (repeatData.eventSource) {
                g_source_remove(repeatData.eventSource);
                repeatData = { 0, 0, 0, 0 };
            }
        }
    },
};

const struct wl_touch_listener WindowViewBackend::s_touchListener = {
    // down
    [](void* data, struct wl_touch*, uint32_t, uint32_t time, struct wl_surface* surface, int32_t id, wl_fixed_t x, wl_fixed_t y)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        if (window.m_surface != surface || id < 0 || id >= 10)
            return;

        auto& seatData = window.m_seatData;
        seatData.touch.tracking = true;
        struct wpe_input_touch_event_raw rawEvent = { wpe_input_touch_event_type_down,
            time, id, wl_fixed_to_int(x), wl_fixed_to_int(y) };
        memcpy(&seatData.touch.points[id], &rawEvent, sizeof(struct wpe_input_touch_event_raw));

        struct wpe_input_touch_event event = { seatData.touch.points, 10,
            rawEvent.type, rawEvent.id, rawEvent.time, window.modifiers() };
        window.dispatchInputTouchEvent(&event);
    },
    // up
    [](void* data, struct wl_touch*, uint32_t, uint32_t time, int32_t id)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        auto& seatData = window.m_seatData;
        if (!seatData.touch.tracking || id < 0 || id >= 10)
            return;

        seatData.touch.tracking = false;

        struct wpe_input_touch_event_raw rawEvent = { wpe_input_touch_event_type_up,
            time, id, seatData.touch.points[id].x, seatData.touch.points[id].y };
        memcpy(&seatData.touch.points[id], &rawEvent, sizeof(struct wpe_input_touch_event_raw));

        struct wpe_input_touch_event event = { seatData.touch.points, 10,
            rawEvent.type, rawEvent.id, rawEvent.time, window.modifiers() };
        window.dispatchInputTouchEvent(&event);

        memset(&seatData.touch.points[id], 0x00, sizeof(struct wpe_input_touch_event_raw));
    },
    // motion
    [](void* data, struct wl_touch*, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        auto& seatData = window.m_seatData;
        if (!seatData.touch.tracking || id < 0 || id >= 10)
            return;

        struct wpe_input_touch_event_raw rawEvent = { wpe_input_touch_event_type_motion,
            time, id, wl_fixed_to_int(x), wl_fixed_to_int(y) };
        memcpy(&seatData.touch.points[id], &rawEvent, sizeof(struct wpe_input_touch_event_raw));

        struct wpe_input_touch_event event = { seatData.touch.points, 10,
            rawEvent.type, rawEvent.id, rawEvent.time, window.modifiers() };
        window.dispatchInputTouchEvent(&event);
    },
    // frame
    [](void*, struct wl_touch*) { },
    // cancel
    [](void*, struct wl_touch*) { },
    // shape
    [](void*, struct wl_touch*, int32_t, wl_fixed_t, wl_fixed_t) { },
    // orientation
    [](void*, struct wl_touch*, int32_t, wl_fixed_t) { },
};

const struct wl_seat_listener WindowViewBackend::s_seatListener = {
    // capabilities
    [](void* data, struct wl_seat* seat, uint32_t capabilities)
    {
        auto* window = static_cast<WindowViewBackend*>(data);
        auto& seatData = window->m_seatData;

        // WL_SEAT_CAPABILITY_POINTER
        const bool hasPointerCap = capabilities & WL_SEAT_CAPABILITY_POINTER;
        if (hasPointerCap && !seatData.pointer.object) {
            seatData.pointer.object = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(seatData.pointer.object, &s_pointerListener, window);
        }
        if (!hasPointerCap && seatData.pointer.object) {
            wl_pointer_destroy(seatData.pointer.object);
            seatData.pointer.object = nullptr;
        }

        // WL_SEAT_CAPABILITY_KEYBOARD
        const bool hasKeyboardCap = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
        if (hasKeyboardCap && !seatData.keyboard.object) {
            seatData.keyboard.object = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(seatData.keyboard.object, &s_keyboardListener, window);
        }
        if (!hasKeyboardCap && seatData.keyboard.object) {
            wl_keyboard_destroy(seatData.keyboard.object);
            seatData.keyboard.object = nullptr;
        }

        // WL_SEAT_CAPABILITY_TOUCH
        const bool hasTouchCap = capabilities & WL_SEAT_CAPABILITY_TOUCH;
        if (hasTouchCap && !seatData.touch.object) {
            seatData.touch.object = wl_seat_get_touch(seat);
            wl_touch_add_listener(seatData.touch.object, &s_touchListener, window);
        }
        if (!hasTouchCap && seatData.touch.object) {
            wl_touch_destroy(seatData.touch.object);
            seatData.touch.object = nullptr;
        }
    },
    // name
    [](void*, struct wl_seat*, const char*) { }
};

const struct xdg_surface_listener WindowViewBackend::XDGStable::s_surfaceListener = {
    // configure
    [](void*, struct xdg_surface* surface, uint32_t serial)
    {
        xdg_surface_ack_configure(surface, serial);
    },
};

const struct xdg_toplevel_listener WindowViewBackend::XDGStable::s_toplevelListener = {
    // configure
    [](void* data, struct xdg_toplevel*, int32_t width, int32_t height, struct wl_array* states)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        window.resize(std::max(0, width), std::max(0, height));

        bool isFocused = false;
        bool isFullscreen = false;
        // FIXME: It would be nice if the following loop could use
        // wl_array_for_each, but at the time of writing it relies on
        // GCC specific extension to work properly:
        // https://gitlab.freedesktop.org/wayland/wayland/issues/34
        uint32_t* pos = static_cast<uint32_t*>(states->data);
        uint32_t* end = static_cast<uint32_t*>(states->data) + states->size;

        for (; pos < end; pos++) {
            uint32_t state = *pos;

            switch (state) {
            case XDG_TOPLEVEL_STATE_ACTIVATED:
                isFocused = true;
                break;
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                isFullscreen = true;
                break;
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
            case XDG_TOPLEVEL_STATE_RESIZING:
            default:
                break;
            }
        }

        if (isFocused)
            window.addActivityState(wpe_view_activity_state_focused);
        else
            window.removeActivityState(wpe_view_activity_state_focused);

        if (window.m_is_fullscreen != isFullscreen)
            window.onFullscreenChanged(isFullscreen);
    },
    // close
    [](void* data, struct xdg_toplevel*)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        window.removeActivityState(wpe_view_activity_state_visible | wpe_view_activity_state_focused | wpe_view_activity_state_in_window);
    },
#ifdef XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION
    // configure_bounds
    [](void*, struct xdg_toplevel*, int32_t, int32_t) { },
#endif
#ifdef XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION
    // wm_capabilities
    [](void*, struct xdg_toplevel*, struct wl_array*) { },
#endif
};

const struct zxdg_surface_v6_listener WindowViewBackend::XDGUnstable::s_surfaceListener = {
    // configure
    [](void*, struct zxdg_surface_v6* surface, uint32_t serial)
    {
        zxdg_surface_v6_ack_configure(surface, serial);
    },
};

const struct zxdg_toplevel_v6_listener WindowViewBackend::XDGUnstable::s_toplevelListener = {
    // configure
    [](void* data, struct zxdg_toplevel_v6*, int32_t width, int32_t height, struct wl_array* states)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        window.resize(std::max(0, width), std::max(0, height));

        bool isFocused = false;
        bool isFullscreen = false;
        // FIXME: It would be nice if the following loop could use
        // wl_array_for_each, but at the time of writing it relies on
        // GCC specific extension to work properly:
        // https://gitlab.freedesktop.org/wayland/wayland/issues/34
        uint32_t* pos = static_cast<uint32_t*>(states->data);
        uint32_t* end = static_cast<uint32_t*>(states->data) + states->size;

        for (; pos < end; pos++) {
            uint32_t state = *pos;

            switch (state) {
            case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
                isFocused = true;
                break;
            case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
                isFullscreen = true;
                break;
            case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
            case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
            default:
                break;
            }
        }

        if (isFocused)
            window.addActivityState(wpe_view_activity_state_focused);
        else
            window.removeActivityState(wpe_view_activity_state_focused);

        if (window.m_is_fullscreen != isFullscreen)
            window.onFullscreenChanged(isFullscreen);
    },
    // close
    [](void* data, struct zxdg_toplevel_v6*)
    {
        auto& window = *static_cast<WindowViewBackend*>(data);
        window.removeActivityState(wpe_view_activity_state_visible | wpe_view_activity_state_focused | wpe_view_activity_state_in_window);
    },
};

#if WPE_CHECK_VERSION(1, 11, 1)

bool WindowViewBackend::onDOMFullscreenRequest(void* data, bool fullscreen)
{
    auto& window = *static_cast<WindowViewBackend*>(data);
    if (window.m_waiting_fullscreen_notify)
        return false;

    if (fullscreen == window.m_is_fullscreen) {
        // Handle situations where DOM fullscreen requests are mixed with system fullscreen commands (e.g F11)
        window.dispatchFullscreenEvent();
        return true;
    }

    window.m_waiting_fullscreen_notify = true;
    if (window.m_xdg.toplevel) {
        if (fullscreen)
            xdg_toplevel_set_fullscreen(window.m_xdg.toplevel, nullptr);
        else
            xdg_toplevel_unset_fullscreen(window.m_xdg.toplevel);
    } else if (window.m_zxdg.toplevel) {
        if (fullscreen)
            zxdg_toplevel_v6_set_fullscreen(window.m_zxdg.toplevel, nullptr);
        else
            zxdg_toplevel_v6_unset_fullscreen(window.m_zxdg.toplevel);
    }

    return true;
}

void WindowViewBackend::dispatchFullscreenEvent()
{
    if (m_is_fullscreen)
        wpe_view_backend_dispatch_did_enter_fullscreen(backend());
    else
        wpe_view_backend_dispatch_did_exit_fullscreen(backend());
}

void WindowViewBackend::onFullscreenChanged(bool fullscreen)
{
    bool wasRequestedFromDOM = m_waiting_fullscreen_notify;
    m_waiting_fullscreen_notify= false;
    m_is_fullscreen = fullscreen;

    if (!fullscreen && !wasRequestedFromDOM)
        wpe_view_backend_dispatch_request_exit_fullscreen(backend());
    else if (wasRequestedFromDOM)
        dispatchFullscreenEvent();
}

#else

void WindowViewBackend::onFullscreenChanged(bool fullscreen)
{
    m_is_fullscreen = fullscreen;
}

#endif // WPE_CHECK_VERSION(1, 11, 1)

WindowViewBackend::WindowViewBackend(uint32_t width, uint32_t height)
    : ViewBackend(width, height)
{
    m_initialSize.width = width;
    m_initialSize.height = height;

    auto& connection = WaylandEGLConnection::singleton();
    if (!connection.display) {
        g_warning("WindowViewBackend: No Wayland EGL connection available");
        return;
    }

    if (connection.eglDisplay == EGL_NO_DISPLAY || !initialize(connection.eglDisplay)) {
        g_warning("WindowViewBackend: Could not initialize EGL display");
        return;
    }

    {
        auto* registry = wl_display_get_registry(connection.display);
        wl_registry_add_listener(registry, &s_registryListener, this);
        wl_display_roundtrip(connection.display);

        if (m_xdg.wm)
            xdg_wm_base_add_listener(m_xdg.wm, &XDGStable::s_wmListener, nullptr);
        else if (m_zxdg.shell)
            zxdg_shell_v6_add_listener(m_zxdg.shell, &XDGUnstable::s_shellListener, nullptr);

        if (m_seat)
            wl_seat_add_listener(m_seat, &s_seatListener, this);
    }

    m_eventSource = g_source_new(&EventSource::sourceFuncs, sizeof(EventSource));
    {
        auto& source = *reinterpret_cast<EventSource*>(m_eventSource);
        source.display = connection.display;

        source.pfd.fd = wl_display_get_fd(connection.display);
        source.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
        source.pfd.revents = 0;
        g_source_add_poll(&source.source, &source.pfd);

        g_source_set_priority(&source.source, G_PRIORITY_DEFAULT);
        g_source_set_can_recurse(&source.source, TRUE);
        g_source_attach(&source.source, g_main_context_get_thread_default());
    }

    m_surface = wl_compositor_create_surface(m_compositor);
    if (m_xdg.wm) {
        m_xdg.surface = xdg_wm_base_get_xdg_surface(m_xdg.wm, m_surface);
        xdg_surface_add_listener(m_xdg.surface, &XDGStable::s_surfaceListener, nullptr);
        m_xdg.toplevel = xdg_surface_get_toplevel(m_xdg.surface);
        if (m_xdg.toplevel) {
            xdg_toplevel_add_listener(m_xdg.toplevel, &XDGStable::s_toplevelListener, this);
            xdg_toplevel_set_title(m_xdg.toplevel, "WPE");
            wl_surface_commit(m_surface);
            addActivityState(wpe_view_activity_state_visible | wpe_view_activity_state_in_window);
        }
    } else if (m_zxdg.shell) {
        m_zxdg.surface = zxdg_shell_v6_get_xdg_surface(m_zxdg.shell, m_surface);
        zxdg_surface_v6_add_listener(m_zxdg.surface, &XDGUnstable::s_surfaceListener, nullptr);
        m_zxdg.toplevel = zxdg_surface_v6_get_toplevel(m_zxdg.surface);
        if (m_zxdg.toplevel) {
            zxdg_toplevel_v6_add_listener(m_zxdg.toplevel, &XDGUnstable::s_toplevelListener, this);
            zxdg_toplevel_v6_set_title(m_zxdg.toplevel, "WPE");
            wl_surface_commit(m_surface);
            addActivityState(wpe_view_activity_state_visible | wpe_view_activity_state_in_window);
        }
    }

    m_eglWindow = wl_egl_window_create(m_surface, m_width, m_height);

    auto createPlatformWindowSurface =
        reinterpret_cast<PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC>(eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT"));
    m_eglSurface = createPlatformWindowSurface(connection.eglDisplay, m_eglConfig, m_eglWindow, nullptr);
    if (!m_eglSurface) {
        g_warning("WindowViewBackend: Could not create EGL platform window surface");
        return;
    }

    if (!eglMakeCurrent(connection.eglDisplay, m_eglSurface, m_eglSurface, m_eglContext)) {
        g_warning("WindowViewBackend: Could not make EGL surface current");
        return;
    }

    imageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    {
        static const char* vertexShaderSource =
            "attribute vec2 pos;\n"
            "attribute vec2 texture;\n"
            "varying vec2 v_texture;\n"
            "void main() {\n"
            "  v_texture = texture;\n"
            "  gl_Position = vec4(pos, 0, 1);\n"
            "}\n";
        static const char* fragmentShaderSource =
            "precision mediump float;\n"
            "uniform sampler2D u_texture;\n"
            "varying vec2 v_texture;\n"
            "void main() {\n"
            "  gl_FragColor = texture2D(u_texture, v_texture);\n"
            "}\n";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);

        m_program = glCreateProgram();
        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);
        glLinkProgram(m_program);

        glBindAttribLocation(m_program, 0, "pos");
        glBindAttribLocation(m_program, 1, "texture");
        m_textureUniform = glGetUniformLocation(m_program, "u_texture");
    }

    createViewTexture();
}

WindowViewBackend::~WindowViewBackend()
{
    auto& connection = WaylandEGLConnection::singleton();

    if (m_eventSource) {
        g_source_destroy(m_eventSource);
        g_source_unref(m_eventSource);
    }

    if (m_xdg.toplevel)
        xdg_toplevel_destroy(m_xdg.toplevel);
    if (m_xdg.surface)
        xdg_surface_destroy(m_xdg.surface);

    if (m_zxdg.toplevel)
        zxdg_toplevel_v6_destroy(m_zxdg.toplevel);
    if (m_zxdg.surface)
        zxdg_surface_v6_destroy(m_zxdg.surface);

    if (m_surface)
        wl_surface_destroy(m_surface);

    if (m_eglWindow)
        wl_egl_window_destroy(m_eglWindow);

    if (m_xdg.wm)
        xdg_wm_base_destroy(m_xdg.wm);

    if (m_zxdg.shell)
        zxdg_shell_v6_destroy(m_zxdg.shell);

    if (m_seat)
        wl_seat_destroy(m_seat);

    if (m_compositor)
        wl_compositor_destroy(m_compositor);

    if (m_eglSurface)
        eglDestroySurface(connection.eglDisplay, m_eglSurface);

    if (m_display)
        wl_display_disconnect(m_display);

    deinitialize(connection.eglDisplay);
}

const struct wl_callback_listener WindowViewBackend::s_frameListener = {
    // frame
    [](void* data, struct wl_callback* callback, uint32_t)
    {
        if (callback)
            wl_callback_destroy(callback);

        auto& window = *static_cast<WindowViewBackend*>(data);
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(window.m_exportable);

        if (window.m_committedImage)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(window.m_exportable, window.m_committedImage);
        window.m_committedImage = nullptr;
    }
};

struct wpe_view_backend* WindowViewBackend::backend() const
{
    return m_exportable ? wpe_view_backend_exportable_fdo_get_view_backend(m_exportable) : nullptr;
}

void WindowViewBackend::createViewTexture()
{
    glGenTextures(1, &m_viewTexture);
    glBindTexture(GL_TEXTURE_2D, m_viewTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void WindowViewBackend::resize(uint32_t width, uint32_t height)
{
    if (!width)
        width = m_initialSize.width;
    if (!height)
        height = m_initialSize.height;

    if (width == m_width && height == m_height)
        return;

    m_width = width;
    m_height = height;

    wl_egl_window_resize(m_eglWindow, m_width, m_height, 0, 0);
    wpe_view_backend_dispatch_set_size(backend(), m_width, m_height);

    if (m_viewTexture)
        glDeleteTextures(1, &m_viewTexture);
    createViewTexture();
}

bool WindowViewBackend::initialize(EGLDisplay eglDisplay)
{
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
        if (!eglGetConfigs(eglDisplay, nullptr, 0, &count) || count < 1)
            return false;

        EGLConfig* configs = g_new0(EGLConfig, count);
        EGLint matched = 0;
        if (eglChooseConfig(eglDisplay, configAttributes, configs, count, &matched) && !!matched)
            m_eglConfig = configs[0];
        g_free(configs);
    }

    static const EGLint contextAttributes[3] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_eglContext = eglCreateContext(eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (!m_eglContext)
        return false;

    static struct wpe_view_backend_exportable_fdo_egl_client exportableClient = {
        // export_egl_image
        nullptr,
        // export_fdo_egl_image
        [](void* data, struct wpe_fdo_egl_exported_image* image)
        {
            static_cast<WindowViewBackend*>(data)->displayBuffer(image);
        },
#if WPE_FDO_CHECK_VERSION(1, 5, 0)
        // export_shm_buffer
        [](void* data, struct wpe_fdo_shm_exported_buffer* buffer)
        {
            static_cast<WindowViewBackend*>(data)->displayBuffer(buffer);
        },
        // padding
        nullptr, nullptr
#else
        // padding
        nullptr, nullptr, nullptr
#endif

    };
    m_exportable = wpe_view_backend_exportable_fdo_egl_create(&exportableClient, this, m_width, m_height);

#if WPE_CHECK_VERSION(1, 11, 1)
    wpe_view_backend_set_fullscreen_handler(backend(), onDOMFullscreenRequest, this);
#endif

    initializeAccessibility();

    return true;
}

void WindowViewBackend::deinitialize(EGLDisplay eglDisplay)
{
    m_inputClient = nullptr;

    if (m_eglContext)
        eglDestroyContext(eglDisplay, m_eglContext);

    if (m_exportable)
        wpe_view_backend_exportable_fdo_destroy(m_exportable);
}

void WindowViewBackend::displayBuffer(struct wpe_fdo_egl_exported_image* image)
{
    if (!m_eglContext)
        return;

    auto& connection = WaylandEGLConnection::singleton();
    eglMakeCurrent(connection.eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);

    glViewport(0, 0, m_width, m_height);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_viewTexture);
    imageTargetTexture2DOES(GL_TEXTURE_2D, wpe_fdo_egl_exported_image_get_egl_image(image));
    glUniform1i(m_textureUniform, 0);

    m_committedImage = image;

    static const GLfloat vertices[4][2] = {
        { -1.0, 1.0 },
        { 1.0, 1.0 },
        { -1.0, -1.0 },
        { 1.0, -1.0 },
    };

    static const GLfloat texturePos[4][2] = {
        { 0, 0 },
        { 1, 0 },
        { 0, 1 },
        { 1, 1 },
    };

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texturePos);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    struct wl_callback* callback = wl_surface_frame(m_surface);
    wl_callback_add_listener(callback, &s_frameListener, this);

    eglSwapBuffers(connection.eglDisplay, m_eglSurface);
}

#if WPE_FDO_CHECK_VERSION(1, 5, 0)
void WindowViewBackend::displayBuffer(struct wpe_fdo_shm_exported_buffer*)
{
    g_warning("WindowViewBackend: cannot yet handle wpe_fdo_shm_exported_buffer.");
}
#endif

void WindowViewBackend::handleKeyEvent(uint32_t key, uint32_t state, uint32_t time)
{
    uint32_t keysym = wpe_input_xkb_context_get_key_code(wpe_input_xkb_context_get_default(), key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
    if (!keysym)
        return;

    if (m_seatData.keyboard.target) {
        struct wpe_input_keyboard_event event = { time, keysym, key, !!state, modifiers() };
        dispatchInputKeyboardEvent(&event);
    }
}

uint32_t WindowViewBackend::modifiers() const
{
    uint32_t mask = m_seatData.keyboard.modifiers;
    if (m_seatData.pointer.object)
        mask |= m_seatData.pointer.modifiers;
    return mask;
}

} // namespace WPEToolingBackends
