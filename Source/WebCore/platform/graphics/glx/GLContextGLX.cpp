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
#include <cstdlib>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(ACCELERATED_2D_CANVAS)
#include <cairo-gl.h>
#endif

namespace WebCore {

// Because of driver bugs, exiting the program when there are active pbuffers
// can crash the X server (this has been observed with the official Nvidia drivers).
// We need to ensure that we clean everything up on exit. There are several reasons
// that GraphicsContext3Ds will still be alive at exit, including user error (memory
// leaks) and the page cache. In any case, we don't want the X server to crash.
static HashSet<GLContextGLX*>& activeContexts()
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<HashSet<GLContextGLX*>> contexts;
    std::call_once(onceFlag, [] {
        contexts.construct();
        std::atexit([] {
            for (auto* context : activeContexts())
                context->clear();
        });
    });
    return contexts;
}

#if !defined(PFNGLXSWAPINTERVALSGIPROC)
typedef int (*PFNGLXSWAPINTERVALSGIPROC) (int);
#endif

static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI;

static bool hasSGISwapControlExtension(Display* display)
{
    static bool initialized = false;
    if (initialized)
        return !!glXSwapIntervalSGI;

    initialized = true;
    if (!GLContext::isExtensionSupported(glXQueryExtensionsString(display, 0), "GLX_SGI_swap_control"))
        return false;

    glXSwapIntervalSGI = reinterpret_cast<PFNGLXSWAPINTERVALSGIPROC>(glXGetProcAddress(reinterpret_cast<const unsigned char*>("glXSwapIntervalSGI")));
    return !!glXSwapIntervalSGI;
}

std::unique_ptr<GLContextGLX> GLContextGLX::createWindowContext(GLNativeWindowType window, PlatformDisplay& platformDisplay, GLXContext sharingContext)
{
    Display* display = downcast<PlatformDisplayX11>(platformDisplay).native();
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display, static_cast<Window>(window), &attributes))
        return nullptr;

    XVisualInfo visualInfo;
    visualInfo.visualid = XVisualIDFromVisual(attributes.visual);

    int numReturned = 0;
    XUniquePtr<XVisualInfo> visualInfoList(XGetVisualInfo(display, VisualIDMask, &visualInfo, &numReturned));

    XUniqueGLXContext context(glXCreateContext(display, visualInfoList.get(), sharingContext, True));
    if (!context)
        return nullptr;

    return std::unique_ptr<GLContextGLX>(new GLContextGLX(platformDisplay, WTFMove(context), window));
}

std::unique_ptr<GLContextGLX> GLContextGLX::createPbufferContext(PlatformDisplay& platformDisplay, GLXContext sharingContext)
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
    Display* display = downcast<PlatformDisplayX11>(platformDisplay).native();
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

    return std::unique_ptr<GLContextGLX>(new GLContextGLX(platformDisplay, WTFMove(context), WTFMove(pbuffer)));
}

std::unique_ptr<GLContextGLX> GLContextGLX::createPixmapContext(PlatformDisplay& platformDisplay, GLXContext sharingContext)
{
    static int visualAttributes[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 1,
        0
    };

    Display* display = downcast<PlatformDisplayX11>(platformDisplay).native();
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

    return std::unique_ptr<GLContextGLX>(new GLContextGLX(platformDisplay, WTFMove(context), WTFMove(pixmap), WTFMove(glxPixmap)));
}

std::unique_ptr<GLContextGLX> GLContextGLX::createContext(GLNativeWindowType window, PlatformDisplay& platformDisplay)
{
    GLXContext glxSharingContext = platformDisplay.sharingGLContext() ? static_cast<GLContextGLX*>(platformDisplay.sharingGLContext())->m_context.get() : nullptr;
    auto context = window ? createWindowContext(window, platformDisplay, glxSharingContext) : nullptr;
    if (!context)
        context = createPbufferContext(platformDisplay, glxSharingContext);
    if (!context)
        context = createPixmapContext(platformDisplay, glxSharingContext);

    return context;
}

std::unique_ptr<GLContextGLX> GLContextGLX::createSharingContext(PlatformDisplay& platformDisplay)
{
    auto context = createPbufferContext(platformDisplay);
    if (!context)
        context = createPixmapContext(platformDisplay);
    return context;
}

GLContextGLX::GLContextGLX(PlatformDisplay& display, XUniqueGLXContext&& context, GLNativeWindowType window)
    : GLContext(display)
    , m_x11Display(downcast<PlatformDisplayX11>(m_display).native())
    , m_context(WTFMove(context))
    , m_window(static_cast<Window>(window))
{
    activeContexts().add(this);
}

GLContextGLX::GLContextGLX(PlatformDisplay& display, XUniqueGLXContext&& context, XUniqueGLXPbuffer&& pbuffer)
    : GLContext(display)
    , m_x11Display(downcast<PlatformDisplayX11>(m_display).native())
    , m_context(WTFMove(context))
    , m_pbuffer(WTFMove(pbuffer))
{
    activeContexts().add(this);
}

GLContextGLX::GLContextGLX(PlatformDisplay& display, XUniqueGLXContext&& context, XUniquePixmap&& pixmap, XUniqueGLXPixmap&& glxPixmap)
    : GLContext(display)
    , m_x11Display(downcast<PlatformDisplayX11>(m_display).native())
    , m_context(WTFMove(context))
    , m_pixmap(WTFMove(pixmap))
    , m_glxPixmap(WTFMove(glxPixmap))
{
    activeContexts().add(this);
}

GLContextGLX::~GLContextGLX()
{
    clear();
    activeContexts().remove(this);
}

void GLContextGLX::clear()
{
    if (!m_context)
        return;

    if (m_cairoDevice) {
        cairo_device_destroy(m_cairoDevice);
        m_cairoDevice = nullptr;
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glXMakeCurrent(m_x11Display, None, None);

    m_context = nullptr;
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
    if (!XGetGeometry(m_x11Display, m_window, &rootWindow, &x, &y, &width, &height, &borderWidth, &depth))
        return IntSize();

    return IntSize(width, height);
}

bool GLContextGLX::makeContextCurrent()
{
    ASSERT(m_context && (m_window || m_pbuffer || m_glxPixmap));

    GLContext::makeContextCurrent();
    if (glXGetCurrentContext() == m_context.get())
        return true;

    if (m_window)
        return glXMakeCurrent(m_x11Display, m_window, m_context.get());

    if (m_pbuffer)
        return glXMakeCurrent(m_x11Display, m_pbuffer.get(), m_context.get());

    return ::glXMakeCurrent(m_x11Display, m_glxPixmap.get(), m_context.get());
}

void GLContextGLX::swapBuffers()
{
    if (m_window)
        glXSwapBuffers(m_x11Display, m_window);
}

void GLContextGLX::waitNative()
{
    glXWaitX();
}

void GLContextGLX::swapInterval(int interval)
{
    if (!hasSGISwapControlExtension(m_x11Display))
        return;
    glXSwapIntervalSGI(interval);
}

cairo_device_t* GLContextGLX::cairoDevice()
{
    if (m_cairoDevice)
        return m_cairoDevice;

#if ENABLE(ACCELERATED_2D_CANVAS) && CAIRO_HAS_GLX_FUNCTIONS
    m_cairoDevice = cairo_glx_device_create(m_x11Display, m_context.get());
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
