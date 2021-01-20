/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
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
#include "NicosiaBackingStoreTextureMapperImpl.h"

#if USE(TEXTURE_MAPPER)

namespace Nicosia {

auto BackingStoreTextureMapperImpl::createFactory() -> Factory
{
    return Factory(
        [](BackingStore&) {
            return makeUnique<BackingStoreTextureMapperImpl>();
        });
}

BackingStoreTextureMapperImpl::BackingStoreTextureMapperImpl() = default;
BackingStoreTextureMapperImpl::~BackingStoreTextureMapperImpl() = default;

void BackingStoreTextureMapperImpl::tiledBackingStoreHasPendingTileCreation()
{
    m_layerState.hasPendingTileCreation = true;
}

void BackingStoreTextureMapperImpl::createTile(uint32_t tileID, float scale)
{
    ASSERT(m_layerState.isFlushing);
    auto& update = m_layerState.update;

    // Assert no tile with this ID has been registered yet.
#if ASSERT_ENABLED
    auto matchesTile = [tileID](auto& tile) { return tile.tileID == tileID; };
#endif
    ASSERT(std::none_of(update.tilesToCreate.begin(), update.tilesToCreate.end(), matchesTile));
    ASSERT(std::none_of(update.tilesToUpdate.begin(), update.tilesToUpdate.end(), matchesTile));
    ASSERT(std::none_of(update.tilesToRemove.begin(), update.tilesToRemove.end(), matchesTile));

    update.tilesToCreate.append({ tileID, scale });
}

void BackingStoreTextureMapperImpl::updateTile(uint32_t tileID, const WebCore::SurfaceUpdateInfo& updateInfo, const WebCore::IntRect& tileRect)
{
    ASSERT(m_layerState.isFlushing);
    auto& update = m_layerState.update;

    // Assert no tile with this ID has been registered for removal yet. It might have
    // already been created in a previous update, so it makes no sense to check tilesToCreate.
    ASSERT(std::none_of(update.tilesToRemove.begin(), update.tilesToRemove.end(),
        [tileID](auto& tile) { return tile.tileID == tileID; }));

    update.tilesToUpdate.append({ tileID, tileRect, updateInfo });
}

void BackingStoreTextureMapperImpl::removeTile(uint32_t tileID)
{
    ASSERT(m_layerState.isFlushing || m_layerState.isPurging);
    auto& update = m_layerState.update;

    // Remove any creations or updates registered for this tile ID.
    auto matchesTile = [tileID](auto& tile) { return tile.tileID == tileID; };
    update.tilesToCreate.removeAllMatching(matchesTile);
    update.tilesToUpdate.removeAllMatching(matchesTile);

    // Assert no tile with this ID has been registered for removal yet.
    ASSERT(std::none_of(update.tilesToRemove.begin(), update.tilesToRemove.end(), matchesTile));

    update.tilesToRemove.append(TileUpdate::RemovalData { tileID });
}

void BackingStoreTextureMapperImpl::flushUpdate()
{
    ASSERT(!m_layerState.isFlushing);
    m_layerState.hasPendingTileCreation = false;

    // Incrementally store updates as they are being flushed from the layer-side.
    {
        LockHolder locker(m_update.lock);
        m_update.pending.tilesToCreate.appendVector(m_layerState.update.tilesToCreate);
        m_update.pending.tilesToUpdate.appendVector(m_layerState.update.tilesToUpdate);
        m_update.pending.tilesToRemove.appendVector(m_layerState.update.tilesToRemove);
    }

    m_layerState.update = { };
}

auto BackingStoreTextureMapperImpl::takeUpdate() -> TileUpdate
{
    LockHolder locker(m_update.lock);
    return WTFMove(m_update.pending);
}

} // namespace Nicosia

#endif // USE(TEXTURE_MAPPER)
