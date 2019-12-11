/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "GLContext.h"

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

typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLSurface;

namespace WebCore {

class GLContextEGL final : public GLContext {
    WTF_MAKE_NONCOPYABLE(GLContextEGL);
public:
    static std::unique_ptr<GLContextEGL> createContext(GLNativeWindowType, PlatformDisplay&);
    static std::unique_ptr<GLContextEGL> createSharingContext(PlatformDisplay&);

    static const char* errorString(int statusCode);
    static const char* lastErrorString();

    virtual ~GLContextEGL();

private:
    static EGLContext createContextForEGLVersion(PlatformDisplay&, EGLConfig, EGLContext);

    bool makeContextCurrent() override;
    void swapBuffers() override;
    void waitNative() override;
    bool canRenderToDefaultFramebuffer() override;
    IntSize defaultFrameBufferSize() override;
    void swapInterval(int) override;
#if USE(CAIRO)
    cairo_device_t* cairoDevice() override;
#endif
    bool isEGLContext() const override { return true; }

#if ENABLE(GRAPHICS_CONTEXT_3D)
    PlatformGraphicsContext3D platformContext() override;
#endif

    enum EGLSurfaceType { PbufferSurface, WindowSurface, PixmapSurface, Surfaceless };

    GLContextEGL(PlatformDisplay&, EGLContext, EGLSurface, EGLSurfaceType);
#if PLATFORM(X11)
    GLContextEGL(PlatformDisplay&, EGLContext, EGLSurface, XUniquePixmap&&);
#endif
#if PLATFORM(WAYLAND)
    GLContextEGL(PlatformDisplay&, EGLContext, EGLSurface, WlUniquePtr<struct wl_surface>&&, struct wl_egl_window*);
    void destroyWaylandWindow();
#endif
#if USE(WPE_RENDERER)
    GLContextEGL(PlatformDisplay&, EGLContext, EGLSurface, struct wpe_renderer_backend_egl_offscreen_target*);
    void destroyWPETarget();
#endif

    static std::unique_ptr<GLContextEGL> createWindowContext(GLNativeWindowType, PlatformDisplay&, EGLContext sharingContext = nullptr);
    static std::unique_ptr<GLContextEGL> createPbufferContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static std::unique_ptr<GLContextEGL> createSurfacelessContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
#if PLATFORM(X11)
    static std::unique_ptr<GLContextEGL> createPixmapContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static EGLSurface createWindowSurfaceX11(EGLDisplay, EGLConfig, GLNativeWindowType);
#endif
#if PLATFORM(WAYLAND)
    static std::unique_ptr<GLContextEGL> createWaylandContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static EGLSurface createWindowSurfaceWayland(EGLDisplay, EGLConfig, GLNativeWindowType);
#endif
#if USE(WPE_RENDERER)
    static std::unique_ptr<GLContextEGL> createWPEContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static EGLSurface createWindowSurfaceWPE(EGLDisplay, EGLConfig, GLNativeWindowType);
#endif

    static bool getEGLConfig(EGLDisplay, EGLConfig*, EGLSurfaceType);

    EGLContext m_context { nullptr };
    EGLSurface m_surface { nullptr };
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
#if USE(CAIRO)
    cairo_device_t* m_cairoDevice { nullptr };
#endif
};

} // namespace WebCore

#endif // USE(EGL)
