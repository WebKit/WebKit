/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef TextureCacheCompositingThread_h
#define TextureCacheCompositingThread_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "LayerTileIndex.h"
#include "Texture.h"

#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class IntRect;

// A LRU cache for OpenGL textures.
class TextureCacheCompositingThread {
    WTF_MAKE_NONCOPYABLE(TextureCacheCompositingThread);
    WTF_MAKE_FAST_ALLOCATED;
public:
    friend TextureCacheCompositingThread* textureCacheCompositingThread();

    // Creates a new texture managed by this cache.
    PassRefPtr<Texture> createTexture()
    {
        return Texture::create();
    }

    // Retrieve a texture from the cache.
    PassRefPtr<Texture> textureForTiledContents(const SkBitmap& contents, const IntRect& tileRect, const TileIndex&, bool isOpaque);
    PassRefPtr<Texture> textureForColor(const Color&);

    // Update contents of an existing texture, or create a new one if texture is 0.
    PassRefPtr<Texture> updateContents(const RefPtr<Texture>&, const SkBitmap& contents, const IntRect& dirtyRect, const IntRect& tileRect, bool isOpaque);

    size_t memoryUsage() const { return m_memoryUsage; }
    size_t memoryLimit() const { return m_memoryLimit; }
    void setMemoryLimit(size_t limit) { m_memoryLimit = limit; }

    // Evict unprotected textures until we reach the limit provided.
    void prune(size_t limit);
    void prune() { prune(memoryLimit()); }

    // Evict all textures from the cache.
    void clear();

    // Update the LRU list.
    void textureAccessed(Texture*);

    // Deleting destroyed textures is provided as a separate method, because
    // there might not be an OpenGL context current when the texture destructor
    // was called. Calling this makes sure to free any such textures.
    void collectGarbage();

    void textureDestroyed(Texture*);
    void textureResized(Texture*, const IntSize& oldSize);

    // Undo the effects of eviction, if possible.
    bool install(Texture*);

private:
    struct ZombieTexture {
        explicit ZombieTexture(Texture* texture)
            : id(texture->textureId())
            , size(texture->size())
        {
        }

        unsigned id;
        IntSize size;
    };
    typedef ListHashSet<Texture* > TextureSet;
    typedef std::pair<uint32_t, size_t> ContentsKey;
    typedef HashMap<TileIndex, RefPtr<Texture> > TextureMap;
    typedef Vector<ZombieTexture> Garbage;

    TextureCacheCompositingThread();
    ~TextureCacheCompositingThread();

    unsigned allocateTextureId();
    void freeTextureId(unsigned id);

    void incMemoryUsage(int delta) { setMemoryUsage(memoryUsage() + delta); }
    void decMemoryUsage(int delta) { setMemoryUsage(memoryUsage() - delta); }
    void setMemoryUsage(size_t);

    ContentsKey key(const SkBitmap& contents);

    void evict(const TextureSet::iterator&);

    // LRU set of weak pointers to textures.
    TextureSet m_textures;
    size_t m_memoryUsage;
    size_t m_memoryLimit;

    // Map of refCounted pointers to textures.
    HashMap<ContentsKey, TextureMap> m_cache;
    struct ColorHash {
        static unsigned hash(const Color& key) { return WTF::intHash(key.rgb()); }
        static bool equal(const Color& a, const Color& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = true;
    };
    struct ColorHashTraits : WTF::GenericHashTraits<Color> {
        static const bool emptyValueIsZero = true;
        // The deleted value is an invalid color with an RGB value of 0xFFFFFFFF.
        // Such values do not naturally occur as colors.
        // And empty value is an invalid color with an RGB value of 0x0.
        static void constructDeletedValue(Color& slot) { new (&slot) Color(); *reinterpret_cast<RGBA32*>(&slot) = Color::white; }
        static bool isDeletedValue(const Color& value) { return !value.isValid() && value.rgb() == Color::white; }
    };
    typedef HashMap<Color, RefPtr<Texture>, ColorHash, ColorHashTraits> ColorTextureMap;
    ColorTextureMap m_colors;
    Garbage m_garbage;
};

TextureCacheCompositingThread* textureCacheCompositingThread();

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // TextureCacheCompositingThread_h
