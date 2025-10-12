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
#include "GLFence.h"

#include "GLContext.h"
#include "GLDisplay.h"
#include "GLFenceEGL.h"
#include "GLFenceGL.h"
#include "PlatformDisplay.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GLFence);

static inline bool eglVersionSupportsFences(const GLDisplay& display)
{
    return display.checkVersion(1, 5);
}

static inline bool eglExtensionsSupportFences(const GLDisplay& display)
{
    return display.extensions().KHR_fence_sync;
}

static inline bool glContextSupportsFences(GLContextWrapper* context)
{
    return context->glVersion() >= 300;
}

bool GLFence::isSupported(const GLDisplay& display)
{
    auto* context = GLContextWrapper::currentContext();
    if (!context)
        return false;

    if (eglVersionSupportsFences(display) || eglExtensionsSupportFences(display))
        return true;

    return glContextSupportsFences(context);
}

std::unique_ptr<GLFence> GLFence::create(const GLDisplay& display)
{
#if HAVE(GL_FENCE)
    auto* context = GLContextWrapper::currentContext();
    if (!context)
        return nullptr;

    if (eglVersionSupportsFences(display))
        return GLFenceEGL::create(display);

    // Prefer EGL if server wait is supported,
    if (eglExtensionsSupportFences(display) && display.extensions().KHR_wait_sync)
        return GLFenceEGL::create(display);

    if (glContextSupportsFences(context))
        return GLFenceGL::create();

    if (eglExtensionsSupportFences(display))
        return GLFenceEGL::create(display);
#endif
    return nullptr;
}

#if OS(UNIX)
std::unique_ptr<GLFence> GLFence::createExportable(const GLDisplay& display)
{
#if HAVE(GL_FENCE)
    if (!GLContextWrapper::currentContext())
        return nullptr;

    if (display.extensions().ANDROID_native_fence_sync)
        return GLFenceEGL::createExportable(display);
#endif
    return nullptr;
}

std::unique_ptr<GLFence> GLFence::importFD(const GLDisplay& display, UnixFileDescriptor&& fd)
{
#if HAVE(GL_FENCE)
    if (!GLContextWrapper::currentContext())
        return nullptr;

    if (display.extensions().ANDROID_native_fence_sync)
        return GLFenceEGL::importFD(display, WTFMove(fd));
#else
    UNUSED_PARAM(fd);
#endif
    return nullptr;
}
#endif

} // namespace WebCore
