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

#include "config.h"
#include "BackingStore.h"

#include "BackingStoreClient.h"
#include "BackingStoreTile.h"
#include "BackingStoreVisualizationViewportAccessor.h"
#include "BackingStore_p.h"
#include "FatFingers.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "SurfacePool.h"
#include "WebPage.h"
#include "WebPageClient.h"
#include "WebPageCompositorClient.h"
#include "WebPageCompositor_p.h"
#include "WebPage_p.h"
#include "WebSettings.h"

#include <BlackBerryPlatformExecutableMessage.h>
#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformIntRectRegion.h>
#include <BlackBerryPlatformLog.h>
#include <BlackBerryPlatformMessage.h>
#include <BlackBerryPlatformMessageClient.h>
#include <BlackBerryPlatformScreen.h>
#include <BlackBerryPlatformSettings.h>
#include <BlackBerryPlatformViewportAccessor.h>
#include <BlackBerryPlatformWindow.h>

#include <SkImageDecoder.h>

#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/NotFound.h>

#define SUPPRESS_NON_VISIBLE_REGULAR_RENDER_JOBS 0
#define ENABLE_SCROLLBARS 1
#define ENABLE_REPAINTONSCROLL 1
#define DEBUG_BACKINGSTORE 0
#define DEBUG_CHECKERBOARD 0
#define DEBUG_WEBCORE_REQUESTS 0
#define DEBUG_VISUALIZE 0
#define DEBUG_TILEMATRIX 0

using namespace WebCore;
using namespace std;

using BlackBerry::Platform::Graphics::Window;
using BlackBerry::Platform::IntRect;
using BlackBerry::Platform::IntPoint;
using BlackBerry::Platform::IntSize;

namespace BlackBerry {
namespace WebKit {

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

static bool divisorIsPerfectWidth(Divisor divisor, Platform::IntSize size, int tileWidth)
{
    return size.width() <= divisor.first * tileWidth && abs(size.width() - divisor.first * tileWidth) < tileWidth;
}

static bool divisorIsPerfectHeight(Divisor divisor, Platform::IntSize size, int tileHeight)
{
    return size.height() <= divisor.second * tileHeight && abs(size.height() - divisor.second * tileHeight) < tileHeight;
}

static bool divisorIsPreferredDirection(Divisor divisor, BackingStorePrivate::TileMatrixDirection direction)
{
    if (direction == BackingStorePrivate::Vertical)
        return divisor.second > divisor.first;
    return divisor.first > divisor.second;
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
    static DivisorList divisorList = divisors(surfacePool->numberOfBackingStoreFrontBuffers());

    // The ratio we're looking to best imitate.
    float ratio = static_cast<float>(size.width()) / static_cast<float>(size.height());

    Divisor bestDivisor;
    for (size_t i = 0; i < divisorList.size(); ++i) {
        Divisor divisor = divisorList[i];

        const bool isPerfectWidth = divisorIsPerfectWidth(divisor, size, tileWidth);
        const bool isPerfectHeight = divisorIsPerfectHeight(divisor, size, tileHeight);
        const bool isValidWidth = divisor.first >= minimumNumberOfTilesWide || isPerfectWidth;
        const bool isValidHeight = divisor.second >= minimumNumberOfTilesHigh || isPerfectHeight;
        if (!isValidWidth || !isValidHeight)
            continue;

        if (isPerfectWidth || isPerfectHeight) {
            bestDivisor = divisor; // Found a perfect fit!
#if DEBUG_TILEMATRIX
            Platform::logAlways(Platform::LogLevelCritical,
                "bestDivisor found perfect size isPerfectWidth=%s isPerfectHeight=%s",
                isPerfectWidth ? "true" : "false",
                isPerfectHeight ? "true" : "false");
#endif
            break;
        }

        // Store basis of comparison.
        if (!bestDivisor.first || !bestDivisor.second) {
            bestDivisor = divisor;
            continue;
        }

        // If the current best divisor agrees with the preferred tile matrix direction,
        // then continue if the current candidate does not.
        if (divisorIsPreferredDirection(bestDivisor, direction) && !divisorIsPreferredDirection(divisor, direction))
            continue;

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

bool BackingStoreGeometry::isTileCorrespondingToBuffer(TileIndex index, TileBuffer* tileBuffer) const
{
    return tileBuffer
        && scale() == tileBuffer->lastRenderScale()
        && originOfTile(index) == tileBuffer->lastRenderOrigin();
}

BackingStorePrivate::BackingStorePrivate()
    : m_suspendScreenUpdates(0)
    , m_suspendBackingStoreUpdates(0)
    , m_resumeOperation(BackingStore::None)
    , m_suspendRenderJobs(false)
    , m_suspendRegularRenderJobs(false)
    , m_tileMatrixNeedsUpdate(false)
    , m_isScrollingOrZooming(false)
    , m_webPage(0)
    , m_client(0)
    , m_renderQueue(adoptPtr(new RenderQueue(this)))
    , m_defersBlit(true)
    , m_hasBlitJobs(false)
    , m_webPageBackgroundColor(WebCore::Color::white)
    , m_currentWindowBackBuffer(0)
    , m_preferredTileMatrixDimension(Vertical)
#if USE(ACCELERATED_COMPOSITING)
    , m_isDirectRenderingAnimationMessageScheduled(false)
#endif
{
    m_frontState = reinterpret_cast<unsigned>(new BackingStoreGeometry);

    // Need a recursive mutex to achieve a global lock.
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

BackingStorePrivate::~BackingStorePrivate()
{
    BackingStoreGeometry* front = reinterpret_cast<BackingStoreGeometry*>(m_frontState);
    delete front;
    m_frontState = 0;

    pthread_mutex_destroy(&m_mutex);
}

void BackingStorePrivate::instrumentBeginFrame()
{
    WebCore::InspectorInstrumentation::didBeginFrame(WebPagePrivate::core(m_webPage));
}

void BackingStorePrivate::instrumentCancelFrame()
{
    WebCore::InspectorInstrumentation::didCancelFrame(WebPagePrivate::core(m_webPage));
}

bool BackingStorePrivate::shouldDirectRenderingToWindow() const
{
    // Direct rendering doesn't work with OpenGL compositing code paths due to
    // a race condition on which thread's EGL context gets to make the surface
    // current, see PR 105750.
    // As a workaround, we will be using compositor to draw the root layer.
    if (isOpenGLCompositing())
        return false;

    if (m_webPage->settings()->isDirectRenderingToWindowEnabled())
        return true;

    // If the BackingStore is inactive, see if there's a compositor to do the
    // work of rendering the root layer.
    if (!isActive())
        return !m_webPage->d->compositorDrawsRootLayer();

    const BackingStoreGeometry* currentState = frontState();
    const unsigned tilesNecessary = minimumNumberOfTilesWide() * minimumNumberOfTilesHigh();
    const unsigned tilesAvailable = currentState->numberOfTilesWide() * currentState->numberOfTilesHigh();
    return tilesAvailable < tilesNecessary;
}

bool BackingStorePrivate::isOpenGLCompositing() const
{
    if (Window* window = m_webPage->client()->window())
        return window->windowUsage() == Window::GLES2Usage;

    // If there's no window, OpenGL rendering is currently the only option.
    return true;
}

void BackingStorePrivate::suspendBackingStoreUpdates()
{
    if (atomic_add_value(&m_suspendBackingStoreUpdates, 0)) {
        BBLOG(Platform::LogLevelInfo,
            "Backingstore already suspended, increasing suspend counter.");
    }

    atomic_add(&m_suspendBackingStoreUpdates, 1);
}

void BackingStorePrivate::suspendScreenUpdates()
{
    if (m_suspendScreenUpdates) {
        BBLOG(Platform::LogLevelInfo,
            "Screen already suspended, increasing suspend counter.");
    }

    // Make sure the user interface thread gets the message before we proceed
    // because blitVisibleContents() can be called from the user interface
    // thread and it must honor this flag.
    ++m_suspendScreenUpdates;

    BlackBerry::Platform::userInterfaceThreadMessageClient()->syncToCurrentMessage();
}

void BackingStorePrivate::resumeBackingStoreUpdates()
{
    unsigned currentValue = atomic_add_value(&m_suspendBackingStoreUpdates, 0);
    ASSERT(currentValue >= 1);
    if (currentValue < 1) {
        Platform::logAlways(Platform::LogLevelCritical,
            "Call mismatch: Backingstore hasn't been suspended, therefore won't resume!");
        return;
    }

    // Set a flag indicating that we're about to resume backingstore updates and
    // the tile matrix should be updated as a consequence by the first render
    // job that happens after this resumption of backingstore updates.
    if (currentValue == 1)
        setTileMatrixNeedsUpdate();

    atomic_sub(&m_suspendBackingStoreUpdates, 1);
}

void BackingStorePrivate::resumeScreenUpdates(BackingStore::ResumeUpdateOperation op)
{
    ASSERT(m_suspendScreenUpdates);

    if (!m_suspendScreenUpdates) {
        Platform::logAlways(Platform::LogLevelCritical,
            "Call mismatch: Screen hasn't been suspended, therefore won't resume!");
        return;
    }

    // Out of all nested resume calls, resume with the maximum-impact operation.
    if (op == BackingStore::RenderAndBlit
        || (m_resumeOperation == BackingStore::None && op == BackingStore::Blit))
        m_resumeOperation = op;

    if (m_suspendScreenUpdates >= 2) { // we're still suspended
        BBLOG(Platform::LogLevelInfo,
            "Screen and backingstore still suspended, decreasing suspend counter.");
        --m_suspendScreenUpdates;
        return;
    }

    op = m_resumeOperation;
    m_resumeOperation = BackingStore::None;

#if USE(ACCELERATED_COMPOSITING)
    if (op != BackingStore::None) {
        if (isOpenGLCompositing() && !isActive()) {
            m_webPage->d->setCompositorDrawsRootLayer(true);
            m_webPage->d->setNeedsOneShotDrawingSynchronization();
            --m_suspendScreenUpdates;
            BlackBerry::Platform::userInterfaceThreadMessageClient()->syncToCurrentMessage();
            return;
        }

        m_webPage->d->setNeedsOneShotDrawingSynchronization();
    }
#endif

    // For the direct rendering case, there is no such operation as blit,
    // we have to render to get anything to the screen.
    if (shouldDirectRenderingToWindow() && op == BackingStore::Blit)
        op = BackingStore::RenderAndBlit;

    // Do some rendering if necessary.
    if (op == BackingStore::RenderAndBlit)
        renderVisibleContents();

    // Make sure the user interface thread gets the message before we proceed
    // because blitVisibleContents() can be called from the user interface
    // thread and it must honor this flag.
    --m_suspendScreenUpdates;
    BlackBerry::Platform::userInterfaceThreadMessageClient()->syncToCurrentMessage();

    if (op == BackingStore::None)
        return;
#if USE(ACCELERATED_COMPOSITING)
    // It needs layout and render before committing root layer if we set OSDS
    if (m_webPage->d->needsOneShotDrawingSynchronization())
        m_webPage->d->requestLayoutIfNeeded();

    // This will also blit since we set the OSDS flag above.
    m_webPage->d->commitRootLayerIfNeeded();
#else
    // Do some blitting if necessary.
    if ((op == BackingStore::Blit || op == BackingStore::RenderAndBlit) && !shouldDirectRenderingToWindow())
        blitVisibleContents();
#endif
}

void BackingStorePrivate::repaint(const Platform::IntRect& windowRect,
                                  bool contentChanged, bool immediate)
{
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

#if DEBUG_WEBCORE_REQUESTS
        Platform::logAlways(Platform::LogLevelCritical,
            "BackingStorePrivate::repaint rect=%s contentChanged=%s immediate=%s",
            rect.toString().c_str(),
            contentChanged ? "true" : "false",
            immediate ? "true" : "false");
#endif

        if (immediate) {
            if (m_suspendBackingStoreUpdates)
                return;

            if (render(rect)) {
                if (!shouldDirectRenderingToWindow() && !m_webPage->d->commitRootLayerIfNeeded())
                    blitVisibleContents();
                m_webPage->d->m_client->notifyPixelContentRendered(rect);
            }
        } else
            m_renderQueue->addToQueue(RenderQueue::RegularRender, rect);
    }
}

void BackingStorePrivate::slowScroll(const Platform::IntSize& delta, const Platform::IntRect& windowRect, bool immediate)
{
    ASSERT(BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread());

#if DEBUG_BACKINGSTORE
    // Start the time measurement...
    double time = WTF::currentTime();
#endif

    scrollingStartedHelper(delta);

    // This windowRect is in untransformed coordinates relative to the viewport, but
    // it needs to be transformed coordinates relative to the transformed contents.
    Platform::IntRect rect = m_webPage->d->mapToTransformed(m_client->mapFromViewportToContents(windowRect));

    if (immediate) {
        if (render(rect) && !isSuspended() && !shouldDirectRenderingToWindow() && !m_webPage->d->commitRootLayerIfNeeded())
            blitVisibleContents();
    } else {
        m_renderQueue->addToQueue(RenderQueue::VisibleScroll, rect);
        // We only blit here if the client did not generate the scroll as the client
        // now supports blitting asynchronously during scroll operations.
        if (!m_client->isClientGeneratedScroll() && !shouldDirectRenderingToWindow())
            blitVisibleContents();
    }

#if DEBUG_BACKINGSTORE
    // Stop the time measurement.
    double elapsed = WTF::currentTime() - time;
    Platform::logAlways(Platform::LogLevelCritical, "BackingStorePrivate::slowScroll elapsed=%f", elapsed);
#endif
}

void BackingStorePrivate::scroll(const Platform::IntSize& delta,
                                 const Platform::IntRect& scrollViewRect,
                                 const Platform::IntRect& clipRect)
{
    ASSERT(BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread());

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
    Platform::logAlways(Platform::LogLevelCritical, "BackingStorePrivate::scroll dx=%d, dy=%d elapsed=%f", delta.width(), delta.height(), elapsed);
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

static const BlackBerry::Platform::Message::Type RenderJobMessageType = BlackBerry::Platform::Message::generateUniqueMessageType();
class RenderJobMessage : public BlackBerry::Platform::ExecutableMessage {
public:
    RenderJobMessage(BlackBerry::Platform::MessageDelegate* delegate)
        : BlackBerry::Platform::ExecutableMessage(delegate, BlackBerry::Platform::ExecutableMessage::UniqueCoalescing, RenderJobMessageType)
    { }
};

void BackingStorePrivate::dispatchRenderJob()
{
    BlackBerry::Platform::MessageDelegate* messageDelegate = BlackBerry::Platform::createMethodDelegate(&BackingStorePrivate::renderJob, this);
    BlackBerry::Platform::webKitThreadMessageClient()->dispatchMessage(new RenderJobMessage(messageDelegate));
}

void BackingStorePrivate::renderJob()
{
    if (!shouldPerformRenderJobs())
        return;

    instrumentBeginFrame();

#if DEBUG_BACKINGSTORE
    Platform::logAlways(Platform::LogLevelCritical, "BackingStorePrivate::renderJob");
#endif

    m_renderQueue->render(!m_suspendRegularRenderJobs);

#if USE(ACCELERATED_COMPOSITING)
    m_webPage->d->commitRootLayerIfNeeded();
#endif

    if (shouldPerformRenderJobs())
        dispatchRenderJob();
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

void BackingStorePrivate::setBackingStoreRect(const Platform::IntRect& backingStoreRect, double scale)
{
    if (!m_webPage->isVisible())
        return;

    if (!isActive()) {
        m_webPage->d->setShouldResetTilesWhenShown(true);
        return;
    }

    if (m_suspendBackingStoreUpdates)
        return;

    Platform::IntRect currentBackingStoreRect = frontState()->backingStoreRect();
    double currentScale = frontState()->scale();

    if (backingStoreRect == currentBackingStoreRect && scale == currentScale)
        return;

#if DEBUG_TILEMATRIX
    Platform::logAlways(Platform::LogLevelCritical,
        "BackingStorePrivate::setBackingStoreRect changed from %s to %s",
        currentBackingStoreRect.toString().c_str(),
        backingStoreRect.toString().c_str());
#endif

    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    TileIndexList indexesToFill = indexesForBackingStoreRect(backingStoreRect);

    ASSERT(static_cast<int>(indexesToFill.size()) == currentMap.size());

    m_renderQueue->clear(currentBackingStoreRect, false /*clearRegularRenderJobs*/);

    TileMap newTileMap;
    TileMap leftOverTiles;

    // Iterate through our current tile map and add tiles that are rendered with
    // our new backing store rect.
    TileMap::const_iterator tileMapEnd = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != tileMapEnd; ++it) {
        TileIndex oldIndex = it->key;
        TileBuffer* oldTileBuffer = it->value;

        // If the new backing store rect contains this origin, then insert the tile there
        // and mark it as no longer shifted. Note: Platform::IntRect::contains checks for a 1x1 rect
        // below and to the right of the origin so it is correct usage here.
        if (oldTileBuffer && backingStoreRect.contains(oldTileBuffer->lastRenderOrigin())) {
            TileIndex newIndex = indexOfTile(oldTileBuffer->lastRenderOrigin(), backingStoreRect);

            size_t i = indexesToFill.find(newIndex);
            ASSERT(i != WTF::notFound);
            indexesToFill.remove(i);
            newTileMap.add(newIndex, oldTileBuffer);
        } else {
            // Store this tile and index so we can add it to the remaining left over spots...
            leftOverTiles.add(oldIndex, oldTileBuffer);
        }
    }

    ASSERT(static_cast<int>(indexesToFill.size()) == leftOverTiles.size());
    size_t i = 0;
    TileMap::const_iterator leftOverEnd = leftOverTiles.end();
    for (TileMap::const_iterator it = leftOverTiles.begin(); it != leftOverEnd; ++it) {
        TileBuffer* oldTileBuffer = it->value;
        if (i >= indexesToFill.size()) {
            ASSERT_NOT_REACHED();
            break;
        }

        TileIndex newIndex = indexesToFill.at(i);
        newTileMap.add(newIndex, oldTileBuffer);

        ++i;
    }

    // Checks to make sure we haven't lost any tiles.
    ASSERT(currentMap.size() == newTileMap.size());

    BackingStoreGeometry* newGeometry = new BackingStoreGeometry;
    newGeometry->setScale(scale);
    newGeometry->setNumberOfTilesWide(backingStoreRect.width() / tileWidth());
    newGeometry->setNumberOfTilesHigh(backingStoreRect.height() / tileHeight());
    newGeometry->setBackingStoreOffset(backingStoreRect.location());
    newGeometry->setTileMap(newTileMap);
    adoptAsFrontState(newGeometry); // swap into UI thread

    // Mark tiles as needing update.
    updateTilesAfterBackingStoreRectChange();
}

void BackingStorePrivate::updateTilesAfterBackingStoreRectChange()
{
    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    TileMap::const_iterator end = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != end; ++it) {
        TileIndex index = it->key;
        TileBuffer* tileBuffer = it->value;
        Platform::IntPoint tileOrigin = currentState->originOfTile(index);
        // The rect in transformed contents coordinates.
        Platform::IntRect rect(tileOrigin, tileSize());

        if (currentState->isTileCorrespondingToBuffer(index, tileBuffer)) {
            if (m_renderQueue->regularRenderJobsPreviouslyAttemptedButNotRendered(rect)) {
                // If the render queue previously tried to render this tile, but the
                // tile wasn't visible at the time we can't simply restore the tile
                // since the content is now invalid as far as WebKit is concerned.
                // Instead, we clear that part of the tile if it is visible and then
                // put the tile in the render queue again.

                // Intersect the tile with the not rendered region to get the areas
                // of the tile that we need to clear.
                Platform::IntRectRegion tileNotRenderedRegion = Platform::IntRectRegion::intersectRegions(m_renderQueue->regularRenderJobsNotRenderedRegion(), rect);
                clearAndUpdateTileOfNotRenderedRegion(index, tileBuffer, tileNotRenderedRegion, currentState);
#if DEBUG_BACKINGSTORE
                Platform::logAlways(Platform::LogLevelCritical,
                    "BackingStorePrivate::updateTilesAfterBackingStoreRectChange did clear tile %s",
                    tileNotRenderedRegion.extents().toString().c_str());
#endif
            } else {
                if (!tileBuffer || !tileBuffer->isRendered(tileVisibleContentsRect(index, currentState), currentState->scale())
                    && !isCurrentVisibleJob(index, tileBuffer, currentState))
                    updateTile(tileOrigin, false /*immediate*/);
            }
        } else if (rect.intersects(expandedContentsRect()))
            updateTile(tileOrigin, false /*immediate*/);
    }
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

void BackingStorePrivate::clearAndUpdateTileOfNotRenderedRegion(const TileIndex& index, TileBuffer* tileBuffer,
                                                                const Platform::IntRectRegion& tileNotRenderedRegion,
                                                                BackingStoreGeometry* geometry,
                                                                bool update)
{
    if (tileNotRenderedRegion.isEmpty())
        return;

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
    }

    if (!tileBuffer)
        return;

    // If the region in question is already marked as not rendered, return early
    if (Platform::IntRectRegion::intersectRegions(tileBuffer->renderedRegion(), tileNotRenderedRegion).isEmpty())
        return;

    // Clear the tile of this region. The back buffer region is invalid anyway, but the front
    // buffer must not be manipulated without synchronization with the compositing thread, or
    // we have a race.
    // Instead of using the customary sequence of copy-back, modify and swap, we send a synchronous
    // message to the compositing thread to avoid the copy-back step and save memory bandwidth.
    // The trade-off is that the WebKit thread might wait a little longer for the compositing thread
    // than it would from a waitForCurrentMessage() call.

    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());
    if (!Platform::webKitThreadMessageClient()->isCurrentThread())
        return;

    Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
        Platform::createMethodCallMessage(&BackingStorePrivate::clearRenderedRegion,
            this, tileBuffer, tileNotRenderedRegion));
}

void BackingStorePrivate::clearRenderedRegion(TileBuffer* tileBuffer, const Platform::IntRectRegion& region)
{
    ASSERT(Platform::userInterfaceThreadMessageClient()->isCurrentThread());
    if (!Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        return;
    if (!tileBuffer)
        return;

    tileBuffer->clearRenderedRegion(region);
}

bool BackingStorePrivate::isCurrentVisibleJob(const TileIndex& index, TileBuffer* tileBuffer, BackingStoreGeometry* geometry) const
{
    // First check if the whole rect is in the queue.
    Platform::IntPoint tileOrigin = geometry->originOfTile(index);
    Platform::IntRect wholeRect = Platform::IntRect(tileOrigin, tileSize());
    if (m_renderQueue->isCurrentVisibleScrollJob(wholeRect) || m_renderQueue->isCurrentVisibleScrollJobCompleted(wholeRect))
        return true;

    // Second check if the individual parts of the non-rendered region are in the regular queue.
    if (!tileBuffer)
        return m_renderQueue->isCurrentRegularRenderJob(wholeRect);

    IntRectList tileNotRenderedRegionRects = tileBuffer->notRenderedRegion().rects();

    for (size_t i = 0; i < tileNotRenderedRegionRects.size(); ++i) {
        if (!m_renderQueue->isCurrentRegularRenderJob(tileNotRenderedRegionRects.at(i)))
            return false;
    }
    return true;
}

void BackingStorePrivate::scrollBackingStore(int deltaX, int deltaY)
{
    ASSERT(BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread());

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
    Platform::logAlways(Platform::LogLevelCritical,
        "BackingStorePrivate::scrollBackingStore divisor %dx%d",
        divisor.first, divisor.second);
#endif

    // Initialize a rect with that new geometry.
    Platform::IntRect backingStoreRect(0, 0, divisor.first * tileWidth(), divisor.second * tileHeight());

    // Scroll that rect so that it fits our contents and viewport and scroll delta.
    backingStoreRect = backingStoreRectForScroll(deltaX, deltaY, backingStoreRect);

    ASSERT(!backingStoreRect.isEmpty());

    setBackingStoreRect(backingStoreRect, m_webPage->d->currentScale());
}

bool BackingStorePrivate::renderDirectToWindow(const Platform::IntRect& rect)
{
    requestLayoutIfNeeded();

    Platform::IntRect dirtyRect = rect;
    dirtyRect.intersect(unclippedVisibleContentsRect());

    if (dirtyRect.isEmpty())
        return false;

    Platform::IntRect screenRect = m_client->mapFromTransformedContentsToTransformedViewport(dirtyRect);
    windowFrontBufferState()->clearBlittedRegion(screenRect);

    paintDefaultBackground(dirtyRect, m_webPage->webkitThreadViewportAccessor(), true /*flush*/);

    const Platform::IntPoint origin = unclippedVisibleContentsRect().location();
    // We don't need a buffer since we're direct rendering to window.
    renderContents(0, origin, dirtyRect);
    windowBackBufferState()->addBlittedRegion(screenRect);

#if USE(ACCELERATED_COMPOSITING)
    m_isDirectRenderingAnimationMessageScheduled = false;

    if (m_webPage->d->isAcceleratedCompositingActive()) {
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
            BlackBerry::Platform::createMethodCallMessage(
                &BackingStorePrivate::drawAndBlendLayersForDirectRendering,
                this, dirtyRect));
    }
#endif

    invalidateWindow(screenRect);
    return true;
}

bool BackingStorePrivate::render(const Platform::IntRect& rect)
{
    if (!m_webPage->isVisible())
        return false;

    requestLayoutIfNeeded();

    if (shouldDirectRenderingToWindow())
        return renderDirectToWindow(rect);

    // If direct rendering is off, even though we're not active, someone else
    // has to render the root layer. There are no tiles available for us to
    // draw to.
    if (!isActive())
        return false;

    // This is the first render job that has been performed since resumption of
    // backingstore updates and the tile matrix needs updating since we suspend
    // tile matrix updating with backingstore updates
    updateTileMatrixIfNeeded();

#if DEBUG_BACKINGSTORE
    Platform::logAlways(Platform::LogLevelCritical,
        "BackingStorePrivate::render rect=%s, m_suspendBackingStoreUpdates = %s",
        rect.toString().c_str(),
        m_suspendBackingStoreUpdates ? "true" : "false");
#endif

    BackingStoreGeometry* currentState = frontState();
    TileMap oldTileMap = currentState->tileMap();
    double currentScale = currentState->scale();

    TileRectList tileRectList = mapFromPixelContentsToTiles(rect, currentState);
    if (tileRectList.isEmpty())
        return false;

    BackingStoreGeometry* newGeometry = new BackingStoreGeometry;
    newGeometry->setScale(currentState->scale());
    newGeometry->setNumberOfTilesWide(currentState->numberOfTilesWide());
    newGeometry->setNumberOfTilesHigh(currentState->numberOfTilesHigh());
    newGeometry->setBackingStoreOffset(currentState->backingStoreOffset());
    TileMap newTileMap(oldTileMap); // copy a new, writable version

    for (size_t i = 0; i < tileRectList.size(); ++i) {
        TileRect tileRect = tileRectList[i];
        TileIndex index = tileRect.first;
        Platform::IntRect dirtyRect = tileRect.second;
        TileBuffer* frontBuffer = oldTileMap.get(index);

        if (!SurfacePool::globalSurfacePool()->hasBackBuffer()) {
            newGeometry->setTileMap(newTileMap);
            adoptAsFrontState(newGeometry); // this should get us at least one more.

            // newGeometry is now the front state and shouldn't be messed with.
            // Let's create a new one. (The old one will be automatically
            // destroyed by adoptAsFrontState() on being swapped out again.)
            currentState = frontState();
            newGeometry = new BackingStoreGeometry;
            newGeometry->setScale(currentState->scale());
            newGeometry->setNumberOfTilesWide(currentState->numberOfTilesWide());
            newGeometry->setNumberOfTilesHigh(currentState->numberOfTilesHigh());
            newGeometry->setBackingStoreOffset(currentState->backingStoreOffset());
        }

        // Paint default background if contents rect is empty.
        if (!expandedContentsRect().isEmpty()) {
            // Otherwise we should clip the contents size and render the content.
            dirtyRect.intersect(expandedContentsRect());

            // We probably have extra tiles since the contents size is so small.
            // Save some cycles here...
            if (dirtyRect.isEmpty())
                continue;
        }

        TileBuffer* backBuffer = SurfacePool::globalSurfacePool()->takeBackBuffer();
        ASSERT(backBuffer);

        // If the tile has been created, but this is the first time we are painting on it
        // then it hasn't been given a default background yet so that we can save time during
        // startup. That's why we are doing it here instead...
        if (!backBuffer->backgroundPainted())
            backBuffer->paintBackground();

        Platform::IntPoint tileOrigin = currentState->originOfTile(index);
        backBuffer->setLastRenderScale(currentScale);
        backBuffer->setLastRenderOrigin(tileOrigin);
        backBuffer->clearRenderedRegion();

        BlackBerry::Platform::Graphics::Buffer* nativeBuffer = backBuffer->nativeBuffer();
        BlackBerry::Platform::Graphics::setBufferOpaqueHint(nativeBuffer, !Color(m_webPage->settings()->backgroundColor()).hasAlpha());

        // TODO: This code is only needed for EGLImage code path, but preferrably BackingStore
        // should not know that, and the synchronization should be in BlackBerry::Platform::Graphics
        // if possible.
        if (isOpenGLCompositing())
            SurfacePool::globalSurfacePool()->waitForBuffer(backBuffer);

        // Modify the buffer only after we've waited for it to become available above.
        bool frontBufferHasUsableContents = currentState->isTileCorrespondingToBuffer(index, frontBuffer);
        if (frontBufferHasUsableContents)
            copyPreviousContentsToTileBuffer(dirtyRect, backBuffer, frontBuffer);

        // FIXME: modify render to take a Vector<IntRect> parameter so we're not recreating
        // GraphicsContext on the stack each time.
        renderContents(nativeBuffer, tileOrigin, dirtyRect);

        // Add the newly rendered region to the tile so it can keep track for blits.
        backBuffer->addRenderedRegion(dirtyRect);

        // Thanks to the copyPreviousContentsToBackSurfaceOfTile() call above, we know that
        // the rendered region of the back buffer contains the rendered region of the front buffer.
        // Assert this just to make sure.
        // For previously displaced buffers, the front buffer's rendered region is not relevant.
        ASSERT(!frontBufferHasUsableContents || backBuffer->isRendered(frontBuffer->renderedRegion(), currentScale));

        newTileMap.set(index, backBuffer);
    }

    newGeometry->setTileMap(newTileMap);
    adoptAsFrontState(newGeometry);

    return true;
}

void BackingStorePrivate::requestLayoutIfNeeded() const
{
    m_webPage->d->requestLayoutIfNeeded();
}

bool BackingStorePrivate::renderVisibleContents()
{
    updateTileMatrixIfNeeded();
    Platform::IntRect renderRect = shouldDirectRenderingToWindow() ? visibleContentsRect() : visibleTilesRect(frontState());
    if (render(renderRect)) {
        m_renderQueue->clear(renderRect, true /*clearRegularRenderJobs*/);
        return true;
    }
    return false;
}

bool BackingStorePrivate::renderBackingStore()
{
    updateTileMatrixIfNeeded();
    return render(frontState()->backingStoreRect());
}

void BackingStorePrivate::copyPreviousContentsToBackSurfaceOfWindow()
{
    Platform::IntRectRegion previousContentsRegion
        = Platform::IntRectRegion::subtractRegions(windowFrontBufferState()->blittedRegion(), windowBackBufferState()->blittedRegion());

    if (previousContentsRegion.isEmpty())
        return;

    if (Window* window = m_webPage->client()->window())
        window->copyFromFrontToBack(previousContentsRegion);
    windowBackBufferState()->addBlittedRegion(previousContentsRegion);
}

void BackingStorePrivate::copyPreviousContentsToTileBuffer(const Platform::IntRect& excludeContentsRect, TileBuffer* dstTileBuffer, TileBuffer* srcTileBuffer)
{
    ASSERT(dstTileBuffer);
    ASSERT(srcTileBuffer);

    Platform::IntRectRegion previousContentsRegion
        = Platform::IntRectRegion::subtractRegions(srcTileBuffer->renderedRegion(), excludeContentsRect);

    IntRectList previousContentsRects = previousContentsRegion.rects();

    for (size_t i = 0; i < previousContentsRects.size(); ++i) {
        Platform::IntRect previousContentsRect = previousContentsRects.at(i);
        dstTileBuffer->addRenderedRegion(previousContentsRect);

        previousContentsRect.move(-srcTileBuffer->lastRenderOrigin().x(), -srcTileBuffer->lastRenderOrigin().y());
        BlackBerry::Platform::Graphics::blitToBuffer(
            dstTileBuffer->nativeBuffer(), previousContentsRect,
            srcTileBuffer->nativeBuffer(), previousContentsRect);
    }
}

void BackingStorePrivate::paintDefaultBackground(const Platform::IntRect& dstRect, Platform::ViewportAccessor* viewportAccessor, bool flush)
{
    Platform::IntRect clippedDstRect = dstRect;

    // Because of rounding it is possible that overScrollRect could be off-by-one larger
    // than the surface size of the window. We prevent this here, by clamping
    // it to ensure that can't happen.
    clippedDstRect.intersect(Platform::IntRect(Platform::IntPoint(0, 0), surfaceSize()));

    if (clippedDstRect.isEmpty())
        return;

    // We have to paint the default background in the case of overzoom and
    // make sure it is invalidated.
    const Platform::IntRect pixelContentsRect = viewportAccessor->pixelContentsRect();
    Platform::IntRectRegion overScrollRegion = Platform::IntRectRegion::subtractRegions(
        clippedDstRect, viewportAccessor->pixelViewportFromContents(pixelContentsRect));

    IntRectList overScrollRects = overScrollRegion.rects();
    for (size_t i = 0; i < overScrollRects.size(); ++i) {
        Platform::IntRect overScrollRect = overScrollRects.at(i);

        if (m_webPage->settings()->isEnableDefaultOverScrollBackground()) {
            fillWindow(BlackBerry::Platform::Graphics::DefaultBackgroundPattern,
                overScrollRect, overScrollRect.location(), 1.0 /*contentsScale*/);
        } else {
            Color color(m_webPage->settings()->overScrollColor());
            clearWindow(overScrollRect, color.red(), color.green(), color.blue(), color.alpha());
        }
    }
}

void BackingStorePrivate::blitVisibleContents(bool force)
{
    // Blitting must never happen for direct rendering case.
    // Use invalidateWindow() instead.
    ASSERT(!shouldDirectRenderingToWindow());
    if (shouldDirectRenderingToWindow()) {
        Platform::logAlways(Platform::LogLevelCritical,
            "BackingStore::blitVisibleContents operation not supported in direct rendering mode");
        return;
    }

    if (!m_webPage->isVisible() || m_suspendScreenUpdates) {
        // Avoid client going into busy loop while blit is impossible.
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

    if (m_defersBlit && !force) {
#if USE(ACCELERATED_COMPOSITING)
        // If there's a WebPageCompositorClient, let it schedule the blit.
        if (WebPageCompositorPrivate* compositor = m_webPage->d->compositor()) {
            if (WebPageCompositorClient* client = compositor->client()) {
                client->invalidate(0);
                return;
            }
        }
#endif

        m_hasBlitJobs = true;
        return;
    }

    m_hasBlitJobs = false;

    Platform::ViewportAccessor* viewportAccessor = m_webPage->client()->userInterfaceViewportAccessor();
    if (!viewportAccessor)
        return;
    const Platform::IntRect dstRect = viewportAccessor->destinationSurfaceRect();

    const Platform::IntRect pixelViewportRect = viewportAccessor->pixelViewportRect();
    const Platform::FloatRect documentViewportRect = viewportAccessor->documentFromPixelContents(pixelViewportRect);
    Platform::IntRect pixelSrcRect = pixelViewportRect;
    Platform::FloatRect documentSrcRect = documentViewportRect;

#if DEBUG_VISUALIZE
    // Substitute a srcRect that consists of the whole backingstore geometry
    // instead of the normal viewport so we can visualize the entire
    // backingstore and what it is doing when we scroll and zoom!
    Platform::ViewportAccessor* debugViewportAccessor = new BackingStoreVisualizationViewportAccessor(viewportAccessor, this);
    if (isActive()) {
        viewportAccessor = debugViewportAccessor;
        documentSrcRect = debugViewportAccessor->documentViewportRect();
        pixelSrcRect = debugViewportAccessor->pixelViewportRect();
    }
#endif

#if DEBUG_BACKINGSTORE
    Platform::logAlways(Platform::LogLevelCritical,
        "BackingStorePrivate::blitVisibleContents(): dstRect=%s, documentSrcRect=%s, scale=%f",
        dstRect.toString().c_str(), documentSrcRect.toString().c_str(), viewportAccessor->scale());
#endif

#if DEBUG_CHECKERBOARD
    bool blitCheckered = false;
#endif

    Vector<TileBuffer*> blittedTiles;

    if (isActive() && !m_webPage->d->compositorDrawsRootLayer()) {
        paintDefaultBackground(dstRect, viewportAccessor, false /*flush*/);

        BackingStoreGeometry* currentState = frontState();
        TileMap currentMap = currentState->tileMap();
        double currentScale = currentState->scale();

        const Platform::IntRect transformedContentsRect = Platform::IntRect(Platform::IntPoint(0, 0), m_client->transformedContentsSize());

        // For blitting backingstore tiles, we need the srcRect to be specified
        // in backingstore tile pixel coordinates. If our viewport accessor is
        //  at a different scale, we calculate those coordinates by ourselves.
        const Platform::IntRect transformedSrcRect = currentScale == viewportAccessor->scale()
            ? pixelSrcRect
            : viewportAccessor->roundFromDocumentContents(documentSrcRect, currentScale);

        Platform::IntRect clippedTransformedSrcRect = transformedSrcRect;
        const Platform::IntPoint origin = transformedSrcRect.location();

        // FIXME: This should not explicitly depend on WebCore::.
        TransformationMatrix transformation;
        if (!transformedSrcRect.isEmpty())
            transformation = TransformationMatrix::rectToRect(FloatRect(FloatPoint(0.0, 0.0), WebCore::IntSize(transformedSrcRect.size())), WebCore::IntRect(dstRect));

        // Don't clip to contents if it is empty so we can still paint default background.
        if (!transformedContentsRect.isEmpty()) {
            clippedTransformedSrcRect.intersect(transformedContentsRect);
            if (clippedTransformedSrcRect.isEmpty()) {
                invalidateWindow(dstRect);
                return;
            }

            Platform::IntRectRegion transformedSrcRegion = clippedTransformedSrcRect;
            Platform::IntRectRegion backingStoreRegion = currentState->backingStoreRect();
            Platform::IntRectRegion checkeredRegion
                = Platform::IntRectRegion::subtractRegions(transformedSrcRegion, backingStoreRegion);

            // Blit checkered to those parts that are not covered by the backingStoreRect.
            IntRectList checkeredRects = checkeredRegion.rects();
            for (size_t i = 0; i < checkeredRects.size(); ++i) {
                Platform::IntRect clippedDstRect = transformation.mapRect(Platform::IntRect(
                    Platform::IntPoint(checkeredRects.at(i).x() - origin.x(), checkeredRects.at(i).y() - origin.y()),
                                       checkeredRects.at(i).size()));
                // To eliminate 1 pixel inflation due to transformation rounding.
                clippedDstRect.intersect(dstRect);
#if DEBUG_CHECKERBOARD
                blitCheckered = true;
#endif

                fillWindow(BlackBerry::Platform::Graphics::CheckerboardPattern,
                    clippedDstRect, checkeredRects.at(i).location(), transformation.a());
            }
        }

        // Get the list of tile rects that makeup the content.
        TileRectList tileRectList = mapFromPixelContentsToTiles(clippedTransformedSrcRect, currentState);
        for (size_t i = 0; i < tileRectList.size(); ++i) {
            TileRect tileRect = tileRectList[i];
            TileIndex index = tileRect.first;
            Platform::IntRect dirtyRect = tileRect.second;

            // Don't clip to contents if it is empty so we can still paint default background.
            if (!transformedContentsRect.isEmpty()) {
                // Otherwise we should clip the contents size and blit.
                dirtyRect.intersect(transformedContentsRect);
            }

            // Save some cycles here...
            if (dirtyRect.isEmpty())
                continue;

            // Now, this dirty rect is in transformed coordinates relative to the
            // transformed contents, but ultimately it needs to be transformed
            // coordinates relative to the viewport.
            dirtyRect.move(-origin.x(), -origin.y());

            TileBuffer* tileBuffer = currentMap.get(index);

            bool isTileCorrespondingToBuffer = currentState->isTileCorrespondingToBuffer(index, tileBuffer);
            bool rendered = tileBuffer && tileBuffer->isRendered(tileRect.second, currentScale);
            bool paintCheckered = !isTileCorrespondingToBuffer || !rendered;

            if (paintCheckered) {
                Platform::IntRect dirtyRectT = transformation.mapRect(dirtyRect);

                if (!transformation.isIdentity()) {
                    // Because of rounding it is possible that dirtyRect could be off-by-one larger
                    // than the surface size of the dst buffer. We prevent this here, by clamping
                    // it to ensure that can't happen.
                    dirtyRectT.intersect(Platform::IntRect(Platform::IntPoint(0, 0), surfaceSize()));
                }
                const Platform::IntPoint contentsOrigin(dirtyRect.x() + origin.x(), dirtyRect.y() + origin.y());
#if DEBUG_CHECKERBOARD
                blitCheckered = true;
#endif
                fillWindow(BlackBerry::Platform::Graphics::CheckerboardPattern,
                    dirtyRectT, contentsOrigin, transformation.a());
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
                    TileBuffer* visibleTileBuffer = SurfacePool::globalSurfacePool()->visibleTileBuffer();
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
                        dirtyRectT.intersect(Platform::IntRect(Platform::IntPoint(0, 0), surfaceSize()));
                    }

                    blitToWindow(dirtyRectT, visibleTileBuffer->nativeBuffer(), dirtyTileRect, BlackBerry::Platform::Graphics::SourceCopy, 255);
                }
            } else if (isTileCorrespondingToBuffer) {
                // Intersect the rendered region.
                Platform::IntRectRegion renderedRegion = tileBuffer->renderedRegion();
                IntRectList dirtyRenderedRects = renderedRegion.rects();
                for (size_t j = 0; j < dirtyRenderedRects.size(); ++j) {
                    Platform::IntRect dirtyRenderedRect = intersection(tileRect.second, dirtyRenderedRects.at(j));
                    if (dirtyRenderedRect.isEmpty())
                        continue;
                    // Blit the rendered parts.
                    blitTileRect(tileBuffer, dirtyRenderedRect, origin, transformation, currentState);
                }
                blittedTiles.append(tileBuffer);
            }
        }
    }

    // TODO: This code is only needed for EGLImage code path, but preferrably BackingStore
    // should not know that, and the synchronization should be in BlackBerry::Platform::Graphics
    // if possible.
    if (isOpenGLCompositing())
        SurfacePool::globalSurfacePool()->notifyBuffersComposited(blittedTiles);

#if USE(ACCELERATED_COMPOSITING)
    if (WebPageCompositorPrivate* compositor = m_webPage->d->compositor()) {
        compositor->drawLayers(dstRect, documentSrcRect);
        if (compositor->drawsRootLayer())
            paintDefaultBackground(dstRect, viewportAccessor, false /*flush*/);
    }
#endif

#if ENABLE_SCROLLBARS
    if (isScrollingOrZooming() && m_client->isMainFrame()) {
        blitHorizontalScrollbar();
        blitVerticalScrollbar();
    }
#endif

#if DEBUG_VISUALIZE
    if (debugViewportAccessor) {
        Platform::Graphics::Buffer* targetBuffer = buffer();
        Platform::IntRect wkViewport = debugViewportAccessor->roundToPixelFromDocumentContents(Platform::IntRect(m_client->visibleContentsRect()));
        Platform::IntRect uiViewport = debugViewportAccessor->roundToPixelFromDocumentContents(documentViewportRect);
        wkViewport.move(-pixelSrcRect.x(), -pixelSrcRect.y());
        uiViewport.move(-pixelSrcRect.x(), -pixelSrcRect.y());

        // Draw a blue rect for the webkit thread viewport.
        Platform::Graphics::clearBuffer(targetBuffer, Platform::IntRect(wkViewport.x(), wkViewport.y(), wkViewport.width(), 1), 0, 0, 255, 255);
        Platform::Graphics::clearBuffer(targetBuffer, Platform::IntRect(wkViewport.x(), wkViewport.y(), 1, wkViewport.height()), 0, 0, 255, 255);
        Platform::Graphics::clearBuffer(targetBuffer, Platform::IntRect(wkViewport.x(), wkViewport.bottom() - 1, wkViewport.width(), 1), 0, 0, 255, 255);
        Platform::Graphics::clearBuffer(targetBuffer, Platform::IntRect(wkViewport.right() - 1, wkViewport.y(), 1, wkViewport.height()), 0, 0, 255, 255);

        // Draw a red rect for the ui thread viewport.
        Platform::Graphics::clearBuffer(targetBuffer, Platform::IntRect(uiViewport.x(), uiViewport.y(), uiViewport.width(), 1), 255, 0, 0, 255);
        Platform::Graphics::clearBuffer(targetBuffer, Platform::IntRect(uiViewport.x(), uiViewport.y(), 1, uiViewport.height()), 255, 0, 0, 255);
        Platform::Graphics::clearBuffer(targetBuffer, Platform::IntRect(uiViewport.x(), uiViewport.bottom() - 1, uiViewport.width(), 1), 255, 0, 0, 255);
        Platform::Graphics::clearBuffer(targetBuffer, Platform::IntRect(uiViewport.right() - 1, uiViewport.y(), 1, uiViewport.height()), 255, 0, 0, 255);

        delete debugViewportAccessor;
    }
#endif

#if DEBUG_CHECKERBOARD
    static double lastCheckeredTime = 0;

    if (blitCheckered && !lastCheckeredTime) {
        lastCheckeredTime = WTF::currentTime();
        Platform::logAlways(Platform::LogLevelCritical,
            "Blitting checkered pattern at %f", lastCheckeredTime);
    } else if (blitCheckered && lastCheckeredTime) {
        Platform::logAlways(Platform::LogLevelCritical,
            "Blitting checkered pattern at %f", WTF::currentTime());
    } else if (!blitCheckered && lastCheckeredTime) {
        double time = WTF::currentTime();
        Platform::logAlways(Platform::LogLevelCritical,
            "Blitting over checkered pattern at %f took %f", time, time - lastCheckeredTime);
        lastCheckeredTime = 0;
    }
#endif

    invalidateWindow(dstRect);
}

#if USE(ACCELERATED_COMPOSITING)
void BackingStorePrivate::compositeContents(WebCore::LayerRenderer* layerRenderer, const WebCore::TransformationMatrix& transform, const WebCore::FloatRect& contents, bool contentsOpaque)
{
    const Platform::IntRect transformedContentsRect = Platform::IntRect(Platform::IntPoint(0, 0), m_client->transformedContentsSize());
    Platform::IntRect transformedContents = enclosingIntRect(m_webPage->d->m_transformationMatrix->mapRect(contents));
    transformedContents.intersect(transformedContentsRect);
    if (transformedContents.isEmpty())
        return;

    if (!isActive())
        return;

    if (m_webPage->d->compositorDrawsRootLayer())
        return;

    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();
    Vector<TileBuffer*> compositedTiles;

    Platform::IntRectRegion transformedContentsRegion = transformedContents;
    Platform::IntRectRegion backingStoreRegion = currentState->backingStoreRect();
    Platform::IntRectRegion checkeredRegion
        = Platform::IntRectRegion::subtractRegions(transformedContentsRegion, backingStoreRegion);

    // Blit checkered to those parts that are not covered by the backingStoreRect.
    IntRectList checkeredRects = checkeredRegion.rects();
    for (size_t i = 0; i < checkeredRects.size(); ++i)
        layerRenderer->drawCheckerboardPattern(transform, m_webPage->d->mapFromTransformedFloatRect(WebCore::IntRect(checkeredRects.at(i))));

    // Get the list of tile rects that makeup the content.
    TileRectList tileRectList = mapFromPixelContentsToTiles(transformedContents, currentState);
    for (size_t i = 0; i < tileRectList.size(); ++i) {
        TileRect tileRect = tileRectList[i];
        TileIndex index = tileRect.first;
        Platform::IntRect dirtyRect = tileRect.second;
        TileBuffer* tileBuffer = currentMap.get(index);

        if (!tileBuffer || !currentState->isTileCorrespondingToBuffer(index, tileBuffer))
            layerRenderer->drawCheckerboardPattern(transform, m_webPage->d->mapFromTransformedFloatRect(Platform::FloatRect(dirtyRect)));
        else {
            Platform::IntPoint tileOrigin = tileBuffer->lastRenderOrigin();
            Platform::FloatRect tileDocumentContentsRect = m_webPage->d->mapFromTransformedFloatRect(Platform::FloatRect(tileBuffer->pixelContentsRect()));

            layerRenderer->compositeBuffer(transform, tileDocumentContentsRect, tileBuffer->nativeBuffer(), contentsOpaque, 1.0f);
            compositedTiles.append(tileBuffer);

            // Intersect the rendered region and clear unrendered parts.
            Platform::IntRectRegion notRenderedRegion = Platform::IntRectRegion::subtractRegions(dirtyRect, tileBuffer->renderedRegion());
            IntRectList notRenderedRects = notRenderedRegion.rects();
            for (size_t i = 0; i < notRenderedRects.size(); ++i) {
                Platform::IntRect tileSurfaceRect = notRenderedRects.at(i);
                tileSurfaceRect.move(-tileOrigin.x(), -tileOrigin.y());
                layerRenderer->drawCheckerboardPattern(transform, m_webPage->d->mapFromTransformedFloatRect(Platform::FloatRect(tileSurfaceRect)));
            }
        }
    }

    SurfacePool::globalSurfacePool()->notifyBuffersComposited(compositedTiles);
}
#endif

Platform::IntRect BackingStorePrivate::blitTileRect(TileBuffer* tileBuffer,
    const Platform::IntRect& tilePixelContentsRect,
    const Platform::IntPoint& origin,
    const WebCore::TransformationMatrix& matrix,
    BackingStoreGeometry* geometry)
{
    if (!m_webPage->isVisible() || !isActive() || !tileBuffer)
        return Platform::IntRect();

    Platform::IntRect srcRect = tilePixelContentsRect;
    Platform::IntPoint tileOrigin = tileBuffer->lastRenderOrigin();
    srcRect.move(-tileOrigin.x(), -tileOrigin.y());

    // Now, this dirty rect is in transformed coordinates relative to the
    // transformed contents, but ultimately it needs to be transformed
    // coordinates relative to the viewport.
    Platform::IntRect dstRect = tilePixelContentsRect;
    dstRect.move(-origin.x(), -origin.y());
    dstRect = matrix.mapRect(dstRect);

    if (!matrix.isIdentity()) {
        // Because of rounding it is possible that dstRect could be off-by-one larger
        // than the surface size of the dst buffer. We prevent this here, by clamping
        // it to ensure that can't happen.
        dstRect.intersect(Platform::IntRect(Platform::IntPoint(0, 0), surfaceSize()));
    }

    ASSERT(!dstRect.isEmpty());
    ASSERT(!srcRect.isEmpty());
    if (dstRect.isEmpty() || srcRect.isEmpty())
        return Platform::IntRect();

    blitToWindow(dstRect, tileBuffer->nativeBuffer(), srcRect, BlackBerry::Platform::Graphics::SourceCopy, 255);
    return dstRect;
}

void BackingStorePrivate::blitHorizontalScrollbar()
{
    if (!m_webPage->isVisible())
        return;

    m_webPage->client()->drawHorizontalScrollbar();
}

void BackingStorePrivate::blitVerticalScrollbar()
{
    if (!m_webPage->isVisible())
        return;

    m_webPage->client()->drawVerticalScrollbar();
}

bool BackingStorePrivate::isTileVisible(const TileIndex& index, BackingStoreGeometry* geometry) const
{
    return isTileVisible(geometry->originOfTile(index));
}

bool BackingStorePrivate::isTileVisible(const Platform::IntPoint& origin) const
{
    return Platform::IntRect(origin, tileSize()).intersects(visibleContentsRect());
}

Platform::IntRect BackingStorePrivate::visibleTilesRect(BackingStoreGeometry* geometry) const
{
    TileMap tileMap = geometry->tileMap();

    Platform::IntRect rect;
    TileMap::const_iterator end = tileMap.end();
    for (TileMap::const_iterator it = tileMap.begin(); it != end; ++it) {
        Platform::IntRect tilePixelContentsRect(geometry->originOfTile(it->key), tileSize());
        if (tilePixelContentsRect.intersects(visibleContentsRect()))
            rect = Platform::unionOfRects(rect, tilePixelContentsRect);
    }
    return rect;
}

Platform::IntRect BackingStorePrivate::tileVisibleContentsRect(const TileIndex& index, BackingStoreGeometry* geometry) const
{
    if (!isTileVisible(index, geometry))
        return Platform::IntRect();

    return tileContentsRect(index, visibleContentsRect(), geometry);
}

Platform::IntRect BackingStorePrivate::tileContentsRect(const TileIndex& index, const Platform::IntRect& pixelContentsRect, BackingStoreGeometry* state) const
{
    TileRectList tileRectList = mapFromPixelContentsToTiles(pixelContentsRect, state);
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

void BackingStorePrivate::resetTiles()
{
    BackingStoreGeometry* currentState = frontState();

    m_renderQueue->clear(currentState->backingStoreRect(), true /*clearRegularRenderJobs*/);

    BackingStoreGeometry* newGeometry = new BackingStoreGeometry;
    newGeometry->setScale(currentState->scale());
    newGeometry->setNumberOfTilesWide(currentState->numberOfTilesWide());
    newGeometry->setNumberOfTilesHigh(currentState->numberOfTilesHigh());
    newGeometry->setBackingStoreOffset(currentState->backingStoreOffset());

    TileMap currentMap = currentState->tileMap();
    TileMap newTileMap;

    TileMap::const_iterator end = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != end; ++it)
        newTileMap.add(it->key, 0); // clear all buffer info from the tile

    newGeometry->setTileMap(newTileMap);
    adoptAsFrontState(newGeometry); // swap into UI thread
}

void BackingStorePrivate::updateTiles(bool updateVisible, bool immediate)
{
    if (!isActive())
        return;

    BackingStoreGeometry* currentState = frontState();
    TileMap currentMap = currentState->tileMap();

    TileMap::const_iterator end = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != end; ++it) {
        bool isVisible = isTileVisible(it->key, currentState);
        if (!updateVisible && isVisible)
            continue;
        updateTile(currentState->originOfTile(it->key), immediate);
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

    bool isLoading = m_client->loadState() == WebPagePrivate::Committed;
    bool forceVisible = checkLoading && isLoading;

    TileMap::const_iterator end = currentMap.end();
    for (TileMap::const_iterator it = currentMap.begin(); it != end; ++it) {
        TileIndex index = it->key;
        TileBuffer* tileBuffer = it->value;
        bool isVisible = isTileVisible(index, currentState);
        Platform::IntPoint tileOrigin = currentState->originOfTile(index);
        // The rect in transformed contents coordinates.
        Platform::IntRect rect(tileOrigin, tileSize());
        if (currentState->isTileCorrespondingToBuffer(index, tileBuffer)
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
                                                      tileBuffer,
                                                      tileNotRenderedRegion,
                                                      currentState,
                                                      false /*update*/);
#if DEBUG_BACKINGSTORE
                Platform::logAlways(Platform::LogLevelCritical,
                    "BackingStorePrivate::updateTilesForScroll did clear tile %s",
                    tileNotRenderedRegion.extents().toString().c_str());
#endif
            }
            updateTile(tileOrigin, false /*immediate*/);
        } else if (isVisible
            && (forceVisible || !tileBuffer || !tileBuffer->isRendered(tileVisibleContentsRect(index, currentState), currentState->scale()))
            && !isCurrentVisibleJob(index, tileBuffer, currentState))
            updateTile(tileOrigin, false /*immediate*/);
    }
}

void BackingStorePrivate::updateTile(const Platform::IntPoint& origin, bool immediate)
{
    if (!isActive())
        return;

    Platform::IntRect updateRect = Platform::IntRect(origin, tileSize());
    RenderQueue::JobType jobType = isTileVisible(origin) ? RenderQueue::VisibleScroll : RenderQueue::NonVisibleScroll;
    if (immediate)
        render(updateRect);
    else
        m_renderQueue->addToQueue(jobType, updateRect);
}

BackingStorePrivate::TileRectList BackingStorePrivate::mapFromPixelContentsToTiles(const Platform::IntRect& rect, BackingStoreGeometry* geometry) const
{
    TileMap tileMap = geometry->tileMap();

    TileRectList tileRectList;
    TileMap::const_iterator end = tileMap.end();
    for (TileMap::const_iterator it = tileMap.begin(); it != end; ++it) {
        TileIndex index = it->key;

        // Need to map the rect to tile coordinates.
        Platform::IntRect r = rect;

        // Do we intersect the current tile or no?
        r.intersect(Platform::IntRect(geometry->originOfTile(index), tileSize()));
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
    ASSERT(BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread());

    if (!m_tileMatrixNeedsUpdate)
        return;

    m_tileMatrixNeedsUpdate = false;

    // This will update the tile matrix.
    scrollBackingStore(0, 0);
}

void BackingStorePrivate::contentsSizeChanged(const Platform::IntSize&)
{
    setTileMatrixNeedsUpdate();
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
        TileRectList tileRectList = mapFromPixelContentsToTiles(visibleContentsRect(), currentState);
        for (size_t i = 0; i < tileRectList.size(); ++i) {
            TileRect tileRect = tileRectList[i];
            TileIndex index = tileRect.first;
            Platform::IntRect dirtyRect = tileRect.second;
            TileBuffer* tileBuffer = currentMap.get(index);

            // Invalidate the whole rect.
            Platform::IntRect wholeRect(currentState->originOfTile(index), tileSize());
            m_renderQueue->addToQueue(RenderQueue::VisibleZoom, wholeRect);

            if (!tileBuffer)
                continue;

            // Copy the visible contents into the visibleTileBuffer if we don't have
            // any current visible zoom jobs.
            if (!hasCurrentVisibleZoomJob) {
                Platform::IntRect tileSrcRect = dirtyRect;
                Platform::IntPoint tileOrigin = currentState->originOfTile(index);
                tileSrcRect.move(-tileOrigin.x(), -tileOrigin.y());

                // Map to the destination's coordinate system.
                Platform::IntPoint visibleTileOrigin = m_visibleTileBufferRect.location();
                Platform::IntRect visibleTileDstRect = dirtyRect;
                visibleTileDstRect.move(-visibleTileOrigin.x(), -visibleTileOrigin.y());

                TileBuffer* visibleTileBuffer = SurfacePool::globalSurfacePool()->visibleTileBuffer();
                ASSERT(visibleTileBuffer->size() == Platform::IntSize(m_webPage->d->transformedViewportSize()));
                BlackBerry::Platform::Graphics::blitToBuffer(
                    visibleTileBuffer->nativeBuffer(), visibleTileDstRect,
                    tileBuffer->nativeBuffer(), tileSrcRect);
            }
        }
    }

    m_renderQueue->reset();
    resetTiles();
}

void BackingStorePrivate::orientationChanged()
{
    ASSERT(BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread());
    setTileMatrixNeedsUpdate();
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

    // Don't try to blit to screen unless we have a buffer.
    if (!buffer())
        suspendScreenUpdates();

    SurfacePool* surfacePool = SurfacePool::globalSurfacePool();
    surfacePool->initialize(tileSize());

    if (surfacePool->isEmpty()) // Settings specify 0 tiles / no backing store.
        return;

    const Divisor divisor = bestDivisor(expandedContentsSize(), tileWidth(), tileHeight(), minimumNumberOfTilesWide(), minimumNumberOfTilesHigh(), m_preferredTileMatrixDimension);

    int numberOfTilesWide = divisor.first;
    int numberOfTilesHigh = divisor.second;

    TileMap newTileMap;
    for (int y = 0; y < numberOfTilesHigh; ++y) {
        for (int x = 0; x < numberOfTilesWide; ++x) {
            TileIndex index(x, y);
            newTileMap.add(index, 0); // no buffers initially assigned.
        }
    }

    // Set the initial state of the backingstore geometry.
    BackingStoreGeometry* newGeometry = new BackingStoreGeometry;
    newGeometry->setScale(m_webPage->d->currentScale());
    newGeometry->setNumberOfTilesWide(divisor.first);
    newGeometry->setNumberOfTilesHigh(divisor.second);
    newGeometry->setTileMap(newTileMap);
    adoptAsFrontState(newGeometry); // swap into UI thread

    createVisibleTileBufferForWebPage(m_webPage->d);
}

void BackingStorePrivate::createVisibleTileBuffer()
{
    if (!m_webPage->isVisible() || !isActive())
        return;

    createVisibleTileBufferForWebPage(m_webPage->d);
}

Platform::IntPoint BackingStoreGeometry::originOfTile(const TileIndex& index) const
{
    return Platform::IntPoint(backingStoreRect().x() + (index.i() * BackingStorePrivate::tileWidth()),
                              backingStoreRect().y() + (index.j() * BackingStorePrivate::tileHeight()));
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
    return tileSize().width();
}

int BackingStorePrivate::tileHeight()
{
    return tileSize().height();
}

Platform::IntSize BackingStorePrivate::tileSize()
{
    static Platform::IntSize tileSize = BlackBerry::Platform::Settings::instance()->tileSize();
    return tileSize;
}

void BackingStorePrivate::renderContents(Platform::Graphics::Drawable* drawable,
                                         const Platform::IntRect& contentsRect,
                                         const Platform::IntSize& destinationSize) const
{
    if (!drawable || contentsRect.isEmpty())
        return;

    requestLayoutIfNeeded();

    PlatformGraphicsContext* platformGraphicsContext = SurfacePool::globalSurfacePool()->createPlatformGraphicsContext(drawable);
    GraphicsContext graphicsContext(platformGraphicsContext);

    graphicsContext.translate(-contentsRect.x(), -contentsRect.y());

    WebCore::IntRect transformedContentsRect(contentsRect.x(), contentsRect.y(), contentsRect.width(), contentsRect.height());

    float widthScale = static_cast<float>(destinationSize.width()) / contentsRect.width();
    float heightScale = static_cast<float>(destinationSize.height()) / contentsRect.height();

    if (widthScale != 1.0 && heightScale != 1.0) {
        TransformationMatrix matrix;
        matrix.scaleNonUniform(1.0 / widthScale, 1.0 / heightScale);
        transformedContentsRect = matrix.mapRect(transformedContentsRect);

        // We extract from the contentsRect but draw a slightly larger region than
        // we were told to, in order to avoid pixels being rendered only partially.
        const int atLeastOneDevicePixel = static_cast<int>(ceilf(std::max(1.0 / widthScale, 1.0 / heightScale)));
        transformedContentsRect.inflate(atLeastOneDevicePixel);
        graphicsContext.scale(FloatSize(widthScale, heightScale));
    }

    graphicsContext.clip(transformedContentsRect);
    m_client->frame()->view()->paintContents(&graphicsContext, transformedContentsRect);

    delete platformGraphicsContext;
}

void BackingStorePrivate::renderContents(BlackBerry::Platform::Graphics::Buffer* tileBuffer,
                                         const Platform::IntPoint& surfaceOffset,
                                         const Platform::IntRect& contentsRect) const
{
    // If tileBuffer == 0, we render directly to the window.
    if (!m_webPage->isVisible() && tileBuffer)
        return;

#if DEBUG_BACKINGSTORE
    Platform::logAlways(Platform::LogLevelCritical,
        "BackingStorePrivate::renderContents tileBuffer=0x%p surfaceOffset=%s contentsRect=%s",
        tileBuffer, surfaceOffset.toString().c_str(), contentsRect.toString().c_str());
#endif

    // It is up to callers of this method to perform layout themselves!
    ASSERT(!m_webPage->d->mainFrame()->view()->needsLayout());

    Platform::IntSize contentsSize = m_client->contentsSize();
    Color backgroundColor(m_webPage->settings()->backgroundColor());

    BlackBerry::Platform::Graphics::Buffer* targetBuffer = tileBuffer
        ? tileBuffer
        : buffer();

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

        graphicsContext.restore();
    }

    // Grab the requested region from the drawing surface into the tile image.

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
    const Platform::Graphics::Buffer* srcBuffer,
    const Platform::IntRect& srcRect,
    Platform::Graphics::BlendMode blendMode,
    unsigned char globalAlpha)
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());

    windowFrontBufferState()->clearBlittedRegion(dstRect);
    windowBackBufferState()->addBlittedRegion(dstRect);

    BlackBerry::Platform::Graphics::Buffer* dstBuffer = buffer();
    ASSERT(dstBuffer);
    ASSERT(srcBuffer);
    if (!dstBuffer)
        Platform::logAlways(Platform::LogLevelWarn, "Empty window buffer, couldn't blitToWindow");

    BlackBerry::Platform::Graphics::blitToBuffer(dstBuffer, dstRect, srcBuffer, srcRect, blendMode, globalAlpha);

#if DEBUG_FAT_FINGERS
    drawDebugRect(dstBuffer, dstRect, FatFingers::m_debugFatFingerRect, 210, 210, 250);
    drawDebugRect(dstBuffer, dstRect, Platform::IntRect(FatFingers::m_debugFatFingerClickPosition, Platform::IntSize(3, 3)), 0, 0, 0);
    drawDebugRect(dstBuffer, dstRect, Platform::IntRect(FatFingers::m_debugFatFingerAdjustedPosition, Platform::IntSize(5, 5)), 100, 100, 100);
#endif

}

void BackingStorePrivate::fillWindow(Platform::Graphics::FillPattern pattern,
                                     const Platform::IntRect& dstRect,
                                     const Platform::IntPoint& contentsOrigin,
                                     double contentsScale)
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());

    windowFrontBufferState()->clearBlittedRegion(dstRect);
    windowBackBufferState()->addBlittedRegion(dstRect);

    BlackBerry::Platform::Graphics::Buffer* dstBuffer = buffer();
    ASSERT(dstBuffer);
    if (!dstBuffer)
        Platform::logAlways(Platform::LogLevelWarn, "Empty window buffer, couldn't fillWindow");

    if (pattern == BlackBerry::Platform::Graphics::CheckerboardPattern && BlackBerry::Platform::Settings::isPublicBuild()) {
        // For public builds, convey the impression of less checkerboard.
        // For developer builds, keep the checkerboard to get it fixed better.
        BlackBerry::Platform::Graphics::clearBuffer(dstBuffer, dstRect,
            m_webPageBackgroundColor.red(), m_webPageBackgroundColor.green(),
            m_webPageBackgroundColor.blue(), m_webPageBackgroundColor.alpha());
        return;
    }

    BlackBerry::Platform::Graphics::fillBuffer(dstBuffer, pattern, dstRect, contentsOrigin, contentsScale);
}

WebCore::Color BackingStorePrivate::webPageBackgroundColorUserInterfaceThread() const
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());
    return m_webPageBackgroundColor;
}

void BackingStorePrivate::setWebPageBackgroundColor(const WebCore::Color& color)
{
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        typedef void (BlackBerry::WebKit::BackingStorePrivate::*FunctionType)(const WebCore::Color&);

        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage<FunctionType, BackingStorePrivate, WebCore::Color>(
                &BackingStorePrivate::setWebPageBackgroundColor, this, color));
        return;
    }

    m_webPageBackgroundColor = color;
}

void BackingStorePrivate::invalidateWindow()
{
    // Grab a rect appropriate for the current thread.
    if (BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        if (m_webPage->client()->userInterfaceViewportAccessor())
            invalidateWindow(m_webPage->client()->userInterfaceViewportAccessor()->destinationSurfaceRect());
    } else
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
    Platform::logAlways(Platform::LogLevelCritical,
        "BackingStorePrivate::invalidateWindow dst = %s",
        dst.toString().c_str());
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
    Platform::logAlways(Platform::LogLevelCritical,
        "BackingStorePrivate::invalidateWindow posting = %s",
        dstRect.toString().c_str());
#endif

    m_currentWindowBackBuffer = (m_currentWindowBackBuffer + 1) % 2;
    if (Window* window = m_webPage->client()->window())
        window->post(dstRect);
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

    BlackBerry::Platform::Graphics::Buffer* dstBuffer = buffer();
    ASSERT(dstBuffer);
    if (!dstBuffer)
        Platform::logAlways(Platform::LogLevelWarn, "Empty window buffer, couldn't clearWindow");

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

    m_webPage->d->m_mainFrame->view()->setConstrainsScrollingToContentEdge(!scrollingOrZooming);

    // Clear this flag since we don't care if the render queue is under pressure
    // or not since we are scrolling and it is more important to not lag than
    // it is to ensure animations achieve better framerates!
    if (scrollingOrZooming)
        m_renderQueue->setCurrentRegularRenderJobBatchUnderPressure(false);
#if ENABLE_SCROLLBARS
    else if (shouldBlit && !shouldDirectRenderingToWindow())
        blitVisibleContents();
#endif
    if (!scrollingOrZooming && shouldPerformRegularRenderJobs())
        dispatchRenderJob();
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

void BackingStorePrivate::adoptAsFrontState(BackingStoreGeometry* newFrontState)
{
    // Remember the buffers we'll use in the new front state for comparison.
    WTF::Vector<TileBuffer*> newTileBuffers;
    TileMap newTileMap = newFrontState->tileMap();
    TileMap::const_iterator end = newTileMap.end();
    for (TileMap::const_iterator it = newTileMap.begin(); it != end; ++it) {
        if (it->value)
            newTileBuffers.append(it->value);
    }

    unsigned newFront = reinterpret_cast<unsigned>(newFrontState);
    BackingStoreGeometry* oldFrontState = frontState();

    // Atomic change.
    _smp_xchg(&m_frontState, newFront);

    // Wait until the user interface thread won't access the old front state anymore.
    BlackBerry::Platform::userInterfaceThreadMessageClient()->syncToCurrentMessage();

    // Reclaim unused old tile buffers as back buffers.
    TileMap oldTileMap = oldFrontState->tileMap();
    end = oldTileMap.end();
    for (TileMap::const_iterator it = oldTileMap.begin(); it != end; ++it) {
        TileBuffer* tileBuffer = it->value;
        if (tileBuffer && !newTileBuffers.contains(tileBuffer))
            SurfacePool::globalSurfacePool()->addBackBuffer(tileBuffer);
    }

    delete oldFrontState;
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
void BackingStorePrivate::drawAndBlendLayersForDirectRendering(const Platform::IntRect& dirtyRect)
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        return;

    // Because we're being called sync from the WebKit thread, we can use
    // regular WebPage size and transformation functions without concerns.
    WebCore::IntRect contentsRect = visibleContentsRect();
    WebCore::FloatRect untransformedContentsRect = m_webPage->d->mapFromTransformedFloatRect(WebCore::FloatRect(contentsRect));
    WebCore::IntRect contentsScreenRect = m_client->mapFromTransformedContentsToTransformedViewport(contentsRect);
    WebCore::IntRect dstRect = intersection(contentsScreenRect,
        WebCore::IntRect(WebCore::IntPoint(0, 0), m_webPage->d->transformedViewportSize()));

    // Check if rendering caused a commit and we need to redraw the layers.
    if (WebPageCompositorPrivate* compositor = m_webPage->d->compositor())
        compositor->drawLayers(dstRect, untransformedContentsRect);
}
#endif

// static
void BackingStorePrivate::setCurrentBackingStoreOwner(WebPage* webPage)
{
    // Let the previously active backingstore release its tile buffers so
    // the new one (e.g. another tab) can use the buffers to render contents.
    if (BackingStorePrivate::s_currentBackingStoreOwner && BackingStorePrivate::s_currentBackingStoreOwner != webPage)
        BackingStorePrivate::s_currentBackingStoreOwner->d->m_backingStore->d->resetTiles();

    BackingStorePrivate::s_currentBackingStoreOwner = webPage;
}

bool BackingStorePrivate::isActive() const
{
    return BackingStorePrivate::s_currentBackingStoreOwner == m_webPage && SurfacePool::globalSurfacePool()->isActive();
}

void BackingStorePrivate::didRenderContent(const Platform::IntRect& renderedRect)
{
    if (isScrollingOrZooming())
        return;

    if (!shouldDirectRenderingToWindow()) {
        if (!m_webPage->d->needsOneShotDrawingSynchronization())
            blitVisibleContents();
    } else
        invalidateWindow();

    m_webPage->client()->notifyPixelContentRendered(renderedRect);
}

BackingStore::BackingStore(WebPage* webPage, BackingStoreClient* client)
    : d(new BackingStorePrivate)
{
    d->m_webPage = webPage;
    d->m_client = client;
}

BackingStore::~BackingStore()
{
    deleteGuardedObject(d);
    d = 0;
}

void BackingStore::createSurface()
{
    static bool initialized = false;
    if (!initialized) {
        BlackBerry::Platform::Graphics::initialize();
        initialized = true;
    }

    // Triggers creation of surfaces in backingstore.
    d->createSurfaces();

    // Focusing the WebPage triggers a repaint, so while we want it to be
    // focused initially this has to happen after creation of the surface.
    d->m_webPage->setFocused(true);
}

void BackingStore::suspendBackingStoreUpdates()
{
    d->suspendBackingStoreUpdates();
}

void BackingStore::resumeBackingStoreUpdates()
{
    d->resumeBackingStoreUpdates();
}

void BackingStore::suspendScreenUpdates()
{
    d->suspendScreenUpdates();
}

void BackingStore::resumeScreenUpdates(ResumeUpdateOperation op)
{
    d->resumeScreenUpdates(op);
}

bool BackingStore::isScrollingOrZooming() const
{
    return d->isScrollingOrZooming();
}

void BackingStore::setScrollingOrZooming(bool scrollingOrZooming)
{
    d->setScrollingOrZooming(scrollingOrZooming);
}

void BackingStore::blitVisibleContents()
{
    d->blitVisibleContents(false /*force*/);
}

void BackingStore::repaint(int x, int y, int width, int height,
                           bool contentChanged, bool immediate)
{
    d->repaint(Platform::IntRect(x, y, width, height), contentChanged, immediate);
}

bool BackingStore::isDirectRenderingToWindow() const
{
    BackingStoreMutexLocker locker(d);
    return d->shouldDirectRenderingToWindow();
}

void BackingStore::createBackingStoreMemory()
{
    if (BackingStorePrivate::s_currentBackingStoreOwner == d->m_webPage)
        SurfacePool::globalSurfacePool()->createBuffers();
    resumeBackingStoreUpdates();
    resumeScreenUpdates(BackingStore::RenderAndBlit);
}

void BackingStore::releaseBackingStoreMemory()
{
    suspendBackingStoreUpdates();
    suspendScreenUpdates();
    if (BackingStorePrivate::s_currentBackingStoreOwner == d->m_webPage)
        SurfacePool::globalSurfacePool()->releaseBuffers();
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
#if USE(ACCELERATED_COMPOSITING)
    // If there's a WebPageCompositorClient, let it schedule the blit.
    WebPageCompositorPrivate* compositor = d->m_webPage->d->compositor();
    if (compositor && compositor->client())
        return false;
#endif

    // Normally, this would be called from the compositing thread,
    // and the flag is set on the compositing thread, so no need for
    // synchronization.
    return d->m_hasBlitJobs;
}

void BackingStore::blitOnIdle()
{
#if USE(ACCELERATED_COMPOSITING)
    // If there's a WebPageCompositorClient, let it schedule the blit.
    WebPageCompositorPrivate* compositor = d->m_webPage->d->compositor();
    if (compositor && compositor->client())
        return;
#endif

    d->blitVisibleContents(true /*force*/);
}

Platform::IntSize BackingStorePrivate::surfaceSize() const
{
    if (Window* window = m_webPage->client()->window())
        return window->surfaceSize();

#if USE(ACCELERATED_COMPOSITING)
    if (WebPageCompositorPrivate* compositor = m_webPage->d->compositor())
        return compositor->context()->surfaceSize();
#endif

    return Platform::IntSize();
}

Platform::Graphics::Buffer* BackingStorePrivate::buffer() const
{
    if (Window* window = m_webPage->client()->window())
        return window->buffer();

#if USE(ACCELERATED_COMPOSITING)
    if (WebPageCompositorPrivate* compositor = m_webPage->d->compositor())
        return compositor->context() ? compositor->context()->buffer() : 0;
#endif

    return 0;
}

void BackingStore::drawContents(Platform::Graphics::Drawable* drawable, const Platform::IntRect& contentsRect, const Platform::IntSize& destinationSize)
{
    d->renderContents(drawable, contentsRect, destinationSize);
}

}
}
