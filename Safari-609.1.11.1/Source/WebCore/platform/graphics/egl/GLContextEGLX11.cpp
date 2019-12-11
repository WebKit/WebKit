/*
 * Copyright (C) 2016 Igalia, S.L.
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

#if USE(EGL) && PLATFORM(X11)
#include "PlatformDisplayX11.h"
#include "XErrorTrapper.h"
#include "XUniquePtr.h"
#include <EGL/egl.h>
#include <X11/Xlib.h>

namespace WebCore {

GLContextEGL::GLContextEGL(PlatformDisplay& display, EGLContext context, EGLSurface surface, XUniquePixmap&& pixmap)
    : GLContext(display)
    , m_context(context)
    , m_surface(surface)
    , m_type(PixmapSurface)
    , m_pixmap(WTFMove(pixmap))
{
}

EGLSurface GLContextEGL::createWindowSurfaceX11(EGLDisplay display, EGLConfig config, GLNativeWindowType window)
{
    return eglCreateWindowSurface(display, config, static_cast<EGLNativeWindowType>(window), nullptr);
}

std::unique_ptr<GLContextEGL> GLContextEGL::createPixmapContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, PixmapSurface))
        return nullptr;

    EGLContext context = createContextForEGLVersion(platformDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    EGLint visualId;
    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visualId)) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    Display* x11Display = downcast<PlatformDisplayX11>(platformDisplay).native();

    XVisualInfo visualInfo;
    visualInfo.visualid = visualId;
    int numVisuals = 0;
    XUniquePtr<XVisualInfo> visualInfoList(XGetVisualInfo(x11Display, VisualIDMask, &visualInfo, &numVisuals));
    if (!visualInfoList || !numVisuals) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    // We are using VisualIDMask so there must be only one.
    ASSERT(numVisuals == 1);
    XUniquePixmap pixmap = XCreatePixmap(x11Display, DefaultRootWindow(x11Display), 1, 1, visualInfoList.get()[0].depth);
    if (!pixmap) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    // Some drivers fail to create the surface producing BadDrawable X error and the default XError handler normally aborts.
    // However, if the X error is ignored, eglCreatePixmapSurface() ends up returning a surface and we can continue creating
    // the context. Since this is an offscreen context, it doesn't matter if the pixmap used is not valid because we never do
    // swap buffers. So, we use a custom XError handler here that ignores BadDrawable errors and only warns about any other
    // errors without aborting in any case.
    XErrorTrapper trapper(x11Display, XErrorTrapper::Policy::Warn, { BadDrawable });
    EGLSurface surface = eglCreatePixmapSurface(display, config, reinterpret_cast<EGLNativePixmapType>(pixmap.get()), 0);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, WTFMove(pixmap)));
}

} // namespace WebCore

#endif // USE(EGL) && PLATFORM(X11)
