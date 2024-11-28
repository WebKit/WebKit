/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
#include "CoordinatedBackingStoreTile.h"

#if USE(COORDINATED_GRAPHICS)
#include "BitmapTexture.h"
#include "CoordinatedTileBuffer.h"
#include "GraphicsLayer.h"
#include "TextureMapper.h"
#include <wtf/SystemTracing.h>

namespace WebCore {

CoordinatedBackingStoreTile::CoordinatedBackingStoreTile(float scale)
    : m_scale(scale)
{
}

CoordinatedBackingStoreTile::~CoordinatedBackingStoreTile() = default;

void CoordinatedBackingStoreTile::addUpdate(Update&& update)
{
    m_updates.append(WTFMove(update));
}

void CoordinatedBackingStoreTile::processPendingUpdates(TextureMapper& textureMapper)
{
    auto updates = WTFMove(m_updates);
    auto updatesCount = updates.size();
    if (!updatesCount)
        return;

    WTFBeginSignpost(this, CoordinatedSwapBuffers, "%lu updates", updatesCount);
    for (unsigned updateIndex = 0; updateIndex < updatesCount; ++updateIndex) {
        auto& update = updates[updateIndex];
        if (!update.buffer)
            continue;

        WTFBeginSignpost(this, CoordinatedSwapBuffer, "%u/%lu, rect %ix%i+%i+%i", updateIndex + 1, updatesCount, update.tileRect.x(), update.tileRect.y(), update.tileRect.width(), update.tileRect.height());

        ASSERT(textureMapper.maxTextureSize().width() >= update.tileRect.size().width());
        ASSERT(textureMapper.maxTextureSize().height() >= update.tileRect.size().height());

        FloatRect unscaledTileRect(update.tileRect);
        unscaledTileRect.scale(1. / m_scale);

        OptionSet<BitmapTexture::Flags> flags;
        if (update.buffer->supportsAlpha())
            flags.add(BitmapTexture::Flags::SupportsAlpha);

        WTFBeginSignpost(this, AcquireTexture);
        if (!m_texture || unscaledTileRect != m_rect) {
            m_rect = unscaledTileRect;
            m_texture = textureMapper.acquireTextureFromPool(update.tileRect.size(), flags);
        } else if (update.buffer->supportsAlpha() == m_texture->isOpaque())
            m_texture->reset(update.tileRect.size(), flags);
        WTFEndSignpost(this, AcquireTexture);

        WTFBeginSignpost(this, WaitPaintingCompletion);
        update.buffer->waitUntilPaintingComplete();
        WTFEndSignpost(this, WaitPaintingCompletion);

#if USE(SKIA)
        if (update.buffer->isBackedByOpenGL()) {
            WTFBeginSignpost(this, CopyTextureGPUToGPU);
            auto& buffer = static_cast<CoordinatedAcceleratedTileBuffer&>(*update.buffer);

            // Fast path: whole tile content changed -- take ownership of the incoming texture, replacing the existing tile buffer (avoiding texture copies).
            if (update.sourceRect.size() == update.tileRect.size()) {
                ASSERT(update.sourceRect.location().isZero());
                m_texture->swapTexture(buffer.texture());
            } else
                m_texture->copyFromExternalTexture(buffer.texture().id(), update.sourceRect, toIntSize(update.bufferOffset));

            update.buffer = nullptr;
            WTFEndSignpost(this, CopyTextureGPUToGPU);
            WTFEndSignpost(this, CoordinatedSwapBuffer);
            continue;
        }
#endif

        WTFBeginSignpost(this, CopyTextureCPUToGPU);
        ASSERT(!update.buffer->isBackedByOpenGL());
        auto& buffer = static_cast<CoordinatedUnacceleratedTileBuffer&>(*update.buffer);
        m_texture->updateContents(buffer.data(), update.sourceRect, update.bufferOffset, buffer.stride(), buffer.pixelFormat());
        update.buffer = nullptr;
        WTFEndSignpost(this, CopyTextureCPUToGPU);

        WTFEndSignpost(this, CoordinatedSwapBuffer);
    }
    WTFEndSignpost(this, CoordinatedSwapBuffers);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
