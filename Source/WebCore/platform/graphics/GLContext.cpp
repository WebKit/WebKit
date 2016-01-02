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

#if ENABLE(GRAPHICS_CONTEXT_3D)

#include "GLContext.h"

#include "PlatformDisplay.h"
#include <wtf/ThreadSpecific.h>

#if USE(EGL)
#include "GLContextEGL.h"
#endif

#if USE(GLX)
#include "GLContextGLX.h"
#endif

using WTF::ThreadSpecific;

namespace WebCore {

class ThreadGlobalGLContext {
public:
    static ThreadSpecific<ThreadGlobalGLContext>* staticGLContext;

    void setContext(GLContext* context) { m_context = context; }
    GLContext* context() { return m_context; }

private:
    GLContext* m_context;
};

ThreadSpecific<ThreadGlobalGLContext>* ThreadGlobalGLContext::staticGLContext;

inline ThreadGlobalGLContext* currentContext()
{
    if (!ThreadGlobalGLContext::staticGLContext)
        ThreadGlobalGLContext::staticGLContext = new ThreadSpecific<ThreadGlobalGLContext>;
    return *ThreadGlobalGLContext::staticGLContext;
}

GLContext* GLContext::sharingContext()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(std::unique_ptr<GLContext>, sharing, (createOffscreenContext()));
    return sharing.get();
}

#if PLATFORM(X11)
// Because of driver bugs, exiting the program when there are active pbuffers
// can crash the X server (this has been observed with the official Nvidia drivers).
// We need to ensure that we clean everything up on exit. There are several reasons
// that GraphicsContext3Ds will still be alive at exit, including user error (memory
// leaks) and the page cache. In any case, we don't want the X server to crash.
typedef Vector<GLContext*> ActiveContextList;
static ActiveContextList& activeContextList()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(ActiveContextList, activeContexts, ());
    return activeContexts;
}

void GLContext::addActiveContext(GLContext* context)
{
    static bool addedAtExitHandler = false;
    if (!addedAtExitHandler) {
        atexit(&GLContext::cleanupActiveContextsAtExit);
        addedAtExitHandler = true;
    }
    activeContextList().append(context);
}

static bool gCleaningUpAtExit = false;

void GLContext::removeActiveContext(GLContext* context)
{
    // If we are cleaning up the context list at exit, don't bother removing the context
    // from the list, since we don't want to modify the list while it's being iterated.
    if (gCleaningUpAtExit)
        return;

    ActiveContextList& contextList = activeContextList();
    size_t i = contextList.find(context);
    if (i != notFound)
        contextList.remove(i);
}

void GLContext::cleanupActiveContextsAtExit()
{
    gCleaningUpAtExit = true;

    ActiveContextList& contextList = activeContextList();
    for (size_t i = 0; i < contextList.size(); ++i)
        delete contextList[i];
}
#endif // PLATFORM(X11)


std::unique_ptr<GLContext> GLContext::createContextForWindow(GLNativeWindowType windowHandle, GLContext* sharingContext)
{
#if PLATFORM(WAYLAND) && USE(EGL)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland) {
        if (auto eglContext = GLContextEGL::createContext(windowHandle, sharingContext))
            return WTFMove(eglContext);
        return nullptr;
    }
#endif

#if USE(GLX)
#if PLATFORM(WAYLAND) // Building both X11 and Wayland targets
    XID GLXWindowHandle = reinterpret_cast<XID>(windowHandle);
#else
    XID GLXWindowHandle = static_cast<XID>(windowHandle);
#endif
    if (auto glxContext = GLContextGLX::createContext(GLXWindowHandle, sharingContext))
        return WTFMove(glxContext);
#endif
#if USE(EGL)
    if (auto eglContext = GLContextEGL::createContext(windowHandle, sharingContext))
        return WTFMove(eglContext);
#endif
    return nullptr;
}

GLContext::GLContext()
{
#if PLATFORM(X11)
    addActiveContext(this);
#endif
}

std::unique_ptr<GLContext> GLContext::createOffscreenContext(GLContext* sharingContext)
{
    return createContextForWindow(0, sharingContext);
}

GLContext::~GLContext()
{
    if (this == currentContext()->context())
        currentContext()->setContext(0);
#if PLATFORM(X11)
    removeActiveContext(this);
#endif
}

bool GLContext::makeContextCurrent()
{
    currentContext()->setContext(this);
    return true;
}

GLContext* GLContext::getCurrent()
{
    return currentContext()->context();
}

} // namespace WebCore

#endif
