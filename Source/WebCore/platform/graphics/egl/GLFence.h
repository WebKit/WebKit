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

#pragma once

#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

#if OS(UNIX)
#include <wtf/unix/UnixFileDescriptor.h>
#endif

typedef struct __GLsync* GLsync;

namespace WebCore {

class GLFence {
    WTF_MAKE_NONCOPYABLE(GLFence);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static bool isSupported();
    WEBCORE_EXPORT static std::unique_ptr<GLFence> create();
#if OS(UNIX)
    WEBCORE_EXPORT static std::unique_ptr<GLFence> createExportable();
    WEBCORE_EXPORT static std::unique_ptr<GLFence> importFD(WTF::UnixFileDescriptor&&);
#endif
    virtual ~GLFence() = default;

    virtual void clientWait() = 0;
    virtual void serverWait() = 0;
#if OS(UNIX)
    virtual WTF::UnixFileDescriptor exportFD() { return { }; }
#endif

protected:
    GLFence() = default;

    struct Capabilities {
        bool eglSupported { false };
        bool eglServerWaitSupported { false };
        bool eglExportableSupported { false };
        bool glSupported { false };
    };
    static const Capabilities& capabilities();
};

} // namespace WebCore
