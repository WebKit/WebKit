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

#if ENABLE(GRAPHICS_CONTEXT_GL)
#include "GLContext.h"
#include <wtf/ThreadSpecific.h>

#if USE(EGL)
#include "GLContextEGL.h"
#endif

#if USE(GLX)
#include "GLContextGLX.h"
#include "OpenGLShims.h"
#endif

using WTF::ThreadSpecific;

namespace WebCore {

class ThreadGlobalGLContext {
public:
    static ThreadSpecific<ThreadGlobalGLContext>* staticGLContext;

    void setContext(GLContext* context) { m_context = context; }
    GLContext* context() { return m_context; }

private:
    GLContext* m_context { nullptr };
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
#if USE(OPENGL_ES) || USE(LIBEPOXY)
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
            return eglContext;
        return nullptr;
    }
#endif

#if USE(GLX)
    if (display.type() == PlatformDisplay::Type::X11) {
        if (auto glxContext = GLContextGLX::createContext(windowHandle, display))
            return glxContext;
    }
#endif
#if USE(EGL)
    if (auto eglContext = GLContextEGL::createContext(windowHandle, display))
        return eglContext;
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
            return glxContext;
    }
#endif

#if USE(EGL) || PLATFORM(WAYLAND) || PLATFORM(WPE)
    if (auto eglContext = GLContextEGL::createSharingContext(display))
        return eglContext;
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

bool GLContext::isExtensionSupported(const char* extensionList, const char* extension)
{
    if (!extensionList)
        return false;

    ASSERT(extension);
    int extensionLen = strlen(extension);
    const char* extensionListPtr = extensionList;
    while ((extensionListPtr = strstr(extensionListPtr, extension))) {
        if (extensionListPtr[extensionLen] == ' ' || extensionListPtr[extensionLen] == '\0')
            return true;
        extensionListPtr += extensionLen;
    }
    return false;
}

unsigned GLContext::version()
{
    if (!m_version) {
        // Version string can start with the version number (all versions except GLES 1 and 2) or with
        // "OpenGL". Different fields inside the version string are separated by spaces.
        String versionString = String(reinterpret_cast<const char*>(::glGetString(GL_VERSION)));
        Vector<String> versionStringComponents = versionString.split(' ');

        Vector<String> versionDigits;
        if (versionStringComponents[0] == "OpenGL") {
            // If the version string starts with "OpenGL" it can be GLES 1 or 2. In GLES1 version string starts
            // with "OpenGL ES-<profile> major.minor" and in GLES2 with "OpenGL ES major.minor". Version is the
            // third component in both cases.
            versionDigits = versionStringComponents[2].split('.');
        } else {
            // Version is the first component. The version number is always "major.minor" or
            // "major.minor.release". Ignore the release number.
            versionDigits = versionStringComponents[0].split('.');
        }

        m_version = versionDigits[0].toUInt() * 100 + versionDigits[1].toUInt() * 10;
    }
    return m_version;
}

} // namespace WebCore

#endif
