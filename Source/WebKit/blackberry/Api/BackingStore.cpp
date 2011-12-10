/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
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

#include "config.h"
#include "BackingStore.h"

#include "BackingStoreClient.h"
#include "BackingStoreCompositingSurface.h"
#include "BackingStoreTile.h"
#include "BackingStore_p.h"
#include "FatFingers.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "InspectorController.h"
#include "Page.h"
#include "SurfacePool.h"
#include "WebPage.h"
#include "WebPageClient.h"
#include "WebPage_p.h"
#include "WebSettings.h"

#include <BlackBerryPlatformExecutableMessage.h>
#include <BlackBerryPlatformIntRectRegion.h>
#include <BlackBerryPlatformMessage.h>
#include <BlackBerryPlatformMessageClient.h>

#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/NotFound.h>

#define SUPPRESS_NON_VISIBLE_REGULAR_RENDER_JOBS 0
#define ENABLE_SCROLLBARS 1
#define ENABLE_REPAINTONSCROLL 1
#define DEBUG_BACKINGSTORE 0
#define DEBUG_VISUALIZE 0
#define DEBUG_TILEMATRIX 0
#define DEBUG_COMPOSITING_DIRTY_REGION 0

#if USE(OPENVG)
#include "EGLDisplayOpenVG.h"
#include "EGLUtils.h"
#endif

#include <BlackBerryPlatformScreen.h>

using namespace WebCore;
using namespace std;

using BlackBerry::Platform::IntRect;
using BlackBerry::Platform::IntPoint;
using BlackBerry::Platform::IntSize;

namespace BlackBerry {
namespace WebKit {

const int s_renderTimerTimeout = 1.0;
WebPage* BackingStorePrivate::s_currentBackingStoreOwner = 0;

typedef std::pair<int, int> Divisor;
typedef Vector<Divisor> DivisorList;
// FIXME: Cache this and/or use a smarter algorithm.
static DivisorList divisors(unsigned n)
{
    DivisorList divisors;
    for (unsigned i = 1; i <= n; ++i)
        if (!(n % i))
            divisors.append(std::make_pair(i, n / i));
    return divisors;
}

// Compute best divisor given the ratio determined by size.
static Divisor bestDivisor(Platform::IntSize size, int tileWidth, int tileHeight,
                           int minimumNumberOfTilesWide, int minimumNumberOfTilesHigh,
                           BackingStorePrivate::TileMatrixDirection direction)
{
    // The point of this function is to determine the number of tiles in each
    // dimension. We do this by looking to match the tile matrix width/height
    // ratio as closely as possible with the width/height ratio of the contents.
    // We also look at the direction passed to give preference to one dimension
    // over another. This method could probably be made faster, but it gets the
    // job done.
    SurfacePool* surfacePool = SurfacePool::globalSurfacePool();
    ASSERT(!surfacePool->isEmpty());

    // Store a static list of possible divisors.
    static DivisorList divisorList = divisors(surfacePool->size());

    // The ratio we're looking to best imitate.
    float ratio = static_cast<float>(size.width()) / static_cast<float>(size.height());

    Divisor bestDivisor;
    for (size_t i = 0; i < divisorList.size(); ++i) {
        Divisor divisor = divisorList[i];

        bool divisorWidthIsPerfect = size.width() <= divisor.first * tileWidth && abs(size.width() - divisor.first * tileWidth) < tileWidth;
        bool divisorHeightIsPerfect = size.height() <= divisor.second * tileHeight && abs(size.height() - divisor.second * tileHeight) < tileHeight;
        bool divisorWidthIsValid = divisor.first >= minimumNumberOfTilesWide || divisorWidthIsPerfect;
        bool divisorHeightIsValid = divisor.second >= minimumNumberOfTilesHigh || divisorHeightIsPerfect;
        if (!divisorWidthIsValid || !divisorHeightIsValid)
            continue;

        if (divisor.first > divisor.second && direction == BackingStorePrivate::Vertical && !divisorHeightIsPerfect)
            continue;

        if (divisor.second > divisor.first && direction == BackingStorePrivate::Horizontal && !divisorWidthIsPerfect)
            continue;

        if (divisorWidthIsPerfect || divisorHeightIsPerfect) {
            bestDivisor = divisor; // Found a perfect fit!
#if DEBUG_TILEMATRIX
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "bestDivisor found perfect size widthPerfect=%s heightPerfect=%s",
                                   divisorWidthIsPerfect ? "true" : "false",
                                   divisorHeightIsPerfect ? "true" : "false");
#endif
            break;
        }

        // Store basis of comparison.
        if (!bestDivisor.first && !bestDivisor.second) {
            bestDivisor = divisor;
            continue;
        }

        // Compare ratios.
        float diff1 = fabs((static_cast<float>(divisor.first) / static_cast<float>(divisor.second)) - ratio);
        float diff2 = fabs((static_cast<float>(bestDivisor.first) / static_cast<float>(bestDivisor.second)) - ratio);
        if (diff1 < diff2)
            bestDivisor = divisor;
    }

    return bestDivisor;
}

struct BackingStoreMutexLocker {
    BackingStoreMutexLocker(BackingStorePrivate* backingStorePrivate)
        : m_backingStorePrivate(backingStorePrivate)
    {
        m_backingStorePrivate->lockBackingStore();
    }

    ~BackingStoreMutexLocker()
    {
        m_backingStorePrivate->unlockBackingStore();
    }

private:
    BackingStorePrivate* m_backingStorePrivate;
};

Platform::IntRect BackingStoreGeometry::backingStoreRect() const
{
    return Platform::IntRect(backingStoreOffset(), backingStoreSize());
}

Platform::IntSize BackingStoreGeometry::backingStoreSize() const
{
    return Platform::IntSize(numberOfTilesWide() * BackingStorePrivate::tileWidth(), numberOfTilesHigh() * BackingStorePrivate::tileHeight());
}

BackingStorePrivate::BackingStorePrivate()
    : m_suspendScreenUpdates(false)
    , m_suspendBackingStoreUpdates(false)
    , m_suspendRenderJobs(false)
    , m_suspendRegularRenderJobs(false)
    , m_isScrollingOrZooming(false)
    , m_webPage(0)
    , m_client(0)
    , m_renderQueue(adoptPtr(new RenderQueue(this)))
    , m_defersBlit(true)
    , m_hasBlitJobs(false)
#if USE(OPENVG)
    , m_eglDisplay(EGL_NO_DISPLAY)
#endif
    , m_currentWindowBackBuffer(0)
    , m_preferredTileMatrixDimension(Vertical)
    , m_blitGeneration(-1)
{
    m_frontState = reinterpret_cast<unsigned>(new BackingStoreGeometry);
    m_backState = reinterpret_cast<unsigned>(new BackingStoreGeometry);

    m_renderTimer = adoptPtr(new Timer<BackingStorePrivate>(this, &BackingStorePrivate::renderOnTimer));

    // Need a recursive mutex to achieve a global lock.
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    pthread_mutex_init(&m_blitGenerationLock, 0);
    pthread_cond_init(&m_blitGenerationCond, 0);
}

BackingStorePrivate::~BackingStorePrivate()
{
    BackingStoreGeometry* front = reinterpret_cast<BackingStoreGeometry*>(m_frontState);
    delete front;
    m_frontState = 0;

    BackingStoreGeometry* back = reinterpret_cast<BackingStoreGeometry*>(m_backState);
    delete back;
    m_backState = 0;

    pthread_mutex_destroy(&m_mutex);
}

bool BackingStorePrivate::shouldDirectRenderingToWindow() const
{
    if (m_webPage->settings()->isDirectRenderingToWindowEnabled() || !isActive())
        return true;

    const BackingStoreGeometry* currentState = frontState();
    const unsigned tilesNecessary = minimumNumberOfTilesWide() * minimumNumberOfTilesHigh();
    const unsigned tilesAvailable = currentState->numberOfTilesWide() * currentState->numberOfTilesHigh();
    return tilesAvailable < tilesNecessary;
}

void BackingStorePrivate::suspendScreenAndBackingStoreUpdates()
{
    m_suspendBackingStoreUpdates = true;

    // Make sure the user interface thread gets the message before we proceed
    // because blitContents can be called from this thread and it must honor
    // this flag.
    m_suspendScreenUpdates = true;
    BlackBerry::Platform::userInterfaceThreadMessageClient()->syncToCurrentMessage();

#if USE(ACCELERATED_COMPOSITING)
    m_webPage->d->resetCompositingSurface();
#endif
}

void BackingStorePrivate::resumeScreenAndBackingStoreUpdates(BackingStore::ResumeUpdateOperation op)
{
    m_suspendBackingStoreUpdates = false;

#if USE(ACCELERATED_COMPOSITING)
    if (op != BackingStore::None)
        m_webPage->d->setNeedsOneShotDrawingSynchronization();
#endif

    // For the direct rendering case, there is no such operation as blit,
    // we have to render to get anything to the screen.
    if (shouldDirectRenderingToWindow() && op == BackingStore::Blit)
        op = BackingStore::RenderAndBlit;

    // Do some rendering if necessary.
    if (op == BackingStore::RenderAndBlit)
        renderVisibleContents(true /*renderContentOnly*/);

    // Make sure the user interface thread gets the message before we proceed
    // because blitContents can be called from the user interface thread and
    // it must honor this flag.
    m_suspendScreenUpdates = false;
    BlackBerry::Platform::userInterfaceThreadMessageClient()->syncToCurrentMessage();

    // Do some blitting if necessary.
    if ((op == BackingStore::Blit || op == BackingStore::RenderAndBlit) && !shouldDirectRenderingToWindow())
        blitVisibleContents();
}

void BackingStorePrivate::repaint(const Platform::IntRect& windowRect,
                                  bool contentChanged, bool immediate)
{
#if USE(OPENVG)
    ASSERT(m_eglDisplay != EGL_NO_DISPLAY);
#endif

    if (m_suspendBackingStoreUpdates)
        return;

     // If immediate is true, then we're being asked to perform synchronously.
     // NOTE: WebCore::ScrollView will call this method with immediate:true and contentChanged:false.
     // This is a special case introduced specifically for the Apple's windows port and can be safely ignored I believe.
     // Now this method will be called from WebPagePrivate::repaint().

    if (contentChanged && !windowRect.isEmpty()) {
        // This windowRect is in untransformed coordinates relative to the viewport, but
        // it needs to be transformed coordinates relative to the transformed contents.
        Platform::IntRect rect = m_webPage->d->mapToTransformed(m_client->mapFromViewportToContents(windowRect));
        rect.inflate(1 /*dx*/, 1 /*dy*/); // Account for anti-aliasing of previous rendering runs.

        // FIXME: This should not explicitely depend on WebCore::.
        WebCore::IntRect tmpRect = rect;
        m_client->clipToTransformedContentsRect(tmpRect);

        rect = tmpRect;
        if (rect.isEmpty())
            return;

        if (immediate)
            render(rect, false /*renderContentOnly*/);
        else
            m_renderQueue->addToQueue(RenderQueue::RegularRender, rect);
    }
}

void BackingStorePrivate::slowScroll(const Platform::IntSize& delta, const Platform::IntRect& windowRect, bool immediate)
{
#if DEBUG_BACKINGSTORE
    // Start the time measurement...
    double time = WTF::currentTime();
#endif

    scrollingStartedHelper(delta);

    // This windowRect is in untransformed coordinates relative to the viewport, but
    // it needs to be transformed coordinates relative to the transformed contents.
    Platform::IntRect rect = m_webPage->d->mapToTransformed(m_client->mapFromViewportToContents(windowRect));

    if (immediate)
        render(rect, false /*renderContentOnly*/);
    else {
        m_renderQueue->addToQueue(RenderQueue::VisibleScroll, rect);
        // We only blit here if the client did not generate the scroll as the client
        // now supports blitting asynchronously during scroll operations.
        if (!m_client->isClientGeneratedScroll() && !shouldDirectRenderingToWindow())
            blitVisibleContents();
    }

#if DEBUG_BACKINGSTORE
    // Stop the time measurement.
    double elapsed = WTF::currentTime() - time;
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::slowScroll elapsed=%f", elapsed);
#endif
}

void BackingStorePrivate::scroll(const Platform::IntSize& delta,
                                 const Platform::IntRect& scrollViewRect,
                                 const Platform::IntRect& clipRect)
{
    // If we are direct rendering then we are forced to go down the slow path
    // to scrolling.
    if (shouldDirectRenderingToWindow()) {
        Platform::IntRect viewportRect(Platform::IntPoint(0, 0), m_webPage->d->transformedViewportSize());
        slowScroll(delta, m_webPage->d->mapFromTransformed(viewportRect), true /*immediate*/);
        return;
    }

#if DEBUG_BACKINGSTORE
    // Start the time measurement...
    double time = WTF::currentTime();
#endif

    scrollingStartedHelper(delta);

    // We only blit here if the client did not generate the scroll as the client
    // now supports blitting asynchronously during scroll operations.
    if (!m_client->isClientGeneratedScroll())
        blitVisibleContents();

#if DEBUG_BACKINGSTORE
    // Stop the time measurement.
    double elapsed = WTF::currentTime() - time;
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::scroll dx=%d, dy=%d elapsed=%f", delta.width(), delta.height(), elapsed);
#endif
}

void BackingStorePrivate::scrollingStartedHelper(const Platform::IntSize& delta)
{
    // Notify the render queue so that it can shuffle accordingly.
    m_renderQueue->updateSortDirection(delta.width(), delta.height());
    m_renderQueue->visibleContentChanged(visibleContentsRect());

    // Scroll the actual backingstore.
    scrollBackingStore(delta.width(), delta.height());

    // Add any newly visible tiles that have not been previously rendered to the queue
    // and check if the tile was previously rendered by regular render job.
    updateTilesForScrollOrNotRenderedRegion();
}

bool BackingStorePrivate::shouldSuppressNonVisibleRegularRenderJobs() const
{
#if SUPPRESS_NON_VISIBLE_REGULAR_RENDER_JOBS
    return true;
#else
    // Always suppress when loading as this drastically decreases page loading
    // time...
    return m_client->isLoading();
#endif
}

bool BackingStorePrivate::shouldPerformRenderJobs() const
{
    return (m_webPage->isVisible() || shouldDirectRenderingToWindow()) && !m_suspendRenderJobs && !m_suspendBackingStoreUpdates && !m_renderQueue->isEmpty(!m_suspendRegularRenderJobs);
}

bool BackingStorePrivate::shouldPerformRegularRenderJobs() const
{
    return shouldPerformRenderJobs() && !m_suspendRegularRenderJobs;
}

void BackingStorePrivate::startRenderTimer()
{
    // Called when render queue has a new job added.
    if (m_renderTimer->isActive() || m_renderQueue->isEmpty(!m_suspendRegularRenderJobs))
        return;

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::startRenderTimer time=%f", WTF::currentTime());
#endif
    m_renderTimer->startOneShot(s_renderTimerTimeout);
}

void BackingStorePrivate::stopRenderTimer()
{
    if (!m_renderTimer->isActive())
        return;

    // Called when we render something to restart.
#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::stopRenderTimer time=%f", WTF::currentTime());
#endif
    m_renderTimer->stop();
}

void BackingStorePrivate::renderOnTimer(WebCore::Timer<BackingStorePrivate>*)
{
    // This timer is a third method of starting a render operation that is a catch-all. If more
    // than s_renderTimerTimeout elapses with no rendering taking place and render jobs in the queue, then
    // renderOnTimer will be called which will actually render.
    if (!shouldPerformRenderJobs())
        return;

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::renderOnTimer time=%f", WTF::currentTime());
#endif
    while (m_renderQueue->hasCurrentVisibleZoomJob() || m_renderQueue->hasCurrentVisibleScrollJob())
        m_renderQueue->render(!m_suspendRegularRenderJobs);

    if (!shouldPerformRegularRenderJobs() || !m_renderQueue->hasCurrentRegularRenderJob())
        return;

    m_renderQueue->renderAllCurrentRegularRenderJobs();
}

void BackingStorePrivate::renderOnIdle()
{
    ASSERT(shouldPerformRenderJobs());

    // Let the render queue know that we entered a new event queue cycle
    // so it can determine if it is under pressure.
    m_renderQueue->eventQueueCycled();

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::renderOnIdle");
#endif

    m_renderQueue->render(!m_suspendRegularRenderJobs);
}

bool BackingStorePrivate::willFireTimer()
{
    // Let the render queue know that we entered a new event queue cycle
    // so it can determine if it is under pressure.
    m_renderQueue->eventQueueCycled();

    if (!shouldPerformRegularRenderJobs() || !m_renderQueue->hasCurrentRegularRenderJob() || !m_renderQueue->currentRegularRenderJobBatchUnderPressure())
        return true;

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::willFireTimer");
#endif

    // We've detected that the regular render jobs are coming under pressure likely
    // due to timers firing producing invalidation jobs and our efforts to break them
    // up into bite size pieces has produced a situation where we can not complete
    // a batch of them before receiving more that intersect them which causes us
    // to start the batch over. To mitigate this we have to empty the current batch
    // when this is detected.

    // We still want to perform priority jobs first to avoid redundant paints.
    while (m_renderQueue->hasCurrentVisibleZoomJob() || m_renderQueue->hasCurrentVisibleScrollJob())
        m_renderQueue->render(!m_suspendRegularRenderJobs);

    if (m_renderQueue->hasCurrentRegularRenderJob())
        m_renderQueue->renderAllCurrentRegularRenderJobs();

    // Let the caller yield and reschedule the timer.
    return false;
}

Platform::IntRect BackingStorePrivate::expandedContentsRect() const
{
    return Platform::IntRect(Platform::IntPoint(0, 0), expandedContentsSize());
}

Platform::IntRect BackingStorePrivate::visibleContentsRect() const
{
    return intersection(m_client->transformedVisibleContentsRect(),
                        Platform::IntRect(Platform::IntPoint(0, 0), m_client->transformedContentsSize()));
}

Platform::IntRect BackingStorePrivate::unclippedVisibleContentsRect() const
{
    return m_client->transformedVisibleContentsRect();
}

bool BackingStorePrivate::shouldMoveLeft(const Platform::IntRect& backingStoreRect) const
{
    return canMoveX(backingStoreRect)
            && backingStoreRect.x() > visibleContentsRect().x()
            && backingStoreRect.x() > expandedContentsRect().x();
}

bool BackingStorePrivate::shouldMoveRight(const Platform::IntRect& backingStoreRect) const
{
    return canMoveX(backingStoreRect)
            && backingStoreRect.right() < visibleContentsRect().right()
            && backingStoreRect.right() < expandedContentsRect().right();
}

bool BackingStorePrivate::shouldMoveUp(const Platform::IntRect& backingStoreRect) const
{
    return canMoveY(backingStoreRect)
            && backingStoreRect.y() > visibleContentsRect().y()
            && backingStoreRect.y() > expandedContentsRect().y();
}

bool BackingStorePrivate::shouldMoveDown(const Platform::IntRect& backingStoreRect) const
{
    return canMoveY(backingStoreRect)
            && backingStoreRect.bottom() < visibleContentsRect().bottom()
            && backingStoreRect.bottom() < expandedContentsRect().bottom();
}

bool BackingStorePrivate::canMoveX(const Platform::IntRect& backingStoreRect) const
{
    return backingStoreRect.width() > visibleContentsRect().width();
}

bool BackingStorePrivate::canMoveY(const Platform::IntRect& backingStoreRect) const
{
    return backingStoreRect.height() > visibleContentsRect().height();
}

bool BackingStorePrivate::canMoveLeft(const Platform::IntRect& rect) const
{
    Platform::IntRect backingStoreRect = rect;
    Platform::IntRect visibleContentsRect = this->visibleContentsRect();
    Platform::IntRect contentsRect = this->expandedContentsRect();
    backingStoreRect.move(-tileWidth(), 0);
    return backingStoreRect.right() >= visibleContentsRect.right()
            && backingStoreRect.x() >= contentsRect.x();
}

bool BackingStorePrivate::canMoveRight(const Platform::IntRect& rect) const
{
    Platform::IntRect backingStoreRect = rect;
    Platform::IntRect visibleContentsRect = this->visibleContentsRect();
    Platform::IntRect contentsRect = this->expandedContentsRect();
    backingStoreRect.move(tileWidth(), 0);
    return backingStoreRect.x() <= visibleContentsRect.x()
            && (backingStoreRect.right() <= contentsRect.right()
            || (backingStoreRect.right() - contentsRect.right()) < tileWidth());
}

bool BackingStorePrivate::canMoveUp(const Platform::IntRect& rect) const
{
    Platform::IntRect backingStoreRect = rect;
    Platform::IntRect visibleContentsRect = this->visibleContentsRect();
    Platform::IntRect contentsRect = this->expandedContentsRect();
    backingStoreRect.move(0, -tileHeight());
    return backingStoreRect.bottom() >= visibleContentsRect.bottom()
            && backingStoreRect.y() >= contentsRect.y();
}

bool BackingStorePrivate::canMoveDown(const Platform::IntRect& rect) const
{
    Platform::IntRect backingStoreRect = rect;
    Platform::IntRect visibleContentsRect = this->visibleContentsRect();
    Platform::IntRect contentsRect = this->expandedContentsRect();
    backingStoreRect.move(0, tileHeight());
    return backingStoreRect.y() <= visibleContentsRect.y()
            && (backingStoreRect.bottom() <= contentsRect.bottom()
            || (backingStoreRect.bottom() - contentsRect.bottom()) < tileHeight());
}

Platform::IntRect BackingStorePrivate::backingStoreRectForScroll(int deltaX, int deltaY, const Platform::IntRect& rect) const
{
    // The current rect.
    Platform::IntRect backingStoreRect = rect;

    // This method uses the delta values to describe the backingstore rect
    // given the current scroll direction and the viewport position. However,
    // this method can be called with no deltas whatsoever for instance when
    // the contents size changes or the orientation changes. In this case, we
    // want to use the previous scroll direction to describe the backingstore
    // rect. This will result in less checkerboard.
    if (!deltaX && !deltaY) {
        deltaX = m_previousDelta.width();
        deltaY = m_previousDelta.height();
    }
    m_previousDelta = Platform::IntSize(deltaX, deltaY);

    // Return to origin if need be.
    if (!canMoveX(backingStoreRect) && backingStoreRect.x())
        backingStoreRect.setX(0);

    if (!canMoveY(backingStoreRect) && backingStoreRect.y())
        backingStoreRect.setY(0);

    // Move the rect left.
    while (shouldMoveLeft(backingStoreRect) || (deltaX > 0 && canMoveLeft(backingStoreRect)))
        backingStoreRect.move(-tileWidth(), 0);

    // Move the rect right.
    while (shouldMoveRight(backingStoreRect) || (deltaX < 0 && canMoveRight(backingStoreRect)))
        backingStoreRect.move(tileWidth(), 0);

    // Move the rect up.
    while (shouldMoveUp(backingStoreRect) || (deltaY > 0 && canMoveUp(backingStoreRect)))
        backingStoreRect.move(0, -tileHeight());

    // Move the rect down.
    while (shouldMoveDown(backingStoreRect) || (deltaY < 0 && canMoveDown(backingStoreRect)))
        backingStoreRect.move(0, tileHeight());

    return backingStoreRect;
}

void BackingStorePrivate::setBackingStoreRect(const Platform::IntRect& backingStoreRect)
{
    if (!m_webPage->isVisible())
        return;

    if (!isActive()) {
        m_webPage->d->setShouldResetTilesWhenShown(true);
        return;
    }

    Platform::IntRect currentBackingStoreRect = frontState()->backingStoreRect();

    if (backingStoreRect == currentBackingStoreRect)
        return;

#if DEBUG_TILEMATRIX
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::setBackingStoreRect changed from (%d,%d %dx%d) to (%d,%d %dx%d)",
                           currentBackingStoreRect.x(),
                           currentBackingStoreRect.y(),
                           currentBackingStoreRect.width(),
                           currentBackingStoreRect.height(),
                           backingStoreRect.x(),
                           backingStoreRect.y(),
                           backingStoreRect.width(),
                           backingStoreRect.height());
#endif

    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    TileIndexList indexesToFill = indexesForBackingStoreRect(backingStoreRect);

    ASSERT(static_cast<int>(indexesToFill.size()) == currentMap.size());

    TileMap newTileMap;
    TileMap leftOverTiles;

    // Iterate through our current tile map and add tiles that are rendered with
    // our new backing store rect.
    TileMap::const_iterator tileMapEnd = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != tileMapEnd; ++it) {
        TileIndex oldIndex = it->first;
        BackingStoreTile* tile = it->second;

        // Reset the old index.
        resetTile(oldIndex, tile, false /*resetBackground*/);

        // Origin of last committed render for tile in transformed content coordinates.
        Platform::IntPoint origin = originOfLastRenderForTile(oldIndex, tile, currentBackingStoreRect);

        // If the new backing store rect contains this origin, then insert the tile there
        // and mark it as no longer shifted. Note: Platform::IntRect::contains checks for a 1x1 rect
        // below and to the right of the origin so it is correct usage here.
        if (backingStoreRect.contains(origin)) {
            TileIndex newIndex = indexOfTile(origin, backingStoreRect);
            Platform::IntRect rect(origin, tileSize());
            if (m_renderQueue->regularRenderJobsPreviouslyAttemptedButNotRendered(rect)) {
                // If the render queue previously tried to render this tile, but the
                // backingstore wasn't in the correct place or the tile wasn't visible
                // at the time then we can't simply restore the tile since the content
                // is now invalid as far as WebKit is concerned. Instead, we clear
                // the tile here of the region and then put the tile in the render
                // queue again.

                // Intersect the tile with the not rendered region to get the areas
                // of the tile that we need to clear.
                Platform::IntRectRegion tileNotRenderedRegion = Platform::IntRectRegion::intersectRegions(m_renderQueue->regularRenderJobsNotRenderedRegion(), rect);
                clearAndUpdateTileOfNotRenderedRegion(newIndex, tile, tileNotRenderedRegion, backingStoreRect);
#if DEBUG_BACKINGSTORE
                Platform::IntRect extents = tileNotRenderedRegion.extents();
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::setBackingStoreRect did clear tile %d,%d %dx%d",
                                       extents.x(), extents.y(), extents.width(), extents.height());
#endif
            } else {
                // Mark as needing update.
                if (!tile->frontBuffer()->isRendered()
                    && !isCurrentVisibleJob(newIndex, tile, backingStoreRect))
                    updateTile(origin, false /*immediate*/);
            }

            // Do some bookkeeping with shifting tiles...
            tile->clearShift();
            tile->setCommitted(true);

            size_t i = indexesToFill.find(newIndex);
            ASSERT(i != WTF::notFound);
            indexesToFill.remove(i);
            newTileMap.add(newIndex, tile);
        } else {
            // Store this tile and index so we can add it to the remaining left over spots...
            leftOverTiles.add(oldIndex, tile);
        }
    }

    ASSERT(static_cast<int>(indexesToFill.size()) == leftOverTiles.size());
    size_t i = 0;
    TileMap::const_iterator leftOverEnd = leftOverTiles.end();
    for (TileMap::const_iterator it = leftOverTiles.begin(); it != leftOverEnd; ++it) {
        TileIndex oldIndex = it->first;
        BackingStoreTile* tile = it->second;
        if (i >= indexesToFill.size()) {
            ASSERT_NOT_REACHED();
            break;
        }

        TileIndex newIndex = indexesToFill.at(i);

        // Origin of last committed render for tile in transformed content coordinates.
        Platform::IntPoint originOfOld = originOfLastRenderForTile(oldIndex, tile, currentBackingStoreRect);
        // Origin of the new index for the new backing store rect.
        Platform::IntPoint originOfNew = originOfTile(newIndex, backingStoreRect);

        // Mark as needing update.
        updateTile(originOfNew, false /*immediate*/);

        tile->clearShift();
        tile->setCommitted(false);
        tile->setHorizontalShift((originOfOld.x() - originOfNew.x()) / tileWidth());
        tile->setVerticalShift((originOfOld.y() - originOfNew.y()) / tileHeight());

        newTileMap.add(newIndex, tile);

        ++i;
    }

    // Checks to make sure we haven't lost any tiles.
    ASSERT(currentMap.size() == newTileMap.size());

    backState()->setNumberOfTilesWide(backingStoreRect.width() / tileWidth());
    backState()->setNumberOfTilesHigh(backingStoreRect.height() / tileHeight());
    backState()->setBackingStoreOffset(backingStoreRect.location());
    backState()->setTileMap(newTileMap);

    swapState();
}

BackingStorePrivate::TileIndexList BackingStorePrivate::indexesForBackingStoreRect(const Platform::IntRect& backingStoreRect) const
{
    TileIndexList indexes;
    int numberOfTilesWide = backingStoreRect.width() / tileWidth();
    int numberOfTilesHigh = backingStoreRect.height() / tileHeight();
    for (int y = 0; y < numberOfTilesHigh; ++y) {
        for (int x = 0; x < numberOfTilesWide; ++x) {
            TileIndex index(x, y);
            indexes.append(index);
        }
    }
    return indexes;
}

Platform::IntPoint BackingStorePrivate::originOfLastRenderForTile(const TileIndex& index,
                                                                 BackingStoreTile* tile,
                                                                 const Platform::IntRect& backingStoreRect) const
{
    return originOfTile(indexOfLastRenderForTile(index, tile), backingStoreRect);
}

TileIndex BackingStorePrivate::indexOfLastRenderForTile(const TileIndex& index, BackingStoreTile* tile) const
{
    return TileIndex(index.i() + tile->horizontalShift(), index.j() + tile->verticalShift());
}

TileIndex BackingStorePrivate::indexOfTile(const Platform::IntPoint& origin,
                                           const Platform::IntRect& backingStoreRect) const
{
    int offsetX = origin.x() - backingStoreRect.x();
    int offsetY = origin.y() - backingStoreRect.y();
    if (offsetX)
        offsetX = offsetX / tileWidth();
    if (offsetY)
        offsetY = offsetY / tileHeight();
    return TileIndex(offsetX, offsetY);
}

void BackingStorePrivate::clearAndUpdateTileOfNotRenderedRegion(const TileIndex& index, BackingStoreTile* tile,
                                                                const Platform::IntRectRegion& tileNotRenderedRegion,
                                                                const Platform::IntRect& backingStoreRect,
                                                                bool update)
{
    // Intersect the tile with the not rendered region to get the areas
    // of the tile that we need to clear.
    IntRectList tileNotRenderedRegionRects = tileNotRenderedRegion.rects();
    for (size_t i = 0; i < tileNotRenderedRegionRects.size(); ++i) {
        Platform::IntRect tileNotRenderedRegionRect = tileNotRenderedRegionRects.at(i);
        // Clear the render queue of this rect.
        m_renderQueue->clear(tileNotRenderedRegionRect, true /*clearRegularRenderJobs*/);

        if (update) {
            // Add it again as a regular render job.
            m_renderQueue->addToQueue(RenderQueue::RegularRender, tileNotRenderedRegionRect);
        }

        // Find the origin of this tile.
        Platform::IntPoint origin = originOfTile(index, backingStoreRect);

        // Map to tile coordinates.
        tileNotRenderedRegionRect.move(-origin.x(), -origin.y());

        // Clear the tile of this region.
        tile->frontBuffer()->clearRenderedRegion(tileNotRenderedRegionRect);
        tile->backBuffer()->clearRenderedRegion(tileNotRenderedRegionRect);
    }
}

bool BackingStorePrivate::isCurrentVisibleJob(const TileIndex& index, BackingStoreTile* tile, const Platform::IntRect& backingStoreRect) const
{
    // First check if the whole rect is in the queue.
    Platform::IntRect wholeRect = Platform::IntRect(originOfTile(index, backingStoreRect), tileSize());
    if (m_renderQueue->isCurrentVisibleScrollJob(wholeRect) || m_renderQueue->isCurrentVisibleScrollJobCompleted(wholeRect))
        return true;

    // Second check if the individual parts of the non-rendered region are in the regular queue.
    bool isCurrent = true; // It is true until it isn't :)

    IntRectList tileNotRenderedRegionRects = tile->frontBuffer()->notRenderedRegion().rects();
    for (size_t i = 0; i < tileNotRenderedRegionRects.size(); ++i) {
        Platform::IntRect tileNotRenderedRegionRect = tileNotRenderedRegionRects.at(i);
        Platform::IntPoint origin = originOfTile(index, backingStoreRect);

        // Map to transformed contents coordinates.
        tileNotRenderedRegionRect.move(origin.x(), origin.y());

        isCurrent = m_renderQueue->isCurrentRegularRenderJob(tileNotRenderedRegionRect) ? isCurrent : false;
    }

    return isCurrent;
}

void BackingStorePrivate::scrollBackingStore(int deltaX, int deltaY)
{
    if (!m_webPage->isVisible())
        return;

    if (!isActive()) {
        m_webPage->d->setShouldResetTilesWhenShown(true);
        return;
    }

    // Calculate our new preferred matrix dimension.
    if (deltaX || deltaY)
        m_preferredTileMatrixDimension = abs(deltaX) > abs(deltaY) ? Horizontal : Vertical;

    // Calculate our preferred matrix geometry.
    Divisor divisor = bestDivisor(expandedContentsSize(),
                                  tileWidth(), tileHeight(),
                                  minimumNumberOfTilesWide(), minimumNumberOfTilesHigh(),
                                  m_preferredTileMatrixDimension);

#if DEBUG_TILEMATRIX
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::scrollBackingStore divisor %dx%d",
                           divisor.first,
                           divisor.second);
#endif

    // Initialize a rect with that new geometry.
    Platform::IntRect backingStoreRect(0, 0, divisor.first * tileWidth(), divisor.second * tileHeight());

    // Scroll that rect so that it fits our contents and viewport and scroll delta.
    backingStoreRect = backingStoreRectForScroll(deltaX, deltaY, backingStoreRect);

    ASSERT(!backingStoreRect.isEmpty());

    setBackingStoreRect(backingStoreRect);
}

bool BackingStorePrivate::render(const IntRectList& rectList, bool renderContentOnly)
{
    // FIXME: We cycle through one by one and only blit the contents at the end.
    // This can be improved upon if we had a mapFromTransformedContentsToTiles that
    // took a set of rects and decomposed them appropriately.
    bool rendered = false;
    for (size_t i = 0; i < rectList.size(); ++i)
        rendered = render(rectList.at(i), true) ? true : rendered;

    if (rendered && !renderContentOnly) {
        if (!shouldDirectRenderingToWindow())
            blitVisibleContents();
        else
            invalidateWindow();
    }

    return rendered;
}

bool BackingStorePrivate::renderDirectToWindow(const Platform::IntRect& rect, bool renderContentOnly)
{
    requestLayoutIfNeeded();

    Platform::IntRect dirtyRect = rect;
    dirtyRect.intersect(unclippedVisibleContentsRect());

    if (dirtyRect.isEmpty())
        return false;

    Platform::IntRect screenRect = m_client->mapFromTransformedContentsToTransformedViewport(dirtyRect);
    windowFrontBufferState()->clearBlittedRegion(screenRect);
    copyPreviousContentsToBackSurfaceOfWindow();

    paintDefaultBackground(dirtyRect, TransformationMatrix(), true /*flush*/);

    const Platform::IntPoint origin = unclippedVisibleContentsRect().location();
    // We don't need a buffer since we're direct rendering to window.
    renderContents(0, origin, dirtyRect);
    windowBackBufferState()->addBlittedRegion(screenRect);

#if USE(ACCELERATED_COMPOSITING) && ENABLE_COMPOSITING_SURFACE
    if (m_webPage->d->m_client->window()->windowUsage() != BlackBerry::Platform::Graphics::Window::GLES2Usage) {
        Platform::IntRect clippedRect = intersection(dirtyRect, visibleContentsRect());
        blendCompositingSurface(clippedRect);
    }
#endif

    bool shouldInvalidate = !m_suspendScreenUpdates && !renderContentOnly;
    if (!shouldInvalidate)
        return true;

    invalidateWindow(screenRect);
    return true;
}

bool BackingStorePrivate::render(const Platform::IntRect& rect, bool renderContentOnly)
{
    if (!m_webPage->isVisible())
        return false;

    requestLayoutIfNeeded();

    if (shouldDirectRenderingToWindow())
        return renderDirectToWindow(rect, renderContentOnly);

    TileRectList tileRectList = mapFromTransformedContentsToTiles(rect);
    if (tileRectList.isEmpty())
        return false;

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical,
                           "BackingStorePrivate::render rect=(%d,%d %dx%d), renderContentOnly=%s, m_suspendBackingStoreUpdates = %s",
                           rect.x(), rect.y(), rect.width(), rect.height(),
                           renderContentOnly ? "true" : "false",
                           m_suspendBackingStoreUpdates ? "true" : "false");
#endif

    bool blittingDirectlyToCompositingWindow = m_webPage->d->m_client->window()->windowUsage() == BlackBerry::Platform::Graphics::Window::GLES2Usage;

    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    Platform::IntRect dirtyContentsRect;
    bool shouldBlit = !m_suspendBackingStoreUpdates && !renderContentOnly;
    const Platform::IntRect contentsRect = Platform::IntRect(Platform::IntPoint(0, 0), m_client->transformedContentsSize());
    const Platform::IntRect viewportRect = Platform::IntRect(Platform::IntPoint(0, 0), m_client->transformedViewportSize());

    for (size_t i = 0; i < tileRectList.size(); ++i) {
        TileRect tileRect = tileRectList[i];
        TileIndex index = tileRect.first;
        Platform::IntRect dirtyTileRect = tileRect.second;
        BackingStoreTile* tile = currentMap.get(index);

        // This dirty tile rect is in tile coordinates, but it needs to be in
        // transformed contents coordinates.
        Platform::IntRect dirtyRect = mapFromTilesToTransformedContents(tileRect);

        // If we're not yet committed, then commit now by clearing the rendered region
        // and setting the committed flag as well as clearing the shift.
        if (!tile->isCommitted()) {
            tile->setCommitted(true);
            tile->frontBuffer()->clearRenderedRegion();
            tile->backBuffer()->clearRenderedRegion();
            tile->clearShift();
        }

        // If the tile has been created, but this is the first time we are painting on it
        // then it hasn't been given a default background yet so that we can save time during
        // startup. That's why we are doing it here instead...
        if (!tile->backgroundPainted())
            tile->paintBackground();

        // Paint default background if contents rect is empty.
        if (!expandedContentsRect().isEmpty()) {
            // Otherwise we should clip the contents size and render the content.
            dirtyRect.intersect(expandedContentsRect());

            dirtyTileRect.intersect(tileContentsRect(index, expandedContentsRect(), currentState));

            // We probably have extra tiles since the contents size is so small.
            // Save some cycles here...
            if (dirtyRect.isEmpty())
                continue;
        }

        copyPreviousContentsToBackSurfaceOfTile(dirtyTileRect, tile);

        BlackBerry::Platform::Graphics::Buffer* nativeBuffer
            = tile->backBuffer()->nativeBuffer();

        if (blittingDirectlyToCompositingWindow) {
            pthread_mutex_lock(&m_blitGenerationLock);
            while (m_blitGeneration == tile->backBuffer()->blitGeneration()) {
                int err = pthread_cond_timedwait(&m_blitGenerationCond, &m_blitGenerationLock, &m_currentBlitEnd);
                if (err == ETIMEDOUT) {
                    ++m_blitGeneration;
                    break;
                }
                if (err) {
                    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical,
                                              "cond_timedwait failed (%s)", strerror(err));
                    break;
                }
            }
            pthread_mutex_unlock(&m_blitGenerationLock);
        }

        // FIXME: modify render to take a Vector<IntRect> parameter so we're not recreating
        // GraphicsContext on the stack each time.
        renderContents(nativeBuffer, originOfTile(index), dirtyRect);

        // Add the newly rendered region to the tile so it can keep track for blits.
        tile->backBuffer()->addRenderedRegion(dirtyTileRect);

        // Check if the contents for this tile's backbuffer are valid when
        // compared to the front buffer.
        bool backBufferIsValid = tile->backBuffer()->isRendered(tile->frontBuffer()->renderedRegion());

        // Our current design demands that the backbuffer is valid after any
        // rendering operation so assert that here. If we hit this assert we
        // know that we're doing something bad that will result in artifacts.
        ASSERT(backBufferIsValid);

        // We will need a swap here because of the shared back buffer.
        if (backBufferIsValid) {
            tile->swapBuffers();
            BlackBerry::Platform::userInterfaceThreadMessageClient()->syncToCurrentMessage();
            tile->backBuffer()->clearRenderedRegion();
        }

        dirtyContentsRect = Platform::unionOfRects(dirtyContentsRect, dirtyRect);
    }

    // Clip to the visible content including overscroll.
    dirtyContentsRect.intersect(unclippedVisibleContentsRect());
    if (dirtyContentsRect.isEmpty())
        return true;

    if (!shouldBlit)
        return true;

    blitVisibleContents();
    return true;
}

void BackingStorePrivate::requestLayoutIfNeeded() const
{
    m_webPage->d->requestLayoutIfNeeded();
}

void BackingStorePrivate::renderVisibleContents(bool renderContentOnly)
{
    Platform::IntRect renderRect = shouldDirectRenderingToWindow() ? visibleContentsRect() : visibleTilesRect();

    render(renderRect, renderContentOnly);
    m_renderQueue->clear(renderRect, true /*clearRegularRenderJobs*/);
}

void BackingStorePrivate::renderBackingStore(bool renderContentOnly)
{
    render(frontState()->backingStoreRect(), renderContentOnly);
}

void BackingStorePrivate::blitVisibleContents(bool force)
{
    // Blitting must never happen for direct rendering case.
    ASSERT(!shouldDirectRenderingToWindow());

    if (m_suspendScreenUpdates) {
        // Avoid client going into busy loop while updates suspended.
        if (force)
            m_hasBlitJobs = false;
        return;
    }

    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage(
                &BackingStorePrivate::blitVisibleContents, this, force));
        return;
    }

    blitContents(m_webPage->client()->userInterfaceBlittedDestinationRect(),
                 m_webPage->client()->userInterfaceBlittedVisibleContentsRect(),
                 force);
}

void BackingStorePrivate::copyPreviousContentsToBackSurfaceOfWindow()
{
    Platform::IntRectRegion previousContentsRegion
        = Platform::IntRectRegion::subtractRegions(windowFrontBufferState()->blittedRegion(), windowBackBufferState()->blittedRegion());

    if (previousContentsRegion.isEmpty())
        return;

    m_webPage->client()->window()->copyFromFrontToBack(previousContentsRegion);
    windowBackBufferState()->addBlittedRegion(previousContentsRegion);
}

void BackingStorePrivate::copyPreviousContentsToBackSurfaceOfTile(const Platform::IntRect& rect,
                                                                  BackingStoreTile* tile)
{
    Platform::IntRectRegion previousContentsRegion
        = Platform::IntRectRegion::subtractRegions(tile->frontBuffer()->renderedRegion(), rect);

    IntRectList previousContentsRects = previousContentsRegion.rects();
    for (size_t i = 0; i < previousContentsRects.size(); ++i) {
        Platform::IntRect previousContentsRect = previousContentsRects.at(i);
        tile->backBuffer()->addRenderedRegion(previousContentsRect);

        BlackBerry::Platform::Graphics::blitToBuffer(
            tile->backBuffer()->nativeBuffer(), previousContentsRect,
            tile->frontBuffer()->nativeBuffer(), previousContentsRect);
    }
}

void BackingStorePrivate::paintDefaultBackground(const Platform::IntRect& contents,
                                                 const WebCore::TransformationMatrix& transformation,
                                                 bool flush)
{
    const Platform::IntRect contentsRect = Platform::IntRect(Platform::IntPoint(0, 0), m_webPage->d->transformedContentsSize());
    Platform::IntPoint origin = contents.location();
    Platform::IntRect contentsClipped = contents;

    // We have to paint the default background in the case of overzoom and
    // make sure it is invalidated.
    Color color(m_webPage->settings()->overZoomColor());

    Platform::IntRectRegion overScrollRegion
            = Platform::IntRectRegion::subtractRegions(Platform::IntRect(contentsClipped), contentsRect);

    IntRectList overScrollRects = overScrollRegion.rects();
    for (size_t i = 0; i < overScrollRects.size(); ++i) {
        Platform::IntRect overScrollRect = overScrollRects.at(i);
        overScrollRect.move(-origin.x(), -origin.y());
        overScrollRect = transformation.mapRect(overScrollRect);

        if (!transformation.isIdentity()) {
            // Because of rounding it is possible that overScrollRect could be off-by-one larger
            // than the surface size of the window. We prevent this here, by clamping
            // it to ensure that can't happen.
            overScrollRect.intersect(Platform::IntRect(Platform::IntPoint(0, 0), m_webPage->client()->window()->surfaceSize()));
        }

        clearWindow(overScrollRect, color.red(), color.green(), color.blue(), color.alpha());
    }
}

void BackingStorePrivate::blitContents(const Platform::IntRect& dstRect,
                                       const Platform::IntRect& srcRect,
                                       bool force)
{
    // Blitting must never happen for direct rendering case.
    // Use invalidateWindow() instead.
    ASSERT(!shouldDirectRenderingToWindow());

    if (!m_webPage->isVisible() || m_suspendScreenUpdates || !isActive()) {
        // Avoid client going into busy loop while blit is impossible.
        if (force)
            m_hasBlitJobs = false;
        return;
    }

    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage(
                &BackingStorePrivate::blitContents, this, dstRect, srcRect, force));
        return;
    }

    if (m_defersBlit && !force) {
        m_hasBlitJobs = true;
        return;
    }

    m_hasBlitJobs = false;

    BackingStoreGeometry* currentState = frontState();
    const Platform::IntRect contentsRect = Platform::IntRect(Platform::IntPoint(0, 0), m_client->transformedContentsSize());

#if DEBUG_VISUALIZE
    // Substitute a debugRect that consists of the union of the backingstore rect
    // and the ui thread viewport rect instead of the normal source rect so we
    // can visualize the entire backingstore and what it is doing when we
    // scroll and zoom!
    // FIXME: This should not explicitely depend on WebCore::.
    WebCore::IntRect debugRect = currentState->backingStoreRect();
    debugRect.unite(m_webPage->client()->userInterfaceBlittedVisibleContentsRect());
    if (debugRect.width() < debugRect.height())
        debugRect.setWidth(ceil(double(srcRect.width()) * (double(debugRect.height()) / srcRect.height())));
    if (debugRect.height() < debugRect.width())
        debugRect.setHeight(ceil(double(srcRect.height()) * (double(debugRect.width()) / srcRect.width())));
    Platform::IntRect contents = debugRect;
#else
    Platform::IntRect contents = srcRect;
#endif

    // FIXME: This should not explicitely depend on WebCore::.
    TransformationMatrix transformation = TransformationMatrix::rectToRect(
        FloatRect(FloatPoint(0.0, 0.0), WebCore::IntSize(contents.size())), WebCore::IntRect(dstRect));

    bool blittingDirectlyToCompositingWindow = m_webPage->d->m_client->window()->windowUsage() == BlackBerry::Platform::Graphics::Window::GLES2Usage;
#if USE(ACCELERATED_COMPOSITING)
    BackingStoreCompositingSurface* compositingSurface =
        SurfacePool::globalSurfacePool()->compositingSurface();

    if (!blittingDirectlyToCompositingWindow)
        drawSubLayers();
#endif

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical,
                           "BackingStorePrivate::blitContents dstRect=(%d,%d %dx%d) srcRect=(%d,%d %dx%d)",
                           dstRect.x(), dstRect.y(), dstRect.width(), dstRect.height(),
                           srcRect.x(), srcRect.y(), srcRect.width(), srcRect.height());
#endif

    Platform::IntPoint origin = contents.location();
    Platform::IntRect contentsClipped = contents;

    paintDefaultBackground(contents, transformation, false /*flush*/);

    TileMap currentMap = currentState->tileMap();

    // Don't clip to contents if it is empty so we can still paint default background.
    if (!contentsRect.isEmpty()) {
        contentsClipped.intersect(contentsRect);
        if (contentsClipped.isEmpty()) {
            invalidateWindow(dstRect);
            return;
        }

        Platform::IntRectRegion contentsRegion = contentsClipped;
        Platform::IntRectRegion backingStoreRegion = currentState->backingStoreRect();
        Platform::IntRectRegion checkeredRegion
            = Platform::IntRectRegion::subtractRegions(contentsRegion, backingStoreRegion);

        // Blit checkered to those parts that are not covered by the backingStoreRect.
        IntRectList checkeredRects = checkeredRegion.rects();
        for (size_t i = 0; i < checkeredRects.size(); ++i) {
            Platform::IntRect dstRect = transformation.mapRect(Platform::IntRect(
                Platform::IntPoint(checkeredRects.at(i).x() - origin.x(), checkeredRects.at(i).y() - origin.y()),
                                   checkeredRects.at(i).size()));
            checkerWindow(dstRect, checkeredRects.at(i).location(), transformation.a());
        }
    }

    Vector<TileBuffer*> blittedTiles;

    // Get the list of tile rects that makeup the content.
    TileRectList tileRectList = mapFromTransformedContentsToTiles(contentsClipped, currentState);
    for (size_t i = 0; i < tileRectList.size(); ++i) {
        TileRect tileRect = tileRectList[i];
        TileIndex index = tileRect.first;
        Platform::IntRect dirtyTileRect = tileRect.second;
        BackingStoreTile* tile = currentMap.get(index);
        TileBuffer* tileBuffer = tile->frontBuffer();

        // This dirty rect is in tile coordinates, but it needs to be in
        // transformed contents coordinates.
        Platform::IntRect dirtyRect
            = mapFromTilesToTransformedContents(tileRect, currentState->backingStoreRect());

        // Don't clip to contents if it is empty so we can still paint default background.
        if (!contentsRect.isEmpty()) {
            // Otherwise we should clip the contents size and blit.
            dirtyRect.intersect(contentsRect);

            // We probably have extra tiles since the contents size is so small.
            // Save some cycles here...
            if (dirtyRect.isEmpty())
                continue;
        }

        // Now, this dirty rect is in transformed coordinates relative to the
        // transformed contents, but ultimately it needs to be transformed
        // coordinates relative to the viewport.
        dirtyRect.move(-origin.x(), -origin.y());

        // Save some cycles here...
        if (dirtyRect.isEmpty() || dirtyTileRect.isEmpty())
            continue;

        TileRect wholeTileRect;
        wholeTileRect.first = index;
        wholeTileRect.second = this->tileRect();
        Platform::IntRect wholeRect
            = mapFromTilesToTransformedContents(wholeTileRect, currentState->backingStoreRect());

        bool committed = tile->isCommitted();
        bool rendered = tileBuffer->isRendered(dirtyTileRect);
        bool paintCheckered = !committed || !rendered;

        if (paintCheckered) {
            Platform::IntRect dirtyRectT = transformation.mapRect(dirtyRect);

            if (!transformation.isIdentity()) {
                // Because of rounding it is possible that dirtyRect could be off-by-one larger
                // than the surface size of the dst buffer. We prevent this here, by clamping
                // it to ensure that can't happen.
                dirtyRectT.intersect(Platform::IntRect(Platform::IntPoint(0, 0), m_webPage->client()->window()->surfaceSize()));
            }
            const Platform::IntPoint contentsOrigin(dirtyRect.x() + origin.x(), dirtyRect.y() + origin.y());
            checkerWindow(dirtyRectT, contentsOrigin, transformation.a());
        }

        // Blit the visible buffer here if we have visible zoom jobs.
        if (m_renderQueue->hasCurrentVisibleZoomJob()) {

            // Needs to be in same coordinate system as dirtyRect.
            Platform::IntRect visibleTileBufferRect = m_visibleTileBufferRect;
            visibleTileBufferRect.move(-origin.x(), -origin.y());

            // Clip to the visibleTileBufferRect.
            dirtyRect.intersect(visibleTileBufferRect);

            // Clip to the dirtyRect.
            visibleTileBufferRect.intersect(dirtyRect);

            if (!dirtyRect.isEmpty() && !visibleTileBufferRect.isEmpty()) {
                BackingStoreTile* visibleTileBuffer
                    = SurfacePool::globalSurfacePool()->visibleTileBuffer();
                ASSERT(visibleTileBuffer->size() == visibleContentsRect().size());

                // The offset of the current viewport with the visble tile buffer.
                Platform::IntPoint difference = origin - m_visibleTileBufferRect.location();
                Platform::IntSize offset = Platform::IntSize(difference.x(), difference.y());

                // Map to the visibleTileBuffer coordinates.
                Platform::IntRect dirtyTileRect = visibleTileBufferRect;
                dirtyTileRect.move(offset.width(), offset.height());

                Platform::IntRect dirtyRectT = transformation.mapRect(dirtyRect);

                if (!transformation.isIdentity()) {
                    // Because of rounding it is possible that dirtyRect could be off-by-one larger
                    // than the surface size of the dst buffer. We prevent this here, by clamping
                    // it to ensure that can't happen.
                    dirtyRectT.intersect(Platform::IntRect(Platform::IntPoint(0, 0), m_webPage->client()->window()->surfaceSize()));
                }

                blitToWindow(dirtyRectT,
                             visibleTileBuffer->frontBuffer()->nativeBuffer(),
                             dirtyTileRect,
                             false /*blend*/, 255);
            }
        } else if (committed) {
            // Intersect the rendered region.
            Platform::IntRectRegion renderedRegion = tileBuffer->renderedRegion();
            IntRectList dirtyRenderedRects = renderedRegion.rects();
            for (size_t i = 0; i < dirtyRenderedRects.size(); ++i) {
                TileRect tileRect;
                tileRect.first = index;
                tileRect.second = intersection(dirtyTileRect, dirtyRenderedRects.at(i));
                if (tileRect.second.isEmpty())
                    continue;
                // Blit the rendered parts.
                blitTileRect(tileBuffer, tileRect, origin, transformation, currentState);
            }
            blittedTiles.append(tileBuffer);
        }
    }

#if USE(ACCELERATED_COMPOSITING)
    if (blittingDirectlyToCompositingWindow) {
        WebCore::FloatRect contentsRect = m_webPage->d->mapFromTransformedFloatRect(
            WebCore::FloatRect(WebCore::IntRect(contents)));
        m_webPage->d->drawSubLayers(dstRect, contentsRect);
    } else if (compositingSurface)
        blendCompositingSurface(dstRect);

#endif

#if ENABLE_SCROLLBARS
    if (isScrollingOrZooming() && m_client->isMainFrame()) {
        if (m_client->scrollsHorizontally())
            blitHorizontalScrollbar(origin);
        if (m_client->scrollsVertically())
            blitVerticalScrollbar(origin);
    }
#endif

#if DEBUG_VISUALIZE
    // FIXME: This should not explicitely depend on WebCore::.
    BlackBerry::Platform::Graphics::Buffer* windowBuffer =
        m_webPage->client()->window()->buffer();
    BlackBerry::Platform::Graphics::Drawable* bufferDrawable =
        BlackBerry::Platform::Graphics::lockBufferDrawable(windowBuffer);
    PlatformGraphicsContext* bufferPlatformGraphicsContext =
        SurfacePool::globalSurfacePool()->createPlatformGraphicsContext(bufferDrawable);
    GraphicsContext graphicsContext(bufferPlatformGraphicsContext);
    FloatRect wkViewport = FloatRect(visibleContentsRect());
    FloatRect uiViewport = FloatRect(m_webPage->client()->userInterfaceBlittedVisibleContentsRect());
    wkViewport.move(-contents.x(), -contents.y());
    uiViewport.move(-contents.x(), -contents.y());

    graphicsContext.save();

    // Draw a blue rect for the webkit thread viewport.
    graphicsContext.setStrokeColor(WebCore::Color(0, 0, 255), WebCore::ColorSpaceDeviceRGB);
    graphicsContext.strokeRect(transformation.mapRect(wkViewport), 1.0);

    // Draw a red rect for the ui thread viewport.
    graphicsContext.setStrokeColor(WebCore::Color(255, 0, 0), WebCore::ColorSpaceDeviceRGB);
    graphicsContext.strokeRect(transformation.mapRect(uiViewport), 1.0);

    graphicsContext.restore();

    delete bufferPlatformGraphicsContext;
    releaseBufferDrawable(windowBuffer);
#endif

    invalidateWindow(dstRect);

    if (blittingDirectlyToCompositingWindow) {
        pthread_mutex_lock(&m_blitGenerationLock);

        ++m_blitGeneration;
        for (unsigned int i = 0; i < blittedTiles.size(); ++i)
            blittedTiles[i]->setBlitGeneration(m_blitGeneration);

        clock_gettime(CLOCK_REALTIME, &m_currentBlitEnd);
        m_currentBlitEnd.tv_nsec += 30 * 1000 * 1000;
        if (m_currentBlitEnd.tv_nsec >= 1000000000L) {
            m_currentBlitEnd.tv_sec  += 1;
            m_currentBlitEnd.tv_nsec -= 1000000000L;
        }

        pthread_mutex_unlock(&m_blitGenerationLock);
        pthread_cond_signal(&m_blitGenerationCond);
    }
}

Platform::IntRect BackingStorePrivate::blitTileRect(TileBuffer* tileBuffer,
                                                   const TileRect& tileRect,
                                                   const Platform::IntPoint& origin,
                                                   const WebCore::TransformationMatrix& matrix,
                                                   BackingStoreGeometry* state)
{
    if (!m_webPage->isVisible() || !isActive())
        return Platform::IntRect();

    Platform::IntRect dirtyTileRect = tileRect.second;

    // This dirty rect is in tile coordinates, but it needs to be in
    // transformed contents coordinates.
    Platform::IntRect dirtyRect = mapFromTilesToTransformedContents(tileRect, state->backingStoreRect());

    // Now, this dirty rect is in transformed coordinates relative to the
    // transformed contents, but ultimately it needs to be transformed
    // coordinates relative to the viewport.
    dirtyRect.move(-origin.x(), -origin.y());
    dirtyRect = matrix.mapRect(dirtyRect);

    if (!matrix.isIdentity()) {
        // Because of rounding it is possible that dirtyRect could be off-by-one larger
        // than the surface size of the dst buffer. We prevent this here, by clamping
        // it to ensure that can't happen.
        dirtyRect.intersect(Platform::IntRect(Platform::IntPoint(0, 0), m_webPage->client()->window()->surfaceSize()));
    }

    ASSERT(!dirtyRect.isEmpty());
    ASSERT(!dirtyTileRect.isEmpty());
    if (dirtyRect.isEmpty() || dirtyTileRect.isEmpty())
        return Platform::IntRect();

    blitToWindow(dirtyRect, tileBuffer->nativeBuffer(), dirtyTileRect,
                 false /*blend*/, 255);
    return dirtyRect;
}

#if USE(ACCELERATED_COMPOSITING)
void BackingStorePrivate::blendCompositingSurface(const Platform::IntRect& dstRect)
{
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        typedef void (BlackBerry::WebKit::BackingStorePrivate::*FunctionType)(const Platform::IntRect&);
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage<FunctionType, BackingStorePrivate, Platform::IntRect>(
                &BackingStorePrivate::blendCompositingSurface, this, dstRect));
        return;
    }

    BackingStoreCompositingSurface* compositingSurface =
        SurfacePool::globalSurfacePool()->compositingSurface();

    if (!compositingSurface || !m_webPage->isVisible())
        return;

    WebCore::LayerRenderingResults lastCompositingResults = m_webPage->d->lastCompositingResults();
    for (size_t i = 0; i < lastCompositingResults.holePunchRectSize(); i++) {
        Platform::IntRect holePunchRect = lastCompositingResults.holePunchRect(i);

        holePunchRect.intersect(dstRect);
        holePunchRect.intersect(Platform::IntRect(
            Platform::IntPoint(0, 0), m_webPage->client()->window()->surfaceSize()));

        if (!holePunchRect.isEmpty())
            clearWindow(holePunchRect, 0, 0, 0, 0);
    }

    CompositingSurfaceBuffer* frontBuffer = compositingSurface->frontBuffer();

    IntRectList rects = lastCompositingResults.dirtyRegion.rects();
    for (size_t i = 0; i < rects.size(); ++i) {
        rects[i].intersect(dstRect);
#if DEBUG_COMPOSITING_DIRTY_REGION
        clearBuffer(m_webPage->client()->window()->buffer(), rects[i], 255, 0, 0, 128);
#endif
        blitToWindow(rects[i], frontBuffer->nativeBuffer(), rects[i], true /*blend*/, 255);
    }
}

void BackingStorePrivate::clearCompositingSurface()
{
    BackingStoreCompositingSurface* compositingSurface =
        SurfacePool::globalSurfacePool()->compositingSurface();

    if (!compositingSurface)
        return;

    CompositingSurfaceBuffer* frontBuffer = compositingSurface->frontBuffer();
    BlackBerry::Platform::Graphics::clearBuffer(frontBuffer->nativeBuffer(), Platform::IntRect(Platform::IntPoint(), frontBuffer->surfaceSize()), 0, 0, 0, 0);
}
#endif // USE(ACCELERATED_COMPOSITING)

void BackingStorePrivate::blitHorizontalScrollbar(const Platform::IntPoint& scrollPosition)
{
    if (!m_webPage->isVisible())
        return;

    ASSERT(m_client->scrollsHorizontally());

    m_webPage->client()->drawHorizontalScrollbar();
}

void BackingStorePrivate::blitVerticalScrollbar(const Platform::IntPoint& scrollPosition)
{
    if (!m_webPage->isVisible())
        return;

    ASSERT(m_client->scrollsVertically());

    m_webPage->client()->drawVerticalScrollbar();
}

bool BackingStorePrivate::isTileVisible(const TileIndex& index) const
{
    TileRect tileRect;
    tileRect.first = index;
    tileRect.second = this->tileRect();
    return mapFromTilesToTransformedContents(tileRect).intersects(visibleContentsRect());
}

bool BackingStorePrivate::isTileVisible(const Platform::IntPoint& origin) const
{
    return Platform::IntRect(origin, tileSize()).intersects(visibleContentsRect());
}

Platform::IntRect BackingStorePrivate::visibleTilesRect() const
{
    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    Platform::IntRect rect;
    TileMap::const_iterator end = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != end; ++it) {
        TileRect tileRect;
        tileRect.first = it->first;
        tileRect.second = this->tileRect();
        Platform::IntRect tile = mapFromTilesToTransformedContents(tileRect);
        if (tile.intersects(visibleContentsRect()))
            rect = Platform::unionOfRects(rect, tile);
    }
    return rect;
}

Platform::IntRect BackingStorePrivate::tileVisibleContentsRect(const TileIndex& index) const
{
    if (!isTileVisible(index))
        return Platform::IntRect();

    return tileContentsRect(index, visibleContentsRect());
}

Platform::IntRect BackingStorePrivate::tileUnclippedVisibleContentsRect(const TileIndex& index) const
{
    if (!isTileVisible(index))
        return Platform::IntRect();

    return tileContentsRect(index, unclippedVisibleContentsRect());
}

Platform::IntRect BackingStorePrivate::tileContentsRect(const TileIndex& index,
                                                       const Platform::IntRect& contents) const
{
    return tileContentsRect(index, contents, frontState());
}

Platform::IntRect BackingStorePrivate::tileContentsRect(const TileIndex& index,
                                                       const Platform::IntRect& contents,
                                                       BackingStoreGeometry* state) const
{
    TileRectList tileRectList = mapFromTransformedContentsToTiles(contents, state);
    for (size_t i = 0; i < tileRectList.size(); ++i) {
        TileRect tileRect = tileRectList[i];
        if (index == tileRect.first)
            return tileRect.second;
    }
    return Platform::IntRect();
}

void BackingStorePrivate::resetRenderQueue()
{
    m_renderQueue->reset();
}

void BackingStorePrivate::clearVisibleZoom()
{
    m_renderQueue->clearVisibleZoom();
}

void BackingStorePrivate::resetTiles(bool resetBackground)
{
    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    TileMap::const_iterator end = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != end; ++it)
        resetTile(it->first, it->second, resetBackground);
}

void BackingStorePrivate::updateTiles(bool updateVisible, bool immediate)
{
    if (!isActive())
        return;

    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    TileMap::const_iterator end = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != end; ++it) {
        bool isVisible = isTileVisible(it->first);
        if (!updateVisible && isVisible)
            continue;
        updateTile(it->first, immediate);
    }
}

void BackingStorePrivate::updateTilesForScrollOrNotRenderedRegion(bool checkLoading)
{
    // This method looks at all the tiles and if they are visible, but not completely
    // rendered or we are loading, then it updates them. For all tiles, visible and
    // non-visible, if a previous attempt was made to render them during a regular
    // render job, but they were not visible at the time, then update them and if
    // they are currently visible, reset them.

    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();
    Platform::IntRect backingStoreRect = currentState->backingStoreRect();

    bool isLoading = m_client->loadState() == WebPagePrivate::Committed;
    bool forceVisible = checkLoading && isLoading;

    TileMap::const_iterator end = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != end; ++it) {
        TileIndex index = it->first;
        BackingStoreTile* tile = it->second;
        bool isVisible = isTileVisible(index);
        // The rect in transformed contents coordinates.
        Platform::IntRect rect(originOfTile(index), tileSize());
        if (tile->isCommitted()
            && m_renderQueue->regularRenderJobsPreviouslyAttemptedButNotRendered(rect)) {
            // If the render queue previously tried to render this tile, but the
            // tile wasn't visible at the time we can't simply restore the tile
            // since the content is now invalid as far as WebKit is concerned.
            // Instead, we clear that part of the tile if it is visible and then
            // put the tile in the render queue again.
            if (isVisible) {
                // Intersect the tile with the not rendered region to get the areas
                // of the tile that we need to clear.
                Platform::IntRectRegion tileNotRenderedRegion
                    = Platform::IntRectRegion::intersectRegions(
                        m_renderQueue->regularRenderJobsNotRenderedRegion(),
                        rect);
                clearAndUpdateTileOfNotRenderedRegion(index,
                                                      tile,
                                                      tileNotRenderedRegion,
                                                      backingStoreRect,
                                                      false /*update*/);
#if DEBUG_BACKINGSTORE
                Platform::IntRect extents = tileNotRenderedRegion.extents();
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical,
                    "BackingStorePrivate::updateTilesForScroll did clear tile %d,%d %dx%d",
                    extents.x(), extents.y(), extents.width(), extents.height());
#endif
            }
            updateTile(index, false /*immediate*/);
        } else if (isVisible
            && (forceVisible || !tile->frontBuffer()->isRendered(tileVisibleContentsRect(index)))
            && !isCurrentVisibleJob(index, tile, backingStoreRect))
            updateTile(index, false /*immediate*/);
    }
}

void BackingStorePrivate::resetTile(const TileIndex& index, BackingStoreTile* tile, bool resetBackground)
{
    if (!m_webPage->isVisible())
        return;

    if (!isActive()) {
        m_webPage->d->setShouldResetTilesWhenShown(true);
        return;
    }

    TileRect tileRect;
    tileRect.first = index;
    tileRect.second = this->tileRect();
    // Only clear regular render jobs if we're clearing the background too.
    m_renderQueue->clear(mapFromTilesToTransformedContents(tileRect), resetBackground /*clearRegularRenderJobs*/);
    if (resetBackground)
        tile->reset();
}

void BackingStorePrivate::updateTile(const TileIndex& index, bool immediate)
{
    if (!isActive())
        return;

    TileRect tileRect;
    tileRect.first = index;
    tileRect.second = this->tileRect();
    Platform::IntRect updateRect = mapFromTilesToTransformedContents(tileRect);
    RenderQueue::JobType jobType = isTileVisible(index) ? RenderQueue::VisibleScroll : RenderQueue::NonVisibleScroll;
    if (immediate)
        render(updateRect, true /*renderContentOnly*/);
    else
        m_renderQueue->addToQueue(jobType, updateRect);
}

void BackingStorePrivate::updateTile(const Platform::IntPoint& origin, bool immediate)
{
    if (!isActive())
        return;

    Platform::IntRect updateRect = Platform::IntRect(origin, tileSize());
    RenderQueue::JobType jobType = isTileVisible(origin) ? RenderQueue::VisibleScroll : RenderQueue::NonVisibleScroll;
    if (immediate)
        render(updateRect, true /*renderContentOnly*/);
    else
        m_renderQueue->addToQueue(jobType, updateRect);
}

Platform::IntRect BackingStorePrivate::mapFromTilesToTransformedContents(const BackingStorePrivate::TileRect& tileRect) const
{
    return mapFromTilesToTransformedContents(tileRect, frontState()->backingStoreRect());
}

Platform::IntRect BackingStorePrivate::mapFromTilesToTransformedContents(const BackingStorePrivate::TileRect& tileRect, const Platform::IntRect& backingStoreRect) const
{
    TileIndex index = tileRect.first;
    Platform::IntRect rect = tileRect.second;
    // The origin of the tile including the backing store offset.
    const Platform::IntPoint originOfTile = this->originOfTile(index, backingStoreRect);
    rect.move(originOfTile.x(), originOfTile.y());
    return rect;
}

BackingStorePrivate::TileRectList BackingStorePrivate::mapFromTransformedContentsToAbsoluteTileBoundaries(const Platform::IntRect& rect) const
{
    if (!m_webPage->isVisible() || !isActive()) {
        ASSERT_NOT_REACHED();
        return TileRectList();
    }

    TileRectList tileRectList;
    int firstXOffset = rect.x() / tileWidth();
    int firstYOffset = rect.y() / tileHeight();
    int lastXOffset = (rect.right() - 1) / tileWidth();
    int lastYOffset = (rect.bottom() - 1) / tileHeight();
    for (int i = firstXOffset; i <= lastXOffset; ++i) {
        for (int j = firstYOffset; j <= lastYOffset; ++j) {
            const int dstX = (i == firstXOffset) ? rect.x() : i * tileWidth();
            const int dstY = (j == firstYOffset) ? rect.y() : j * tileHeight();
            const int dstRight = (i == lastXOffset) ? rect.right() : (i + 1) * tileWidth();
            const int dstBottom = (j == lastYOffset) ? rect.bottom() : (j + 1) * tileHeight();
            const int srcX = dstX % tileWidth();
            const int srcY = dstY % tileHeight();
            TileRect tileRect;
            tileRect.first = TileIndex(i, j);
            tileRect.second = Platform::IntRect(srcX, srcY, dstRight - dstX, dstBottom - dstY);
            tileRectList.append(tileRect);
        }
    }
    return tileRectList;
}


BackingStorePrivate::TileRectList BackingStorePrivate::mapFromTransformedContentsToTiles(const Platform::IntRect& rect) const
{
    return mapFromTransformedContentsToTiles(rect, frontState());
}

BackingStorePrivate::TileRectList BackingStorePrivate::mapFromTransformedContentsToTiles(const Platform::IntRect& rect, BackingStoreGeometry* state) const
{
    TileMap tileMap = state->tileMap();

    TileRectList tileRectList;
    TileMap::const_iterator end = tileMap.end();
    for (TileMap::const_iterator it = tileMap.begin(); it != end; ++it) {
        TileIndex index = it->first;
        BackingStoreTile* tile = it->second;

        // Need to map the rect to tile coordinates.
        Platform::IntRect r = rect;

        // The origin of the tile including the backing store offset.
        const Platform::IntPoint originOfTile = this->originOfTile(index, state->backingStoreRect());

        r.move(-(originOfTile.x()), -(originOfTile.y()));

        // Do we intersect the current tile or no?
        r.intersect(tile->rect());
        if (r.isEmpty())
            continue;

        // If we do append to list and Voila!
        TileRect tileRect;
        tileRect.first = index;
        tileRect.second = r;
        tileRectList.append(tileRect);
    }
    return tileRectList;
}

void BackingStorePrivate::updateTileMatrixIfNeeded()
{
    // This will update the tile matrix.
    scrollBackingStore(0, 0);
}

void BackingStorePrivate::contentsSizeChanged(const Platform::IntSize&)
{
    updateTileMatrixIfNeeded();
}

void BackingStorePrivate::scrollChanged(const Platform::IntPoint&)
{
    // FIXME: Need to do anything here?
}

void BackingStorePrivate::transformChanged()
{
    if (!m_webPage->isVisible())
        return;

    if (!isActive()) {
        m_renderQueue->reset();
        m_renderQueue->addToQueue(RenderQueue::VisibleZoom, visibleContentsRect());
        m_webPage->d->setShouldResetTilesWhenShown(true);
        return;
    }

    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    bool hasCurrentVisibleZoomJob = m_renderQueue->hasCurrentVisibleZoomJob();
    bool isLoading = m_client->isLoading();
    if (isLoading) {
        if (!hasCurrentVisibleZoomJob)
            m_visibleTileBufferRect = visibleContentsRect(); // Cache this for blitVisibleContents.

        // Add the currently visible tiles to the render queue as visible zoom jobs.
        TileRectList tileRectList = mapFromTransformedContentsToTiles(visibleContentsRect());
        for (size_t i = 0; i < tileRectList.size(); ++i) {
            TileRect tileRect = tileRectList[i];
            TileIndex index = tileRect.first;
            Platform::IntRect dirtyTileRect = tileRect.second;
            BackingStoreTile* tile = currentMap.get(index);

            // Invalidate the whole rect.
            tileRect.second = this->tileRect();
            Platform::IntRect wholeRect = mapFromTilesToTransformedContents(tileRect);
            m_renderQueue->addToQueue(RenderQueue::VisibleZoom, wholeRect);

            // Copy the visible contents into the visibleTileBuffer if we don't have
            // any current visible zoom jobs.
            if (!hasCurrentVisibleZoomJob) {
                // Map to the destination's coordinate system.
                Platform::IntPoint difference = this->originOfTile(index) - m_visibleTileBufferRect.location();
                Platform::IntSize offset = Platform::IntSize(difference.x(), difference.y());
                Platform::IntRect dirtyRect = dirtyTileRect;
                dirtyRect.move(offset.width(), offset.height());

                BackingStoreTile* visibleTileBuffer
                    = SurfacePool::globalSurfacePool()->visibleTileBuffer();
                ASSERT(visibleTileBuffer->size() == Platform::IntSize(m_webPage->d->transformedViewportSize()));
                BlackBerry::Platform::Graphics::blitToBuffer(
                    visibleTileBuffer->frontBuffer()->nativeBuffer(), dirtyRect,
                    tile->frontBuffer()->nativeBuffer(), dirtyTileRect);
            }
        }
    }

    m_renderQueue->reset();
    resetTiles(true /*resetBackground*/);
}

void BackingStorePrivate::orientationChanged()
{
    updateTileMatrixIfNeeded();
    createVisibleTileBuffer();
}

void BackingStorePrivate::actualVisibleSizeChanged(const Platform::IntSize& size)
{
}

static void createVisibleTileBufferForWebPage(WebPagePrivate* page)
{
    ASSERT(page);
    SurfacePool* surfacePool = SurfacePool::globalSurfacePool();
    surfacePool->initializeVisibleTileBuffer(page->transformedViewportSize());
}

void BackingStorePrivate::createSurfaces()
{
    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    ASSERT(currentMap.isEmpty());

    if (m_webPage->isVisible()) {
        // This method is only to be called as part of setting up a new web page instance and
        // before said instance is made visible so as to ensure a consistent definition of web
        // page visibility. That is, a web page is said to be visible when explicitly made visible.
        ASSERT_NOT_REACHED();
        return;
    }

    SurfacePool* surfacePool = SurfacePool::globalSurfacePool();
    surfacePool->initialize(tileSize());

    if (surfacePool->isEmpty()) // Settings specify 0 tiles / no backing store.
        return;

    const Divisor divisor = bestDivisor(expandedContentsSize(), tileWidth(), tileHeight(), minimumNumberOfTilesWide(), minimumNumberOfTilesHigh(), m_preferredTileMatrixDimension);

    int numberOfTilesWide = divisor.first;
    int numberOfTilesHigh = divisor.second;

    const SurfacePool::TileList tileList = surfacePool->tileList();
    ASSERT(static_cast<int>(tileList.size()) >= (numberOfTilesWide * numberOfTilesHigh));

    TileMap newTileMap;
    for (int y = 0; y < numberOfTilesHigh; ++y) {
        for (int x = 0; x < numberOfTilesWide; ++x) {
            TileIndex index(x, y);
            newTileMap.add(index, tileList.at(x + y * numberOfTilesWide));
        }
    }

    // Set the initial state of the backingstore geometry.
    backState()->setNumberOfTilesWide(divisor.first);
    backState()->setNumberOfTilesHigh(divisor.second);
    backState()->setTileMap(newTileMap);

    // Swap back/front state.
    swapState();

    createVisibleTileBufferForWebPage(m_webPage->d);
}

void BackingStorePrivate::createVisibleTileBuffer()
{
    if (!m_webPage->isVisible() || !isActive())
        return;

    createVisibleTileBufferForWebPage(m_webPage->d);
}

Platform::IntPoint BackingStorePrivate::originOfTile(const TileIndex& index) const
{
    return originOfTile(index, frontState()->backingStoreRect());
}

Platform::IntPoint BackingStorePrivate::originOfTile(const TileIndex& index, const Platform::IntRect& backingStoreRect) const
{
    return Platform::IntPoint(backingStoreRect.x() + (index.i() * tileWidth()),
                              backingStoreRect.y() + (index.j() * tileHeight()));
}

int BackingStorePrivate::minimumNumberOfTilesWide() const
{
    // The minimum number of tiles wide required to fill the viewport + 1 tile extra to allow scrolling.
    return static_cast<int>(ceilf(m_client->transformedViewportSize().width() / static_cast<float>(tileWidth()))) + 1;
}

int BackingStorePrivate::minimumNumberOfTilesHigh() const
{
    // The minimum number of tiles high required to fill the viewport + 1 tile extra to allow scrolling.
    return static_cast<int>(ceilf(m_client->transformedViewportSize().height() / static_cast<float>(tileHeight()))) + 1;
}

Platform::IntSize BackingStorePrivate::expandedContentsSize() const
{
    return m_client->transformedContentsSize().expandedTo(m_client->transformedViewportSize());
}

int BackingStorePrivate::tileWidth()
{
    static int tileWidth = BlackBerry::Platform::Graphics::Screen::landscapeWidth();
    return tileWidth;
}

int BackingStorePrivate::tileHeight()
{
    static int tileHeight = BlackBerry::Platform::Graphics::Screen::landscapeHeight();
    return tileHeight;
}

Platform::IntSize BackingStorePrivate::tileSize()
{
    return Platform::IntSize(tileWidth(), tileHeight());
}

Platform::IntRect BackingStorePrivate::tileRect()
{
    return Platform::IntRect(0, 0, tileWidth(), tileHeight());
}

void BackingStorePrivate::renderContents(BlackBerry::Platform::Graphics::Buffer* tileBuffer,
                                         const Platform::IntPoint& surfaceOffset,
                                         const Platform::IntRect& contentsRect) const
{
    // If tileBuffer == 0, we render directly to the window.
    if (!m_webPage->isVisible() && tileBuffer)
        return;

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical,
                           "BackingStorePrivate::renderContents tileBuffer=0x%x surfaceOffset=(%d,%d) contentsRect=(%d,%d %dx%d)",
                           tileBuffer, surfaceOffset.x(), surfaceOffset.y(),
                           contentsRect.x(), contentsRect.y(), contentsRect.width(), contentsRect.height());
#endif

    // It is up to callers of this method to perform layout themselves!
    ASSERT(!m_webPage->d->mainFrame()->view()->needsLayout());

    Platform::IntSize contentsSize = m_client->contentsSize();
    Color backgroundColor(m_webPage->settings()->backgroundColor());

    BlackBerry::Platform::Graphics::Buffer* targetBuffer = tileBuffer
        ? tileBuffer
        : m_webPage->client()->window()->buffer();

    if (contentsSize.isEmpty()
        || !Platform::IntRect(Platform::IntPoint(0, 0), m_client->transformedContentsSize()).contains(contentsRect)
        || backgroundColor.hasAlpha()) {
        // Clear the area if it's not fully covered by (opaque) contents.
        BlackBerry::Platform::IntRect clearRect = BlackBerry::Platform::IntRect(
            contentsRect.x() - surfaceOffset.x(), contentsRect.y() - surfaceOffset.y(),
            contentsRect.width(), contentsRect.height());

        BlackBerry::Platform::Graphics::clearBuffer(targetBuffer, clearRect,
            backgroundColor.red(), backgroundColor.green(),
            backgroundColor.blue(), backgroundColor.alpha());
    }

    if (contentsSize.isEmpty())
        return;

#if USE(ACCELERATED_COMPOSITING)
    m_webPage->d->commitRootLayerIfNeeded();
#endif

    BlackBerry::Platform::Graphics::Drawable* bufferDrawable =
        BlackBerry::Platform::Graphics::lockBufferDrawable(targetBuffer);

    PlatformGraphicsContext* bufferPlatformGraphicsContext = bufferDrawable
        ? SurfacePool::globalSurfacePool()->createPlatformGraphicsContext(bufferDrawable)
        : 0;
    PlatformGraphicsContext* targetPlatformGraphicsContext = bufferPlatformGraphicsContext
        ? bufferPlatformGraphicsContext
        : SurfacePool::globalSurfacePool()->lockTileRenderingSurface();

    ASSERT(targetPlatformGraphicsContext);

    {
        GraphicsContext graphicsContext(targetPlatformGraphicsContext);

        // Believe it or not this is important since the WebKit Skia backend
        // doesn't store the original state unless you call save first :P
        graphicsContext.save();

        // Translate context according to offset.
        graphicsContext.translate(-surfaceOffset.x(), -surfaceOffset.y());

        // Add our transformation matrix as the global transform.
        AffineTransform affineTransform(
            m_webPage->d->transformationMatrix()->a(),
            m_webPage->d->transformationMatrix()->b(),
            m_webPage->d->transformationMatrix()->c(),
            m_webPage->d->transformationMatrix()->d(),
            m_webPage->d->transformationMatrix()->e(),
            m_webPage->d->transformationMatrix()->f());
        graphicsContext.concatCTM(affineTransform);

        // Now that the matrix is applied we need untranformed contents coordinates.
        Platform::IntRect untransformedContentsRect = m_webPage->d->mapFromTransformed(contentsRect);

        // We extract from the contentsRect but draw a slightly larger region than
        // we were told to, in order to avoid pixels being rendered only partially.
        const int atLeastOneDevicePixel =
            static_cast<int>(ceilf(1.0 / m_webPage->d->transformationMatrix()->a()));
        untransformedContentsRect.inflate(atLeastOneDevicePixel, atLeastOneDevicePixel);

        // Make sure the untransformed rectangle for the (slightly larger than
        // initially requested) repainted region is within the bounds of the page.
        untransformedContentsRect.intersect(Platform::IntRect(Platform::IntPoint(0, 0), contentsSize));

        // Some WebKit painting backends *cough* Skia *cough* don't set this automatically
        // to the dirtyRect so do so here explicitly.
        graphicsContext.clip(untransformedContentsRect);

        // Take care of possible left overflow on RTL page.
        if (int leftOverFlow = m_client->frame()->view()->minimumScrollPosition().x()) {
            untransformedContentsRect.move(leftOverFlow, 0);
            graphicsContext.translate(-leftOverFlow, 0);
        }

        // Let WebCore render the page contents into the drawing surface.
        m_client->frame()->view()->paintContents(&graphicsContext, untransformedContentsRect);

#if ENABLE(INSPECTOR)
        if (m_webPage->d->m_page->inspectorController()->enabled()) {
            WebCore::IntPoint scrollPosition = m_client->frame()->view()->scrollPosition();
            graphicsContext.translate(scrollPosition.x(), scrollPosition.y());
            m_webPage->d->m_page->inspectorController()->drawNodeHighlight(graphicsContext);
        }
#endif

        graphicsContext.restore();
    }

    // Grab the requested region from the drawing surface into the tile image.
#if USE(OPENVG)
    surface->makeCurrent();
#endif

    delete bufferPlatformGraphicsContext;

    if (bufferDrawable)
        releaseBufferDrawable(targetBuffer);
    else {
        const Platform::IntPoint dstPoint(contentsRect.x() - surfaceOffset.x(),
                                          contentsRect.y() - surfaceOffset.y());
        const Platform::IntRect dstRect(dstPoint, contentsRect.size());
        const Platform::IntRect srcRect = dstRect;

        // If we couldn't directly draw to the buffer, copy from the drawing surface.
        SurfacePool::globalSurfacePool()->releaseTileRenderingSurface(targetPlatformGraphicsContext);
        BlackBerry::Platform::Graphics::blitToBuffer(targetBuffer, dstRect, BlackBerry::Platform::Graphics::drawingSurface(), srcRect);
    }
}

#if DEBUG_FAT_FINGERS
static void drawDebugRect(BlackBerry::Platform::Graphics::Buffer* dstBuffer, const Platform::IntRect& dstRect, const Platform::IntRect& srcRect, unsigned char red, unsigned char green, unsigned char blue)
{
    Platform::IntRect drawRect(srcRect);
    drawRect.intersect(dstRect);
    if (!drawRect.isEmpty())
        BlackBerry::Platform::Graphics::clearBuffer(dstBuffer, drawRect, red, green, blue, 128);
}
#endif

void BackingStorePrivate::blitToWindow(const Platform::IntRect& dstRect,
                                       const BlackBerry::Platform::Graphics::Buffer* srcBuffer,
                                       const Platform::IntRect& srcRect,
                                       bool blend,
                                       unsigned char globalAlpha)
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());

    windowFrontBufferState()->clearBlittedRegion(dstRect);
    windowBackBufferState()->addBlittedRegion(dstRect);

    BlackBerry::Platform::Graphics::Buffer* dstBuffer = m_webPage->client()->window()->buffer();
    ASSERT(dstBuffer);
    ASSERT(srcBuffer);
    if (!dstBuffer)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelWarn, "Empty window buffer, couldn't blitToWindow");

    BlackBerry::Platform::Graphics::BlendMode blendMode = blend
        ? BlackBerry::Platform::Graphics::SourceOver
        : BlackBerry::Platform::Graphics::SourceCopy;

    BlackBerry::Platform::Graphics::blitToBuffer(dstBuffer, dstRect, srcBuffer, srcRect, blendMode, globalAlpha);

#if DEBUG_FAT_FINGERS
    drawDebugRect(dstBuffer, dstRect, FatFingers::m_debugFatFingerRect, 210, 210, 250);
    drawDebugRect(dstBuffer, dstRect, Platform::IntRect(FatFingers::m_debugFatFingerClickPosition, Platform::IntSize(3, 3)), 0, 0, 0);
    drawDebugRect(dstBuffer, dstRect, Platform::IntRect(FatFingers::m_debugFatFingerAdjustedPosition, Platform::IntSize(5, 5)), 100, 100, 100);
#endif

}

void BackingStorePrivate::checkerWindow(const Platform::IntRect& dstRect,
                                        const Platform::IntPoint& contentsOrigin,
                                        double contentsScale)
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());

    windowFrontBufferState()->clearBlittedRegion(dstRect);
    windowBackBufferState()->addBlittedRegion(dstRect);

    BlackBerry::Platform::Graphics::Buffer* dstBuffer = m_webPage->client()->window()->buffer();
    ASSERT(dstBuffer);
    if (!dstBuffer)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelWarn, "Empty window buffer, couldn't checkerWindow");

    Color color(m_webPage->settings()->backgroundColor());
    unsigned char alpha = color.alpha();
    BlackBerry::Platform::Graphics::checkerBuffer(dstBuffer, dstRect, contentsOrigin, contentsScale, alpha);
}

void BackingStorePrivate::invalidateWindow()
{
    // Grab a rect appropriate for the current thread.
    if (BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        invalidateWindow(m_webPage->client()->userInterfaceBlittedDestinationRect());
    else
        invalidateWindow(Platform::IntRect(Platform::IntPoint(0, 0), m_client->transformedViewportSize()));
}

void BackingStorePrivate::invalidateWindow(const Platform::IntRect& dst)
{
    if (dst.isEmpty())
        return;

    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread() && !shouldDirectRenderingToWindow()) {
        // This needs to be sync in order to swap the recently drawn thing...
        // This will only be called from WebKit thread during direct rendering.
        typedef void (BlackBerry::WebKit::BackingStorePrivate::*FunctionType)(const Platform::IntRect&);
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
            BlackBerry::Platform::createMethodCallMessage<FunctionType, BackingStorePrivate, Platform::IntRect>(
                &BackingStorePrivate::invalidateWindow, this, dst));
        return;
    }

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::invalidateWindow dst = %s", dst.toString().utf8().data());
#endif

    // Since our window may also be double buffered, we need to also copy the
    // front buffer's contents to the back buffer before we swap them. It is
    // analogous to what we do with our double buffered tiles by calling
    // copyPreviousContentsToBackingSurfaceOfTile(). It only affects partial
    // screen updates since when we are scrolling or zooming, the whole window
    // is invalidated anyways and no copying is needed.
    copyPreviousContentsToBackSurfaceOfWindow();

    Platform::IntRect dstRect = dst;

    Platform::IntRect viewportRect(Platform::IntPoint(0, 0), m_client->transformedViewportSize());
    dstRect.intersect(viewportRect);

    if (dstRect.width() <= 0 || dstRect.height() <= 0)
        return;

#if DEBUG_BACKINGSTORE
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "BackingStorePrivate::invalidateWindow posting = %s", dstRect.toString().utf8().data());
#endif

    m_currentWindowBackBuffer = (m_currentWindowBackBuffer + 1) % 2;
    m_webPage->client()->window()->post(dstRect);
}

void BackingStorePrivate::clearWindow()
{
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread() && !shouldDirectRenderingToWindow()) {
        typedef void (BlackBerry::WebKit::BackingStorePrivate::*FunctionType)();
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage<FunctionType, BackingStorePrivate>(
                &BackingStorePrivate::clearWindow, this));
        return;
    }

    BlackBerry::Platform::Graphics::Buffer* dstBuffer = m_webPage->client()->window()->buffer();
    ASSERT(dstBuffer);
    if (!dstBuffer)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelWarn, "Empty window buffer, couldn't clearWindow");

    windowFrontBufferState()->clearBlittedRegion();
    windowBackBufferState()->addBlittedRegion(Platform::IntRect(
        Platform::IntPoint(0, 0), m_webPage->client()->window()->surfaceSize()));

    Color color(m_webPage->settings()->backgroundColor());
    BlackBerry::Platform::Graphics::clearBuffer(dstBuffer,
        color.red(), color.green(), color.blue(), color.alpha());
}

void BackingStorePrivate::clearWindow(const Platform::IntRect& rect,
                                      unsigned char red,
                                      unsigned char green,
                                      unsigned char blue,
                                      unsigned char alpha)
{
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread() && !shouldDirectRenderingToWindow()) {
        typedef void (BlackBerry::WebKit::BackingStorePrivate::*FunctionType)(const Platform::IntRect&,
                                                                           unsigned char,
                                                                           unsigned char,
                                                                           unsigned char,
                                                                           unsigned char);
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage<FunctionType,
                                                       BackingStorePrivate,
                                                       Platform::IntRect,
                                                       unsigned char,
                                                       unsigned char,
                                                       unsigned char,
                                                       unsigned char>(
                &BackingStorePrivate::clearWindow, this, rect, red, green, blue, alpha));
        return;
    }

    BlackBerry::Platform::Graphics::Buffer* dstBuffer = m_webPage->client()->window()->buffer();
    ASSERT(dstBuffer);
    if (!dstBuffer)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelWarn, "Empty window buffer, couldn't clearWindow");

    windowFrontBufferState()->clearBlittedRegion(rect);
    windowBackBufferState()->addBlittedRegion(rect);

    BlackBerry::Platform::Graphics::clearBuffer(dstBuffer, rect, red, green, blue, alpha);
}

bool BackingStorePrivate::isScrollingOrZooming() const
{
    BackingStoreMutexLocker locker(const_cast<BackingStorePrivate*>(this));
    return m_isScrollingOrZooming;
}

void BackingStorePrivate::setScrollingOrZooming(bool scrollingOrZooming, bool shouldBlit)
{
    {
        BackingStoreMutexLocker locker(this);
        m_isScrollingOrZooming = scrollingOrZooming;
    }

#if !ENABLE_REPAINTONSCROLL
    m_suspendRenderJobs = scrollingOrZooming; // Suspend the rendering of everything.
#endif

    if (!m_webPage->settings()->shouldRenderAnimationsOnScrollOrZoom())
        m_suspendRegularRenderJobs = scrollingOrZooming; // Suspend the rendering of animations.

    // Clear this flag since we don't care if the render queue is under pressure
    // or not since we are scrolling and it is more important to not lag than
    // it is to ensure animations achieve better framerates!
    if (scrollingOrZooming)
        m_renderQueue->setCurrentRegularRenderJobBatchUnderPressure(false);
#if ENABLE_SCROLLBARS
    else if (shouldBlit && !shouldDirectRenderingToWindow())
        blitVisibleContents();
#endif
}

void BackingStorePrivate::lockBackingStore()
{
    pthread_mutex_lock(&m_mutex);
}

void BackingStorePrivate::unlockBackingStore()
{
    pthread_mutex_unlock(&m_mutex);
}

BackingStoreGeometry* BackingStorePrivate::frontState() const
{
    return reinterpret_cast<BackingStoreGeometry*>(m_frontState);
}

BackingStoreGeometry* BackingStorePrivate::backState() const
{
    return reinterpret_cast<BackingStoreGeometry*>(m_backState);
}

void BackingStorePrivate::swapState()
{
    unsigned front = reinterpret_cast<unsigned>(frontState());
    unsigned back = reinterpret_cast<unsigned>(backState());

    // Atomic change.
    _smp_xchg(&m_frontState, back);
    _smp_xchg(&m_backState, front);
    BlackBerry::Platform::userInterfaceThreadMessageClient()->syncToCurrentMessage();
}

BackingStoreWindowBufferState* BackingStorePrivate::windowFrontBufferState() const
{
    return &m_windowBufferState[(m_currentWindowBackBuffer + 1) % 2];
}

BackingStoreWindowBufferState* BackingStorePrivate::windowBackBufferState() const
{
    return &m_windowBufferState[m_currentWindowBackBuffer];
}

#if USE(ACCELERATED_COMPOSITING)
bool BackingStorePrivate::drawSubLayers()
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        return false;

    if (m_suspendBackingStoreUpdates)
        return false;

    Platform::IntRect dst = m_webPage->client()->userInterfaceBlittedDestinationRect();
    if (dst.isEmpty())
        return false;

    Platform::IntRect src = m_webPage->client()->userInterfaceBlittedVisibleContentsRect();
    WebCore::FloatRect contentsRect = m_webPage->d->mapFromTransformedFloatRect(
        WebCore::FloatRect(WebCore::IntRect(src)));
    return m_webPage->d->drawSubLayers(dst, contentsRect);
}
#endif

bool BackingStorePrivate::isActive() const
{
    return BackingStorePrivate::s_currentBackingStoreOwner == m_webPage && !SurfacePool::globalSurfacePool()->isEmpty();
}

BackingStore::BackingStore(WebPage* webPage, BackingStoreClient* client)
    : d(new BackingStorePrivate)
{
    d->m_webPage = webPage;
    d->m_client = client;
}

BackingStore::~BackingStore()
{
    delete d;
    d = 0;
}

void BackingStore::createSurface()
{
    static bool initialized = false;
    if (!initialized) {
        BlackBerry::Platform::Graphics::initialize();
        initialized = true;
    }

#if USE(OPENVG)
    d->m_eglDisplay = BlackBerry::Platform::Graphics::eglDisplay();

    // Make sure we are using OpenVG.
    eglBindAPI(EGL_OPENVG_API);
    ASSERT_EGL_NO_ERROR();

    EGLDisplayOpenVG::setCurrentDisplay(d->m_eglDisplay);
#endif

    // Triggers creation of surfaces in backingstore.
    d->createSurfaces();

#if USE(OPENVG)
    EGLDisplayOpenVG::current()->sharedPlatformSurface()->makeCurrent();
#endif

    // Focusing the WebPage triggers a repaint, so while we want it to be
    // focused initially this has to happen after creation of the surface.
    d->m_webPage->setFocused(true);
}

void BackingStore::suspendScreenAndBackingStoreUpdates()
{
    d->suspendScreenAndBackingStoreUpdates();
}

void BackingStore::resumeScreenAndBackingStoreUpdates(ResumeUpdateOperation op)
{
    d->resumeScreenAndBackingStoreUpdates(op);
}

bool BackingStore::isScrollingOrZooming() const
{
    return d->isScrollingOrZooming();
}

void BackingStore::setScrollingOrZooming(bool scrollingOrZooming)
{
    d->setScrollingOrZooming(scrollingOrZooming);
}

void BackingStore::blitContents(const BlackBerry::Platform::IntRect& dstRect, const BlackBerry::Platform::IntRect& contents)
{
    // Blitting during direct rendering is not supported.
    if (isDirectRenderingToWindow()) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical,
                               "BackingStore::blitContents operation not supported in direct rendering mode");
        return;
    }

    d->blitContents(dstRect, contents);
}

void BackingStore::repaint(int x, int y, int width, int height,
                           bool contentChanged, bool immediate)
{
    d->repaint(Platform::IntRect(x, y, width, height), contentChanged, immediate);
}

bool BackingStore::hasRenderJobs() const
{
    return d->shouldPerformRenderJobs();
}

void BackingStore::renderOnIdle()
{
    d->renderOnIdle();
}

bool BackingStore::isDirectRenderingToWindow() const
{
    BackingStoreMutexLocker locker(d);
    return d->shouldDirectRenderingToWindow();
}

bool BackingStore::defersBlit() const
{
        return d->m_defersBlit;
}

void BackingStore::setDefersBlit(bool b)
{
        d->m_defersBlit = b;
}

bool BackingStore::hasBlitJobs() const
{
    // Normally, this would be called from the compositing thread,
    // and the flag is set on the compositing thread, so no need for
    // synchronization.
    return d->m_hasBlitJobs;
}

void BackingStore::blitOnIdle()
{
    d->blitVisibleContents(true /*force*/);
}

}
}
