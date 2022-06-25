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

#ifndef GLContext_h
#define GLContext_h

#include "IntSize.h"
#include "PlatformDisplay.h"
#include <wtf/Noncopyable.h>

#if USE(EGL) && !PLATFORM(GTK)
// FIXME: For now default to the GBM EGL platform, but this should really be
// somehow deducible from the build configuration. This is needed with libepoxy
// as it could have been configured with X11 support enabled, resulting in
// transitive inclusions of headers with definitions that clash with WebCore.
#define __GBM__ 1
#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else // !USE(LIBEPOXY)
#include <EGL/eglplatform.h>
#endif // USE(LIBEPOXY)
typedef EGLNativeWindowType GLNativeWindowType;
#else // !USE(EGL) || PLATFORM(GTK)
typedef uint64_t GLNativeWindowType;
#endif // USE(EGL) && !PLATFORM(GTK)

#if USE(CAIRO)
typedef struct _cairo_device cairo_device_t;
#endif

typedef void* GCGLContext;

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

class IntSize;

class GLContext {
    WTF_MAKE_NONCOPYABLE(GLContext); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static std::unique_ptr<GLContext> createContextForWindow(GLNativeWindowType windowHandle, PlatformDisplay* = nullptr);
    static std::unique_ptr<GLContext> createOffscreenContext(PlatformDisplay* = nullptr);
    static std::unique_ptr<GLContext> createSharingContext(PlatformDisplay&);
    static GLContext* current();
    static bool isExtensionSupported(const char* extensionList, const char* extension);

    PlatformDisplay& display() const { return m_display; }
    unsigned version();

    virtual ~GLContext();
    virtual bool makeContextCurrent();
    virtual void swapBuffers() = 0;
    virtual void waitNative() = 0;
    virtual bool canRenderToDefaultFramebuffer() = 0;
    virtual IntSize defaultFrameBufferSize() = 0;
    virtual void swapInterval(int) = 0;

    virtual bool isEGLContext() const = 0;

    virtual GCGLContext platformContext() = 0;

protected:
    GLContext(PlatformDisplay&);

    PlatformDisplay& m_display;
    unsigned m_version { 0 };
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_GLCONTEXT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::GLContext& context) { return context.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // GLContext_h
