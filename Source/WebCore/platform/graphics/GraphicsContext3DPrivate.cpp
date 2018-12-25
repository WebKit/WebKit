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

#if ENABLE(GRAPHICS_CONTEXT_3D)
#include "GraphicsContext3DPrivate.h"

#include "HostWindow.h"
#include <wtf/StdLibExtras.h>


#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif USE(OPENGL_ES)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include "OpenGLShims.h"
#endif


namespace WebCore {

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GraphicsContext3D*, GraphicsContext3D::RenderStyle renderStyle)
{
    switch (renderStyle) {
    case GraphicsContext3D::RenderOffscreen:
        m_glContext = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing());
        break;
    case GraphicsContext3D::RenderDirectlyToHostWindow:
        ASSERT_NOT_REACHED();
        break;
    }
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate() = default;

bool GraphicsContext3DPrivate::makeContextCurrent()
{
    return m_glContext ? m_glContext->makeContextCurrent() : false;
}

PlatformGraphicsContext3D GraphicsContext3DPrivate::platformContext()
{
    return m_glContext ? m_glContext->platformContext() : GLContext::current()->platformContext();
}

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
