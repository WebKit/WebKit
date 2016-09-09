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

static bool initializeOpenGLShimsIfNeeded()
{
#if USE(OPENGL_ES_2)
    return true;
#else
    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
        success = initializeOpenGLShims();
        initialized = true;
    }
    return success;
#endif
}

std::unique_ptr<GLContext> GLContext::createContextForWindow(GLNativeWindowType windowHandle, PlatformDisplay* platformDisplay)
{
    if (!initializeOpenGLShimsIfNeeded())
        return nullptr;

    PlatformDisplay& display = platformDisplay ? *platformDisplay : PlatformDisplay::sharedDisplay();
#if PLATFORM(WAYLAND)
    if (display.type() == PlatformDisplay::Type::Wayland) {
        if (auto eglContext = GLContextEGL::createContext(windowHandle, display))
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
    if (auto glxContext = GLContextGLX::createContext(GLXWindowHandle, display))
        return WTFMove(glxContext);
#endif
#if USE(EGL)
    if (auto eglContext = GLContextEGL::createContext(windowHandle, display))
        return WTFMove(eglContext);
#endif
    return nullptr;
}

std::unique_ptr<GLContext> GLContext::createOffscreenContext(PlatformDisplay* platformDisplay)
{
    if (!initializeOpenGLShimsIfNeeded())
        return nullptr;

    return createContextForWindow(0, platformDisplay ? platformDisplay : &PlatformDisplay::sharedDisplay());
}

std::unique_ptr<GLContext> GLContext::createSharingContext(PlatformDisplay& display)
{
    if (!initializeOpenGLShimsIfNeeded())
        return nullptr;

#if USE(GLX)
    if (display.type() == PlatformDisplay::Type::X11) {
        if (auto glxContext = GLContextGLX::createSharingContext(display))
            return WTFMove(glxContext);
    }
#endif

#if USE(EGL) || PLATFORM(WAYLAND)
    if (auto eglContext = GLContextEGL::createSharingContext(display))
        return WTFMove(eglContext);
#endif

    return nullptr;
}

GLContext::GLContext(PlatformDisplay& display)
    : m_display(display)
{
}

GLContext::~GLContext()
{
    if (this == currentContext()->context())
        currentContext()->setContext(nullptr);
}

bool GLContext::makeContextCurrent()
{
    currentContext()->setContext(this);
    return true;
}

GLContext* GLContext::current()
{
    return currentContext()->context();
}

} // namespace WebCore

#endif
