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

#if USE(GLX)

#include "GLContext.h"
#include "XUniquePtr.h"
#include "XUniqueResource.h"

typedef unsigned char GLubyte;
typedef unsigned long Window;
typedef void* ContextKeyType;
typedef struct _XDisplay Display;

namespace WebCore {

class GLContextGLX final : public GLContext {
    WTF_MAKE_NONCOPYABLE(GLContextGLX);
public:
    static std::unique_ptr<GLContextGLX> createContext(GLNativeWindowType, PlatformDisplay&);
    static std::unique_ptr<GLContextGLX> createSharingContext(PlatformDisplay&);

    virtual ~GLContextGLX();

private:
    bool makeContextCurrent() override;
    void swapBuffers() override;
    void waitNative() override;
    bool canRenderToDefaultFramebuffer() override;
    IntSize defaultFrameBufferSize() override;
    void swapInterval(int) override;
    cairo_device_t* cairoDevice() override;
    bool isEGLContext() const override { return false; }

#if ENABLE(GRAPHICS_CONTEXT_GL)
    PlatformGraphicsContextGL platformContext() override;
#endif

    GLContextGLX(PlatformDisplay&, XUniqueGLXContext&&, GLNativeWindowType);
    GLContextGLX(PlatformDisplay&, XUniqueGLXContext&&, XUniqueGLXPbuffer&&);
    GLContextGLX(PlatformDisplay&, XUniqueGLXContext&&, XUniquePixmap&&, XUniqueGLXPixmap&&);

    static std::unique_ptr<GLContextGLX> createWindowContext(GLNativeWindowType, PlatformDisplay&, GLXContext sharingContext = nullptr);
    static std::unique_ptr<GLContextGLX> createPbufferContext(PlatformDisplay&, GLXContext sharingContext = nullptr);
    static std::unique_ptr<GLContextGLX> createPixmapContext(PlatformDisplay&, GLXContext sharingContext = nullptr);

    Display* m_x11Display { nullptr };
    XUniqueGLXContext m_context;
    Window m_window { 0 };
    XUniqueGLXPbuffer m_pbuffer;
    XUniquePixmap m_pixmap;
    XUniqueGLXPixmap m_glxPixmap;
    cairo_device_t* m_cairoDevice { nullptr };
};

} // namespace WebCore

#endif // USE(GLX)

