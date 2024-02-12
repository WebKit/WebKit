/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CoordinatedGraphicsLayer.h"

#if USE(COORDINATED_GRAPHICS) && USE(CAIRO)
#include "GraphicsContext.h"
#include "NicosiaPaintingContext.h"
#include "NicosiaPaintingEngine.h"

namespace WebCore {

Ref<Nicosia::Buffer> CoordinatedGraphicsLayer::paintTile(const IntRect& tileRect, const IntRect& mappedTileRect, float contentsScale)
{
    auto buffer = Nicosia::UnacceleratedBuffer::create(tileRect.size(), contentsOpaque() ? Nicosia::Buffer::NoFlags : Nicosia::Buffer::SupportsAlpha);
    m_coordinator->paintingEngine().paint(*this, buffer.get(), tileRect, mappedTileRect, IntRect { { 0, 0 }, tileRect.size() }, contentsScale);
    return buffer;
}

Ref<Nicosia::Buffer> CoordinatedGraphicsLayer::paintImage(Image& image)
{
    auto buffer = Nicosia::UnacceleratedBuffer::create(IntSize(image.size()), !image.currentFrameKnownToBeOpaque() ? Nicosia::Buffer::SupportsAlpha : Nicosia::Buffer::NoFlags);
    Nicosia::PaintingContext::paint(buffer, [&image](GraphicsContext& context) {
        IntRect rect { { }, IntSize { image.size() } };
        context.drawImage(image, rect, rect, ImagePaintingOptions(CompositeOperator::Copy));
    });
    return buffer;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(CAIRO)
