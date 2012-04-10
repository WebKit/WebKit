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

#ifndef TextureMapperBackingStore_h
#define TextureMapperBackingStore_h

#include "FloatRect.h"
#include "Image.h"
#include "TextureMapper.h"
#include "TextureMapperPlatformLayer.h"

#include <wtf/RefPtr.h>

namespace WebCore {

class GraphicsLayer;

class TextureMapperBackingStore : public TextureMapperPlatformLayer, public RefCounted<TextureMapperBackingStore> {
public:
    virtual PassRefPtr<BitmapTexture> texture() const = 0;
    virtual void paintToTextureMapper(TextureMapper*, const FloatRect&, const TransformationMatrix&, float, BitmapTexture*) = 0;
    virtual ~TextureMapperBackingStore() { }
};

class TextureMapperTile {
public:
    inline PassRefPtr<BitmapTexture> texture() const { return m_texture; }
    inline FloatRect rect() const { return m_rect; }
    inline void setTexture(BitmapTexture* texture) { m_texture = texture; }
    inline void setRect(const FloatRect& rect) { m_rect = rect; }

    void updateContents(TextureMapper*, Image*, const IntRect&);
    virtual void paint(TextureMapper*, const TransformationMatrix&, float, BitmapTexture*);
    virtual ~TextureMapperTile() { }

    TextureMapperTile(const FloatRect& rect)
        : m_rect(rect)
    {
    }

private:
    RefPtr<BitmapTexture> m_texture;
    FloatRect m_rect;
};

class TextureMapperTiledBackingStore : public TextureMapperBackingStore {
public:
    void updateContentsFromLayer(TextureMapper*, GraphicsLayer*, const IntRect&);
    virtual ~TextureMapperTiledBackingStore() { }
    virtual void paintToTextureMapper(TextureMapper*, const FloatRect&, const TransformationMatrix&, float, BitmapTexture*);
    virtual PassRefPtr<BitmapTexture> texture() const;
    void updateContents(TextureMapper*, Image*, const FloatSize&, const IntRect&);
    void updateContents(TextureMapper* textureMapper, Image* image) { updateContents(textureMapper, image, image->size(), image->rect()); }
    inline FloatRect rect() const { return FloatRect(FloatPoint::zero(), m_size); }
    static PassRefPtr<TextureMapperTiledBackingStore> create() { return adoptRef(new TextureMapperTiledBackingStore); }
    void setContentsToImage(Image* image) { m_image = image; }
    void updateContentsFromImageIfNeeded(TextureMapper*);

private:
    TextureMapperTiledBackingStore() { }
    void createOrDestroyTilesIfNeeded(const FloatSize& backingStoreSize, const IntSize& tileSize, bool hasAlpha);

    Vector<TextureMapperTile> m_tiles;
    FloatSize m_size;
    RefPtr<Image> m_image;
};

}
#endif // TextureMapperBackingStore_h
