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

#ifndef GLContextEGL_h
#define GLContextEGL_h

#if USE(EGL)

#include "GLContext.h"
#include <EGL/egl.h>

#if PLATFORM(X11)
#include "XUniqueResource.h"
#endif

namespace WebCore {

class GLContextEGL : public GLContext {
    WTF_MAKE_NONCOPYABLE(GLContextEGL);
public:
    enum EGLSurfaceType { PbufferSurface, WindowSurface, PixmapSurface };
    static std::unique_ptr<GLContextEGL> createContext(EGLNativeWindowType, GLContext* sharingContext = 0);
    static std::unique_ptr<GLContextEGL> createWindowContext(EGLNativeWindowType, GLContext* sharingContext, std::unique_ptr<GLContext::Data>&& = nullptr);

    GLContextEGL(EGLContext, EGLSurface, EGLSurfaceType);
#if PLATFORM(X11)
    GLContextEGL(EGLContext, EGLSurface, XUniquePixmap&&);
#endif
    virtual ~GLContextEGL();
    virtual bool makeContextCurrent();
    virtual void swapBuffers();
    virtual void waitNative();
    virtual bool canRenderToDefaultFramebuffer();
    virtual IntSize defaultFrameBufferSize();
#if USE(CAIRO)
    virtual cairo_device_t* cairoDevice();
#endif
    virtual bool isEGLContext() const { return true; }

#if ENABLE(GRAPHICS_CONTEXT_3D)
    virtual PlatformGraphicsContext3D platformContext();
#endif

private:
    static std::unique_ptr<GLContextEGL> createPbufferContext(EGLContext sharingContext);
#if PLATFORM(X11)
    static std::unique_ptr<GLContextEGL> createPixmapContext(EGLContext sharingContext);
#endif

    static void addActiveContext(GLContextEGL*);
    static void cleanupSharedEGLDisplay(void);

    EGLContext m_context;
    EGLSurface m_surface;
    EGLSurfaceType m_type;
#if PLATFORM(X11)
    XUniquePixmap m_pixmap;
#endif
#if USE(CAIRO)
    cairo_device_t* m_cairoDevice { nullptr };
#endif
};

} // namespace WebCore

#endif // USE(EGL)

#endif // GLContextEGL_h
