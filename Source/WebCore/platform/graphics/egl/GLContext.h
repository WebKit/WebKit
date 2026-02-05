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

#include "GLContextWrapper.h"
#include "IntSize.h"
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

#if !PLATFORM(GTK) && !PLATFORM(WPE)
#include <EGL/eglplatform.h>
typedef EGLNativeWindowType GLNativeWindowType;
#else
typedef uint64_t GLNativeWindowType;
#endif

#if ENABLE(MEDIA_TELEMETRY)
#include "MediaTelemetry.h"
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
class GLDisplay;
class PlatformDisplay;

class GLContext final : public GLContextWrapper
#if ENABLE(MEDIA_TELEMETRY)
    , public MediaTelemetryWaylandInfoGetter
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(GLContext);
    WTF_MAKE_NONCOPYABLE(GLContext);
public:
    enum class Target : uint8_t {
        Default,
        Surfaceless,
#if USE(GBM)
        GBM,
#endif
#if USE(WPE_RENDERER)
        WPE,
#endif
    };
    WEBCORE_EXPORT static std::unique_ptr<GLContext> create(GLDisplay&, Target, GLContext* = nullptr, GLNativeWindowType = 0);

    WEBCORE_EXPORT static std::unique_ptr<GLContext> create(PlatformDisplay&, GLNativeWindowType);
    static std::unique_ptr<GLContext> createOffscreen(PlatformDisplay&);
    static std::unique_ptr<GLContext> createSharing(PlatformDisplay&);

    static GLContext* current();
    static bool isExtensionSupported(const char* extensionList, const char* extension);
    static unsigned versionFromString(const char* versionString);

    static const char* errorString(int statusCode);
    static const char* lastErrorString();

    GLContext(GLDisplay&, EGLContext, EGLSurface, EGLConfig);
#if USE(WPE_RENDERER)
    GLContext(GLDisplay&, EGLContext, EGLSurface, EGLConfig, struct wpe_renderer_backend_egl_offscreen_target*);
#endif
    WEBCORE_EXPORT ~GLContext();

    RefPtr<GLDisplay> display() const;
    unsigned version() const;
    EGLConfig config() const { return m_config; }

    WEBCORE_EXPORT bool makeContextCurrent();
    bool unmakeContextCurrent();
    WEBCORE_EXPORT void swapBuffers();
    GCGLContext platformContext() const;

    struct GLExtensions {
        bool OES_texture_npot { false };
        bool EXT_unpack_subimage { false };
        bool APPLE_sync { false };
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
            GLContext* glContext { nullptr };
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
    static EGLContext createContextForEGLVersion(EGLDisplay, EGLConfig, EGLContext);

    static std::unique_ptr<GLContext> createWindowContext(GLDisplay&, Target, GLNativeWindowType, EGLContext sharingContext);
    static std::unique_ptr<GLContext> createSurfacelessContext(GLDisplay&, Target, EGLContext sharingContext);
    static std::unique_ptr<GLContext> createPbufferContext(GLDisplay&, EGLContext sharingContext);
    static std::unique_ptr<GLContext> createOffscreenContext(GLDisplay&, Target, EGLContext sharingContext);

#if USE(WPE_RENDERER)
    static std::unique_ptr<GLContext> createWPEContext(GLDisplay&, EGLContext sharingContext = nullptr);
    static EGLSurface createWindowSurfaceWPE(EGLDisplay, EGLConfig, GLNativeWindowType);
    void destroyWPETarget();
#endif

    static bool getEGLConfig(EGLDisplay, EGLConfig*, int);

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    bool enableDebugLogging();
#endif

    // GLContextWrapper
    GLContextWrapper::Type type() const override { return GLContextWrapper::Type::Native; }
    bool makeCurrentImpl() override;
    bool unmakeCurrentImpl() override;
    unsigned glVersion() const override;

#if ENABLE(MEDIA_TELEMETRY)
    EGLDisplay eglDisplay() const final;
    EGLConfig eglConfig() const final;
    EGLSurface eglSurface() const final;
    EGLContext eglContext() const final;
    unsigned windowWidth() const final;
    unsigned windowHeight() const final;
#endif

    ThreadSafeWeakPtr<GLDisplay> m_display;
    mutable unsigned m_version { 0 };
    EGLContext m_context { nullptr };
    EGLSurface m_surface { nullptr };
    EGLConfig m_config { nullptr };
#if USE(WPE_RENDERER)
    struct wpe_renderer_backend_egl_offscreen_target* m_wpeTarget { nullptr };
#endif
    mutable GLExtensions m_glExtensions;
};

} // namespace WebCore
