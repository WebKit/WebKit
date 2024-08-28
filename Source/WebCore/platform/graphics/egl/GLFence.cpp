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
#include "GLFenceEGL.h"
#include "GLFenceGL.h"

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#endif

namespace WebCore {

const GLFence::Capabilities& GLFence::capabilities()
{
    static std::once_flag onceFlag;
    static Capabilities capabilities;
    std::call_once(onceFlag, [] {
        auto& display = PlatformDisplay::sharedDisplay();
        const auto& extensions = display.eglExtensions();
        if (display.eglCheckVersion(1, 5)) {
            capabilities.eglSupported = true;
            capabilities.eglServerWaitSupported = true;
        } else {
            capabilities.eglSupported = extensions.KHR_fence_sync;
            capabilities.eglServerWaitSupported = extensions.KHR_wait_sync;
        }
#if OS(UNIX)
        capabilities.eglExportableSupported = extensions.ANDROID_native_fence_sync;
#endif
        capabilities.glSupported = GLContext::versionFromString(reinterpret_cast<const char*>(glGetString(GL_VERSION))) >= 300;
    });
    return capabilities;
}

bool GLFence::isSupported()
{
    const auto& fenceCapabilities = capabilities();
    return fenceCapabilities.eglSupported || fenceCapabilities.glSupported;
}

std::unique_ptr<GLFence> GLFence::create()
{
    if (!GLContextWrapper::currentContext())
        return nullptr;

    const auto& fenceCapabilities = capabilities();
    if (fenceCapabilities.eglSupported && fenceCapabilities.eglServerWaitSupported)
        return GLFenceEGL::create();

    if (fenceCapabilities.glSupported)
        return GLFenceGL::create();

    if (fenceCapabilities.eglSupported)
        return GLFenceEGL::create();

    return nullptr;
}

#if OS(UNIX)
std::unique_ptr<GLFence> GLFence::createExportable()
{
    if (!GLContextWrapper::currentContext())
        return nullptr;

    const auto& fenceCapabilities = capabilities();
    if (fenceCapabilities.eglSupported && fenceCapabilities.eglExportableSupported)
        return GLFenceEGL::createExportable();

    return nullptr;
}

std::unique_ptr<GLFence> GLFence::importFD(UnixFileDescriptor&& fd)
{
    if (!GLContextWrapper::currentContext())
        return nullptr;

    const auto& fenceCapabilities = capabilities();
    if (fenceCapabilities.eglSupported && fenceCapabilities.eglExportableSupported)
        return GLFenceEGL::importFD(WTFMove(fd));

    return nullptr;
}
#endif

} // namespace WebCore
