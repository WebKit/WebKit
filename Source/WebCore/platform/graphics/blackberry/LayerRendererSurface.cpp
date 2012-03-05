/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerRendererSurface.h"

#include "LayerCompositingThread.h"
#include "LayerRenderer.h"
#include "TextureCacheCompositingThread.h"

namespace WebCore {

LayerRendererSurface::LayerRendererSurface(LayerRenderer* renderer, LayerCompositingThread* owner)
    : m_ownerLayer(owner)
    , m_layerRenderer(renderer)
    , m_opacity(1.0)
{
}

LayerRendererSurface::~LayerRendererSurface()
{
}

void LayerRendererSurface::setContentRect(const IntRect& contentRect)
{
    m_contentRect = contentRect;
    m_size = contentRect.size();
}

FloatRect LayerRendererSurface::drawRect() const
{
    float bx = m_size.width() / 2.0;
    float by = m_size.height() / 2.0;

    FloatQuad transformedBounds;
    transformedBounds.setP1(m_drawTransform.mapPoint(FloatPoint(-bx, -by)));
    transformedBounds.setP2(m_drawTransform.mapPoint(FloatPoint(-bx, by)));
    transformedBounds.setP3(m_drawTransform.mapPoint(FloatPoint(bx, by)));
    transformedBounds.setP4(m_drawTransform.mapPoint(FloatPoint(bx, -by)));

    FloatRect rect = transformedBounds.boundingBox();

    if (m_ownerLayer->replicaLayer()) {
        FloatQuad bounds;
        bounds.setP1(m_replicaDrawTransform.mapPoint(FloatPoint(-bx, -by)));
        bounds.setP2(m_replicaDrawTransform.mapPoint(FloatPoint(-bx, by)));
        bounds.setP3(m_replicaDrawTransform.mapPoint(FloatPoint(bx, by)));
        bounds.setP4(m_replicaDrawTransform.mapPoint(FloatPoint(bx, -by)));
        rect.unite(bounds.boundingBox());
    }

    return rect;
}

bool LayerRendererSurface::ensureTexture()
{
    if (!m_texture)
        m_texture = textureCacheCompositingThread()->createTexture();

    return m_texture->protect(m_size);
}

void LayerRendererSurface::releaseTexture()
{
    m_texture->unprotect();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
