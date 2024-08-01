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
#include "GLFenceGL.h"

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES3/gl3.h>
#endif

namespace WebCore {

std::unique_ptr<GLFence> GLFenceGL::create()
{
    if (auto* sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)) {
        glFlush();
        return makeUnique<GLFenceGL>(sync);
    }

    return nullptr;
}

GLFenceGL::GLFenceGL(GLsync sync)
    : m_sync(sync)
{
}

GLFenceGL::~GLFenceGL()
{
    glDeleteSync(m_sync);
}

void GLFenceGL::clientWait()
{
    glClientWaitSync(m_sync, 0, GL_TIMEOUT_IGNORED);
}

void GLFenceGL::serverWait()
{
    glWaitSync(m_sync, 0, GL_TIMEOUT_IGNORED);
}

} // namespace WebCore
