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

#include <WebCore/TextureMapper.h>
#include <WebCore/TextureMapperBackingStore.h>
#include <WebCore/TextureMapperTile.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>


namespace WebCore {
class CoordinatedSurface;
}

namespace WebKit {

class CoordinatedBackingStoreTile : public WebCore::TextureMapperTile {
public:
    explicit CoordinatedBackingStoreTile(float scale = 1)
        : WebCore::TextureMapperTile(WebCore::FloatRect())
        , m_scale(scale)
    {
    }

    inline float scale() const { return m_scale; }
    void swapBuffers(WebCore::TextureMapper&);
    void setBackBuffer(const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<WebCore::CoordinatedSurface> buffer, const WebCore::IntPoint&);

private:
    RefPtr<WebCore::CoordinatedSurface> m_surface;
    WebCore::IntRect m_sourceRect;
    WebCore::IntRect m_tileRect;
    WebCore::IntPoint m_surfaceOffset;
    float m_scale;
};

class CoordinatedBackingStore : public WebCore::TextureMapperBackingStore {
public:
    void createTile(uint32_t tileID, float);
    void removeTile(uint32_t tileID);
    void removeAllTiles();
    void updateTile(uint32_t tileID, const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<WebCore::CoordinatedSurface>, const WebCore::IntPoint&);
    static Ref<CoordinatedBackingStore> create() { return adoptRef(*new CoordinatedBackingStore); }
    void commitTileOperations(WebCore::TextureMapper&);
    RefPtr<WebCore::BitmapTexture> texture() const override;
    void setSize(const WebCore::FloatSize&);
    virtual void paintToTextureMapper(WebCore::TextureMapper&, const WebCore::FloatRect&, const WebCore::TransformationMatrix&, float) override;
    virtual void drawBorder(WebCore::TextureMapper&, const WebCore::Color&, float borderWidth, const WebCore::FloatRect&, const WebCore::TransformationMatrix&) override;
    virtual void drawRepaintCounter(WebCore::TextureMapper&, int repaintCount, const WebCore::Color&, const WebCore::FloatRect&, const WebCore::TransformationMatrix&) override;

private:
    CoordinatedBackingStore()
        : m_scale(1.)
    { }
    void paintTilesToTextureMapper(Vector<WebCore::TextureMapperTile*>&, WebCore::TextureMapper&, const WebCore::TransformationMatrix&, float, const WebCore::FloatRect&);
    WebCore::TransformationMatrix adjustedTransformForRect(const WebCore::FloatRect&);
    WebCore::FloatRect rect() const { return WebCore::FloatRect(WebCore::FloatPoint::zero(), m_size); }

    typedef HashMap<uint32_t, CoordinatedBackingStoreTile> CoordinatedBackingStoreTileMap;
    CoordinatedBackingStoreTileMap m_tiles;
    HashSet<uint32_t> m_tilesToRemove;
    // FIXME: m_pendingSize should be removed after the following bug is fixed: https://bugs.webkit.org/show_bug.cgi?id=108294
    WebCore::FloatSize m_pendingSize;
    WebCore::FloatSize m_size;
    float m_scale;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedBackingStore_h
