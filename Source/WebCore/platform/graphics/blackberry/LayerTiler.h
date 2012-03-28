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

#ifndef LayerTiler_h
#define LayerTiler_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "LayerTile.h"
#include "LayerTileIndex.h"

#include <SkBitmap.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class LayerCompositingThread;
class LayerWebKitThread;

class LayerTiler : public ThreadSafeRefCounted<LayerTiler> {
public:
    TileIndex indexOfTile(const IntPoint& origin);
    IntPoint originOfTile(const TileIndex&);
    IntRect rectForTile(const TileIndex&, const IntSize& bounds);

    static PassRefPtr<LayerTiler> create(LayerWebKitThread* layer)
    {
        return adoptRef(new LayerTiler(layer));
    }

    virtual ~LayerTiler();

    // WebKit thread
    LayerWebKitThread* layer() const { return m_layer; }
    void layerWebKitThreadDestroyed();
    void setNeedsDisplay(const FloatRect& dirtyRect);
    void setNeedsDisplay();
    void updateTextureContentsIfNeeded(double scale);
    void disableTiling(bool);

    // Compositing thread
    void layerCompositingThreadDestroyed();
    void uploadTexturesIfNeeded();
    void drawTextures(LayerCompositingThread*, int positionLocation, int texCoordLocation);
    bool hasMissingTextures() const { return m_hasMissingTextures; }
    void drawMissingTextures(LayerCompositingThread*, int positionLocation, int texCoordLocation);
    void deleteTextures();
    void commitPendingTextureUploads();
    void layerVisibilityChanged(bool visible);
    bool hasDirtyTiles() const;
    void bindContentsTexture();

    // Thread safe
    void addRenderJob(const TileIndex&);
    void removeRenderJob(const TileIndex&);

private:
    struct TextureJob {
        enum Type { Unknown, SetContents, SetContentsToColor, UpdateContents, DiscardContents, ResizeContents, DirtyContents };

        TextureJob()
            : m_type(Unknown)
        {
        }

        TextureJob(Type type, const IntSize& newSize)
            : m_type(type)
            , m_isOpaque(false)
            , m_dirtyRect(IntPoint::zero(), newSize)
        {
            ASSERT(type == ResizeContents);
        }

        TextureJob(Type type, const IntRect& dirtyRect)
            : m_type(type)
            , m_isOpaque(false)
            , m_dirtyRect(dirtyRect)
        {
            ASSERT(type == DiscardContents || type == DirtyContents);
        }

        TextureJob(Type type, const SkBitmap& contents, const IntRect& dirtyRect, bool isOpaque)
            : m_type(type)
            , m_contents(contents)
            , m_isOpaque(isOpaque)
            , m_dirtyRect(dirtyRect)
        {
            ASSERT(type == UpdateContents || type == SetContents);
            ASSERT(!contents.isNull());
        }

        TextureJob(Type type, const Color& color, const TileIndex& index)
            : m_type(type)
            , m_isOpaque(false)
            , m_color(color)
            , m_index(index)
        {
            ASSERT(type == SetContentsToColor);
        }

        static TextureJob setContents(const SkBitmap& contents, bool isOpaque) { return TextureJob(SetContents, contents, IntRect(IntPoint::zero(), IntSize(contents.width(), contents.height())), isOpaque); }
        static TextureJob setContentsToColor(const Color& color, const TileIndex& index) { return TextureJob(SetContentsToColor, color, index); }
        static TextureJob updateContents(const SkBitmap& contents, const IntRect& dirtyRect, bool isOpaque) { return TextureJob(UpdateContents, contents, dirtyRect, isOpaque); }
        static TextureJob discardContents(const IntRect& dirtyRect) { return TextureJob(DiscardContents, dirtyRect); }
        static TextureJob resizeContents(const IntSize& newSize) { return TextureJob(ResizeContents, newSize); }
        static TextureJob dirtyContents(const IntRect& dirtyRect) { return TextureJob(DirtyContents, dirtyRect); }

        bool isNull() { return m_type == Unknown; }

        Type m_type;
        SkBitmap m_contents;
        bool m_isOpaque;
        IntRect m_dirtyRect;
        Color m_color;
        TileIndex m_index;
    };

    typedef HashMap<TileIndex, LayerTile*> TileMap;
    typedef HashMap<TileIndex, LayerTileData> VisibilityMap;
    typedef HashMap<TileIndex, const TextureJob*> TileJobsMap;

    IntSize tileSize() const { return m_tileSize; }
    void updateTileSize();

    LayerTiler(LayerWebKitThread*);

    // WebKit thread
    void addTextureJob(const TextureJob&);
    void clearTextureJobs();
    bool shouldPerformRenderJob(const TileIndex&, bool allowPrefill);
    bool shouldPrefillTile(const TileIndex&);

    // Compositing thread
    void updateTileContents(const TextureJob&, const IntRect&);
    void addTileJob(const TileIndex&, const TextureJob&, TileJobsMap&);
    void performTileJob(LayerTile*, const TextureJob&, const IntRect&);
    void processTextureJob(const TextureJob&, TileJobsMap&);
    void drawTexturesInternal(LayerCompositingThread*, int positionLocation, int texCoordLocation, bool missing);
    void pruneTextures();
    void visibilityChanged(bool needsDisplay);

    // Clear all pending update content texture jobs
    template<typename T>
    static void removeUpdateContentsJobs(T& jobs)
    {
        // Clear all pending update content jobs
        T list;
        for (typename T::iterator it = jobs.begin(); it != jobs.end(); ++it) {
            if ((*it).m_type != TextureJob::UpdateContents)
                list.append(*it);
        }
        jobs = list;
    }

    LayerWebKitThread* m_layer;

    TileMap m_tilesCompositingThread;

    VisibilityMap m_tilesWebKitThread;

    bool m_tilingDisabled;

    bool m_contentsDirty;
    FloatRect m_dirtyRect;

    IntSize m_pendingTextureSize; // Resized, but not committed yet.
    IntSize m_requiredTextureSize;
    IntSize m_tileSize;

    bool m_clearTextureJobs;
    Vector<TextureJob> m_pendingTextureJobs; // Added, but not committed yet.
    Deque<TextureJob> m_textureJobs;
    bool m_hasMissingTextures;

    HashSet<TileIndex> m_renderJobs;
    Mutex m_renderJobsMutex;
    double m_contentsScale;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerTiler_h
