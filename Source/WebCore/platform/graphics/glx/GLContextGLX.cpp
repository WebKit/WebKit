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
#include <GL/glx.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

// We do not want to call glXMakeContextCurrent using different Display pointers,
// because it might lead to crashes in some drivers (fglrx). We use a shared display
// pointer here.
static Display* gSharedDisplay = 0;
static Display* sharedDisplay()
{
    if (!gSharedDisplay)
        gSharedDisplay = XOpenDisplay(0);
    return gSharedDisplay;
}

// Because of driver bugs, exiting the program when there are active pbuffers
// can crash the X server (this has been observed with the official Nvidia drivers).
// We need to ensure that we clean everything up on exit. There are several reasons
// that GraphicsContext3Ds will still be alive at exit, including user error (memory
// leaks) and the page cache. In any case, we don't want the X server to crash.
typedef Vector<GLContext*> ActiveContextList;
static ActiveContextList& activeContextList()
{
    DEFINE_STATIC_LOCAL(ActiveContextList, activeContexts, ());
    return activeContexts;
}

void GLContextGLX::addActiveContext(GLContextGLX* context)
{
    static bool addedAtExitHandler = false;
    if (!addedAtExitHandler) {
        atexit(&GLContextGLX::cleanupActiveContextsAtExit);
        addedAtExitHandler = true;
    }
    activeContextList().append(context);
}

void GLContextGLX::removeActiveContext(GLContext* context)
{
    ActiveContextList& contextList = activeContextList();
    size_t i = contextList.find(context);
    if (i != notFound)
        contextList.remove(i);
}

void GLContextGLX::cleanupActiveContextsAtExit()
{
    ActiveContextList& contextList = activeContextList();
    for (size_t i = 0; i < contextList.size(); ++i)
        delete contextList[i];

    if (!gSharedDisplay)
        return;
    XCloseDisplay(gSharedDisplay);
    gSharedDisplay = 0;
}

GLContext* GLContextGLX::createOffscreenSharingContext()
{
    return createContext(0, m_context);
}

GLContextGLX* GLContextGLX::createWindowContext(XID window, GLXContext sharingContext)
{
    Display* display = sharedDisplay();
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display, window, &attributes))
        return 0;

    XVisualInfo visualInfo;
    visualInfo.visualid = XVisualIDFromVisual(attributes.visual);

    int numReturned = 0;
    XVisualInfo* visualInfoList = XGetVisualInfo(display, VisualIDMask, &visualInfo, &numReturned);
    GLXContext context = glXCreateContext(display, visualInfoList, sharingContext, True);
    XFree(visualInfoList);

    if (!context)
        return 0;

    // GLXPbuffer and XID are both the same types underneath, so we have to share
    // a constructor here with the window path.
    GLContextGLX* contextWrapper = new GLContextGLX(context);
    contextWrapper->m_window = window;
    return contextWrapper;
}

GLContextGLX* GLContextGLX::createPbufferContext(GLXContext sharingContext)
{
    int fbConfigAttributes[] = {
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
    Display* display = sharedDisplay();
    GLXFBConfig* configs = glXChooseFBConfig(display, 0, fbConfigAttributes, &returnedElements);
    if (!returnedElements) {
        XFree(configs);
        return 0;
    }

    // We will be rendering to a texture, so our pbuffer does not need to be large.
    static const int pbufferAttributes[] = { GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, 0 };
    GLXPbuffer pbuffer = glXCreatePbuffer(display, configs[0], pbufferAttributes);
    if (!pbuffer) {
        XFree(configs);
        return 0;
    }

    GLXContext context = glXCreateNewContext(display, configs[0], GLX_RGBA_TYPE, sharingContext, GL_TRUE);
    XFree(configs);
    if (!context)
        return 0;

    // GLXPbuffer and XID are both the same types underneath, so we have to share
    // a constructor here with the window path.
    GLContextGLX* contextWrapper = new GLContextGLX(context);
    contextWrapper->m_pbuffer = pbuffer;
    return contextWrapper;
}

GLContextGLX* GLContextGLX::createPixmapContext(GLXContext sharingContext)
{
    static int visualAttributes[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 1,
        0
    };

    Display* display = sharedDisplay();
    XVisualInfo* visualInfo = glXChooseVisual(display, DefaultScreen(display), visualAttributes);
    if (!visualInfo)
        return 0;

    GLXContext context = glXCreateContext(display, visualInfo, sharingContext, GL_TRUE);
    if (!context) {
        XFree(visualInfo);
        return 0;
    }

    Pixmap pixmap = XCreatePixmap(display, DefaultRootWindow(display), 1, 1, visualInfo->depth);
    if (!pixmap) {
        XFree(visualInfo);
        return 0;
    }

    GLXPixmap glxPixmap = glXCreateGLXPixmap(display, visualInfo, pixmap);
    if (!glxPixmap) {
        XFreePixmap(display, pixmap);
        XFree(visualInfo);
        return 0;
    }

    return new GLContextGLX(context, pixmap, glxPixmap);
}

GLContextGLX* GLContextGLX::createContext(XID window, GLXContext sharingContext)
{
    if (!sharedDisplay())
        return 0;

    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
        success = initializeOpenGLShims();
        initialized = true;
    }
    if (!success)
        return 0;

    GLContextGLX* context = window ? createWindowContext(window, sharingContext) : 0;
    if (!context)
        context = createPbufferContext(sharingContext);
    if (!context)
        context = createPixmapContext(sharingContext);
    if (!context)
        return 0;

    return context;
}

GLContextGLX::GLContextGLX(GLXContext context)
    : m_context(context)
    , m_window(0)
    , m_pbuffer(0)
    , m_pixmap(0)
    , m_glxPixmap(0)
{
    addActiveContext(this);
}

GLContextGLX::GLContextGLX(GLXContext context, Pixmap pixmap, GLXPixmap glxPixmap)
    : m_context(context)
    , m_window(0)
    , m_pbuffer(0)
    , m_pixmap(pixmap)
    , m_glxPixmap(glxPixmap)
{
    addActiveContext(this);
}

GLContextGLX::~GLContextGLX()
{
    if (m_context) {
        // This may be necessary to prevent crashes with NVidia's closed source drivers. Originally
        // from Mozilla's 3D canvas implementation at: http://bitbucket.org/ilmari/canvas3d/
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glXMakeCurrent(sharedDisplay(), None, None);
        glXDestroyContext(sharedDisplay(), m_context);
    }

    if (m_pbuffer) {
        glXDestroyPbuffer(sharedDisplay(), m_pbuffer);
        m_pbuffer = 0;
    }
    if (m_glxPixmap) {
        glXDestroyGLXPixmap(sharedDisplay(), m_glxPixmap);
        m_glxPixmap = 0;
    }
    if (m_pixmap) {
        XFreePixmap(sharedDisplay(), m_pixmap);
        m_pixmap = 0;
    }
    removeActiveContext(this);
}

bool GLContextGLX::canRenderToDefaultFramebuffer()
{
    return m_window;
}

bool GLContextGLX::makeContextCurrent()
{
    ASSERT(m_context && (m_window || m_pbuffer || m_glxPixmap));

    GLContext::makeContextCurrent();
    if (glXGetCurrentContext() == m_context)
        return true;

    if (m_window)
        return glXMakeCurrent(sharedDisplay(), m_window, m_context);

    if (m_pbuffer)
        return glXMakeCurrent(sharedDisplay(), m_pbuffer, m_context);

    return ::glXMakeCurrent(sharedDisplay(), m_glxPixmap, m_context);
}

void GLContextGLX::swapBuffers()
{
    if (m_window)
        glXSwapBuffers(sharedDisplay(), m_window);
}

#if ENABLE(WEBGL)
PlatformGraphicsContext3D GLContextGLX::platformContext()
{
    return m_context;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL) || && USE(TEXTURE_MAPPER_GL)
