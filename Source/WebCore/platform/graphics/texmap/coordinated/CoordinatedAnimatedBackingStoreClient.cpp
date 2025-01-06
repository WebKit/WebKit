/*
 * Copyright (C) 2019 Metrological Group B.V.
 * Copyright (C) 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CoordinatedAnimatedBackingStoreClient.h"

#if USE(COORDINATED_GRAPHICS)
#include "FloatQuad.h"
#include "GraphicsLayerCoordinated.h"
#include "TransformationMatrix.h"
#include <wtf/MainThread.h>

namespace WebCore {

Ref<CoordinatedAnimatedBackingStoreClient> CoordinatedAnimatedBackingStoreClient::create(GraphicsLayer& layer)
{
    return adoptRef(*new CoordinatedAnimatedBackingStoreClient(layer));
}

CoordinatedAnimatedBackingStoreClient::CoordinatedAnimatedBackingStoreClient(GraphicsLayer& layer)
    : m_layer(&layer)
{
}

void CoordinatedAnimatedBackingStoreClient::invalidate()
{
    ASSERT(isMainThread());
    m_layer = nullptr;
}

void CoordinatedAnimatedBackingStoreClient::update(const FloatRect& visibleRect, const FloatRect& coverRect, const FloatSize& size, float contentsScale)
{
    ASSERT(isMainThread());
    m_visibleRect = visibleRect;
    m_coverRect = coverRect;
    m_size = size;
    m_contentsScale = contentsScale;
}

void CoordinatedAnimatedBackingStoreClient::requestBackingStoreUpdateIfNeeded(const TransformationMatrix& transform)
{
    // This is called from the compositor thread.
    ASSERT(!isMainThread());

    // Calculate the contents rectangle of the layer in backingStore coordinates.
    FloatRect contentsRect = { { }, m_size };
    contentsRect.scale(m_contentsScale);

    // If the area covered by tiles (the coverRect, already in backingStore coordinates) covers the whole
    // layer contents then we don't need to do anything.
    if (m_coverRect.contains(contentsRect))
        return;

    // Non-invertible layers are not visible.
    if (!transform.isInvertible())
        return;

    // Calculate the inverse of the layer transformation. The inverse transform will have the inverse of the
    // scaleFactor applied, so we need to scale it back.
    TransformationMatrix inverse = transform.inverse().value_or(TransformationMatrix()).scale(m_contentsScale);

    // Apply the inverse transform to the visible rectangle, so we have the visible rectangle in layer coordinates.
    FloatRect rect = inverse.clampedBoundsOfProjectedQuad(FloatQuad(m_visibleRect));
    GraphicsLayerCoordinated::clampToSizeIfRectIsInfinite(rect, m_size);
    FloatRect transformedVisibleRect = enclosingIntRect(rect);

    // Convert the calculated visible rectangle to backingStore coordinates.
    transformedVisibleRect.scale(m_contentsScale);

    // Restrict the calculated visible rect to the contents rectangle of the layer.
    transformedVisibleRect.intersect(contentsRect);

    if (m_coverRect.contains(transformedVisibleRect))
        return;

    // The coverRect doesn't contain the calculated visible rectangle we need to request a backingStore
    // update to render more tiles.
    callOnMainThread([this, protectedThis = Ref { *this }]() {
        if (m_layer)
            m_layer->client().notifyFlushRequired(m_layer);
    });
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
