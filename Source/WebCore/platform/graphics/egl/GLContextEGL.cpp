/*
 * Copyright (C) 2012 Igalia, S.L.
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
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GLContextEGL.h"

#if USE(EGL)

#include "GraphicsContext3D.h"
#include "PlatformDisplay.h"

#if USE(CAIRO)
#include <cairo.h>
#endif

#if USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include "OpenGLShims.h"
#endif

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#include <X11/Xlib.h>
#endif

#if ENABLE(ACCELERATED_2D_CANVAS)
// cairo-gl.h includes some definitions from GLX that conflict with
// the ones provided by us. Since GLContextEGL doesn't use any GLX
// functions we can safely disable them.
#undef CAIRO_HAS_GLX_FUNCTIONS
#include <cairo-gl.h>
#endif

namespace WebCore {

static EGLDisplay sharedEGLDisplay()
{
    return PlatformDisplay::sharedDisplay().eglDisplay();
}

static const EGLint gContextAttributes[] = {
#if USE(OPENGL_ES_2)
    EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
    EGL_NONE
};

static bool getEGLConfig(EGLConfig* config, GLContextEGL::EGLSurfaceType surfaceType)
{
    EGLint attributeList[] = {
#if USE(OPENGL_ES_2)
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_NONE,
        EGL_NONE
    };

    switch (surfaceType) {
    case GLContextEGL::PbufferSurface:
        attributeList[13] = EGL_PBUFFER_BIT;
        break;
    case GLContextEGL::PixmapSurface:
        attributeList[13] = EGL_PIXMAP_BIT;
        break;
    case GLContextEGL::WindowSurface:
        attributeList[13] = EGL_WINDOW_BIT;
        break;
    }

    EGLint numberConfigsReturned;
    return eglChooseConfig(sharedEGLDisplay(), attributeList, config, 1, &numberConfigsReturned) && numberConfigsReturned;
}

std::unique_ptr<GLContextEGL> GLContextEGL::createWindowContext(EGLNativeWindowType window, GLContext* sharingContext, std::unique_ptr<GLContext::Data>&& contextData)
{
    EGLContext eglSharingContext = sharingContext ? static_cast<GLContextEGL*>(sharingContext)->m_context : 0;

    EGLDisplay display = sharedEGLDisplay();
    if (display == EGL_NO_DISPLAY)
        return nullptr;

    EGLConfig config;
    if (!getEGLConfig(&config, WindowSurface))
        return nullptr;

    EGLContext context = eglCreateContext(display, config, eglSharingContext, gContextAttributes);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    EGLSurface surface = eglCreateWindowSurface(display, config, window, 0);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    auto glContext = std::make_unique<GLContextEGL>(context, surface, WindowSurface);
    glContext->m_contextData = WTFMove(contextData);
    return glContext;
}

std::unique_ptr<GLContextEGL> GLContextEGL::createPbufferContext(EGLContext sharingContext)
{
    EGLDisplay display = sharedEGLDisplay();
    if (display == EGL_NO_DISPLAY)
        return nullptr;

    EGLConfig config;
    if (!getEGLConfig(&config, PbufferSurface))
        return nullptr;

    EGLContext context = eglCreateContext(display, config, sharingContext, gContextAttributes);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    static const int pbufferAttributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbufferAttributes);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    return std::make_unique<GLContextEGL>(context, surface, PbufferSurface);
}

#if PLATFORM(X11)
std::unique_ptr<GLContextEGL> GLContextEGL::createPixmapContext(EGLContext sharingContext)
{
    EGLDisplay display = sharedEGLDisplay();
    if (display == EGL_NO_DISPLAY)
        return nullptr;

    EGLConfig config;
    if (!getEGLConfig(&config, PixmapSurface))
        return nullptr;

    EGLContext context = eglCreateContext(display, config, sharingContext, gContextAttributes);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    EGLint depth;
    if (!eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    Display* x11Display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
    XUniquePixmap pixmap = XCreatePixmap(x11Display, DefaultRootWindow(x11Display), 1, 1, depth);
    if (!pixmap) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    EGLSurface surface = eglCreatePixmapSurface(display, config, reinterpret_cast<EGLNativePixmapType>(pixmap.get()), 0);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    return std::make_unique<GLContextEGL>(context, surface, WTFMove(pixmap));
}
#endif // PLATFORM(X11)

std::unique_ptr<GLContextEGL> GLContextEGL::createContext(EGLNativeWindowType window, GLContext* sharingContext)
{
    if (!sharedEGLDisplay())
        return nullptr;

    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
#if !USE(OPENGL_ES_2)
        success = initializeOpenGLShims();
#endif
        initialized = true;
    }
    if (!success)
        return nullptr;

    EGLContext eglSharingContext = sharingContext ? static_cast<GLContextEGL*>(sharingContext)->m_context : 0;
    auto context = window ? createWindowContext(window, sharingContext) : nullptr;
#if PLATFORM(X11)
    if (!context)
        context = createPixmapContext(eglSharingContext);
#endif
    if (!context)
        context = createPbufferContext(eglSharingContext);

    return WTFMove(context);
}

GLContextEGL::GLContextEGL(EGLContext context, EGLSurface surface, EGLSurfaceType type)
    : m_context(context)
    , m_surface(surface)
    , m_type(type)
{
    ASSERT(type != PixmapSurface);
}

#if PLATFORM(X11)
GLContextEGL::GLContextEGL(EGLContext context, EGLSurface surface, XUniquePixmap&& pixmap)
    : m_context(context)
    , m_surface(surface)
    , m_type(PixmapSurface)
    , m_pixmap(WTFMove(pixmap))
{
}
#endif

GLContextEGL::~GLContextEGL()
{
#if USE(CAIRO)
    if (m_cairoDevice)
        cairo_device_destroy(m_cairoDevice);
#endif

    EGLDisplay display = sharedEGLDisplay();
    if (m_context) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, m_context);
    }

    if (m_surface)
        eglDestroySurface(display, m_surface);
}

bool GLContextEGL::canRenderToDefaultFramebuffer()
{
    return m_type == WindowSurface;
}

IntSize GLContextEGL::defaultFrameBufferSize()
{
    if (!canRenderToDefaultFramebuffer())
        return IntSize();

    EGLint width, height;
    if (!eglQuerySurface(sharedEGLDisplay(), m_surface, EGL_WIDTH, &width)
        || !eglQuerySurface(sharedEGLDisplay(), m_surface, EGL_HEIGHT, &height))
        return IntSize();

    return IntSize(width, height);
}

bool GLContextEGL::makeContextCurrent()
{
    ASSERT(m_context && m_surface);

    GLContext::makeContextCurrent();
    if (eglGetCurrentContext() == m_context)
        return true;

    return eglMakeCurrent(sharedEGLDisplay(), m_surface, m_surface, m_context);
}

void GLContextEGL::swapBuffers()
{
    ASSERT(m_surface);
    eglSwapBuffers(sharedEGLDisplay(), m_surface);
}

void GLContextEGL::waitNative()
{
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
}

#if USE(CAIRO)
cairo_device_t* GLContextEGL::cairoDevice()
{
    if (m_cairoDevice)
        return m_cairoDevice;

#if ENABLE(ACCELERATED_2D_CANVAS)
    m_cairoDevice = cairo_egl_device_create(sharedEGLDisplay(), m_context);
#endif

    return m_cairoDevice;
}
#endif

#if ENABLE(GRAPHICS_CONTEXT_3D)
PlatformGraphicsContext3D GLContextEGL::platformContext()
{
    return m_context;
}
#endif

} // namespace WebCore

#endif // USE(EGL)
