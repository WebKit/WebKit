//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WaylandWindow.cpp: Implementation of OSWindow for Wayland

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "util/linux/wayland/WaylandWindow.h"

#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "common/debug.h"

// C++ workaround for Wayland macro
#define WL_ARRAY_FOR_EACH(pos, array, type) \
    for (pos = (type)(array)->data;         \
         (const char *)pos < ((const char *)(array)->data + (array)->size); (pos)++)

// The wl_display is intentionally process-scoped: per Wayland convention the
// connection lives for the lifetime of the process. Tying it to per-WaylandWindow
// lifetime breaks multi-window apps and corrupts Mesa Vulkan WSI internal state
// that retains wl_display proxy wrappers across vkCreateSwapchainKHR calls.
static struct wl_display *gDisplay = nullptr;
static bool gLocalDisplay          = false;

namespace
{
// Forward declaration so messageLoop (defined earlier in this file than the
// keymap table at the bottom) can call it for synthetic key-repeat events.
angle::KeyType LinuxKeyCodeToKey(uint32_t code);

void DisconnectGDisplayAtExit()
{
    if (gLocalDisplay && gDisplay)
    {
        wl_display_flush(gDisplay);
        wl_display_disconnect(gDisplay);
        gDisplay      = nullptr;
        gLocalDisplay = false;
    }
}
}  // namespace

WaylandWindow::WaylandWindow(void *nativeDisplay)
    : mCompositor{nullptr}, mSurface{nullptr}, mWindow{nullptr}
{
    if (nativeDisplay != nullptr)
    {
        if (gDisplay != nullptr && nativeDisplay != gDisplay)
        {
            if (gLocalDisplay)
            {
                wl_display_disconnect(gDisplay);
                gLocalDisplay = false;
            }
            fprintf(stderr, "WaylandWindow::WaylandWindow: Display reassignment\n");
        }
        gDisplay = reinterpret_cast<wl_display *>(nativeDisplay);
    }
    else if (!gDisplay)
    {
        gDisplay = wl_display_connect(nullptr);
        if (gDisplay)
        {
            gLocalDisplay = true;
            std::atexit(DisconnectGDisplayAtExit);
        }
    }
}

WaylandWindow::~WaylandWindow()
{
    destroy();
    // Do not disconnect gDisplay here: see the comment on the static above.
    // The atexit handler installed when we connected handles cleanup at
    // process exit so Valgrind doesn't see a leak.
}

void WaylandWindow::RegistryHandleGlobal(void *data,
                                         struct wl_registry *registry,
                                         uint32_t name,
                                         const char *interface,
                                         uint32_t version)
{
    WaylandWindow *vc = reinterpret_cast<WaylandWindow *>(data);

    if (strcmp(interface, xdg_wm_base_interface.name) == 0)
    {
        vc->mXdgWmBase = static_cast<xdg_wm_base *>(wl_registry_bind(
            registry, name, &xdg_wm_base_interface, std::min(static_cast<uint32_t>(3), version)));
        xdg_wm_base_add_listener(vc->mXdgWmBase, &xdg_wm_base_listener, data);
    }
    else if (strcmp(interface, wl_compositor_interface.name) == 0)
    {
        // v4+ is needed for wl_surface.damage_buffer (opcode 9). Cap at 4 since
        // we don't use later additions (v5 offset / v6 preferred_buffer_*).
        vc->mCompositor = static_cast<wl_compositor *>(wl_registry_bind(
            registry, name, &wl_compositor_interface, std::min(static_cast<uint32_t>(4), version)));
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0)
    {
        // v5+ gives us pointer.frame / axis_discrete; clamp to 5 since that
        // covers every common compositor and our pointer listener stops there.
        vc->mSeat = static_cast<wl_seat *>(wl_registry_bind(
            registry, name, &wl_seat_interface, std::min(static_cast<uint32_t>(5), version)));
        wl_seat_add_listener(vc->mSeat, &seat_listener, vc);
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        // Bound for the setVisible(true) placeholder buffer. Version 1 is
        // sufficient -- we only use create_pool/create_buffer.
        vc->mShm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    }
    else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
    {
        // Optional global. Compositors that don't advertise it leave this
        // null; the window is then decorationless.
        vc->mDecorationManager = static_cast<zxdg_decoration_manager_v1 *>(
            wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
    }
}

void WaylandWindow::RegistryHandleGlobalRemove(void *data,
                                               struct wl_registry *registry,
                                               uint32_t name)
{}

const struct wl_registry_listener WaylandWindow::registryListener = {
    WaylandWindow::RegistryHandleGlobal, WaylandWindow::RegistryHandleGlobalRemove};

bool WaylandWindow::initializeImpl(const std::string &name, int width, int height)
{
    destroy();

    if (!gDisplay)
    {
        gDisplay      = wl_display_connect(nullptr);
        gLocalDisplay = true;
        if (!gDisplay)
        {
            return false;
        }
    }

    // Get registry of display
    struct wl_registry *registry = wl_display_get_registry(gDisplay);
    wl_registry_add_listener(registry, &registryListener, this);

    // Round-trip to get globals
    wl_display_roundtrip(gDisplay);
    if (!mCompositor)
    {
        return false;
    }

    // We don't need this anymore; we don't track output changes
    wl_registry_destroy(registry);

    mSurface = wl_compositor_create_surface(mCompositor);
    if (!mSurface)
    {
        return false;
    }

    {
        mXdgSurface = xdg_wm_base_get_xdg_surface(mXdgWmBase, mSurface);
        xdg_surface_add_listener(mXdgSurface, &xdg_surface_listener, this);

        mXdgToplevel = xdg_surface_get_toplevel(mXdgSurface);
        xdg_toplevel_add_listener(mXdgToplevel, &xdg_toplevel_listener, this);
        xdg_toplevel_set_app_id(mXdgToplevel, name.c_str());
        xdg_toplevel_set_title(mXdgToplevel, name.c_str());

        // Request server-side decorations. Must happen before the initial
        // surface commit so the first configure carries the granted mode.
        // Null mDecorationManager means the compositor doesn't expose the
        // protocol; the window is then decorationless.
        if (mDecorationManager)
        {
            mToplevelDecoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
                mDecorationManager, mXdgToplevel);
            zxdg_toplevel_decoration_v1_add_listener(mToplevelDecoration, &decoration_listener,
                                                     this);
            zxdg_toplevel_decoration_v1_set_mode(mToplevelDecoration,
                                                 ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        }

        mWaitForConfigure = true;
        wl_surface_commit(mSurface);
        while (mWaitForConfigure)
        {
            if (wl_display_dispatch(gDisplay) < 0)
            {
                break;
            }
        }
    }

    mWindow = wl_egl_window_create(mSurface, width, height);
    if (!mWindow)
    {
        return false;
    }

    fds[0] = {wl_display_get_fd(gDisplay), POLLIN, 0};
    // Lazy-create the key-repeat timerfd (one per WaylandWindow). Closed in
    // destroy(). Non-blocking + cloexec is the Wayland-client convention.
    if (mRepeatTimerFd < 0)
    {
        mRepeatTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    }
    fds[1] = {mRepeatTimerFd, POLLIN, 0};

    mY      = 0;
    mX      = 0;
    mWidth  = width;
    mHeight = height;

    return true;
}

void WaylandWindow::disableErrorMessageDialog() {}

void WaylandWindow::destroy()
{
    // Tear down in reverse-dependency order so children are released before
    // their parents: input objects -> wl_egl_window -> wl_surface chain ->
    // wl_seat / xdg_wm_base / wl_compositor (the registry globals).
    if (mKeyboard)
    {
        wl_keyboard_release(mKeyboard);
        mKeyboard = nullptr;
    }
    if (mPointer)
    {
        wl_pointer_release(mPointer);
        mPointer = nullptr;
    }
    if (mSeat)
    {
        wl_seat_destroy(mSeat);
        mSeat = nullptr;
    }

    if (mRepeatTimerFd >= 0)
    {
        close(mRepeatTimerFd);
        mRepeatTimerFd = -1;
        mRepeatKey     = 0;
    }

    releaseShmPlaceholder();
    mEglHandoffComplete = false;

    if (mShm)
    {
        wl_shm_destroy(mShm);
        mShm = nullptr;
    }

    if (mWindow)
    {
        wl_egl_window_destroy(mWindow);
        mWindow = nullptr;
    }

    // The toplevel_decoration is a child of xdg_toplevel; release it first.
    if (mToplevelDecoration)
    {
        zxdg_toplevel_decoration_v1_destroy(mToplevelDecoration);
        mToplevelDecoration = nullptr;
    }

    if (mXdgToplevel)
    {
        xdg_toplevel_destroy(mXdgToplevel);
        mXdgToplevel = nullptr;
    }

    if (mXdgSurface)
    {
        xdg_surface_destroy(mXdgSurface);
        mXdgSurface = nullptr;
    }

    if (mSurface)
    {
        wl_surface_destroy(mSurface);
        mSurface = nullptr;
    }

    if (mDecorationManager)
    {
        zxdg_decoration_manager_v1_destroy(mDecorationManager);
        mDecorationManager = nullptr;
    }

    if (mXdgWmBase)
    {
        xdg_wm_base_destroy(mXdgWmBase);
        mXdgWmBase = nullptr;
    }

    if (mCompositor)
    {
        wl_compositor_destroy(mCompositor);
        mCompositor = nullptr;
    }
}

void WaylandWindow::resetNativeWindow() {}

void WaylandWindow::setNativeDisplay(EGLNativeDisplayType display)
{
    if (gDisplay && gDisplay != display)
    {
        fprintf(stderr, "WaylandWindow::setNativeDisplay: Display already acquired\n");
        return;
    }
    gDisplay = reinterpret_cast<wl_display *>(display);
}

EGLNativeWindowType WaylandWindow::getNativeWindow() const
{
    // First call hands the wl_surface to EGL (the caller passes this to
    // eglCreateWindowSurface or an equivalent path used by dEQP and the
    // perf tests). Retire the shm placeholder so subsequent setVisible/
    // resize stop touching the surface behind EGL's back.
    if (!mEglHandoffComplete)
    {
        mEglHandoffComplete = true;
        releaseShmPlaceholder();
    }
    return reinterpret_cast<EGLNativeWindowType>(mWindow);
}

EGLNativeDisplayType WaylandWindow::getNativeDisplay() const
{
    return reinterpret_cast<EGLNativeDisplayType>(gDisplay);
}

EGLenum WaylandWindow::getNativeDisplayPlatformType() const
{
    return EGL_PLATFORM_WAYLAND_EXT;
}

void WaylandWindow::messageLoop()
{
    while (wl_display_prepare_read(gDisplay) != 0)
    {
        wl_display_dispatch_pending(gDisplay);
    }
    if (wl_display_flush(gDisplay) < 0 && errno != EAGAIN)
    {
        wl_display_cancel_read(gDisplay);
        return;
    }

    // Poll both wayland and the repeat timer. Timeout 0 = non-blocking; this
    // function is invoked from a tight render loop, so we never want to wait.
    const nfds_t nfds = (mRepeatTimerFd >= 0) ? 2 : 1;
    const int ret     = poll(fds, nfds, 0);

    if (ret > 0 && (fds[0].revents & POLLIN))
    {
        wl_display_read_events(gDisplay);
        wl_display_dispatch_pending(gDisplay);
    }
    else
    {
        wl_display_cancel_read(gDisplay);
    }

    if (ret > 0 && nfds > 1 && (fds[1].revents & POLLIN) && mRepeatKey != 0)
    {
        uint64_t expirations = 0;
        if (read(mRepeatTimerFd, &expirations, sizeof(expirations)) == sizeof(expirations))
        {
            // Cap to a sane upper bound so a stalled loop catching up doesn't
            // dump hundreds of synthetic key events at once.
            const uint64_t kMaxBurst = 8;
            const uint64_t n         = std::min(expirations, kMaxBurst);
            for (uint64_t i = 0; i < n; ++i)
            {
                Event event{};
                event.Type        = Event::EVENT_KEY_PRESSED;
                event.Key.Code    = LinuxKeyCodeToKey(mRepeatKey);
                event.Key.Shift   = mShiftDown;
                event.Key.Control = mControlDown;
                event.Key.Alt     = mAltDown;
                event.Key.System  = mSuperDown;
                pushEvent(std::move(event));
            }
        }
    }
}

void WaylandWindow::setMousePosition(int x, int y) {}

bool WaylandWindow::setOrientation(int /*width*/, int /*height*/)
{
    UNIMPLEMENTED();
    return false;
}

bool WaylandWindow::setPosition(int /*x*/, int /*y*/)
{
    // Wayland forbids client-initiated absolute window positioning; the
    // compositor places toplevels. Returning true keeps the OSWindow contract
    // for callers that don't gate on the result.
    return true;
}

bool WaylandWindow::resize(int width, int height)
{
    // mWindow may be null during the initial xdg_toplevel configure (which can
    // arrive before initializeImpl creates the wl_egl_window).
    if (mWindow)
    {
        wl_egl_window_resize(mWindow, width, height, 0, 0);
    }

    // pushEvent below updates mWidth/mHeight via OSWindow::pushEvent; do it now
    // so ensureShmBuffer (which reads mWidth/mHeight) sees the new dimensions.
    const bool sizeChanged = (width != mWidth || height != mHeight);
    if (sizeChanged)
    {
        Event event{};
        event.Type        = Event::EVENT_RESIZED;
        event.Size.Width  = width;
        event.Size.Height = height;
        pushEvent(std::move(event));
    }

    // Reallocate the placeholder for the new size, only while we still own
    // the surface. After handoff mShmBuffer is null and this block is
    // skipped; EGL handles resize via wl_egl_window_resize above.
    if (sizeChanged && mVisible && mShmBuffer && mSurface && mShm)
    {
        wl_buffer_destroy(mShmBuffer);
        mShmBuffer       = nullptr;
        mShmBufferWidth  = 0;
        mShmBufferHeight = 0;
        if (ensureShmBuffer())
        {
            wl_surface_attach(mSurface, mShmBuffer, 0, 0);
            wl_surface_damage_buffer(mSurface, 0, 0, mShmBufferWidth, mShmBufferHeight);
            wl_surface_commit(mSurface);
            wl_display_flush(gDisplay);
        }
    }

    return true;
}

bool WaylandWindow::ensureShmBuffer()
{
    if (mShmBuffer != nullptr)
    {
        return true;
    }
    if (!mShm)
    {
        // Compositor didn't advertise wl_shm; nothing we can attach.
        return false;
    }

    const int32_t width  = mWidth > 0 ? mWidth : 1;
    const int32_t height = mHeight > 0 ? mHeight : 1;
    const int32_t stride = width * 4;
    const off_t size     = static_cast<off_t>(stride) * height;

    int fd = memfd_create("waylandwindow-shm", MFD_CLOEXEC);
    if (fd < 0)
    {
        return false;
    }
    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        return false;
    }

    void *data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        close(fd);
        return false;
    }

    // Fill with opaque dark gray so a surface that never gets EGL-rendered
    // still has visible pixels. WL_SHM_FORMAT_ARGB8888 specifies the byte
    // sequence B, G, R, A in memory regardless of CPU endianness, so we
    // write bytes directly rather than a uint32_t.
    static constexpr uint8_t kPixel[4] = {0x20, 0x20, 0x20, 0xFF};  // B, G, R, A
    uint8_t *bytes                     = static_cast<uint8_t *>(data);
    for (int32_t i = 0; i < width * height; ++i)
    {
        std::memcpy(bytes + i * 4, kPixel, sizeof(kPixel));
    }
    munmap(data, size);

    struct wl_shm_pool *pool = wl_shm_create_pool(mShm, fd, static_cast<int32_t>(size));
    mShmBuffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    if (!mShmBuffer)
    {
        return false;
    }
    mShmBufferWidth  = width;
    mShmBufferHeight = height;
    return true;
}

void WaylandWindow::releaseShmPlaceholder() const
{
    if (mShmBuffer)
    {
        wl_buffer_destroy(mShmBuffer);
        mShmBuffer       = nullptr;
        mShmBufferWidth  = 0;
        mShmBufferHeight = 0;
    }
}

void WaylandWindow::setVisible(bool isVisible)
{
    // Idempotent: skip the protocol round-trip if state already matches.
    if (mVisible == isVisible)
    {
        return;
    }

    if (!mSurface || !gDisplay)
    {
        return;
    }

    // After EGL handoff the swap chain owns the wl_surface; only the
    // visibility flag can change here. The compositor keeps showing
    // EGL's most recent frame until eglSwapBuffers replaces it.
    if (mEglHandoffComplete)
    {
        mVisible = isVisible;
        return;
    }

    if (isVisible)
    {
        // Compositors ignore xdg_toplevel until they see a buffered
        // surface, so map by attaching the placeholder + committing.
        if (!ensureShmBuffer())
        {
            return;
        }
        wl_surface_attach(mSurface, mShmBuffer, 0, 0);
        wl_surface_damage_buffer(mSurface, 0, 0, mShmBufferWidth, mShmBufferHeight);
        wl_surface_commit(mSurface);
    }
    else
    {
        // Unmap: NULL buffer attach + commit. The compositor removes the
        // surface from screen until a new buffer is attached.
        wl_surface_attach(mSurface, nullptr, 0, 0);
        wl_surface_commit(mSurface);
    }

    mVisible = isVisible;
    wl_display_flush(gDisplay);
}

void WaylandWindow::signalTestEvent()
{
    Event event{};
    event.Type = Event::EVENT_TEST;
    pushEvent(std::move(event));
}

void WaylandWindow::xdg_wm_base_ping(void * /* data */,
                                     struct xdg_wm_base *xdg_wm_base,
                                     uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

const struct xdg_wm_base_listener WaylandWindow::xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

void WaylandWindow::handle_xdg_surface_configure(void *data,
                                                 struct xdg_surface *xdg_surface,
                                                 uint32_t serial)
{
    auto *w = static_cast<WaylandWindow *>(data);
    xdg_surface_ack_configure(xdg_surface, serial);
    w->mWaitForConfigure = false;
}

const struct xdg_surface_listener WaylandWindow::xdg_surface_listener = {
    .configure = handle_xdg_surface_configure};

void WaylandWindow::handle_toplevel_configure(void *data,
                                              struct xdg_toplevel * /* toplevel */,
                                              int32_t width,
                                              int32_t height,
                                              struct wl_array *states)
{
    auto *w = static_cast<WaylandWindow *>(data);

    // Snapshot mActivated before recomputing so GAINED/LOST fire exactly once
    // per real focus change. The activated bit is the WM-level focus signal;
    // wl_keyboard enter/leave is input-routing and is unreliable (headless
    // compositors don't advertise a keyboard capability).
    const bool wasActivated = w->mActivated;
    w->mFullscreen          = false;
    w->mMaximized           = false;
    w->mResize              = false;
    w->mActivated           = false;

    const uint32_t *state;
    WL_ARRAY_FOR_EACH(state, states, const uint32_t *)
    {
        switch (*state)
        {
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                w->mFullscreen = true;
                break;
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
                w->mMaximized = true;
                break;
            case XDG_TOPLEVEL_STATE_RESIZING:
                // EVENT_RESIZED is emitted from WaylandWindow::resize() below,
                // using the post-clamp mGeometry -- the raw configure (width,
                // height) here can be (0, 0) meaning "client picks".
                w->mResize = true;
                break;
            case XDG_TOPLEVEL_STATE_ACTIVATED:
                w->mActivated = true;
                break;
        }
    }

    if (w->mActivated != wasActivated)
    {
        Event event{};
        event.Type = w->mActivated ? Event::EVENT_GAINED_FOCUS : Event::EVENT_LOST_FOCUS;
        w->pushEvent(std::move(event));
    }

    if (width > 0 && height > 0)
    {
        if (!w->mFullscreen && !w->mMaximized)
        {
            w->mWindowSize.width  = width;
            w->mWindowSize.height = height;
        }
        w->mGeometry.width  = width;
        w->mGeometry.height = height;
    }
    else if (!w->mFullscreen && !w->mMaximized)
    {
        w->mGeometry.width  = w->mWindowSize.width;
        w->mGeometry.height = w->mWindowSize.height;
    }

    w->resize(w->mGeometry.width, w->mGeometry.height);
}

void WaylandWindow::handle_toplevel_close(void *data, struct xdg_toplevel * /* xdg_toplevel */)
{
    auto *w = static_cast<WaylandWindow *>(data);

    Event event{};
    event.Type = Event::EVENT_CLOSED;
    w->pushEvent(std::move(event));
}

const struct xdg_toplevel_listener WaylandWindow::xdg_toplevel_listener = {
    .configure        = handle_toplevel_configure,
    .close            = handle_toplevel_close,
    .configure_bounds = nullptr,
    .wm_capabilities  = nullptr,
};

void WaylandWindow::decoration_configure(void * /*data*/,
                                         struct zxdg_toplevel_decoration_v1 * /*decoration*/,
                                         uint32_t mode)
{
    // We only ever request SERVER_SIDE. A CLIENT_SIDE response leaves the
    // window decorationless (we don't draw our own chrome) -- log so the
    // missing decorations are debuggable rather than mysterious.
    if (mode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)
    {
        fprintf(stderr,
                "WaylandWindow: compositor refused SERVER_SIDE decorations "
                "(mode=%u); window will be undecorated.\n",
                mode);
    }
}

const struct zxdg_toplevel_decoration_v1_listener WaylandWindow::decoration_listener = {
    .configure = decoration_configure,
};

// --- Input ----------------------------------------------------------------
//
// Wayland delivers key events as raw Linux input-event-codes (KEY_*) and
// mouse button events as raw codes (BTN_*). We translate directly to ANGLE's
// KeyType / MouseButtonType enums and intentionally skip xkbcommon-based
// keymap translation: this code path is for key identification, not text
// entry. EVENT_TEXT_ENTERED is unimplemented (matches the X11 backend).

namespace
{
angle::KeyType LinuxKeyCodeToKey(uint32_t code)
{
    switch (code)
    {
        case KEY_ESC:
            return angle::KeyType::ESCAPE;
        case KEY_1:
            return angle::KeyType::NUM1;
        case KEY_2:
            return angle::KeyType::NUM2;
        case KEY_3:
            return angle::KeyType::NUM3;
        case KEY_4:
            return angle::KeyType::NUM4;
        case KEY_5:
            return angle::KeyType::NUM5;
        case KEY_6:
            return angle::KeyType::NUM6;
        case KEY_7:
            return angle::KeyType::NUM7;
        case KEY_8:
            return angle::KeyType::NUM8;
        case KEY_9:
            return angle::KeyType::NUM9;
        case KEY_0:
            return angle::KeyType::NUM0;
        case KEY_MINUS:
            return angle::KeyType::DASH;
        case KEY_EQUAL:
            return angle::KeyType::EQUAL;
        case KEY_BACKSPACE:
            return angle::KeyType::BACK;
        case KEY_TAB:
            return angle::KeyType::TAB;
        case KEY_Q:
            return angle::KeyType::Q;
        case KEY_W:
            return angle::KeyType::W;
        case KEY_E:
            return angle::KeyType::E;
        case KEY_R:
            return angle::KeyType::R;
        case KEY_T:
            return angle::KeyType::T;
        case KEY_Y:
            return angle::KeyType::Y;
        case KEY_U:
            return angle::KeyType::U;
        case KEY_I:
            return angle::KeyType::I;
        case KEY_O:
            return angle::KeyType::O;
        case KEY_P:
            return angle::KeyType::P;
        case KEY_LEFTBRACE:
            return angle::KeyType::LBRACKET;
        case KEY_RIGHTBRACE:
            return angle::KeyType::RBRACKET;
        case KEY_ENTER:
            return angle::KeyType::RETURN;
        case KEY_LEFTCTRL:
            return angle::KeyType::LCONTROL;
        case KEY_A:
            return angle::KeyType::A;
        case KEY_S:
            return angle::KeyType::S;
        case KEY_D:
            return angle::KeyType::D;
        case KEY_F:
            return angle::KeyType::F;
        case KEY_G:
            return angle::KeyType::G;
        case KEY_H:
            return angle::KeyType::H;
        case KEY_J:
            return angle::KeyType::J;
        case KEY_K:
            return angle::KeyType::K;
        case KEY_L:
            return angle::KeyType::L;
        case KEY_SEMICOLON:
            return angle::KeyType::SEMICOLON;
        case KEY_APOSTROPHE:
            return angle::KeyType::QUOTE;
        case KEY_GRAVE:
            return angle::KeyType::TILDE;
        case KEY_LEFTSHIFT:
            return angle::KeyType::LSHIFT;
        case KEY_BACKSLASH:
            return angle::KeyType::BACKSLASH;
        case KEY_Z:
            return angle::KeyType::Z;
        case KEY_X:
            return angle::KeyType::X;
        case KEY_C:
            return angle::KeyType::C;
        case KEY_V:
            return angle::KeyType::V;
        case KEY_B:
            return angle::KeyType::B;
        case KEY_N:
            return angle::KeyType::N;
        case KEY_M:
            return angle::KeyType::M;
        case KEY_COMMA:
            return angle::KeyType::COMMA;
        case KEY_DOT:
            return angle::KeyType::PERIOD;
        case KEY_SLASH:
            return angle::KeyType::SLASH;
        case KEY_RIGHTSHIFT:
            return angle::KeyType::RSHIFT;
        case KEY_KPASTERISK:
            return angle::KeyType::MULTIPLY;
        case KEY_LEFTALT:
            return angle::KeyType::LALT;
        case KEY_SPACE:
            return angle::KeyType::SPACE;
        case KEY_F1:
            return angle::KeyType::F1;
        case KEY_F2:
            return angle::KeyType::F2;
        case KEY_F3:
            return angle::KeyType::F3;
        case KEY_F4:
            return angle::KeyType::F4;
        case KEY_F5:
            return angle::KeyType::F5;
        case KEY_F6:
            return angle::KeyType::F6;
        case KEY_F7:
            return angle::KeyType::F7;
        case KEY_F8:
            return angle::KeyType::F8;
        case KEY_F9:
            return angle::KeyType::F9;
        case KEY_F10:
            return angle::KeyType::F10;
        case KEY_KP7:
            return angle::KeyType::NUMPAD7;
        case KEY_KP8:
            return angle::KeyType::NUMPAD8;
        case KEY_KP9:
            return angle::KeyType::NUMPAD9;
        case KEY_KPMINUS:
            return angle::KeyType::SUBTRACT;
        case KEY_KP4:
            return angle::KeyType::NUMPAD4;
        case KEY_KP5:
            return angle::KeyType::NUMPAD5;
        case KEY_KP6:
            return angle::KeyType::NUMPAD6;
        case KEY_KPPLUS:
            return angle::KeyType::ADD;
        case KEY_KP1:
            return angle::KeyType::NUMPAD1;
        case KEY_KP2:
            return angle::KeyType::NUMPAD2;
        case KEY_KP3:
            return angle::KeyType::NUMPAD3;
        case KEY_KP0:
            return angle::KeyType::NUMPAD0;
        case KEY_F11:
            return angle::KeyType::F11;
        case KEY_F12:
            return angle::KeyType::F12;
        case KEY_F13:
            return angle::KeyType::F13;
        case KEY_F14:
            return angle::KeyType::F14;
        case KEY_F15:
            return angle::KeyType::F15;
        case KEY_KPSLASH:
            return angle::KeyType::DIVIDE;
        case KEY_RIGHTALT:
            return angle::KeyType::RALT;
        case KEY_HOME:
            return angle::KeyType::HOME;
        case KEY_UP:
            return angle::KeyType::UP;
        case KEY_PAGEUP:
            return angle::KeyType::PAGEUP;
        case KEY_LEFT:
            return angle::KeyType::LEFT;
        case KEY_RIGHT:
            return angle::KeyType::RIGHT;
        case KEY_END:
            return angle::KeyType::END;
        case KEY_DOWN:
            return angle::KeyType::DOWN;
        case KEY_PAGEDOWN:
            return angle::KeyType::PAGEDOWN;
        case KEY_INSERT:
            return angle::KeyType::INSERT;
        case KEY_DELETE:
            return angle::KeyType::DEL;
        case KEY_PAUSE:
            return angle::KeyType::PAUSE;
        case KEY_LEFTMETA:
            return angle::KeyType::LSYSTEM;
        case KEY_RIGHTMETA:
            return angle::KeyType::RSYSTEM;
        case KEY_RIGHTCTRL:
            return angle::KeyType::RCONTROL;
        case KEY_MENU:
            return angle::KeyType::MENU;
        default:
            return angle::KeyType(0);
    }
}

angle::MouseButtonType LinuxButtonToMouseButton(uint32_t button)
{
    switch (button)
    {
        case BTN_LEFT:
            return angle::MouseButtonType::LEFT;
        case BTN_RIGHT:
            return angle::MouseButtonType::RIGHT;
        case BTN_MIDDLE:
            return angle::MouseButtonType::MIDDLE;
        case BTN_SIDE:
            return angle::MouseButtonType::BUTTON4;
        case BTN_EXTRA:
            return angle::MouseButtonType::BUTTON5;
        default:
            return angle::MouseButtonType::UNKNOWN;
    }
}
}  // namespace

void WaylandWindow::seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
    auto *w = static_cast<WaylandWindow *>(data);

    const bool wantKeyboard = (caps & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
    if (wantKeyboard && !w->mKeyboard)
    {
        w->mKeyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(w->mKeyboard, &keyboard_listener, w);
    }
    else if (!wantKeyboard && w->mKeyboard)
    {
        wl_keyboard_release(w->mKeyboard);
        w->mKeyboard = nullptr;
    }

    const bool wantPointer = (caps & WL_SEAT_CAPABILITY_POINTER) != 0;
    if (wantPointer && !w->mPointer)
    {
        w->mPointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(w->mPointer, &pointer_listener, w);
    }
    else if (!wantPointer && w->mPointer)
    {
        wl_pointer_release(w->mPointer);
        w->mPointer = nullptr;
    }
}

void WaylandWindow::seat_name(void * /*data*/, struct wl_seat * /*seat*/, const char * /*name*/) {}

const struct wl_seat_listener WaylandWindow::seat_listener = {
    .capabilities = seat_capabilities,
    .name         = seat_name,
};

void WaylandWindow::keyboard_keymap(void * /*data*/,
                                    struct wl_keyboard * /*keyboard*/,
                                    uint32_t /*format*/,
                                    int32_t fd,
                                    uint32_t /*size*/)
{
    // We don't translate via xkbcommon; close the fd the compositor mmap'd for
    // us to keep the file table tidy (libwayland-client always sends one).
    close(fd);
}

void WaylandWindow::keyboard_enter(void * /*data*/,
                                   struct wl_keyboard * /*keyboard*/,
                                   uint32_t /*serial*/,
                                   struct wl_surface * /*surface*/,
                                   struct wl_array * /*keys*/)
{
    // Focus events come from xdg_toplevel.state.activated (see
    // handle_toplevel_configure), not from keyboard enter/leave. The
    // activated bit is the WM-level focus signal and works even when the
    // compositor doesn't advertise a keyboard capability.
}

void WaylandWindow::keyboard_leave(void *data,
                                   struct wl_keyboard * /*keyboard*/,
                                   uint32_t /*serial*/,
                                   struct wl_surface * /*surface*/)
{
    // Stop any in-flight key repeat -- the user can no longer release the held
    // key for this surface, so repeats would fire forever. (No focus event is
    // pushed here; see keyboard_enter comment.)
    auto *w = static_cast<WaylandWindow *>(data);
    if (w->mRepeatTimerFd >= 0 && w->mRepeatKey != 0)
    {
        struct itimerspec ts{};
        timerfd_settime(w->mRepeatTimerFd, 0, &ts, nullptr);
        w->mRepeatKey = 0;
    }
}

void WaylandWindow::keyboard_key(void *data,
                                 struct wl_keyboard * /*keyboard*/,
                                 uint32_t /*serial*/,
                                 uint32_t /*time*/,
                                 uint32_t key,
                                 uint32_t state)
{
    auto *w = static_cast<WaylandWindow *>(data);

    Event event{};
    event.Type        = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? Event::EVENT_KEY_PRESSED
                                                                 : Event::EVENT_KEY_RELEASED;
    event.Key.Code    = LinuxKeyCodeToKey(key);
    event.Key.Shift   = w->mShiftDown;
    event.Key.Control = w->mControlDown;
    event.Key.Alt     = w->mAltDown;
    event.Key.System  = w->mSuperDown;
    w->pushEvent(std::move(event));

    // Drive key auto-repeat ourselves to match X11/Win32 semantics -- Wayland
    // does not generate repeat events. Only non-modifier keys repeat (matches
    // libinput's default behavior and avoids spamming Shift/Ctrl events).
    if (w->mRepeatTimerFd < 0)
    {
        return;
    }
    const bool isModifier = (key == KEY_LEFTSHIFT || key == KEY_RIGHTSHIFT || key == KEY_LEFTCTRL ||
                             key == KEY_RIGHTCTRL || key == KEY_LEFTALT || key == KEY_RIGHTALT ||
                             key == KEY_LEFTMETA || key == KEY_RIGHTMETA);
    struct itimerspec ts{};
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && !isModifier && w->mRepeatRate > 0)
    {
        w->mRepeatKey            = key;
        const int64_t intervalNs = 1'000'000'000LL / w->mRepeatRate;
        ts.it_value.tv_sec       = w->mRepeatDelay / 1000;
        ts.it_value.tv_nsec      = (w->mRepeatDelay % 1000) * 1'000'000;
        ts.it_interval.tv_sec    = static_cast<time_t>(intervalNs / 1'000'000'000LL);
        ts.it_interval.tv_nsec   = static_cast<long>(intervalNs % 1'000'000'000LL);
        timerfd_settime(w->mRepeatTimerFd, 0, &ts, nullptr);
    }
    else if (state == WL_KEYBOARD_KEY_STATE_RELEASED && key == w->mRepeatKey)
    {
        // Disarm only on release of the key we were repeating. Releasing some
        // other key (e.g., Shift while W is still held) leaves W repeating.
        timerfd_settime(w->mRepeatTimerFd, 0, &ts, nullptr);
        w->mRepeatKey = 0;
    }
}

void WaylandWindow::keyboard_modifiers(void *data,
                                       struct wl_keyboard * /*keyboard*/,
                                       uint32_t /*serial*/,
                                       uint32_t mods_depressed,
                                       uint32_t /*mods_latched*/,
                                       uint32_t /*mods_locked*/,
                                       uint32_t /*group*/)
{
    // wl_keyboard.modifiers reports the active xkb modifier mask. Without a
    // keymap we don't know which xkb bit is Shift, but compositors using the
    // default libxkbcommon layout set bit 0=Shift, 2=Control, 3=Alt (Mod1),
    // 6=Super (Mod4) -- sufficient for non-text-entry key dispatch.
    auto *w         = static_cast<WaylandWindow *>(data);
    w->mShiftDown   = (mods_depressed & (1u << 0)) != 0;
    w->mControlDown = (mods_depressed & (1u << 2)) != 0;
    w->mAltDown     = (mods_depressed & (1u << 3)) != 0;
    w->mSuperDown   = (mods_depressed & (1u << 6)) != 0;
}

void WaylandWindow::keyboard_repeat_info(void *data,
                                         struct wl_keyboard * /*keyboard*/,
                                         int32_t rate,
                                         int32_t delay)
{
    // rate is chars/sec; rate == 0 means user disabled key repeat in their
    // compositor settings. delay is ms before the first repeat fires.
    auto *w         = static_cast<WaylandWindow *>(data);
    w->mRepeatRate  = rate;
    w->mRepeatDelay = delay;
}

const struct wl_keyboard_listener WaylandWindow::keyboard_listener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

void WaylandWindow::pointer_enter(void *data,
                                  struct wl_pointer * /*pointer*/,
                                  uint32_t /*serial*/,
                                  struct wl_surface * /*surface*/,
                                  wl_fixed_t x,
                                  wl_fixed_t y)
{
    auto *w      = static_cast<WaylandWindow *>(data);
    w->mPointerX = wl_fixed_to_int(x);
    w->mPointerY = wl_fixed_to_int(y);
    Event event{};
    event.Type = Event::EVENT_MOUSE_ENTERED;
    w->pushEvent(std::move(event));
}

void WaylandWindow::pointer_leave(void *data,
                                  struct wl_pointer * /*pointer*/,
                                  uint32_t /*serial*/,
                                  struct wl_surface * /*surface*/)
{
    auto *w = static_cast<WaylandWindow *>(data);
    Event event{};
    event.Type = Event::EVENT_MOUSE_LEFT;
    w->pushEvent(std::move(event));
}

void WaylandWindow::pointer_motion(void *data,
                                   struct wl_pointer * /*pointer*/,
                                   uint32_t /*time*/,
                                   wl_fixed_t x,
                                   wl_fixed_t y)
{
    auto *w      = static_cast<WaylandWindow *>(data);
    w->mPointerX = wl_fixed_to_int(x);
    w->mPointerY = wl_fixed_to_int(y);
    Event event{};
    event.Type        = Event::EVENT_MOUSE_MOVED;
    event.MouseMove.X = w->mPointerX;
    event.MouseMove.Y = w->mPointerY;
    w->pushEvent(std::move(event));
}

void WaylandWindow::pointer_button(void *data,
                                   struct wl_pointer * /*pointer*/,
                                   uint32_t /*serial*/,
                                   uint32_t /*time*/,
                                   uint32_t button,
                                   uint32_t state)
{
    auto *w = static_cast<WaylandWindow *>(data);
    Event event{};
    event.Type = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? Event::EVENT_MOUSE_BUTTON_PRESSED
                                                            : Event::EVENT_MOUSE_BUTTON_RELEASED;
    event.MouseButton.Button = LinuxButtonToMouseButton(button);
    event.MouseButton.X      = w->mPointerX;
    event.MouseButton.Y      = w->mPointerY;
    w->pushEvent(std::move(event));
}

void WaylandWindow::pointer_axis(void *data,
                                 struct wl_pointer * /*pointer*/,
                                 uint32_t /*time*/,
                                 uint32_t axis,
                                 wl_fixed_t value)
{
    // Compositors typically send 10.0 per discrete vertical wheel notch.
    // Normalize to +/-1 to match X11's Button4/5 and Win32's WHEEL_DELTA
    // semantics. Horizontal axis is ignored -- Event::MouseWheelEvent has no
    // horizontal field.
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
    {
        return;
    }
    auto *w        = static_cast<WaylandWindow *>(data);
    const double v = wl_fixed_to_double(value);
    if (v == 0.0)
    {
        return;
    }
    Event event{};
    event.Type             = Event::EVENT_MOUSE_WHEEL_MOVED;
    event.MouseWheel.Delta = (v < 0.0) ? 1 : -1;  // wayland: positive = scroll down
    w->pushEvent(std::move(event));
}

void WaylandWindow::pointer_frame(void * /*data*/, struct wl_pointer * /*pointer*/) {}
void WaylandWindow::pointer_axis_source(void * /*data*/,
                                        struct wl_pointer * /*pointer*/,
                                        uint32_t /*source*/)
{}
void WaylandWindow::pointer_axis_stop(void * /*data*/,
                                      struct wl_pointer * /*pointer*/,
                                      uint32_t /*time*/,
                                      uint32_t /*axis*/)
{}
void WaylandWindow::pointer_axis_discrete(void * /*data*/,
                                          struct wl_pointer * /*pointer*/,
                                          uint32_t /*axis*/,
                                          int32_t /*discrete*/)
{}

const struct wl_pointer_listener WaylandWindow::pointer_listener = {
    .enter         = pointer_enter,
    .leave         = pointer_leave,
    .motion        = pointer_motion,
    .button        = pointer_button,
    .axis          = pointer_axis,
    .frame         = pointer_frame,
    .axis_source   = pointer_axis_source,
    .axis_stop     = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
    // v8+ events. We bind seat at v5, so the compositor will never send these,
    // but the listener struct requires every field.
    .axis_value120           = nullptr,
    .axis_relative_direction = nullptr,
};

bool IsWaylandWindowAvailable()
{
    if (!gDisplay)
    {
        wl_display *display = wl_display_connect(nullptr);
        if (!display)
        {
            return false;
        }
        wl_display_flush(display);
        wl_display_disconnect(display);
    }
    return true;
}
