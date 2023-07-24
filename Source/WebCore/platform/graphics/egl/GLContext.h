/*
 * Copyright (C) 2012,2023 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#pragma once

#if USE(EGL)
#include "IntSize.h"
#include "PlatformDisplay.h"
#include <wtf/Noncopyable.h>

#if !PLATFORM(GTK) && !PLATFORM(WPE)
#include <EGL/eglplatform.h>
typedef EGLNativeWindowType GLNativeWindowType;
#else
typedef uint64_t GLNativeWindowType;
#endif

#if PLATFORM(X11)
#include "XUniqueResource.h"
#endif

#if PLATFORM(WAYLAND)
#include "WlUniquePtr.h"
struct wl_egl_window;
#endif

#if USE(WPE_RENDERER)
struct wpe_renderer_backend_egl_offscreen_target;
#endif

typedef void* GCGLContext;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLDisplay;
typedef void* EGLSurface;

// X11 headers define a bunch of macros with common terms, interfering with WebCore and WTF enum values.
// As a workaround, we explicitly undef them here.
#if defined(None)
#undef None
#endif
#if defined(Above)
#undef Above
#endif
#if defined(Below)
#undef Below
#endif
#if defined(Success)
#undef Success
#endif
#if defined(False)
#undef False
#endif
#if defined(True)
#undef True
#endif
#if defined(Bool)
#undef Bool
#endif
#if defined(Always)
#undef Always
#endif
#if defined(Status)
#undef Status
#endif
#if defined(Continue)
#undef Continue
#endif
#if defined(Region)
#undef Region
#endif

namespace WebCore {

class GLContext {
    WTF_MAKE_NONCOPYABLE(GLContext); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static std::unique_ptr<GLContext> create(GLNativeWindowType, PlatformDisplay&);
    static std::unique_ptr<GLContext> createOffscreen(PlatformDisplay&);
    static std::unique_ptr<GLContext> createSharing(PlatformDisplay&);

    static GLContext* current();
    static bool isExtensionSupported(const char* extensionList, const char* extension);

    static const char* errorString(int statusCode);
    static const char* lastErrorString();

    enum EGLSurfaceType { PbufferSurface, WindowSurface, PixmapSurface, Surfaceless };
    GLContext(PlatformDisplay&, EGLContext, EGLSurface, EGLConfig, EGLSurfaceType);
#if PLATFORM(X11)
    GLContext(PlatformDisplay&, EGLContext, EGLSurface, EGLConfig, XUniquePixmap&&);
#endif
#if PLATFORM(WAYLAND)
    GLContext(PlatformDisplay&, EGLContext, EGLSurface, EGLConfig, WlUniquePtr<struct wl_surface>&&, struct wl_egl_window*);
#endif
#if USE(WPE_RENDERER)
    GLContext(PlatformDisplay&, EGLContext, EGLSurface, EGLConfig, struct wpe_renderer_backend_egl_offscreen_target*);
#endif
    WEBCORE_EXPORT ~GLContext();

    PlatformDisplay& display() const { return m_display; }
    unsigned version();
    EGLConfig config() const { return m_config; }

    WEBCORE_EXPORT bool makeContextCurrent();
    bool unmakeContextCurrent();
    WEBCORE_EXPORT void swapBuffers();
    GCGLContext platformContext() const;

    class ScopedGLContext {
        WTF_MAKE_NONCOPYABLE(ScopedGLContext);
    public:
        explicit ScopedGLContext(std::unique_ptr<GLContext>&&);
        ~ScopedGLContext();
    private:
        struct {
            EGLDisplay display { nullptr };
            EGLContext context { nullptr };
            EGLSurface readSurface { nullptr };
            EGLSurface drawSurface { nullptr };
        } m_previous;
        std::unique_ptr<GLContext> m_context;
    };

    class ScopedGLContextCurrent {
        WTF_MAKE_NONCOPYABLE(ScopedGLContextCurrent);
    public:
        explicit ScopedGLContextCurrent(GLContext&);
        ~ScopedGLContextCurrent();
    private:
        struct {
            GLContext* glContext { nullptr };
            EGLDisplay display { nullptr };
            EGLContext context { nullptr };
            EGLSurface readSurface { nullptr };
            EGLSurface drawSurface { nullptr };
        } m_previous;
        GLContext& m_context;
    };

private:
    static EGLContext createContextForEGLVersion(PlatformDisplay&, EGLConfig, EGLContext);

    static std::unique_ptr<GLContext> createWindowContext(GLNativeWindowType, PlatformDisplay&, EGLContext sharingContext = nullptr);
    static std::unique_ptr<GLContext> createPbufferContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static std::unique_ptr<GLContext> createSurfacelessContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
#if PLATFORM(X11)
    static std::unique_ptr<GLContext> createPixmapContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static EGLSurface createWindowSurfaceX11(EGLDisplay, EGLConfig, GLNativeWindowType);
#endif
#if PLATFORM(WAYLAND)
    static std::unique_ptr<GLContext> createWaylandContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static EGLSurface createWindowSurfaceWayland(EGLDisplay, EGLConfig, GLNativeWindowType);
    void destroyWaylandWindow();
#endif
#if USE(WPE_RENDERER)
    static std::unique_ptr<GLContext> createWPEContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static EGLSurface createWindowSurfaceWPE(EGLDisplay, EGLConfig, GLNativeWindowType);
    void destroyWPETarget();
#endif

    static bool getEGLConfig(PlatformDisplay&, EGLConfig*, EGLSurfaceType, Function<bool(int)>&& = nullptr);

    PlatformDisplay& m_display;
    unsigned m_version { 0 };
    EGLContext m_context { nullptr };
    EGLSurface m_surface { nullptr };
    EGLConfig m_config { nullptr };
    EGLSurfaceType m_type;
#if PLATFORM(X11)
    XUniquePixmap m_pixmap;
#endif
#if PLATFORM(WAYLAND)
    WlUniquePtr<struct wl_surface> m_wlSurface;
    struct wl_egl_window* m_wlWindow { nullptr };
#endif
#if USE(WPE_RENDERER)
    struct wpe_renderer_backend_egl_offscreen_target* m_wpeTarget { nullptr };
#endif
};

} // namespace WebCore

#endif // USE(EGL)
