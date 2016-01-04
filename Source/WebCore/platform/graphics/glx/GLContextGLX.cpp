/*
 * Copyright (C) 2011, 2012 Igalia, S.L.
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
#include "GLContextGLX.h"

#if USE(GLX)
#include "GraphicsContext3D.h"
#include "OpenGLShims.h"
#include "PlatformDisplayX11.h"
#include <GL/glx.h>
#include <cairo.h>

#if ENABLE(ACCELERATED_2D_CANVAS)
#include <cairo-gl.h>
#endif

namespace WebCore {

std::unique_ptr<GLContextGLX> GLContextGLX::createWindowContext(XID window, GLContext* sharingContext)
{
    Display* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display, window, &attributes))
        return nullptr;

    XVisualInfo visualInfo;
    visualInfo.visualid = XVisualIDFromVisual(attributes.visual);

    int numReturned = 0;
    XUniquePtr<XVisualInfo> visualInfoList(XGetVisualInfo(display, VisualIDMask, &visualInfo, &numReturned));

    GLXContext glxSharingContext = sharingContext ? static_cast<GLContextGLX*>(sharingContext)->m_context.get() : nullptr;
    XUniqueGLXContext context(glXCreateContext(display, visualInfoList.get(), glxSharingContext, True));
    if (!context)
        return nullptr;

    return std::make_unique<GLContextGLX>(WTFMove(context), window);
}

std::unique_ptr<GLContextGLX> GLContextGLX::createPbufferContext(GLXContext sharingContext)
{
    static const int fbConfigAttributes[] = {
        GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 1,
        GLX_DOUBLEBUFFER, GL_FALSE,
        0
    };

    int returnedElements;
    Display* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
    XUniquePtr<GLXFBConfig> configs(glXChooseFBConfig(display, 0, fbConfigAttributes, &returnedElements));
    if (!returnedElements)
        return nullptr;

    // We will be rendering to a texture, so our pbuffer does not need to be large.
    static const int pbufferAttributes[] = { GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, 0 };
    XUniqueGLXPbuffer pbuffer(glXCreatePbuffer(display, configs.get()[0], pbufferAttributes));
    if (!pbuffer)
        return nullptr;

    XUniqueGLXContext context(glXCreateNewContext(display, configs.get()[0], GLX_RGBA_TYPE, sharingContext, GL_TRUE));
    if (!context)
        return nullptr;

    return std::make_unique<GLContextGLX>(WTFMove(context), WTFMove(pbuffer));
}

std::unique_ptr<GLContextGLX> GLContextGLX::createPixmapContext(GLXContext sharingContext)
{
    static int visualAttributes[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 1,
        0
    };

    Display* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
    XUniquePtr<XVisualInfo> visualInfo(glXChooseVisual(display, DefaultScreen(display), visualAttributes));
    if (!visualInfo)
        return nullptr;

    XUniqueGLXContext context(glXCreateContext(display, visualInfo.get(), sharingContext, GL_TRUE));
    if (!context)
        return nullptr;

    XUniquePixmap pixmap(XCreatePixmap(display, DefaultRootWindow(display), 1, 1, visualInfo->depth));
    if (!pixmap)
        return nullptr;

    XUniqueGLXPixmap glxPixmap(glXCreateGLXPixmap(display, visualInfo.get(), pixmap.get()));
    if (!glxPixmap)
        return nullptr;

    return std::make_unique<GLContextGLX>(WTFMove(context), WTFMove(pixmap), WTFMove(glxPixmap));
}

std::unique_ptr<GLContextGLX> GLContextGLX::createContext(XID window, GLContext* sharingContext)
{
    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
        success = initializeOpenGLShims();
        initialized = true;
    }
    if (!success)
        return nullptr;

    GLXContext glxSharingContext = sharingContext ? static_cast<GLContextGLX*>(sharingContext)->m_context.get() : nullptr;
    auto context = window ? createWindowContext(window, sharingContext) : nullptr;
    if (!context)
        context = createPbufferContext(glxSharingContext);
    if (!context)
        context = createPixmapContext(glxSharingContext);
    if (!context)
        return nullptr;

    return context;
}

GLContextGLX::GLContextGLX(XUniqueGLXContext&& context, XID window)
    : m_context(WTFMove(context))
    , m_window(window)
{
}

GLContextGLX::GLContextGLX(XUniqueGLXContext&& context, XUniqueGLXPbuffer&& pbuffer)
    : m_context(WTFMove(context))
    , m_pbuffer(WTFMove(pbuffer))
{
}

GLContextGLX::GLContextGLX(XUniqueGLXContext&& context, XUniquePixmap&& pixmap, XUniqueGLXPixmap&& glxPixmap)
    : m_context(WTFMove(context))
    , m_pixmap(WTFMove(pixmap))
    , m_glxPixmap(WTFMove(glxPixmap))
{
}

GLContextGLX::~GLContextGLX()
{
    if (m_cairoDevice)
        cairo_device_destroy(m_cairoDevice);

    if (m_context) {
        // This may be necessary to prevent crashes with NVidia's closed source drivers. Originally
        // from Mozilla's 3D canvas implementation at: http://bitbucket.org/ilmari/canvas3d/
        Display* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glXMakeCurrent(display, None, None);
    }
}

bool GLContextGLX::canRenderToDefaultFramebuffer()
{
    return m_window;
}

IntSize GLContextGLX::defaultFrameBufferSize()
{
    if (!canRenderToDefaultFramebuffer() || !m_window)
        return IntSize();

    int x, y;
    Window rootWindow;
    unsigned int width, height, borderWidth, depth;
    Display* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
    if (!XGetGeometry(display, m_window, &rootWindow, &x, &y, &width, &height, &borderWidth, &depth))
        return IntSize();

    return IntSize(width, height);
}

bool GLContextGLX::makeContextCurrent()
{
    ASSERT(m_context && (m_window || m_pbuffer || m_glxPixmap));

    GLContext::makeContextCurrent();
    if (glXGetCurrentContext() == m_context.get())
        return true;

    Display* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
    if (m_window)
        return glXMakeCurrent(display, m_window, m_context.get());

    if (m_pbuffer)
        return glXMakeCurrent(display, m_pbuffer.get(), m_context.get());

    return ::glXMakeCurrent(display, m_glxPixmap.get(), m_context.get());
}

void GLContextGLX::swapBuffers()
{
    if (m_window)
        glXSwapBuffers(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native(), m_window);
}

void GLContextGLX::waitNative()
{
    glXWaitX();
}

cairo_device_t* GLContextGLX::cairoDevice()
{
    if (m_cairoDevice)
        return m_cairoDevice;

#if ENABLE(ACCELERATED_2D_CANVAS) && CAIRO_HAS_GLX_FUNCTIONS
    m_cairoDevice = cairo_glx_device_create(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native(), m_context.get());
#endif

    return m_cairoDevice;
}

#if ENABLE(GRAPHICS_CONTEXT_3D)
PlatformGraphicsContext3D GLContextGLX::platformContext()
{
    return m_context.get();
}
#endif

} // namespace WebCore

#endif // USE(GLX)
