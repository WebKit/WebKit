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

#if USE(WPE_RENDERER)
struct wpe_renderer_backend_egl_offscreen_target;
#endif

typedef void* GCGLContext;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLDisplay;
typedef void* EGLSurface;

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

    struct GLExtensions {
        bool OES_texture_npot { false };
        bool EXT_unpack_subimage { false };
        bool OES_packed_depth_stencil { false };
    };
    const GLExtensions& glExtensions() const;

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
#if USE(WPE_RENDERER)
    static std::unique_ptr<GLContext> createWPEContext(PlatformDisplay&, EGLContext sharingContext = nullptr);
    static EGLSurface createWindowSurfaceWPE(EGLDisplay, EGLConfig, GLNativeWindowType);
    void destroyWPETarget();
#endif

    static bool getEGLConfig(PlatformDisplay&, EGLConfig*, EGLSurfaceType);

    PlatformDisplay& m_display;
    unsigned m_version { 0 };
    EGLContext m_context { nullptr };
    EGLSurface m_surface { nullptr };
    EGLConfig m_config { nullptr };
    EGLSurfaceType m_type;
#if USE(WPE_RENDERER)
    struct wpe_renderer_backend_egl_offscreen_target* m_wpeTarget { nullptr };
#endif
    mutable GLExtensions m_glExtensions;
};

} // namespace WebCore

#endif // USE(EGL)
