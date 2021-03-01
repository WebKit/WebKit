/*
 * Copyright (C) 2011, 2012 Igalia S.L.
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

#if ENABLE(WEBGL) && PLATFORM(WIN) && USE(CA)
#include "GraphicsContextGLOpenGLPrivate.h"

#include "HostWindow.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

GraphicsContextGLOpenGLPrivate::GraphicsContextGLOpenGLPrivate(GraphicsContextGLOpenGL*)
{
    m_glContext = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing());
}

GraphicsContextGLOpenGLPrivate::~GraphicsContextGLOpenGLPrivate() = default;

bool GraphicsContextGLOpenGLPrivate::makeContextCurrent()
{
    return m_glContext ? m_glContext->makeContextCurrent() : false;
}

PlatformGraphicsContextGL GraphicsContextGLOpenGLPrivate::platformContext()
{
    return m_glContext ? m_glContext->platformContext() : GLContext::current()->platformContext();
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && PLATFORM(WIN) && USE(CA)
