//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WaylandWindow.h: Definition of the implementation of OSWindow for Wayland

#ifndef UTIL_WAYLAND_WINDOW_H
#define UTIL_WAYLAND_WINDOW_H

#include <poll.h>
#include <wayland-client.h>
#include <wayland-egl-core.h>

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include "util/OSWindow.h"
#include "util/util_export.h"

bool IsWaylandWindowAvailable();

class ANGLE_UTIL_EXPORT WaylandWindow : public OSWindow
{
  public:
    WaylandWindow(void *nativeDisplay = nullptr);
    ~WaylandWindow() override;

    void disableErrorMessageDialog() override;
    void destroy() override;

    void resetNativeWindow() override;
    EGLNativeWindowType getNativeWindow() const override;

    void setNativeDisplay(EGLNativeDisplayType display) override;
    EGLNativeDisplayType getNativeDisplay() const override;
    EGLenum getNativeDisplayPlatformType() const override;

    void messageLoop() override;

    void setMousePosition(int x, int y) override;
    bool setOrientation(int width, int height) override;
    bool setPosition(int x, int y) override;
    bool resize(int width, int height) override;
    void setVisible(bool isVisible) override;

    void signalTestEvent() override;

  private:
    struct wl_compositor *mCompositor;
    struct wl_surface *mSurface;
    struct wl_egl_window *mWindow;

    struct xdg_wm_base *mXdgWmBase    = nullptr;
    struct xdg_surface *mXdgSurface   = nullptr;
    struct xdg_toplevel *mXdgToplevel = nullptr;

    // Server-side decoration request (optional -- only used if the compositor
    // advertises zxdg_decoration_manager_v1). Compositors that grant SSD draw
    // the title bar + close/min/max buttons around the surface. Compositors
    // that refuse or don't advertise the protocol leave the window
    // decorationless (we don't draw client-side chrome).
    struct zxdg_decoration_manager_v1 *mDecorationManager   = nullptr;
    struct zxdg_toplevel_decoration_v1 *mToplevelDecoration = nullptr;

    // wl_shm placeholder maps the window before EGL claims the surface.
    // Retired in getNativeWindow() because direct wl_surface_attach once
    // EGL owns the surface is illegal per wayland-egl -- Mesa is lenient,
    // Nvidia's EGLStream backend crashes. WindowTest is the only non-EGL
    // OSWindow caller and never invokes getNativeWindow, so it keeps the
    // full placeholder lifecycle.
    struct wl_shm *mShm                  = nullptr;
    mutable struct wl_buffer *mShmBuffer = nullptr;
    mutable int32_t mShmBufferWidth      = 0;
    mutable int32_t mShmBufferHeight     = 0;
    mutable bool mEglHandoffComplete     = false;
    bool mVisible                        = false;

    bool ensureShmBuffer();
    void releaseShmPlaceholder() const;

    // Input -- bound on demand from wl_seat capabilities. May be null if the
    // compositor's seat doesn't advertise the corresponding capability (e.g.
    // headless setups with no keyboard or pointer).
    struct wl_seat *mSeat         = nullptr;
    struct wl_keyboard *mKeyboard = nullptr;
    struct wl_pointer *mPointer   = nullptr;

    // Tracked modifier state, applied to every key event we push.
    bool mShiftDown   = false;
    bool mControlDown = false;
    bool mAltDown     = false;
    bool mSuperDown   = false;

    // Last pointer position. The compositor delivers wl_fixed_t surface
    // coords; we round to int and reuse the value for the (X, Y) fields of
    // button events (which don't carry their own coordinates).
    int mPointerX = 0;
    int mPointerY = 0;

    struct
    {
        int32_t width;
        int32_t height;
    } mGeometry{};

    struct
    {
        int32_t width;
        int32_t height;
    } mWindowSize{};

    bool mWaitForConfigure = false;

    bool mFullscreen = false;
    bool mMaximized  = false;
    bool mResize     = false;
    bool mActivated  = false;

    // fds[0] = wl_display fd (compositor events)
    // fds[1] = key-repeat timerfd (lazily created in initializeImpl)
    struct pollfd fds[2];

    // Key auto-repeat. Wayland makes this the client's responsibility -- the
    // compositor only sends one press + one release per physical key event,
    // and a separate `repeat_info` hint with the user's preferred rate/delay.
    // We mirror the X11/Win32 contract by re-pushing EVENT_KEY_PRESSED via a
    // timerfd, so downstream samples behave the same across backends.
    int mRepeatTimerFd   = -1;
    uint32_t mRepeatKey  = 0;    // Linux keycode currently being repeated; 0 = none
    int32_t mRepeatRate  = 25;   // chars/sec (libinput default if compositor doesn't tell us)
    int32_t mRepeatDelay = 600;  // ms before first repeat

    static void RegistryHandleGlobal(void *data,
                                     struct wl_registry *registry,
                                     uint32_t name,
                                     const char *interface,
                                     uint32_t version);
    static void RegistryHandleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name);

    bool initializeImpl(const std::string &name, int width, int height) override;

    static const struct wl_registry_listener registryListener;

    static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);

    static const struct xdg_wm_base_listener xdg_wm_base_listener;

    static void handle_xdg_surface_configure(void *data,
                                             struct xdg_surface *xdg_surface,
                                             uint32_t serial);

    static const struct xdg_surface_listener xdg_surface_listener;

    static void handle_toplevel_configure(void *data,
                                          struct xdg_toplevel *toplevel,
                                          int32_t width,
                                          int32_t height,
                                          struct wl_array *states);

    static void handle_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel);

    static const struct xdg_toplevel_listener xdg_toplevel_listener;

    // zxdg_toplevel_decoration_v1: tells us what mode the compositor actually
    // granted in response to set_mode(SERVER_SIDE). We don't change behavior
    // either way -- the listener exists to satisfy the protocol contract and
    // to make a granted-CLIENT_SIDE response observable in logs.
    static void decoration_configure(void *data,
                                     struct zxdg_toplevel_decoration_v1 *decoration,
                                     uint32_t mode);
    static const struct zxdg_toplevel_decoration_v1_listener decoration_listener;

    // wl_seat: routes capability changes to keyboard / pointer setup.
    static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps);
    static void seat_name(void *data, struct wl_seat *seat, const char *name);
    static const struct wl_seat_listener seat_listener;

    // wl_keyboard: keymap is ignored (we map raw Linux codes directly).
    // key/modifiers drive key events; leave disarms any in-flight key repeat.
    // Focus events come from xdg_toplevel.state.activated, not from
    // wl_keyboard enter/leave (see handle_toplevel_configure).
    static void keyboard_keymap(void *data,
                                struct wl_keyboard *keyboard,
                                uint32_t format,
                                int32_t fd,
                                uint32_t size);
    static void keyboard_enter(void *data,
                               struct wl_keyboard *keyboard,
                               uint32_t serial,
                               struct wl_surface *surface,
                               struct wl_array *keys);
    static void keyboard_leave(void *data,
                               struct wl_keyboard *keyboard,
                               uint32_t serial,
                               struct wl_surface *surface);
    static void keyboard_key(void *data,
                             struct wl_keyboard *keyboard,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t key,
                             uint32_t state);
    static void keyboard_modifiers(void *data,
                                   struct wl_keyboard *keyboard,
                                   uint32_t serial,
                                   uint32_t mods_depressed,
                                   uint32_t mods_latched,
                                   uint32_t mods_locked,
                                   uint32_t group);
    static void keyboard_repeat_info(void *data,
                                     struct wl_keyboard *keyboard,
                                     int32_t rate,
                                     int32_t delay);
    static const struct wl_keyboard_listener keyboard_listener;

    // wl_pointer: enter/leave drive mouse enter/leave; motion drives mouse
    // moved; button drives press/release; axis drives wheel.
    static void pointer_enter(void *data,
                              struct wl_pointer *pointer,
                              uint32_t serial,
                              struct wl_surface *surface,
                              wl_fixed_t x,
                              wl_fixed_t y);
    static void pointer_leave(void *data,
                              struct wl_pointer *pointer,
                              uint32_t serial,
                              struct wl_surface *surface);
    static void pointer_motion(void *data,
                               struct wl_pointer *pointer,
                               uint32_t time,
                               wl_fixed_t x,
                               wl_fixed_t y);
    static void pointer_button(void *data,
                               struct wl_pointer *pointer,
                               uint32_t serial,
                               uint32_t time,
                               uint32_t button,
                               uint32_t state);
    static void pointer_axis(void *data,
                             struct wl_pointer *pointer,
                             uint32_t time,
                             uint32_t axis,
                             wl_fixed_t value);
    static void pointer_frame(void *data, struct wl_pointer *pointer);
    static void pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t source);
    static void pointer_axis_stop(void *data,
                                  struct wl_pointer *pointer,
                                  uint32_t time,
                                  uint32_t axis);
    static void pointer_axis_discrete(void *data,
                                      struct wl_pointer *pointer,
                                      uint32_t axis,
                                      int32_t discrete);
    static const struct wl_pointer_listener pointer_listener;
};

#endif  // UTIL_WAYLAND_WINDOW_H
