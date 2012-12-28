/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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

#ifndef BackingStore_p_h
#define BackingStore_p_h

#include "BackingStore.h"
#include "Color.h"
#include "RenderQueue.h"
#include "TileIndex.h"
#include "TileIndexHash.h"
#include "Timer.h"
#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformGuardedPointer.h>
#include <pthread.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {
class IntRect;
class FloatRect;
class LayerRenderer;
class TransformationMatrix;
}

namespace BlackBerry {

namespace Platform {
class ViewportAccessor;
}

namespace WebKit {

class TileBuffer;
class WebPage;
class BackingStoreClient;

typedef WTF::HashMap<TileIndex, TileBuffer*> TileMap;

class BackingStoreGeometry {
public:
    BackingStoreGeometry()
        : m_numberOfTilesWide(0)
        , m_numberOfTilesHigh(0)
        , m_scale(0.0)
    {
    }

    Platform::IntRect backingStoreRect() const;
    Platform::IntSize backingStoreSize() const;

    int numberOfTilesWide() const { return m_numberOfTilesWide; }
    void setNumberOfTilesWide(int numberOfTilesWide) { m_numberOfTilesWide = numberOfTilesWide; }
    int numberOfTilesHigh() const { return m_numberOfTilesHigh; }
    void setNumberOfTilesHigh(int numberOfTilesHigh) { m_numberOfTilesHigh = numberOfTilesHigh; }
    Platform::IntPoint backingStoreOffset() const { return m_backingStoreOffset; }
    void setBackingStoreOffset(const Platform::IntPoint& offset) { m_backingStoreOffset = offset; }
    Platform::IntPoint originOfTile(const TileIndex&) const;
    TileBuffer* tileBufferAt(const TileIndex& index) const { return m_tileMap.get(index); }
    const TileMap& tileMap() const { return m_tileMap; }
    void setTileMap(const TileMap& tileMap) { m_tileMap = tileMap; }

    double scale() const { return m_scale; }
    void setScale(double scale) { m_scale = scale; }

    bool isTileCorrespondingToBuffer(TileIndex, TileBuffer*) const;

  private:
    int m_numberOfTilesWide;
    int m_numberOfTilesHigh;
    double m_scale;
    Platform::IntPoint m_backingStoreOffset;
    TileMap m_tileMap;
};

class BackingStoreWindowBufferState {
public:
    Platform::IntRectRegion blittedRegion() const { return m_blittedRegion; }
    void addBlittedRegion(const Platform::IntRectRegion& region)
    {
        m_blittedRegion = Platform::IntRectRegion::unionRegions(m_blittedRegion, region);
    }
    void clearBlittedRegion(const Platform::IntRectRegion& region)
    {
        m_blittedRegion = Platform::IntRectRegion::subtractRegions(m_blittedRegion, region);
    }
    void clearBlittedRegion() { m_blittedRegion = Platform::IntRectRegion(); }

    bool isRendered(const Platform::IntPoint& scrollPosition, const Platform::IntRectRegion& contents) const
    {
        return Platform::IntRectRegion::subtractRegions(contents, m_blittedRegion).isEmpty();
    }

  private:
    Platform::IntRectRegion m_blittedRegion;
};

class BackingStorePrivate : public BlackBerry::Platform::GuardedPointerBase {
public:
    enum TileMatrixDirection { Horizontal, Vertical };
    BackingStorePrivate();

    void instrumentBeginFrame();
    void instrumentCancelFrame();

    // Returns whether direct rendering is explicitly turned on or is
    // required because the surface pool is not large enough to meet
    // the minimum number of tiles required to scroll.
    bool shouldDirectRenderingToWindow() const;

    // Returns whether we're using the OpenGL code path for compositing the
    // backing store tiles. This can be due to the main window using
    // BlackBerry::Platform::Graphics::Window::GLES2Usage.
    bool isOpenGLCompositing() const;

    bool isSuspended() const { return m_suspendBackingStoreUpdates; }

    // Suspends all backingstore updates so that rendering to the backingstore is disabled.
    void suspendBackingStoreUpdates();

    // Resumes all backingstore updates so that rendering to the backingstore is enabled.
    void resumeBackingStoreUpdates();

    // Suspends all screen updates so that 'blitVisibleContents' is disabled.
    void suspendScreenUpdates();

    // Resumes all screen updates so that 'blitVisibleContents' is enabled.
    void resumeScreenUpdates(BackingStore::ResumeUpdateOperation);

    // The functions repaint(), slowScroll(), scroll(), scrollingStartedHelper() are
    // called from outside WebKit and within WebKit via ChromeClientBlackBerry.
    void repaint(const Platform::IntRect& windowRect, bool contentChanged, bool immediate);

    void slowScroll(const Platform::IntSize& delta, const Platform::IntRect& windowRect, bool immediate);

    void scroll(const Platform::IntSize& delta, const Platform::IntRect& scrollViewRect, const Platform::IntRect& clipRect);
    void scrollingStartedHelper(const Platform::IntSize& delta);

    bool shouldSuppressNonVisibleRegularRenderJobs() const;
    bool shouldPerformRenderJobs() const;
    bool shouldPerformRegularRenderJobs() const;
    void dispatchRenderJob();
    void renderJob();

    // Set of helper methods for the scrollBackingStore() method.
    Platform::IntRect contentsRect() const;
    Platform::IntRect expandedContentsRect() const;
    Platform::IntRect visibleContentsRect() const;
    Platform::IntRect unclippedVisibleContentsRect() const;
    bool shouldMoveLeft(const Platform::IntRect&) const;
    bool shouldMoveRight(const Platform::IntRect&) const;
    bool shouldMoveUp(const Platform::IntRect&) const;
    bool shouldMoveDown(const Platform::IntRect&) const;
    bool canMoveX(const Platform::IntRect&) const;
    bool canMoveY(const Platform::IntRect&) const;
    bool canMoveLeft(const Platform::IntRect&) const;
    bool canMoveRight(const Platform::IntRect&) const;
    bool canMoveUp(const Platform::IntRect&) const;
    bool canMoveDown(const Platform::IntRect&) const;

    Platform::IntRect backingStoreRectForScroll(int deltaX, int deltaY, const Platform::IntRect&) const;
    void setBackingStoreRect(const Platform::IntRect&, double scale);
    void updateTilesAfterBackingStoreRectChange();

    TileIndexList indexesForBackingStoreRect(const Platform::IntRect&) const;
    TileIndexList indexesForVisibleContentsRect(BackingStoreGeometry*) const;

    TileIndex indexOfTile(const Platform::IntPoint& origin, const Platform::IntRect& backingStoreRect) const;
    void clearAndUpdateTileOfNotRenderedRegion(const TileIndex&, TileBuffer*, const Platform::IntRectRegion&, BackingStoreGeometry*, bool update = true);
    bool isCurrentVisibleJob(const TileIndex&, BackingStoreGeometry*) const;

    // Not thread safe. Call only when threads are in sync.
    void clearRenderedRegion(TileBuffer*, const Platform::IntRectRegion&);

    // Responsible for scrolling the backing store and updating the
    // tile matrix geometry.
    void scrollBackingStore(int deltaX, int deltaY);

    // Render the given dirty rect and invalidate the screen.
    Platform::IntRect renderDirectToWindow(const Platform::IntRect&);

    // Render the given tiles if enough back buffers are available.
    // Return the actual set of rendered tiles.
    // NOTE: This should only be called by RenderQueue and resumeScreenUpdates().
    //   If you want to render to get contents to the screen, you should call
    //   renderAndBlitImmediately() or renderAndBlitVisibleContentsImmediately().
    TileIndexList render(const TileIndexList&);

    // Called by the render queue to ensure that the queue is in a
    // constant state before performing a render job.
    void requestLayoutIfNeeded() const;

    // Helper render methods.
    void renderAndBlitVisibleContentsImmediately();
    void renderAndBlitImmediately(const Platform::IntRect&);
    void blitVisibleContents(bool force = false);

    // Assumes the rect to be in window/viewport coordinates.
    void copyPreviousContentsToBackSurfaceOfWindow();
    void copyPreviousContentsToTileBuffer(const Platform::IntRect& excludeRect, TileBuffer* dstTileBuffer, TileBuffer* srcTileBuffer);
    void paintDefaultBackground(const Platform::IntRect& dstRect, BlackBerry::Platform::ViewportAccessor*, bool flush);
    void blitOnIdle();

    Platform::IntRect blitTileRect(TileBuffer*, const Platform::IntRect&, const Platform::IntPoint&, const WebCore::TransformationMatrix&, BackingStoreGeometry*);

#if USE(ACCELERATED_COMPOSITING)
    // Use instead of blitVisibleContents() if you need more control over
    // OpenGL state. Note that contents is expressed in untransformed
    // content coordinates.
    // Preconditions: You have to call prepareFrame and setViewport on the LayerRenderer before
    //                calling this.
    void compositeContents(WebCore::LayerRenderer*, const WebCore::TransformationMatrix&, const WebCore::FloatRect& contents, bool contentsOpaque);

    bool drawLayersOnCommitIfNeeded();
    void drawAndBlendLayersForDirectRendering(const Platform::IntRect& dirtyRect);
    // WebPage will call this when drawing layers to tell us we don't need to
    void willDrawLayersOnCommit() { m_needsDrawLayersOnCommit = false; }
    // WebPageCompositor uses this to cut down on excessive message sending.
    bool isDirectRenderingAnimationMessageScheduled() { return m_isDirectRenderingAnimationMessageScheduled; }
    void setDirectRenderingAnimationMessageScheduled() { m_isDirectRenderingAnimationMessageScheduled = true; }
#endif

    void blitHorizontalScrollbar();
    void blitVerticalScrollbar();

    // Returns whether the tile index is currently visible or not.
    bool isTileVisible(const TileIndex&, BackingStoreGeometry*) const;
    bool isTileVisible(const Platform::IntPoint&) const;

    // Returns a rect that is the union of all tiles that are visible.
    TileIndexList visibleTileIndexes(BackingStoreGeometry*) const;

    // Used to clip to the visible content for instance.
    Platform::IntRect tileVisibleContentsRect(const TileIndex&, BackingStoreGeometry*) const;

    // Used to clip to the contents for instance.
    Platform::IntRect tileContentsRect(const TileIndex&, const Platform::IntRect&, BackingStoreGeometry*) const;

    // This is called by WebPage once load is committed to reset the render queue.
    void resetRenderQueue();

    // This is called by WebPage once load is committed to reset all the tiles.
    void resetTiles();

    // This is called by WebPage after load is complete to update all the tiles.
    void updateTiles(bool updateVisible, bool immediate);

    // This is called during scroll and by the render queue.
    void updateTilesForScrollOrNotRenderedRegion(bool checkLoading = true);

    // Update an individual tile.
    void updateTile(const Platform::IntPoint& tileOrigin, bool immediate);

    typedef std::pair<TileIndex, Platform::IntRect> TileRect;
    typedef WTF::Vector<TileRect> TileRectList;
    TileRectList mapFromPixelContentsToTiles(const Platform::IntRect&, BackingStoreGeometry*) const;

    void setTileMatrixNeedsUpdate() { m_tileMatrixNeedsUpdate = true; }
    void updateTileMatrixIfNeeded();

    // Called by WebPagePrivate::notifyTransformedContentsSizeChanged.
    void contentsSizeChanged(const Platform::IntSize&);

    // Called by WebPagePrivate::notifyTransformedScrollChanged.
    void scrollChanged(const Platform::IntPoint&);

    // Called by WebpagePrivate::notifyTransformChanged.
    void transformChanged();

    // Called by WebpagePrivate::actualVisibleSizeChanged.
    void actualVisibleSizeChanged(const Platform::IntSize&);

    // Called by WebPagePrivate::setScreenRotated.
    void orientationChanged();

    // Sets the geometry of the tile matrix.
    void setGeometryOfTileMatrix(int numberOfTilesWide, int numberOfTilesHigh);

    // Create the surfaces of the backing store.
    void createSurfaces();

    // Various calculations of quantities relevant to backing store.
    int minimumNumberOfTilesWide() const;
    int minimumNumberOfTilesHigh() const;
    Platform::IntSize expandedContentsSize() const;

    // The tile geometry methods are all static function.
    static int tileWidth();
    static int tileHeight();
    static Platform::IntSize tileSize();

    // This takes transformed contents coordinates.
    void renderContents(BlackBerry::Platform::Graphics::Buffer*, const Platform::IntPoint& surfaceOffset, const Platform::IntRect& contentsRect) const;
    void renderContents(Platform::Graphics::Drawable* /*drawable*/, const Platform::IntRect& /*contentsRect*/, const Platform::IntSize& /*destinationSize*/) const;

    void blitToWindow(const Platform::IntRect& dstRect, const BlackBerry::Platform::Graphics::Buffer* srcBuffer, const Platform::IntRect& srcRect, BlackBerry::Platform::Graphics::BlendMode, unsigned char globalAlpha);
    void fillWindow(Platform::Graphics::FillPattern, const Platform::IntRect& dstRect, const Platform::IntPoint& contentsOrigin, double contentsScale);

    WebCore::Color webPageBackgroundColorUserInterfaceThread() const; // use WebSettings::backgroundColor() for the WebKit thread
    void setWebPageBackgroundColor(const WebCore::Color&);

    void invalidateWindow();
    void invalidateWindow(const Platform::IntRect& dst);
    void clearWindow(const Platform::IntRect&, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255);

    bool isScrollingOrZooming() const;
    void setScrollingOrZooming(bool scrollingOrZooming, bool shouldBlit = true);

    void lockBackingStore();
    void unlockBackingStore();

    BackingStoreGeometry* frontState() const;
    void adoptAsFrontState(BackingStoreGeometry* newFrontState);

    BackingStoreWindowBufferState* windowFrontBufferState() const;
    BackingStoreWindowBufferState* windowBackBufferState() const;

    static void setCurrentBackingStoreOwner(WebPage*);
    static WebPage* currentBackingStoreOwner() { return BackingStorePrivate::s_currentBackingStoreOwner; }
    bool isActive() const;

    // Surface abstraction, maybe BlackBerry::Platform::Graphics::Buffer could be made public instead.
    BlackBerry::Platform::IntSize surfaceSize() const;
    BlackBerry::Platform::Graphics::Buffer* buffer() const;

    void didRenderContent(const Platform::IntRectRegion& renderedRegion);

    static WebPage* s_currentBackingStoreOwner;

    unsigned m_suspendScreenUpdates;
    unsigned m_suspendBackingStoreUpdates;
    BackingStore::ResumeUpdateOperation m_resumeOperation;

    bool m_suspendRenderJobs;
    bool m_suspendRegularRenderJobs;
    bool m_tileMatrixNeedsUpdate;
    bool m_isScrollingOrZooming;
    WebPage* m_webPage;
    BackingStoreClient* m_client;
    OwnPtr<RenderQueue> m_renderQueue;
    mutable Platform::IntSize m_previousDelta;

    bool m_hasBlitJobs;

    WebCore::Color m_webPageBackgroundColor; // for user interface thread operations such as blitting

    mutable unsigned m_frontState;

    unsigned m_currentWindowBackBuffer;
    mutable BackingStoreWindowBufferState m_windowBufferState[2];

    TileMatrixDirection m_preferredTileMatrixDimension;

    Platform::IntRect m_visibleTileBufferRect;

    pthread_mutex_t m_mutex;

#if USE(ACCELERATED_COMPOSITING)
    mutable bool m_needsDrawLayersOnCommit; // Not thread safe, WebKit thread only
    bool m_isDirectRenderingAnimationMessageScheduled;
#endif

protected:
    virtual ~BackingStorePrivate();
};
} // namespace WebKit
} // namespace BlackBerry

#endif // BackingStore_p_h
