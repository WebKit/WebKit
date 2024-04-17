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
#include "GLContextWrapper.h"

#include "GLContext.h"
#include "GraphicsContextGLTextureMapperANGLE.h"

namespace WebCore {

static thread_local constinit GLContextWrapper* s_currentContext = nullptr;

GLContextWrapper::~GLContextWrapper()
{
    if (s_currentContext == this)
        s_currentContext = nullptr;
}

bool GLContextWrapper::isCurrent() const
{
    return s_currentContext == this;
}

GLContextWrapper* GLContextWrapper::currentContext()
{
    return s_currentContext;
}

void GLContextWrapper::didMakeContextCurrent()
{
    s_currentContext = this;
}

void GLContextWrapper::didUnmakeContextCurrent()
{
    s_currentContext = nullptr;
}

} // namespace WebCore
