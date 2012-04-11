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
#include "GLContext.h"

#if USE(OPENGL)

#if USE(GLX)
#include "GLContextGLX.h"
#endif

namespace WebCore {

GLContext::GLContext()
{
}

GLContext* GLContext::createOffscreenContext(GLContext* sharing)
{
    if (sharing)
        return sharing->createOffscreenSharingContext();

#if USE(GLX)
    return GLContextGLX::createContext(0, 0);
#endif
}

// FIXME: This should be a thread local eventually if we
// want to support using GLContexts from multiple threads.
static GLContext* gCurrentContext = 0;

GLContext::~GLContext()
{
    if (this == gCurrentContext)
        gCurrentContext = 0;
}

bool GLContext::makeContextCurrent()
{
    gCurrentContext = this;
    return true;
}

GLContext* GLContext::getCurrent()
{
    return gCurrentContext;
}

} // namespace WebCore

#endif // USE(OPENGL)

