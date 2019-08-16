/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef GraphicsContext3DPrivate_h
#define GraphicsContext3DPrivate_h

#include "GLContext.h"
#include "GraphicsContext3D.h"

namespace WebCore {

class BitmapTextureGL;

class GraphicsContext3DPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GraphicsContext3DPrivate(GraphicsContext3D*, GraphicsContext3D::RenderStyle);
    ~GraphicsContext3DPrivate();
    bool makeContextCurrent();
    PlatformGraphicsContext3D platformContext();

private:
    std::unique_ptr<GLContext> m_glContext;
};

}

#endif // GraphicsContext3DPrivate_h
