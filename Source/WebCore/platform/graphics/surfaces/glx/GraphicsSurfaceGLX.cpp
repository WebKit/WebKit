/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GraphicsSurface.h"

#if USE(GRAPHICS_SURFACE)

#include "NotImplemented.h"
#include "TextureMapperGL.h"

#if PLATFORM(QT)
// Qt headers must be included before glx headers.
#include <QGuiApplication>
#include <QOpenGLContext>
#include <qpa/qplatformnativeinterface.h>
#elif PLATFORM(EFL)
#include <GL/gl.h>
#endif

#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>

namespace WebCore {

static long X11OverrideRedirect = 1L << 9;

static PFNGLXBINDTEXIMAGEEXTPROC pGlXBindTexImageEXT = 0;
static PFNGLXRELEASETEXIMAGEEXTPROC pGlXReleaseTexImageEXT = 0;
static PFNGLBINDFRAMEBUFFERPROC pGlBindFramebuffer = 0;
static PFNGLBLITFRAMEBUFFERPROC pGlBlitFramebuffer = 0;
static PFNGLGENFRAMEBUFFERSPROC pGlGenFramebuffers = 0;
static PFNGLDELETEFRAMEBUFFERSPROC pGlDeleteFramebuffers = 0;
static PFNGLFRAMEBUFFERTEXTURE2DPROC pGlFramebufferTexture2D = 0;

static int attributes[] = {
    GLX_LEVEL, 0,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_RED_SIZE,      1,
    GLX_GREEN_SIZE,    1,
    GLX_BLUE_SIZE,     1,
    GLX_ALPHA_SIZE,    1,
    GLX_DEPTH_SIZE,    1,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    GLX_DOUBLEBUFFER,  True,
    None
};

class OffScreenRootWindow {
public:
    OffScreenRootWindow()
    {
        ++m_refCount;
    }

    Window getXWindow()
    {
        if (!m_window) {
            Display* dpy = display();
            m_window = XCreateSimpleWindow(dpy, XDefaultRootWindow(dpy), -1, -1, 1, 1, 0, BlackPixel(dpy, 0), WhitePixel(dpy, 0));
            XSetWindowAttributes attributes;
            attributes.override_redirect = true;
            XChangeWindowAttributes(dpy, m_window, X11OverrideRedirect, &attributes);
            // Map window to the screen
            XMapWindow(dpy, m_window);
        }

        return m_window;
    }

    Display* display()
    {
        if (!m_display)
            m_display = XOpenDisplay(0);
        return m_display;
    }

    ~OffScreenRootWindow()
    {
        if (--m_refCount || !m_display)
            return;

        if (m_window) {
            XUnmapWindow(m_display, m_window);
            XDestroyWindow(m_display, m_window);
            m_window = 0;
        }

        XCloseDisplay(m_display);
        m_display = 0;
    }

private:
    static int m_refCount;
    static Window m_window;
    static Display* m_display;
};

int OffScreenRootWindow::m_refCount = 0;
Window OffScreenRootWindow::m_window = 0;
Display* OffScreenRootWindow::m_display = 0;

static const int glxSpec[] = {
    // The specification is a set key value pairs stored in a simple array.
    GLX_LEVEL,                          0,
    GLX_DRAWABLE_TYPE,                  GLX_PIXMAP_BIT | GLX_WINDOW_BIT,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT,    GLX_TEXTURE_2D_BIT_EXT,
    GLX_BIND_TO_TEXTURE_RGBA_EXT,       TRUE,
    0
};

static const int glxAttributes[] = {
    GLX_TEXTURE_FORMAT_EXT,
    GLX_TEXTURE_FORMAT_RGBA_EXT,
    GLX_TEXTURE_TARGET_EXT,
    GLX_TEXTURE_2D_EXT,
    0
};

struct GraphicsSurfacePrivate {
    GraphicsSurfacePrivate(const PlatformGraphicsContext3D shareContext = 0)
        : m_display(m_offScreenWindow.display())
        , m_xPixmap(0)
        , m_glxPixmap(0)
        , m_surface(0)
        , m_glxSurface(0)
        , m_glContext(0)
        , m_detachedContext(0)
        , m_detachedSurface(0)
        , m_fbConfig(0)
        , m_textureIsYInverted(false)
        , m_hasAlpha(false)
        , m_isReceiver(false)
    {
        GLXContext shareContextObject = 0;

#if PLATFORM(QT)
        if (shareContext) {
            QPlatformNativeInterface* nativeInterface = QGuiApplication::platformNativeInterface();
            shareContextObject = static_cast<GLXContext>(nativeInterface->nativeResourceForContext(QByteArrayLiteral("glxcontext"), shareContext));
            if (!shareContextObject)
                return;
        }
#else
        UNUSED_PARAM(shareContext);
#endif

        int numReturned;
        GLXFBConfig* fbConfigs = glXChooseFBConfig(m_display, DefaultScreen(m_display), attributes, &numReturned);

        // Make sure that we choose a configuration that supports an alpha mask.
        m_fbConfig = findFBConfigWithAlpha(fbConfigs, numReturned);

        XFree(fbConfigs);

        // Create a GLX context for OpenGL rendering
        m_glContext = glXCreateNewContext(m_display, m_fbConfig, GLX_RGBA_TYPE, shareContextObject, true);
    }

    GraphicsSurfacePrivate(uint32_t winId)
        : m_display(m_offScreenWindow.display())
        , m_xPixmap(0)
        , m_glxPixmap(0)
        , m_surface(winId)
        , m_glxSurface(0)
        , m_glContext(0)
        , m_detachedContext(0)
        , m_detachedSurface(0)
        , m_fbConfig(0)
        , m_textureIsYInverted(false)
        , m_hasAlpha(false)
        , m_isReceiver(true)
    { }

    ~GraphicsSurfacePrivate()
    {
        if (m_glxPixmap)
            glXDestroyPixmap(m_display, m_glxPixmap);
        m_glxPixmap = 0;

        if (m_xPixmap)
            XFreePixmap(m_display, m_xPixmap);
        m_xPixmap = 0;

        if (m_glContext)
            glXDestroyContext(m_display, m_glContext);
    }

    uint32_t createSurface(const IntSize& size)
    {
        XVisualInfo* visualInfo = glXGetVisualFromFBConfig(m_display, m_fbConfig);
        if (!visualInfo)
            return 0;

        Colormap cmap = XCreateColormap(m_display, m_offScreenWindow.getXWindow(), visualInfo->visual, AllocNone);

        XSetWindowAttributes a;
        a.background_pixel = WhitePixel(m_display, 0);
        a.border_pixel = BlackPixel(m_display, 0);
        a.colormap = cmap;
        m_surface = XCreateWindow(m_display, m_offScreenWindow.getXWindow(), 0, 0, size.width(), size.height(),
            0, visualInfo->depth, InputOutput, visualInfo->visual,
            CWBackPixel | CWBorderPixel | CWColormap, &a);
        XSetWindowBackgroundPixmap(m_display, m_surface, 0);
        XCompositeRedirectWindow(m_display, m_surface, CompositeRedirectManual);
        m_glxSurface = glXCreateWindow(m_display, m_fbConfig, m_surface, 0);
        XFree(visualInfo);

        // Make sure the XRender Extension is available.
        int eventBasep, errorBasep;
        if (!XRenderQueryExtension(m_display, &eventBasep, &errorBasep))
            return 0;

        XMapWindow(m_display, m_surface);
        return m_surface;
    }

    void createPixmap(uint32_t winId)
    {
        XWindowAttributes attr;
        XGetWindowAttributes(m_display, winId, &attr);

        XRenderPictFormat* format = XRenderFindVisualFormat(m_display, attr.visual);
        m_hasAlpha = (format->type == PictTypeDirect && format->direct.alphaMask);

        int numberOfConfigs;
        GLXFBConfig* configs = glXChooseFBConfig(m_display, XDefaultScreen(m_display), glxSpec, &numberOfConfigs);

        // If origin window has alpha then find config with alpha.
        GLXFBConfig& config = m_hasAlpha ? findFBConfigWithAlpha(configs, numberOfConfigs) : configs[0];

        m_xPixmap = XCompositeNameWindowPixmap(m_display, winId);
        m_glxPixmap = glXCreatePixmap(m_display, config, m_xPixmap, glxAttributes);

        uint inverted = 0;
        glXQueryDrawable(m_display, m_glxPixmap, GLX_Y_INVERTED_EXT, &inverted);
        m_textureIsYInverted = !!inverted;

        XFree(configs);
    }

    bool textureIsYInverted()
    {
        return m_textureIsYInverted;
    }

    void makeCurrent()
    {
        m_detachedContext = glXGetCurrentContext();
        m_detachedSurface = glXGetCurrentDrawable();
        if (m_surface && m_glContext)
            glXMakeCurrent(m_display, m_surface, m_glContext);
    }

    void doneCurrent()
    {
        if (m_detachedContext)
            glXMakeCurrent(m_display, m_detachedSurface, m_detachedContext);
        m_detachedContext = 0;
    }

    void swapBuffers()
    {
        // The buffers are being switched on the writing side, the reading side just reads
        // whatever texture the XWindow contains.
        if (m_isReceiver)
            return;

        GLXContext glContext = glXGetCurrentContext();

        if (m_surface && glContext) {
            GLint oldFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
            pGlBindFramebuffer(GL_FRAMEBUFFER, 0);
            glXSwapBuffers(m_display, m_surface);
            pGlBindFramebuffer(GL_FRAMEBUFFER, oldFBO);
        }
    }

    void copyFromTexture(uint32_t texture, const IntRect& sourceRect)
    {
        makeCurrent();
        int x = sourceRect.x();
        int y = sourceRect.y();
        int width = sourceRect.width();
        int height = sourceRect.height();

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        GLint previousFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);

        GLuint originFBO;
        pGlGenFramebuffers(1, &originFBO);
        pGlBindFramebuffer(GL_READ_FRAMEBUFFER, originFBO);
        glBindTexture(GL_TEXTURE_2D, texture);
        pGlFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        pGlBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        pGlBlitFramebuffer(x, y, width, height, x, y, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        pGlFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        pGlBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
        pGlDeleteFramebuffers(1, &originFBO);

        glPopAttrib();

        swapBuffers();
        doneCurrent();
    }

    Display* display() const { return m_display; }

    GLXPixmap glxPixmap() const
    {
        if (!m_glxPixmap && m_surface)
            const_cast<GraphicsSurfacePrivate*>(this)->createPixmap(m_surface);
        return m_glxPixmap;
    }

    IntSize size() const
    {
        if (m_size.isEmpty()) {
            XWindowAttributes attr;
            XGetWindowAttributes(m_display, m_surface, &attr);
            const_cast<GraphicsSurfacePrivate*>(this)->m_size = IntSize(attr.width, attr.height);
        }
        return m_size;
    }

    bool isReceiver() const { return m_isReceiver; }
private:
    GLXFBConfig& findFBConfigWithAlpha(GLXFBConfig* fbConfigs, int numberOfConfigs)
    {
        for (int i = 0; i < numberOfConfigs; ++i) {
            XVisualInfo* visualInfo = glXGetVisualFromFBConfig(m_display, fbConfigs[i]);
            if (!visualInfo)
                continue;

            XRenderPictFormat* format = XRenderFindVisualFormat(m_display, visualInfo->visual);
            XFree(visualInfo);
            if (format && format->direct.alphaMask > 0) {
                return fbConfigs[i];
                break;
            }
        }

        // Return 1st config as a fallback with no alpha support.
        return fbConfigs[0];
    }

    OffScreenRootWindow m_offScreenWindow;
    IntSize m_size;
    Display* m_display;
    Pixmap m_xPixmap;
    GLXPixmap m_glxPixmap;
    Window m_surface;
    Window m_glxSurface;
    GLXContext m_glContext;
    GLXContext m_detachedContext;
    GLXDrawable m_detachedSurface;
    GLXFBConfig m_fbConfig;
    bool m_textureIsYInverted;
    bool m_hasAlpha;
    bool m_isReceiver;
};

static bool resolveGLMethods()
{
    static bool resolved = false;
    if (resolved)
        return true;
    pGlXBindTexImageEXT = reinterpret_cast<PFNGLXBINDTEXIMAGEEXTPROC>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXBindTexImageEXT")));
    pGlXReleaseTexImageEXT = reinterpret_cast<PFNGLXRELEASETEXIMAGEEXTPROC>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXReleaseTexImageEXT")));
    pGlBindFramebuffer = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glBindFramebuffer")));
    pGlBlitFramebuffer = reinterpret_cast<PFNGLBLITFRAMEBUFFERPROC>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glBlitFramebuffer")));

    pGlGenFramebuffers = reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glGenFramebuffers")));
    pGlDeleteFramebuffers = reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glDeleteFramebuffers")));
    pGlFramebufferTexture2D = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DPROC>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glFramebufferTexture2D")));
    resolved = pGlBlitFramebuffer && pGlBindFramebuffer && pGlXBindTexImageEXT && pGlXReleaseTexImageEXT;

    return resolved;
}

GraphicsSurfaceToken GraphicsSurface::platformExport()
{
    return GraphicsSurfaceToken(m_platformSurface);
}

uint32_t GraphicsSurface::platformGetTextureID()
{
    if (!m_texture) {
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        pGlXBindTexImageEXT(m_private->display(), m_private->glxPixmap(), GLX_FRONT_EXT, 0);
    }

    return m_texture;
}

void GraphicsSurface::platformCopyToGLTexture(uint32_t /*target*/, uint32_t /*id*/, const IntRect& /*targetRect*/, const IntPoint& /*offset*/)
{
    // This is not supported by GLX/Xcomposite.
}

void GraphicsSurface::platformCopyFromTexture(uint32_t texture, const IntRect& sourceRect)
{
    m_private->copyFromTexture(texture, sourceRect);
}


void GraphicsSurface::platformPaintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity, BitmapTexture* mask)
{
    TextureMapperGL* texMapGL = static_cast<TextureMapperGL*>(textureMapper);
    TransformationMatrix adjustedTransform = transform;
    adjustedTransform.multiply(TransformationMatrix::rectToRect(FloatRect(FloatPoint::zero(), m_private->size()), targetRect));
    TextureMapperGL::Flags flags = m_private->textureIsYInverted() ? TextureMapperGL::ShouldFlipTexture : 0;
    flags |= TextureMapperGL::ShouldBlend;
    texMapGL->drawTexture(platformGetTextureID(), flags, m_private->size(), targetRect, adjustedTransform, opacity, mask);
}

uint32_t GraphicsSurface::platformFrontBuffer() const
{
    return 0;
}

uint32_t GraphicsSurface::platformSwapBuffers()
{
    if (m_private->isReceiver()) {
        glBindTexture(GL_TEXTURE_2D, platformGetTextureID());
        // Release previous lock and rebind texture to surface to get frame update.
        pGlXReleaseTexImageEXT(m_private->display(), m_private->glxPixmap(), GLX_FRONT_EXT);
        pGlXBindTexImageEXT(m_private->display(), m_private->glxPixmap(), GLX_FRONT_EXT, 0);
        return 0;
    }

    m_private->swapBuffers();
    return 0;
}

IntSize GraphicsSurface::platformSize() const
{
    return m_private->size();
}

PassRefPtr<GraphicsSurface> GraphicsSurface::platformCreate(const IntSize& size, Flags flags, const PlatformGraphicsContext3D shareContext)
{
    // X11 does not support CopyToTexture, so we do not create a GraphicsSurface if this is requested.
    // GraphicsSurfaceGLX uses an XWindow as native surface. This one always has a front and a back buffer.
    // Therefore single buffered GraphicsSurfaces are not supported.
    if (flags & SupportsCopyToTexture || flags & SupportsSingleBuffered)
        return PassRefPtr<GraphicsSurface>();

    RefPtr<GraphicsSurface> surface = adoptRef(new GraphicsSurface(size, flags));

    surface->m_private = new GraphicsSurfacePrivate(shareContext);
    if (!resolveGLMethods())
        return PassRefPtr<GraphicsSurface>();

    surface->m_platformSurface = surface->m_private->createSurface(size);

    return surface;
}

PassRefPtr<GraphicsSurface> GraphicsSurface::platformImport(const IntSize& size, Flags flags, const GraphicsSurfaceToken& token)
{
    // X11 does not support CopyToTexture, so we do not create a GraphicsSurface if this is requested.
    // GraphicsSurfaceGLX uses an XWindow as native surface. This one always has a front and a back buffer.
    // Therefore single buffered GraphicsSurfaces are not supported.
    if (flags & SupportsCopyToTexture || flags & SupportsSingleBuffered)
        return PassRefPtr<GraphicsSurface>();

    RefPtr<GraphicsSurface> surface = adoptRef(new GraphicsSurface(size, flags));
    surface->m_platformSurface = token.frontBufferHandle;

    surface->m_private = new GraphicsSurfacePrivate(surface->m_platformSurface);
    if (!resolveGLMethods())
        return PassRefPtr<GraphicsSurface>();

    return surface;
}

char* GraphicsSurface::platformLock(const IntRect&, int* /*outputStride*/, LockOptions)
{
    // GraphicsSurface is currently only being used for WebGL, which does not require this locking mechanism.
    return 0;
}

void GraphicsSurface::platformUnlock()
{
    // GraphicsSurface is currently only being used for WebGL, which does not require this locking mechanism.
}

void GraphicsSurface::platformDestroy()
{
    if (m_texture) {
        pGlXReleaseTexImageEXT(m_private->display(), m_private->glxPixmap(), GLX_FRONT_EXT);
        glDeleteTextures(1, &m_texture);
    }

    delete m_private;
    m_private = 0;
}

#if !PLATFORM(QT)
PassOwnPtr<GraphicsContext> GraphicsSurface::platformBeginPaint(const IntSize&, char*, int)
{
    notImplemented();
    return nullptr;
}

PassRefPtr<Image> GraphicsSurface::createReadOnlyImage(const IntRect&)
{
    notImplemented();
    return 0;
}
#endif
}
#endif
