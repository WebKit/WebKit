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

#pragma once

#include "ViewBackend.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include <glib.h>
#include <unordered_map>

typedef void* EGLSurface;
typedef struct wl_egl_window  *EGLNativeWindowType;

namespace WPEToolingBackends {

class WindowViewBackend final : public ViewBackend {
public:
    WindowViewBackend(uint32_t width, uint32_t height);
    virtual ~WindowViewBackend();

private:
    void displayBuffer(struct wpe_fdo_egl_exported_image*) override;

    static const struct wl_registry_listener s_registryListener;
    static const struct zxdg_shell_v6_listener s_xdgWmBaseListener;
    static const struct wl_pointer_listener s_pointerListener;
    static const struct wl_keyboard_listener s_keyboardListener;
    static const struct wl_touch_listener s_touchListener;
    static const struct wl_seat_listener s_seatListener;
    static const struct wl_callback_listener s_frameListener;
    static const struct zxdg_surface_v6_listener s_xdgSurfaceListener;
    static const struct zxdg_toplevel_v6_listener s_xdgToplevelListener;

    void handleKeyEvent(uint32_t key, uint32_t state, uint32_t time);
    uint32_t modifiers() const;

    struct SeatData {
        struct {
            struct wl_pointer* object { nullptr };
            struct wl_surface* target { nullptr };
            std::pair<int, int> coords { 0, 0 };
            uint32_t button { 0 };
            uint32_t state { 0 };
            uint32_t modifiers { 0 };
        } pointer;

        struct {
            struct wl_keyboard* object { nullptr };
            struct wl_surface* target { nullptr };
            uint32_t modifiers { 0 };
        } keyboard;

        struct {
            struct wl_touch* object { nullptr };
            struct wpe_input_touch_event_raw points[10];
            bool tracking { false };
        } touch;

        struct {
            int32_t rate { 0 };
            int32_t delay { 0 };
        } repeatInfo;

        struct {
            uint32_t key { 0 };
            uint32_t time { 0 };
            uint32_t state { 0 };
            uint32_t eventSource { 0 };
        } repeatData;

    } m_seatData;

    struct wl_display* m_display { nullptr };
    struct wl_compositor* m_compositor { nullptr };
    struct zxdg_shell_v6* m_xdg { nullptr };
    struct wl_seat* m_seat { nullptr };
    GSource* m_eventSource { nullptr };
    struct wl_surface* m_surface { nullptr };
    struct zxdg_surface_v6* m_xdgSurface { nullptr };
    struct zxdg_toplevel_v6* m_xdgToplevel { nullptr };
    struct wl_egl_window* m_eglWindow { nullptr };
    EGLSurface m_eglSurface { nullptr };
    unsigned m_program { 0 };
    unsigned m_textureUniform { 0 };
    unsigned m_viewTexture { 0 };
    struct wpe_fdo_egl_exported_image* m_committedImage { nullptr };
};

} // WPEToolingBackends
