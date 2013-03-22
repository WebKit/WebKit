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
#include "GraphicsContext3DPrivate.h"

#if USE(3D_GRAPHICS)

#include "HostWindow.h"
#include "NotImplemented.h"
#include "TransformationMatrix.h"

namespace WebCore {

PassOwnPtr<GraphicsContext3DPrivate> GraphicsContext3DPrivate::create(GraphicsContext3D* context, HostWindow* window)
{
    return adoptPtr(new GraphicsContext3DPrivate(context, window));
}

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GraphicsContext3D* context, HostWindow* window)
    : m_context(context)
    , m_window(window)
{
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
}

bool GraphicsContext3DPrivate::makeContextCurrent()
{
    return false;
}

PlatformGraphicsContext3D GraphicsContext3DPrivate::platformContext()
{
    return 0;
}

#if USE(ACCELERATED_COMPOSITING) && USE(CLUTTER)
void GraphicsContext3DPrivate::paintToGraphicsLayerActor(ClutterActor*, const FloatRect& target, const TransformationMatrix& matrix, float opacity)
{
}
#endif // USE(ACCELERATED_COMPOSITING)

} // namespace WebCore

#endif // USE(3D_GRAPHICS)
