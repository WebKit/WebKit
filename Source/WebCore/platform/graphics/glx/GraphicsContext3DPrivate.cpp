/*
 * Copyright (C) 2011 Igalia S.L.
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

#include "config.h"
#include "GraphicsContext3DPrivate.h"

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"
#include "OpenGLShims.h"
#include <GL/glx.h>
#include <dlfcn.h>

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

namespace WebCore {

// Because of driver bugs, exiting the program when there are active pbuffers
// can crash the X server (this has been observed with the official Nvidia drivers).
// We need to ensure that we clean everything up on exit. There are several reasons
// that GraphicsContext3Ds will still be alive at exit, including user error (memory
// leaks) and the page cache. In any case, we don't want the X server to crash.
static bool cleaningUpAtExit = false;
static Vector<GraphicsContext3D*>& activeGraphicsContexts()
{
    DEFINE_STATIC_LOCAL(Vector<GraphicsContext3D*>, contexts, ());
    return contexts;
}

void GraphicsContext3DPrivate::addActiveGraphicsContext(GraphicsContext3D* context)
{
    static bool addedAtExitHandler = false;
    if (!addedAtExitHandler) {
        atexit(&GraphicsContext3DPrivate::cleanupActiveContextsAtExit);
        addedAtExitHandler = true;
    }
    activeGraphicsContexts().append(context);
}

void GraphicsContext3DPrivate::removeActiveGraphicsContext(GraphicsContext3D* context)
{
    if (cleaningUpAtExit)
        return;

    Vector<GraphicsContext3D*>& contexts = activeGraphicsContexts();
    size_t location = contexts.find(context);
    if (location != WTF::notFound)
        contexts.remove(location);
}

void GraphicsContext3DPrivate::cleanupActiveContextsAtExit()
{
    cleaningUpAtExit = true;

    Vector<GraphicsContext3D*>& contexts = activeGraphicsContexts();
    for (size_t i = 0; i < contexts.size(); i++)
        contexts[i]->~GraphicsContext3D();

    if (!gSharedDisplay)
        return;
    XCloseDisplay(gSharedDisplay);
    gSharedDisplay = 0;
}

PassOwnPtr<GraphicsContext3DPrivate> GraphicsContext3DPrivate::create()
{
    if (!sharedDisplay())
        return nullptr;

    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
        success = initializeOpenGLShims();
        initialized = true;
    }
    if (!success)
        return nullptr;

    GraphicsContext3DPrivate* internal = createPbufferContext();
    if (!internal)
        internal = createPixmapContext();
    if (!internal)
        return nullptr;

    // The GraphicsContext3D constructor requires that this context is the current OpenGL context.
    internal->makeContextCurrent();
    return adoptPtr(internal);
}

GraphicsContext3DPrivate* GraphicsContext3DPrivate::createPbufferContext()
{
    int fbConfigAttributes[] = {
        GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 1,
        GLX_DEPTH_SIZE, 1,
        GLX_STENCIL_SIZE, 1,
        GLX_SAMPLE_BUFFERS, 1,
        GLX_DOUBLEBUFFER, GL_FALSE,
        GLX_SAMPLES, 4,
        0
    };
    int returnedElements;
    GLXFBConfig* configs = glXChooseFBConfig(sharedDisplay(), 0, fbConfigAttributes, &returnedElements);
    if (!configs) {
        fbConfigAttributes[20] = 0; // Attempt without anti-aliasing.
        configs = glXChooseFBConfig(sharedDisplay(), 0, fbConfigAttributes, &returnedElements);
    }
    if (!returnedElements) {
        XFree(configs);
        return 0;
    }

    // We will be rendering to a texture, so our pbuffer does not need to be large.
    static const int pbufferAttributes[] = { GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, 0 };
    GLXPbuffer pbuffer = glXCreatePbuffer(sharedDisplay(), configs[0], pbufferAttributes);
    if (!pbuffer) {
        XFree(configs);
        return 0;
    }

    GLXContext context = glXCreateNewContext(sharedDisplay(), configs[0], GLX_RGBA_TYPE, 0, GL_TRUE);
    XFree(configs);
    if (!context)
        return 0;
    return new GraphicsContext3DPrivate(context, pbuffer);
}

GraphicsContext3DPrivate* GraphicsContext3DPrivate::createPixmapContext()
{
    static int visualAttributes[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 1,
        GLX_DOUBLEBUFFER,
        0
    };

    XVisualInfo* visualInfo = glXChooseVisual(sharedDisplay(), DefaultScreen(sharedDisplay()), visualAttributes);
    if (!visualInfo)
        return 0;
 
    GLXContext context = glXCreateContext(sharedDisplay(), visualInfo, 0, GL_TRUE);
    if (!context) {
        XFree(visualInfo);
        return 0;
    }

    Pixmap pixmap = XCreatePixmap(sharedDisplay(), DefaultRootWindow(sharedDisplay()), 1, 1, visualInfo->depth);
    if (!pixmap) {
        XFree(visualInfo);
        return 0;
    }

    GLXPixmap glxPixmap = glXCreateGLXPixmap(sharedDisplay(), visualInfo, pixmap);
    if (!glxPixmap) {
        XFreePixmap(sharedDisplay(), pixmap);
        XFree(visualInfo);
        return 0;
    }

    return new GraphicsContext3DPrivate(context, pixmap, glxPixmap);
}

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GLXContext context, GLXPbuffer pbuffer)
    : m_context(context)
    , m_pbuffer(pbuffer)
    , m_pixmap(0)
    , m_glxPixmap(0)
{
}

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GLXContext context, Pixmap pixmap, GLXPixmap glxPixmap)
    : m_context(context)
    , m_pbuffer(0)
    , m_pixmap(pixmap)
    , m_glxPixmap(glxPixmap)
{
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
    if (m_context) {
        // This may be necessary to prevent crashes with NVidia's closed source drivers. Originally
        // from Mozilla's 3D canvas implementation at: http://bitbucket.org/ilmari/canvas3d/
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

        ::glXMakeContextCurrent(sharedDisplay(), 0, 0, 0);
        ::glXDestroyContext(sharedDisplay(), m_context);
        m_context = 0;
    }

    if (m_pbuffer) {
        ::glXDestroyPbuffer(sharedDisplay(), m_pbuffer);
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
}

bool GraphicsContext3DPrivate::makeContextCurrent()
{
    if (::glXGetCurrentContext() == m_context)
        return true;
    if (!m_context)
        return false;
    if (m_pbuffer)
        return ::glXMakeCurrent(sharedDisplay(), m_pbuffer, m_context);

    ASSERT(m_glxPixmap);
    return ::glXMakeCurrent(sharedDisplay(), m_glxPixmap, m_context);
}

} // namespace WebCore

#endif // ENABLE_WEBGL
