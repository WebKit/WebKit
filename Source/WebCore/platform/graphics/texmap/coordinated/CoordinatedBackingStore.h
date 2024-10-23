/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedBackingStoreTile.h"
#include "TextureMapperBackingStore.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class CoordinatedTileBuffer;
class TextureMapper;

class CoordinatedBackingStore final : public RefCounted<CoordinatedBackingStore>, public TextureMapperBackingStore {
public:
    static Ref<CoordinatedBackingStore> create()
    {
        return adoptRef(*new CoordinatedBackingStore);
    }
    ~CoordinatedBackingStore() = default;

    void resize(const FloatSize&);

    void createTile(uint32_t tileID, float scale);
    void removeTile(uint32_t tileID);
    void updateTile(uint32_t tileID, const IntRect&, const IntRect&, RefPtr<CoordinatedTileBuffer>&&, const IntPoint&);

    void processPendingUpdates(TextureMapper&);

    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float) override;
    void drawBorder(TextureMapper&, const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&) override;
    void drawRepaintCounter(TextureMapper&, int repaintCount, const Color&, const FloatRect&, const TransformationMatrix&) override;

private:
    CoordinatedBackingStore() = default;

    UncheckedKeyHashMap<uint32_t, CoordinatedBackingStoreTile> m_tiles;
    FloatSize m_size;
    float m_scale { 1. };
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
