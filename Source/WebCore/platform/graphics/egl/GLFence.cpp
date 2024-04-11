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

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

namespace WebCore {

std::unique_ptr<GLFence> GLFence::create()
{
    auto* context = GLContext::current();
    if (!context)
        return nullptr;

    if (context->version() >= 300 || context->glExtensions().APPLE_sync) {
        if (auto* sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)) {
            glFlush();
            return makeUnique<GLFence>(sync);
        }
        return nullptr;
    }

    return nullptr;
}

GLFence::GLFence(GLsync sync)
    : m_sync(sync)
{
}

GLFence::~GLFence()
{
    if (m_sync)
        glDeleteSync(m_sync);
}

unsigned GLFence::wait(FlushCommands flushCommands)
{
    return glClientWaitSync(m_sync, flushCommands == FlushCommands::Yes ? GL_SYNC_FLUSH_COMMANDS_BIT : 0, GL_TIMEOUT_IGNORED);
}

} // namespace WebCore
