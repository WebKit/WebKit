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

#ifndef LayerBackingStore_h
#define LayerBackingStore_h

#if USE(UI_SIDE_COMPOSITING)

#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include <wtf/HashMap.h>

namespace WebKit {

class ShareableSurface;

class LayerBackingStoreTile : public WebCore::TextureMapperTile {
public:
    LayerBackingStoreTile(float scale = 1)
        : TextureMapperTile(WebCore::FloatRect())
        , m_scale(scale)
    {
    }

    inline float scale() const { return m_scale; }
    void swapBuffers(WebCore::TextureMapper*);
    void setBackBuffer(const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<ShareableSurface> buffer, const WebCore::IntPoint&);

private:
    RefPtr<ShareableSurface> m_surface;
    WebCore::IntRect m_sourceRect;
    WebCore::IntRect m_targetRect;
    WebCore::IntPoint m_surfaceOffset;
    float m_scale;
};

class LayerBackingStore : public WebCore::TextureMapperBackingStore {
public:
    void createTile(int, float);
    void removeTile(int);
    void updateTile(int, const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<ShareableSurface>, const WebCore::IntPoint&);
    static PassRefPtr<LayerBackingStore> create() { return adoptRef(new LayerBackingStore); }
    void commitTileOperations(WebCore::TextureMapper*);
    PassRefPtr<WebCore::BitmapTexture> texture() const;
    virtual void paintToTextureMapper(WebCore::TextureMapper*, const WebCore::FloatRect&, const WebCore::TransformationMatrix&, float, WebCore::BitmapTexture*);

private:
    LayerBackingStore()
        : m_scale(1.)
    { }
    HashMap<int, LayerBackingStoreTile> m_tiles;
    Vector<int> m_tilesToRemove;
    float m_scale;
};

} // namespace WebKit
#endif

#endif // LayerBackingStore_h
