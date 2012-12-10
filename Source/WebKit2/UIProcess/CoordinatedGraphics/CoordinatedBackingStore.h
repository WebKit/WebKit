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

#ifndef CoordinatedBackingStore_h
#define CoordinatedBackingStore_h

#if USE(COORDINATED_GRAPHICS)

#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebKit {

class CoordinatedSurface;

class CoordinatedBackingStoreTile : public WebCore::TextureMapperTile {
public:
    explicit CoordinatedBackingStoreTile(float scale = 1)
        : TextureMapperTile(WebCore::FloatRect())
        , m_scale(scale)
        , m_repaintCount(0)
    {
    }

    inline float scale() const { return m_scale; }
    inline void incrementRepaintCount() { ++m_repaintCount; }
    inline int repaintCount() const { return m_repaintCount; }
    void swapBuffers(WebCore::TextureMapper*);
    void setBackBuffer(const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<CoordinatedSurface> buffer, const WebCore::IntPoint&);

private:
    RefPtr<CoordinatedSurface> m_surface;
    WebCore::IntRect m_sourceRect;
    WebCore::IntRect m_tileRect;
    WebCore::IntPoint m_surfaceOffset;
    float m_scale;
    int m_repaintCount;
};

class CoordinatedBackingStore : public WebCore::TextureMapperBackingStore {
public:
    void createTile(uint32_t tileID, float);
    void removeTile(uint32_t tileID);
    void removeAllTiles();
    void updateTile(uint32_t tileID, const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<CoordinatedSurface>, const WebCore::IntPoint&);
    static PassRefPtr<CoordinatedBackingStore> create() { return adoptRef(new CoordinatedBackingStore); }
    void commitTileOperations(WebCore::TextureMapper*);
    PassRefPtr<WebCore::BitmapTexture> texture() const;
    void setSize(const WebCore::FloatSize&);
    virtual void paintToTextureMapper(WebCore::TextureMapper*, const WebCore::FloatRect&, const WebCore::TransformationMatrix&, float, WebCore::BitmapTexture*);

private:
    CoordinatedBackingStore()
        : m_scale(1.)
    { }
    void paintTilesToTextureMapper(Vector<WebCore::TextureMapperTile*>&, WebCore::TextureMapper*, const WebCore::TransformationMatrix&, float, WebCore::BitmapTexture*, const WebCore::FloatRect&);

    HashMap<uint32_t, CoordinatedBackingStoreTile> m_tiles;
    HashSet<uint32_t> m_tilesToRemove;
    WebCore::FloatSize m_size;
    float m_scale;
};

} // namespace WebKit
#endif

#endif // CoordinatedBackingStore_h
