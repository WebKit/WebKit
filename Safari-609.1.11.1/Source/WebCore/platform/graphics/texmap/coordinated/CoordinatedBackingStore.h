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

#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperTile.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace Nicosia {
class Buffer;
}

namespace WebCore {

class CoordinatedBackingStoreTile : public TextureMapperTile {
public:
    explicit CoordinatedBackingStoreTile(float scale = 1)
        : TextureMapperTile(FloatRect())
        , m_scale(scale)
    {
    }

    float scale() const { return m_scale; }

    struct Update {
        RefPtr<Nicosia::Buffer> buffer;
        IntRect sourceRect;
        IntRect tileRect;
        IntPoint bufferOffset;
    };
    void addUpdate(Update&&);

    void swapBuffers(TextureMapper&);

private:
    Vector<Update> m_updates;
    float m_scale;
};

class CoordinatedBackingStore : public RefCounted<CoordinatedBackingStore>, public TextureMapperBackingStore {
public:
    void createTile(uint32_t tileID, float);
    void removeTile(uint32_t tileID);
    void removeAllTiles();
    void updateTile(uint32_t tileID, const IntRect&, const IntRect&, RefPtr<Nicosia::Buffer>&&, const IntPoint&);
    static Ref<CoordinatedBackingStore> create() { return adoptRef(*new CoordinatedBackingStore); }
    void commitTileOperations(TextureMapper&);
    void setSize(const FloatSize&);
    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float) override;
    void drawBorder(TextureMapper&, const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&) override;
    void drawRepaintCounter(TextureMapper&, int repaintCount, const Color&, const FloatRect&, const TransformationMatrix&) override;

private:
    CoordinatedBackingStore()
        : m_scale(1.)
    { }
    void paintTilesToTextureMapper(Vector<TextureMapperTile*>&, TextureMapper&, const TransformationMatrix&, float, const FloatRect&);
    TransformationMatrix adjustedTransformForRect(const FloatRect&);
    FloatRect rect() const { return FloatRect(FloatPoint::zero(), m_size); }

    typedef HashMap<uint32_t, CoordinatedBackingStoreTile> CoordinatedBackingStoreTileMap;
    CoordinatedBackingStoreTileMap m_tiles;
    HashSet<uint32_t> m_tilesToRemove;
    // FIXME: m_pendingSize should be removed after the following bug is fixed: https://bugs.webkit.org/show_bug.cgi?id=108294
    FloatSize m_pendingSize;
    FloatSize m_size;
    float m_scale;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
