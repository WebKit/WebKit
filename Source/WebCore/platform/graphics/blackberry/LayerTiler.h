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
#include "LayerCompositingThreadClient.h"
#include "LayerTile.h"
#include "LayerTileIndex.h"

#include <BlackBerryPlatformGLES2Program.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class LayerCompositingThread;
class LayerWebKitThread;

class LayerTiler : public ThreadSafeRefCounted<LayerTiler>, public LayerCompositingThreadClient {
public:
    TileIndex indexOfTile(const IntPoint& origin);
    IntPoint originOfTile(const TileIndex&);
    IntRect rectForTile(const TileIndex&, const IntSize& bounds);

    static PassRefPtr<LayerTiler> create(LayerWebKitThread* layer)
    {
        return adoptRef(new LayerTiler(layer));
    }

    virtual ~LayerTiler();

    IntSize tileSize() const { return m_tileSize; }

    // WebKit thread
    LayerWebKitThread* layer() const { return m_layer; }
    void layerWebKitThreadDestroyed();
    void setNeedsDisplay(const FloatRect& dirtyRect);
    void setNeedsDisplay();
    void updateTextureContentsIfNeeded(double scale);
    void setNeedsBacking(bool);
    virtual void scheduleCommit();

    // Compositing thread
    virtual void layerCompositingThreadDestroyed(LayerCompositingThread*);
    virtual void layerVisibilityChanged(LayerCompositingThread*, bool visible);
    virtual void uploadTexturesIfNeeded(LayerCompositingThread*);
    virtual Texture* contentsTexture(LayerCompositingThread*);
    virtual void drawTextures(LayerCompositingThread*, double scale, const BlackBerry::Platform::Graphics::GLES2Program&);
    virtual void deleteTextures(LayerCompositingThread*);
    static void willCommit();
    virtual void commitPendingTextureUploads(LayerCompositingThread*);

private:
    struct TextureJob {
        enum Type { Unknown, SetContents, SetContentsToColor, UpdateContents, DiscardContents, ResizeContents, DirtyContents };

        TextureJob()
            : m_type(Unknown)
            , m_contents(0)
        {
        }

        TextureJob(Type type, const IntSize& newSize)
            : m_type(type)
            , m_contents(0)
            , m_isOpaque(false)
            , m_dirtyRect(IntPoint::zero(), newSize)
        {
            ASSERT(type == ResizeContents);
        }

        TextureJob(Type type, const IntRect& dirtyRect)
            : m_type(type)
            , m_contents(0)
            , m_isOpaque(false)
            , m_dirtyRect(dirtyRect)
        {
            ASSERT(type == DiscardContents || type == DirtyContents);
        }

        TextureJob(Type type, const Texture::HostType& contents, const IntRect& dirtyRect, bool isOpaque)
            : m_type(type)
            , m_contents(contents)
            , m_isOpaque(isOpaque)
            , m_dirtyRect(dirtyRect)
        {
            ASSERT(type == UpdateContents || type == SetContents);
            ASSERT(contents);
        }

        TextureJob(Type type, const Color& color, const TileIndex& index)
            : m_type(type)
            , m_contents(0)
            , m_isOpaque(false)
            , m_color(color)
            , m_index(index)
        {
            ASSERT(type == SetContentsToColor);
        }

        static TextureJob setContents(const Texture::HostType& contents, const IntRect& contentsRect, bool isOpaque)
        {
            return TextureJob(SetContents, contents, contentsRect, isOpaque);
        }
        static TextureJob setContentsToColor(const Color& color, const TileIndex& index) { return TextureJob(SetContentsToColor, color, index); }
        static TextureJob updateContents(const Texture::HostType& contents, const IntRect& dirtyRect, bool isOpaque) { return TextureJob(UpdateContents, contents, dirtyRect, isOpaque); }
        static TextureJob discardContents(const IntRect& dirtyRect) { return TextureJob(DiscardContents, dirtyRect); }
        static TextureJob resizeContents(const IntSize& newSize) { return TextureJob(ResizeContents, newSize); }
        static TextureJob dirtyContents(const IntRect& dirtyRect) { return TextureJob(DirtyContents, dirtyRect); }

        bool isNull() { return m_type == Unknown; }

        Type m_type;
        Texture::HostType m_contents;
        bool m_isOpaque;
        IntRect m_dirtyRect;
        Color m_color;
        TileIndex m_index;
    };

    typedef HashMap<TileIndex, LayerTile*> TileMap;
    typedef HashMap<TileIndex, const TextureJob*> TileJobsMap;

    void updateTileSize();

    LayerTiler(LayerWebKitThread*);

    // WebKit thread
    void addTextureJob(const TextureJob&);
    void clearTextureJobs();
    BlackBerry::Platform::Graphics::Buffer* createBuffer(const IntSize&);

    // Compositing thread
    void updateTileContents(const TextureJob&, const IntRect&);
    void addTileJob(const TileIndex&, const TextureJob&, TileJobsMap&);
    void performTileJob(LayerTile*, const TextureJob&, const IntRect&);
    void processTextureJob(const TextureJob&, TileJobsMap&);
    void pruneTextures();
    void visibilityChanged(bool needsDisplay);
    bool drawTile(LayerCompositingThread*, double scale, const TileIndex&, const FloatRect& dst, const BlackBerry::Platform::Graphics::GLES2Program&);

    // Threadsafe
    int needsRender() const { return static_cast<int const volatile &>(m_needsRenderCount); }

    // Clear all pending update content texture jobs
    template<typename T>
    static void removeUpdateContentsJobs(T& jobs)
    {
        // Clear all pending update content jobs
        T list;
        for (typename T::iterator it = jobs.begin(); it != jobs.end(); ++it) {
            if ((*it).m_type != TextureJob::UpdateContents)
                list.append(*it);
            else
                BlackBerry::Platform::Graphics::destroyBuffer((*it).m_contents);
        }
        jobs = list;
    }

    LayerWebKitThread* m_layer;

    Mutex m_tilesMutex;
    TileMap m_tiles; // Protected by m_tilesMutex
    int m_needsRenderCount; // atomic

    bool m_needsBacking;

    bool m_contentsDirty;
    FloatRect m_dirtyRect;

    IntSize m_pendingTextureSize; // Resized, but not committed yet.
    IntSize m_requiredTextureSize;
    IntSize m_tileSize;

    bool m_clearTextureJobs;
    Vector<TextureJob> m_pendingTextureJobs; // Added, but not committed yet.
    Deque<TextureJob> m_textureJobs;

    double m_contentsScale;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerTiler_h
