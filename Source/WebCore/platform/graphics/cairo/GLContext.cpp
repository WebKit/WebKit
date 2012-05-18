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

#include <wtf/MainThread.h>

#if USE(GLX)
#include "GLContextGLX.h"
#endif

namespace WebCore {

GLContext* GLContext::sharingContext()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(OwnPtr<GLContext>, sharing, (createOffscreenContext()));
    return sharing.get();
}

PassOwnPtr<GLContext> GLContext::createContextForWindow(uint64_t windowHandle, GLContext* sharingContext)
{
#if USE(GLX)
    return GLContextGLX::createContext(windowHandle, sharingContext);
#endif
    return nullptr;
}

GLContext::GLContext()
{
}

PassOwnPtr<GLContext> GLContext::createOffscreenContext(GLContext* sharing)
{
#if USE(GLX)
    return GLContextGLX::createContext(0, sharing);
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
    ASSERT(isMainThread());
    gCurrentContext = this;
    return true;
}

GLContext* GLContext::getCurrent()
{
    ASSERT(isMainThread());
    return gCurrentContext;
}

} // namespace WebCore

#endif // USE(OPENGL)

