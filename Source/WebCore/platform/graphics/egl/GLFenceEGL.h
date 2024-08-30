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

#include "GLFence.h"

typedef void* EGLSync;

namespace WebCore {

class GLFenceEGL final : public GLFence {
public:
    static std::unique_ptr<GLFence> create();
#if OS(UNIX)
    static std::unique_ptr<GLFence> createExportable();
    static std::unique_ptr<GLFence> importFD(WTF::UnixFileDescriptor&&);
#endif
    GLFenceEGL(EGLSync, bool);
    virtual ~GLFenceEGL();

private:
    void clientWait() override;
    void serverWait() override;
#if OS(UNIX)
    WTF::UnixFileDescriptor exportFD() override;
#endif

    EGLSync m_sync { nullptr };
    bool m_isExportable { false };
};

} // namespace WebCore
