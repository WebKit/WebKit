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
#include "IntRect.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperTile.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {
class CoordinatedTileBuffer;
class TextureMapper;

class CoordinatedBackingStoreTile final : public TextureMapperTile {
public:
    explicit CoordinatedBackingStoreTile(float scale = 1)
        : TextureMapperTile(FloatRect())
        , m_scale(scale)
    {
    }
    ~CoordinatedBackingStoreTile() = default;

    float scale() const { return m_scale; }

    struct Update {
        RefPtr<CoordinatedTileBuffer> buffer;
        IntRect sourceRect;
        IntRect tileRect;
        IntPoint bufferOffset;
    };
    void addUpdate(Update&&);

    void swapBuffers(TextureMapper&);

private:
    Vector<Update> m_updates;
    float m_scale { 1. };
};

class CoordinatedBackingStore final : public RefCounted<CoordinatedBackingStore>, public TextureMapperBackingStore {
public:
    static Ref<CoordinatedBackingStore> create()
    {
        return adoptRef(*new CoordinatedBackingStore);
    }
    ~CoordinatedBackingStore() = default;

    void resize(const FloatSize&, float scale);

    void createTile(uint32_t tileID);
    void removeTile(uint32_t tileID);
    void updateTile(uint32_t tileID, const IntRect&, const IntRect&, RefPtr<CoordinatedTileBuffer>&&, const IntPoint&);

    void swapBuffers(TextureMapper&);

    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float) override;
    void drawBorder(TextureMapper&, const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&) override;
    void drawRepaintCounter(TextureMapper&, int repaintCount, const Color&, const FloatRect&, const TransformationMatrix&) override;

private:
    CoordinatedBackingStore() = default;

    void paintTilesToTextureMapper(Vector<TextureMapperTile*>&, TextureMapper&, const TransformationMatrix&, float, const FloatRect&);
    TransformationMatrix adjustedTransformForRect(const FloatRect&);
    FloatRect rect() const { return FloatRect(FloatPoint::zero(), m_size); }

    HashMap<uint32_t, CoordinatedBackingStoreTile> m_tiles;
    FloatSize m_size;
    float m_scale { 1. };
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
