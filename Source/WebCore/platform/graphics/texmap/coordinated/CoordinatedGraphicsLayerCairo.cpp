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
#include "CairoPaintingContext.h"
#include "CairoPaintingEngine.h"
#include "CoordinatedTileBuffer.h"
#include "FloatRect.h"
#include "GraphicsContext.h"

namespace WebCore {

Ref<CoordinatedTileBuffer> CoordinatedGraphicsLayer::paintTile(const IntRect& dirtyRect)
{
    auto scale = effectiveContentsScale();
    FloatRect scaledDirtyRect(dirtyRect);
    scaledDirtyRect.scale(1 / scale);

    auto buffer = CoordinatedUnacceleratedTileBuffer::create(dirtyRect.size(), contentsOpaque() ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha);
    m_coordinator->paintingEngine().paint(*this, buffer.get(), dirtyRect, enclosingIntRect(scaledDirtyRect), IntRect { { }, dirtyRect.size() }, scale);
    return buffer;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(CAIRO)
