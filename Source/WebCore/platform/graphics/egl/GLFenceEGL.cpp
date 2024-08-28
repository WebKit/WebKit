/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "GLFenceEGL.h"

#include "PlatformDisplay.h"
#include <wtf/Vector.h>

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#endif

namespace WebCore {

static std::unique_ptr<GLFence> createEGLFence(EGLenum type, const Vector<EGLAttrib>& attributes)
{
    EGLSync sync = nullptr;
    auto& display = PlatformDisplay::sharedDisplay();
    if (display.eglCheckVersion(1, 5))
        sync = eglCreateSync(display.eglDisplay(), type, attributes.isEmpty() ? nullptr : attributes.data());
    else {
        Vector<EGLint> intAttributes = attributes.map<Vector<EGLint>>([] (EGLAttrib value) {
            return value;
        });
        sync = eglCreateSyncKHR(display.eglDisplay(), type, intAttributes.isEmpty() ? nullptr : intAttributes.data());
    }
    if (sync == EGL_NO_SYNC)
        return nullptr;

    glFlush();

#if OS(UNIX)
    bool isExportable = type == EGL_SYNC_NATIVE_FENCE_ANDROID;
#else
    bool isExportable = false;
#endif
    return makeUnique<GLFenceEGL>(sync, isExportable);
}

std::unique_ptr<GLFence> GLFenceEGL::create()
{
    return createEGLFence(EGL_SYNC_FENCE_KHR, { });
}

#if OS(UNIX)
std::unique_ptr<GLFence> GLFenceEGL::createExportable()
{
    return createEGLFence(EGL_SYNC_NATIVE_FENCE_ANDROID, { });
}

std::unique_ptr<GLFence> GLFenceEGL::importFD(UnixFileDescriptor&& fd)
{
    Vector<EGLAttrib> attributes = {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd.release(),
        EGL_NONE
    };
    return createEGLFence(EGL_SYNC_NATIVE_FENCE_ANDROID, attributes);
}
#endif

GLFenceEGL::GLFenceEGL(EGLSyncKHR sync, bool isExportable)
    : m_sync(sync)
    , m_isExportable(isExportable)
{
}

GLFenceEGL::~GLFenceEGL()
{
    auto& display = PlatformDisplay::sharedDisplay();
    if (display.eglCheckVersion(1, 5))
        eglDestroySync(display.eglDisplay(), m_sync);
    else
        eglDestroySyncKHR(display.eglDisplay(), m_sync);
}

void GLFenceEGL::clientWait()
{
    auto& display = PlatformDisplay::sharedDisplay();
    if (display.eglCheckVersion(1, 5))
        eglClientWaitSync(display.eglDisplay(), m_sync, 0, EGL_FOREVER);
    else
        eglClientWaitSyncKHR(display.eglDisplay(), m_sync, 0, EGL_FOREVER_KHR);
}

void GLFenceEGL::serverWait()
{
    if (!capabilities().eglServerWaitSupported) {
        clientWait();
        return;
    }

    auto& display = PlatformDisplay::sharedDisplay();
    if (display.eglCheckVersion(1, 5))
        eglWaitSync(display.eglDisplay(), m_sync, 0);
    else
        eglWaitSyncKHR(display.eglDisplay(), m_sync, 0);
}

#if OS(UNIX)
UnixFileDescriptor GLFenceEGL::exportFD()
{
    if (!m_isExportable)
        return { };

    return UnixFileDescriptor { eglDupNativeFenceFDANDROID(PlatformDisplay::sharedDisplay().eglDisplay(), m_sync), UnixFileDescriptor::Adopt };
}
#endif

} // namespace WebCore
