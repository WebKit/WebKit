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

class ShareableSurface;

class CoordinatedBackingStoreTile : public WebCore::TextureMapperTile {
public:
    CoordinatedBackingStoreTile(float scale = 1)
        : TextureMapperTile(WebCore::FloatRect())
        , m_scale(scale)
        , m_repaintCount(0)
    {
    }

    inline float scale() const { return m_scale; }
    inline void incrementRepaintCount() { ++m_repaintCount; }
    inline int repaintCount() const { return m_repaintCount; }
    void swapBuffers(WebCore::TextureMapper*);
    void setBackBuffer(const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<ShareableSurface> buffer, const WebCore::IntPoint&);

private:
    RefPtr<ShareableSurface> m_surface;
    WebCore::IntRect m_sourceRect;
    WebCore::IntRect m_targetRect;
    WebCore::IntPoint m_surfaceOffset;
    float m_scale;
    int m_repaintCount;
};

class CoordinatedBackingStore : public WebCore::TextureMapperBackingStore {
public:
    void createTile(int, float);
    void removeTile(int);
    void updateTile(int, const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<ShareableSurface>, const WebCore::IntPoint&);
    bool isEmpty() const;
    static PassRefPtr<CoordinatedBackingStore> create() { return adoptRef(new CoordinatedBackingStore); }
    void commitTileOperations(WebCore::TextureMapper*);
    PassRefPtr<WebCore::BitmapTexture> texture() const;
    virtual void paintToTextureMapper(WebCore::TextureMapper*, const WebCore::FloatRect&, const WebCore::TransformationMatrix&, float, WebCore::BitmapTexture*);

private:
    CoordinatedBackingStore()
        : m_scale(1.)
    { }
    HashMap<int, CoordinatedBackingStoreTile> m_tiles;
    HashSet<int> m_tilesToRemove;
    float m_scale;
};

} // namespace WebKit
#endif

#endif // CoordinatedBackingStore_h
