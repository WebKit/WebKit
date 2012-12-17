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
#include "RenderQueue.h"

#include "BackingStore_p.h"
#include "WebPageClient.h"
#include "WebPage_p.h"

#define DEBUG_RENDER_QUEUE 0
#define DEBUG_RENDER_QUEUE_SORT 0

#if DEBUG_RENDER_QUEUE
#include <BlackBerryPlatformLog.h>
#include <wtf/CurrentTime.h>
#endif

namespace BlackBerry {
namespace WebKit {

template<SortDirection sortDirection>
static inline int compareRectOneDirection(const Platform::IntRect& r1, const Platform::IntRect& r2)
{
    switch (sortDirection) {
    case LeftToRight:
        return r1.x() - r2.x();
    case RightToLeft:
        return r2.x() - r1.x();
    case TopToBottom:
        return r1.y() - r2.y();
    case BottomToTop:
        return r2.y() - r1.y();
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

template<SortDirection primarySortDirection, SortDirection secondarySortDirection>
static bool rectIsLessThan(const Platform::IntRect& r1, const Platform::IntRect& r2)
{
    int primaryResult = compareRectOneDirection<primarySortDirection>(r1, r2);
    if (primaryResult || secondarySortDirection == primarySortDirection)
        return primaryResult < 0;
    return compareRectOneDirection<secondarySortDirection>(r1, r2) < 0;
}

typedef bool (*FuncRectLessThan)(const Platform::IntRect& r1, const Platform::IntRect& r2);
static FuncRectLessThan rectLessThanFunction(SortDirection primary, SortDirection secondary)
{
    static FuncRectLessThan s_rectLessThanFunctions[NumSortDirections][NumSortDirections] = { { 0 } };
    static bool s_initialized = false;
    if (!s_initialized) {
#define ADD_COMPARE_FUNCTION(_primary, _secondary) \
        s_rectLessThanFunctions[_primary][_secondary] = rectIsLessThan<_primary, _secondary>

        ADD_COMPARE_FUNCTION(LeftToRight, LeftToRight);
        ADD_COMPARE_FUNCTION(LeftToRight, RightToLeft);
        ADD_COMPARE_FUNCTION(LeftToRight, TopToBottom);
        ADD_COMPARE_FUNCTION(LeftToRight, BottomToTop);

        ADD_COMPARE_FUNCTION(RightToLeft, LeftToRight);
        ADD_COMPARE_FUNCTION(RightToLeft, RightToLeft);
        ADD_COMPARE_FUNCTION(RightToLeft, TopToBottom);
        ADD_COMPARE_FUNCTION(RightToLeft, BottomToTop);

        ADD_COMPARE_FUNCTION(TopToBottom, LeftToRight);
        ADD_COMPARE_FUNCTION(TopToBottom, RightToLeft);
        ADD_COMPARE_FUNCTION(TopToBottom, TopToBottom);
        ADD_COMPARE_FUNCTION(TopToBottom, BottomToTop);

        ADD_COMPARE_FUNCTION(BottomToTop, LeftToRight);
        ADD_COMPARE_FUNCTION(BottomToTop, RightToLeft);
        ADD_COMPARE_FUNCTION(BottomToTop, TopToBottom);
        ADD_COMPARE_FUNCTION(BottomToTop, BottomToTop);
#undef ADD_COMPARE_FUNCTION

        s_initialized = true;
    }

    return s_rectLessThanFunctions[primary][secondary];
}

class RectLessThan {
public:
    RectLessThan(SortDirection primarySortDirection, SortDirection secondarySortDirection)
        : m_rectIsLessThan(rectLessThanFunction(primarySortDirection, secondarySortDirection))
    {
    }

    bool operator()(const Platform::IntRect& r1, const Platform::IntRect& r2)
    {
        return m_rectIsLessThan(r1, r2);
    }

private:
    FuncRectLessThan m_rectIsLessThan;
};

class RenderRectLessThan {
public:
    RenderRectLessThan(SortDirection primarySortDirection, SortDirection secondarySortDirection)
        : m_rectIsLessThan(rectLessThanFunction(primarySortDirection, secondarySortDirection))
    {
    }

    bool operator()(const RenderRect& r1, const RenderRect& r2)
    {
        return m_rectIsLessThan(r1.subRects()[0], r2.subRects()[0]);
    }

private:
    FuncRectLessThan m_rectIsLessThan;
};

RenderRect::RenderRect(const Platform::IntPoint& location, const Platform::IntSize& size, int splittingFactor)
    : Platform::IntRect(location, size)
    , m_splittingFactor(0)
    , m_primarySortDirection(TopToBottom)
    , m_secondarySortDirection(LeftToRight)
{
    initialize(splittingFactor);
}

RenderRect::RenderRect(int x, int y, int width, int height, int splittingFactor)
    : Platform::IntRect(x, y, width, height)
    , m_splittingFactor(0)
    , m_primarySortDirection(TopToBottom)
    , m_secondarySortDirection(LeftToRight)
{
    initialize(splittingFactor);
}

void RenderRect::initialize(int splittingFactor)
{
    m_subRects.push_back(*this);
    for (int i = 0; i < splittingFactor; ++i)
        split();
    quickSort();
}

static void splitRectInHalfAndAddToList(const Platform::IntRect& rect, bool vertical, IntRectList& renderRectList)
{
    if (vertical) {
        int width1 = static_cast<int>(ceilf(rect.width() / 2.0));
        int width2 = static_cast<int>(floorf(rect.width() / 2.0));
        renderRectList.push_back(Platform::IntRect(rect.x(), rect.y(), width1, rect.height()));
        renderRectList.push_back(Platform::IntRect(rect.x() + width1, rect.y(), width2, rect.height()));
    } else {
        int height1 = static_cast<int>(ceilf(rect.height() / 2.0));
        int height2 = static_cast<int>(floorf(rect.height() / 2.0));
        renderRectList.push_back(Platform::IntRect(rect.x(), rect.y(), rect.width(), height1));
        renderRectList.push_back(Platform::IntRect(rect.x(), rect.y() + height1, rect.width(), height2));
    }
}

void RenderRect::split()
{
    ++m_splittingFactor;

    bool vertical = !(m_splittingFactor % 2);

    IntRectList subRects;
    for (size_t i = 0; i < m_subRects.size(); ++i)
        splitRectInHalfAndAddToList(m_subRects.at(i), vertical, subRects);
    m_subRects.swap(subRects);
}

Platform::IntRect RenderRect::rectForRendering()
{
    ASSERT(!m_subRects.empty());
    Platform::IntRect rect = m_subRects[0];
    m_subRects.erase(m_subRects.begin());
    return rect;
}

void RenderRect::updateSortDirection(SortDirection primary, SortDirection secondary)
{
    if (primary == m_primarySortDirection && secondary == m_secondarySortDirection)
        return;

    m_primarySortDirection = primary;
    m_secondarySortDirection = secondary;

    quickSort();
}

void RenderRect::quickSort()
{
    std::sort(m_subRects.begin(), m_subRects.begin(), RectLessThan(m_primarySortDirection, m_secondarySortDirection));
}

RenderQueue::RenderQueue(BackingStorePrivate* parent)
    : m_parent(parent)
    , m_rectsAddedToRegularRenderJobsInCurrentCycle(false)
    , m_currentRegularRenderJobsBatchUnderPressure(false)
    , m_primarySortDirection(TopToBottom)
    , m_secondarySortDirection(LeftToRight)
{
}

void RenderQueue::reset()
{
    m_rectsAddedToRegularRenderJobsInCurrentCycle = false;
    m_currentRegularRenderJobsBatchUnderPressure = false;
    m_primarySortDirection = TopToBottom;
    m_secondarySortDirection = LeftToRight;
    m_visibleZoomJobs.clear();
    m_visibleScrollJobs.clear();
    m_visibleScrollJobsCompleted.clear();
    m_nonVisibleScrollJobs.clear();
    m_nonVisibleScrollJobsCompleted.clear();
    m_regularRenderJobsRegion = Platform::IntRectRegion();
    m_currentRegularRenderJobsBatch.clear();
    m_currentRegularRenderJobsBatchRegion = Platform::IntRectRegion();
    m_regularRenderJobsNotRenderedRegion = Platform::IntRectRegion();
    ASSERT(isEmpty());
}

int RenderQueue::splittingFactor(const Platform::IntRect& rect) const
{
    // This method is used to split up regular render rect jobs and we want it to
    // to be zoom invariant with respect to WebCore. In other words, if WebCore sends
    // us a rect of viewport size to invalidate at zoom 1.0 then we split that up
    // in the exact same way we would at zoom 2.0. The amount of content that is
    // rendered in any one pass should stay fixed with regard to the zoom level.
    Platform::IntRect untransformedRect = m_parent->m_webPage->d->mapFromTransformed(rect);
    double rectArea = untransformedRect.width() * untransformedRect.height();
    double maxArea = m_parent->tileWidth() * m_parent->tileHeight();

    const unsigned splitFactor = 1 << 0;
    double renderRectArea = maxArea / splitFactor;
    return ceil(log(rectArea / renderRectArea) / log(2.0));
}

RenderRect RenderQueue::convertToRenderRect(const Platform::IntRect& rect) const
{
    return RenderRect(rect.location(), rect.size(), splittingFactor(rect));
}

bool RenderQueue::isEmpty(bool shouldPerformRegularRenderJobs) const
{
    return m_visibleZoomJobs.empty() && m_visibleScrollJobs.empty()
        && (!shouldPerformRegularRenderJobs || m_currentRegularRenderJobsBatch.empty())
        && (!shouldPerformRegularRenderJobs || m_regularRenderJobsRegion.isEmpty())
        && m_nonVisibleScrollJobs.empty();
}

bool RenderQueue::hasCurrentRegularRenderJob() const
{
    return !m_currentRegularRenderJobsBatch.empty() || !m_regularRenderJobsRegion.isEmpty();
}

bool RenderQueue::hasCurrentVisibleZoomJob() const
{
    return !m_visibleZoomJobs.empty();
}

bool RenderQueue::hasCurrentVisibleScrollJob() const
{
    return !m_visibleScrollJobs.empty();
}

bool RenderQueue::isCurrentVisibleScrollJob(const Platform::IntRect& rect) const
{
    return std::find(m_visibleScrollJobs.begin(), m_visibleScrollJobs.end(), rect) != m_visibleScrollJobs.end();
}

bool RenderQueue::isCurrentVisibleScrollJobCompleted(const Platform::IntRect& rect) const
{
    return std::find(m_visibleScrollJobsCompleted.begin(), m_visibleScrollJobsCompleted.end(), rect) != m_visibleScrollJobsCompleted.end();
}

bool RenderQueue::isCurrentRegularRenderJob(const Platform::IntRect& rect) const
{
    return m_regularRenderJobsRegion.isRectInRegion(rect) == Platform::IntRectRegion::ContainedInRegion
        || m_currentRegularRenderJobsBatchRegion.isRectInRegion(rect) == Platform::IntRectRegion::ContainedInRegion;
}

bool RenderQueue::currentRegularRenderJobBatchUnderPressure() const
{
    return m_currentRegularRenderJobsBatchUnderPressure;
}

void RenderQueue::setCurrentRegularRenderJobBatchUnderPressure(bool currentRegularRenderJobsBatchUnderPressure)
{
    m_currentRegularRenderJobsBatchUnderPressure = currentRegularRenderJobsBatchUnderPressure;
}

void RenderQueue::eventQueueCycled()
{
    // Called by the backing store when the event queue has cycled to allow the
    // render queue to determine if the regular render jobs are under pressure.
    if (m_rectsAddedToRegularRenderJobsInCurrentCycle && m_currentRegularRenderJobsBatchRegion.isEmpty())
        m_currentRegularRenderJobsBatchUnderPressure = true;
    m_rectsAddedToRegularRenderJobsInCurrentCycle = false;
}

void RenderQueue::addToQueue(JobType type, const IntRectList& rectList)
{
    for (size_t i = 0; i < rectList.size(); ++i)
        addToQueue(type, rectList.at(i));
}

void RenderQueue::addToQueue(JobType type, const Platform::IntRect& rect)
{
    if (type == NonVisibleScroll && std::find(m_visibleScrollJobs.begin(), m_visibleScrollJobs.end(), rect) != m_visibleScrollJobs.end())
        return; // |rect| is in a higher priority queue.

    switch (type) {
    case VisibleZoom:
        addToScrollZoomQueue(convertToRenderRect(rect), &m_visibleZoomJobs);
        return;
    case VisibleScroll:
        addToScrollZoomQueue(convertToRenderRect(rect), &m_visibleScrollJobs);
        return;
    case RegularRender:
        {
            // Flag that we added rects in the current event queue cycle.
            m_rectsAddedToRegularRenderJobsInCurrentCycle = true;

            // We try and detect if this newly added rect intersects or is contained in the currently running
            // batch of render jobs. If so, then we have to start the batch over since we decompose individual
            // rects into subrects and might have already rendered one of them. If the web page's content has
            // changed state then this can lead to artifacts. We mark this by noting the batch is now under pressure
            // and the backingstore will attempt to clear it at the next available opportunity.
            Platform::IntRectRegion::IntersectionState state = m_currentRegularRenderJobsBatchRegion.isRectInRegion(rect);
            if (state == Platform::IntRectRegion::ContainedInRegion || state == Platform::IntRectRegion::PartiallyContainedInRegion) {
                m_regularRenderJobsRegion = Platform::IntRectRegion::unionRegions(m_regularRenderJobsRegion, m_currentRegularRenderJobsBatchRegion);
                m_currentRegularRenderJobsBatch.clear();
                m_currentRegularRenderJobsBatchRegion = Platform::IntRectRegion();
                m_currentRegularRenderJobsBatchUnderPressure = true;
            }
            addToRegularQueue(rect);
        }
        return;
    case NonVisibleScroll:
        addToScrollZoomQueue(convertToRenderRect(rect), &m_nonVisibleScrollJobs);
        return;
    }
    ASSERT_NOT_REACHED();
}

void RenderQueue::addToRegularQueue(const Platform::IntRect& rect)
{
#if DEBUG_RENDER_QUEUE
    if (m_regularRenderJobsRegion.isRectInRegion(rect) != Platform::IntRectRegion::ContainedInRegion) {
        Platform::logAlways(Platform::LogLevelCritical,
            "RenderQueue::addToRegularQueue %s",
            rect.toString().c_str());
    }
#endif

    // Do not let the regular render queue grow past a maximum of 3 disjoint rects.
    if (m_regularRenderJobsRegion.numRects() > 2)
        m_regularRenderJobsRegion = Platform::unionOfRects(m_regularRenderJobsRegion.extents(), rect);
    else
        m_regularRenderJobsRegion = Platform::IntRectRegion::unionRegions(m_regularRenderJobsRegion, rect);

    if (!isEmpty())
        m_parent->dispatchRenderJob();
}

void RenderQueue::addToScrollZoomQueue(const RenderRect& rect, RenderRectList* rectList)
{
    if (std::find(rectList->begin(), rectList->end(), rect) != rectList->end())
        return;

#if DEBUG_RENDER_QUEUE
    Platform::logAlways(Platform::LogLevelCritical,
        "RenderQueue::addToScrollZoomQueue %s",
        rect.toString().c_str());
#endif
    rectList->push_back(rect);

    if (!isEmpty())
        m_parent->dispatchRenderJob();
}

void RenderQueue::quickSort(RenderRectList* queue)
{
    size_t length = queue->size();
    if (!length)
        return;

    for (size_t i = 0; i < length; ++i)
        queue->at(i).updateSortDirection(m_primarySortDirection, m_secondarySortDirection);
    return std::sort(queue->begin(), queue->end(), RenderRectLessThan(m_primarySortDirection, m_secondarySortDirection));
}

void RenderQueue::updateSortDirection(int lastDeltaX, int lastDeltaY)
{
    bool primaryIsHorizontal = abs(lastDeltaX) >= abs(lastDeltaY);
    if (primaryIsHorizontal) {
        m_primarySortDirection = lastDeltaX <= 0 ? LeftToRight : RightToLeft;
        m_secondarySortDirection = lastDeltaY <= 0 ? TopToBottom : BottomToTop;
    } else {
        m_primarySortDirection = lastDeltaY <= 0 ? TopToBottom : BottomToTop;
        m_secondarySortDirection = lastDeltaX <= 0 ? LeftToRight : RightToLeft;
    }
}

void RenderQueue::visibleContentChanged(const Platform::IntRect& visibleContent)
{
    if (m_visibleScrollJobs.empty() && m_nonVisibleScrollJobs.empty()) {
        ASSERT(m_visibleScrollJobsCompleted.empty() && m_nonVisibleScrollJobsCompleted.empty());
        return;
    }

    // Move visibleScrollJobs to nonVisibleScrollJobs if they do not intersect
    // the visible content rect.
    for (size_t i = 0; i < m_visibleScrollJobs.size(); ++i) {
        RenderRect rect = m_visibleScrollJobs.at(i);
        if (!rect.intersects(visibleContent)) {
            m_visibleScrollJobs.erase(m_visibleScrollJobs.begin() + i);
            addToScrollZoomQueue(rect, &m_nonVisibleScrollJobs);
            --i;
        }
    }

    // Do the same for the completed list.
    for (size_t i = 0; i < m_visibleScrollJobsCompleted.size(); ++i) {
        RenderRect rect = m_visibleScrollJobsCompleted.at(i);
        if (!rect.intersects(visibleContent)) {
            m_visibleScrollJobsCompleted.erase(m_visibleScrollJobsCompleted.begin() + i);
            addToScrollZoomQueue(rect, &m_nonVisibleScrollJobsCompleted);
            --i;
        }
    }

    // Move nonVisibleScrollJobs to visibleScrollJobs if they do intersect
    // the visible content rect.
    for (size_t i = 0; i < m_nonVisibleScrollJobs.size(); ++i) {
        RenderRect rect = m_nonVisibleScrollJobs.at(i);
        if (rect.intersects(visibleContent)) {
            m_nonVisibleScrollJobs.erase(m_nonVisibleScrollJobs.begin() + i);
            addToScrollZoomQueue(rect, &m_visibleScrollJobs);
            --i;
        }
    }

    // Do the same for the completed list.
    for (size_t i = 0; i < m_nonVisibleScrollJobsCompleted.size(); ++i) {
        RenderRect rect = m_nonVisibleScrollJobsCompleted.at(i);
        if (rect.intersects(visibleContent)) {
            m_nonVisibleScrollJobsCompleted.erase(m_nonVisibleScrollJobsCompleted.begin() + i);
            addToScrollZoomQueue(rect, &m_visibleScrollJobsCompleted);
            --i;
        }
    }

    if (m_visibleScrollJobs.empty() && !m_visibleScrollJobsCompleted.empty())
        visibleScrollJobsCompleted(false /*shouldBlit*/);

    if (m_nonVisibleScrollJobs.empty() && !m_nonVisibleScrollJobsCompleted.empty())
        nonVisibleScrollJobsCompleted();

    // We shouldn't be empty because the early return above and the fact that this
    // method just shuffles rects from queue to queue hence the total number of
    // rects in the various queues should be conserved.
    ASSERT(!isEmpty());
}

void RenderQueue::clear(const Platform::IntRectRegion& region, bool clearRegularRenderJobs)
{
    IntRectList rects = region.rects();
    for (size_t i = 0; i < rects.size(); ++i)
        clear(rects.at(i), clearRegularRenderJobs);
}

void RenderQueue::clear(const Platform::IntRect& rect, bool clearRegularRenderJobs)
{
    if (m_visibleScrollJobs.empty() && m_nonVisibleScrollJobs.empty())
        ASSERT(m_visibleScrollJobsCompleted.empty() && m_nonVisibleScrollJobsCompleted.empty());

    // Remove all rects from all queues that are contained by this rect.
    for (size_t i = 0; i < m_visibleScrollJobs.size(); ++i) {
        if (rect.contains(m_visibleScrollJobs.at(i))) {
            m_visibleScrollJobs.erase(m_visibleScrollJobs.begin() + i);
            --i;
        }
    }

    for (size_t i = 0; i < m_visibleScrollJobsCompleted.size(); ++i) {
        if (rect.contains(m_visibleScrollJobsCompleted.at(i))) {
            m_visibleScrollJobsCompleted.erase(m_visibleScrollJobsCompleted.begin() + i);
            --i;
        }
    }

    for (size_t i = 0; i < m_nonVisibleScrollJobs.size(); ++i) {
        if (rect.contains(m_nonVisibleScrollJobs.at(i))) {
            m_nonVisibleScrollJobs.erase(m_nonVisibleScrollJobs.begin() + i);
            --i;
        }
    }

    for (size_t i = 0; i < m_nonVisibleScrollJobsCompleted.size(); ++i) {
        if (rect.contains(m_nonVisibleScrollJobsCompleted.at(i))) {
            m_nonVisibleScrollJobsCompleted.erase(m_nonVisibleScrollJobsCompleted.begin() + i);
            --i;
        }
    }

    // Only clear the regular render jobs if the flag has been set.
    if (clearRegularRenderJobs)
        this->clearRegularRenderJobs(rect);

    if (m_visibleScrollJobs.empty() && !m_visibleScrollJobsCompleted.empty())
        visibleScrollJobsCompleted(false /*shouldBlit*/);

    if (m_nonVisibleScrollJobs.empty() && !m_nonVisibleScrollJobsCompleted.empty())
        nonVisibleScrollJobsCompleted();
}

void RenderQueue::clearRegularRenderJobs(const Platform::IntRect& rect)
{
    for (size_t i = 0; i < m_currentRegularRenderJobsBatch.size(); ++i) {
        if (rect.contains(m_currentRegularRenderJobsBatch.at(i))) {
            m_currentRegularRenderJobsBatch.erase(m_currentRegularRenderJobsBatch.begin() + i);
            --i;
        }
    }
    m_regularRenderJobsRegion = Platform::IntRectRegion::subtractRegions(m_regularRenderJobsRegion, rect);
    m_currentRegularRenderJobsBatchRegion = Platform::IntRectRegion::subtractRegions(m_currentRegularRenderJobsBatchRegion, rect);
    m_regularRenderJobsNotRenderedRegion = Platform::IntRectRegion::subtractRegions(m_regularRenderJobsNotRenderedRegion, rect);
}

void RenderQueue::clearVisibleZoom()
{
    m_visibleZoomJobs.clear();
}

bool RenderQueue::regularRenderJobsPreviouslyAttemptedButNotRendered(const Platform::IntRect& rect)
{
    return m_regularRenderJobsNotRenderedRegion.isRectInRegion(rect) != Platform::IntRectRegion::NotInRegion;
}

void RenderQueue::render(bool shouldPerformRegularRenderJobs)
{
    // We request a layout here to ensure that we're executing jobs in the correct
    // order. If we didn't request a layout here then the jobs below could result
    // in a layout and that layout can alter this queue. So request layout if needed
    // to ensure that the queues below are in constant state before performing the
    // next rendering job.

#if DEBUG_RENDER_QUEUE
    // Start the time measurement.
    double time = WTF::currentTime();
#endif

    m_parent->requestLayoutIfNeeded();

#if DEBUG_RENDER_QUEUE
    double elapsed = WTF::currentTime() - time;
    if (elapsed)
        Platform::logAlways(Platform::LogLevelCritical, "RenderQueue::render layout elapsed=%f", elapsed);
#endif

    // Empty the queues in a precise order of priority.
    if (!m_visibleZoomJobs.empty())
        renderVisibleZoomJob();
    else if (!m_visibleScrollJobs.empty())
        renderVisibleScrollJob();
    else if (shouldPerformRegularRenderJobs && (!m_currentRegularRenderJobsBatch.empty() || !m_regularRenderJobsRegion.isEmpty())) {
        if (currentRegularRenderJobBatchUnderPressure())
            renderAllCurrentRegularRenderJobs();
        else
            renderRegularRenderJob();
    } else if (!m_nonVisibleScrollJobs.empty())
        renderNonVisibleScrollJob();
}

void RenderQueue::renderAllCurrentRegularRenderJobs()
{
#if DEBUG_RENDER_QUEUE
    // Start the time measurement...
    double time = WTF::currentTime();
#endif

    // Request layout first
    m_parent->requestLayoutIfNeeded();

#if DEBUG_RENDER_QUEUE
    double elapsed = WTF::currentTime() - time;
    if (elapsed)
        Platform::logAlways(Platform::LogLevelCritical, "RenderQueue::renderAllCurrentRegularRenderJobs layout elapsed=%f", elapsed);
#endif

    // The state of render queue may be modified from inside requestLayoutIfNeeded.
    // In fact, it can even be emptied entirely! Layout can trigger a call to
    // RenderQueue::clear. See PR#101811 for instance. So we should check again here.
    if (!hasCurrentRegularRenderJob())
        return;

    // If there is no current batch of jobs, then create one.
    if (m_currentRegularRenderJobsBatchRegion.isEmpty()) {

        // Create a current region object from our regular render region.
        m_currentRegularRenderJobsBatchRegion = m_regularRenderJobsRegion;

        // Clear this since we're about to render everything.
        m_regularRenderJobsRegion = Platform::IntRectRegion();
    }

    Platform::IntRectRegion regionNotRendered;
    if (m_parent->shouldSuppressNonVisibleRegularRenderJobs()) {
        // Record any part of the region that doesn't intersect the current visible contents rect.
        regionNotRendered = Platform::IntRectRegion::subtractRegions(m_currentRegularRenderJobsBatchRegion, m_parent->visibleContentsRect());
        m_regularRenderJobsNotRenderedRegion = Platform::IntRectRegion::unionRegions(m_regularRenderJobsNotRenderedRegion, regionNotRendered);

#if DEBUG_RENDER_QUEUE
        if (!regionNotRendered.isEmpty())
            Platform::logAlways(Platform::LogLevelCritical, "RenderQueue::renderAllCurrentRegularRenderJobs region not completely rendered!");
#endif

        // Clip to the visible contents so we'll be faster.
        m_currentRegularRenderJobsBatchRegion = Platform::IntRectRegion::intersectRegions(m_currentRegularRenderJobsBatchRegion, m_parent->visibleContentsRect());
    }

    bool rendered = false;
    if (!m_currentRegularRenderJobsBatchRegion.isEmpty()) {
        std::vector<Platform::IntRect> rectList = m_currentRegularRenderJobsBatchRegion.rects();
        for (size_t i = 0; i < rectList.size(); ++i)
            rendered = m_parent->render(rectList.at(i)) ? true : rendered;
    }

#if DEBUG_RENDER_QUEUE
    // Stop the time measurement.
    elapsed = WTF::currentTime() - time;
    Platform::logAlways(Platform::LogLevelCritical,
        "RenderQueue::renderAllCurrentRegularRenderJobs extents=%s numberOfRects=%d elapsed=%f",
        m_currentRegularRenderJobsBatchRegion.extents().toString().c_str(),
        m_currentRegularRenderJobsBatchRegion.rects().size(),
        elapsed);
#endif

    // Clear the region and blit since this batch is now complete.
    Platform::IntRect renderedRect = m_currentRegularRenderJobsBatchRegion.extents();
    m_currentRegularRenderJobsBatch.clear();
    m_currentRegularRenderJobsBatchRegion = Platform::IntRectRegion();
    m_currentRegularRenderJobsBatchUnderPressure = false;

    if (rendered)
        m_parent->didRenderContent(renderedRect);

    if (m_parent->shouldSuppressNonVisibleRegularRenderJobs() && !regionNotRendered.isEmpty())
        m_parent->updateTilesForScrollOrNotRenderedRegion(false /*checkLoading*/);
}

void RenderQueue::startRegularRenderJobBatchIfNeeded()
{
    if (!m_currentRegularRenderJobsBatch.empty())
        return;

    // Decompose the current regular render job region into render rect pieces.
    IntRectList regularRenderJobs = m_regularRenderJobsRegion.rects();

    // The current batch...
    m_currentRegularRenderJobsBatch = regularRenderJobs;

    // Create a region object that will be checked when adding new rects before
    // this batch has been completed.
    m_currentRegularRenderJobsBatchRegion = m_regularRenderJobsRegion;

    // Clear the former region since it is now part of this batch.
    m_regularRenderJobsRegion = Platform::IntRectRegion();

#if DEBUG_RENDER_QUEUE
    Platform::logAlways(Platform::LogLevelCritical,
        "RenderQueue::startRegularRenderJobBatchIfNeeded batch size is %d!",
        m_currentRegularRenderJobsBatch.size());
#endif
}

void RenderQueue::renderVisibleZoomJob()
{
    ASSERT(m_visibleZoomJobs.size() > 0);

#if DEBUG_RENDER_QUEUE
    // Start the time measurement.
    double time = WTF::currentTime();
#endif

    RenderRect* rect = &m_visibleZoomJobs[0];
    ASSERT(!rect->isCompleted());
    Platform::IntRect subRect = rect->rectForRendering();
    if (rect->isCompleted())
        m_visibleZoomJobs.erase(m_visibleZoomJobs.begin());

    m_parent->render(subRect);

    // Record that it has now been rendered via a different type of job...
    clearRegularRenderJobs(subRect);

#if DEBUG_RENDER_QUEUE
    // Stop the time measurement
    double elapsed = WTF::currentTime() - time;
    Platform::logAlways(Platform::LogLevelCritical,
        "RenderQueue::renderVisibleZoomJob rect=%s elapsed=%f",
        subRect.toString().c_str(),
        elapsed);
#endif
}

void RenderQueue::renderVisibleScrollJob()
{
    ASSERT(!m_visibleScrollJobs.empty());

#if DEBUG_RENDER_QUEUE || DEBUG_RENDER_QUEUE_SORT
    // Start the time measurement.
    double time = WTF::currentTime();
    double elapsed;
#endif

    quickSort(&m_visibleScrollJobs);

#if DEBUG_RENDER_QUEUE_SORT
    // Stop the time measurement
    elapsed = WTF::currentTime() - time;
    Platform::logAlways(Platform::LogLevelCritical, "RenderQueue::renderVisibleScrollJob sort elapsed=%f", elapsed);
#endif

    RenderRect rect = m_visibleScrollJobs[0];
    m_visibleScrollJobs.erase(m_visibleScrollJobs.begin());

    ASSERT(!rect.isCompleted());
    Platform::IntRect subRect = rect.rectForRendering();
    if (rect.isCompleted())
        m_visibleScrollJobsCompleted.push_back(rect);
    else
        m_visibleScrollJobs.insert(m_visibleScrollJobs.begin(), rect);

    m_parent->render(subRect);

    // Record that it has now been rendered via a different type of job...
    clearRegularRenderJobs(subRect);

#if DEBUG_RENDER_QUEUE
    // Stop the time measurement
    elapsed = WTF::currentTime() - time;
    Platform::logAlways(Platform::LogLevelCritical,
        "RenderQueue::renderVisibleScrollJob rect=%s elapsed=%f",
        subRect.toString().c_str(),
        elapsed);
#endif

    if (m_visibleScrollJobs.empty())
        visibleScrollJobsCompleted(true /*shouldBlit*/);
}

void RenderQueue::renderRegularRenderJob()
{
#if DEBUG_RENDER_QUEUE
    // Start the time measurement.
    double time = WTF::currentTime();
#endif

    ASSERT(!m_currentRegularRenderJobsBatch.empty() || !m_regularRenderJobsRegion.isEmpty());

    startRegularRenderJobBatchIfNeeded();

    // Take the first job from the regular render job queue.
    Platform::IntRect rect = m_currentRegularRenderJobsBatch[0];
    m_currentRegularRenderJobsBatchRegion = Platform::IntRectRegion::subtractRegions(m_currentRegularRenderJobsBatchRegion, Platform::IntRectRegion(rect));
    m_currentRegularRenderJobsBatch.erase(m_currentRegularRenderJobsBatch.begin());

    Platform::IntRectRegion regionNotRendered;
    if (m_parent->shouldSuppressNonVisibleRegularRenderJobs()) {
        // Record any part of the region that doesn't intersect the current visible tiles rect.
        regionNotRendered = Platform::IntRectRegion::subtractRegions(rect, m_parent->visibleContentsRect());
        m_regularRenderJobsNotRenderedRegion = Platform::IntRectRegion::unionRegions(m_regularRenderJobsNotRenderedRegion, regionNotRendered);

#if DEBUG_RENDER_QUEUE
        if (!regionNotRendered.isEmpty()) {
            Platform::logAlways(Platform::LogLevelCritical,
                "RenderQueue::renderRegularRenderJob rect %s not completely rendered!",
                rect.toString().c_str());
        }
#endif

        // Clip to the visible tiles so we'll be faster.
        rect.intersect(m_parent->visibleContentsRect());
    }

    if (!rect.isEmpty())
        m_parent->render(rect);

#if DEBUG_RENDER_QUEUE
    // Stop the time measurement.
    double elapsed = WTF::currentTime() - time;
    Platform::logAlways(Platform::LogLevelCritical,
        "RenderQueue::renderRegularRenderJob rect=%s elapsed=%f",
        rect.toString().c_str(),
        elapsed);
#endif

    if (m_currentRegularRenderJobsBatch.empty()) {
        Platform::IntRect renderedRect = m_currentRegularRenderJobsBatchRegion.extents();
        // Clear the region and the and blit since this batch is now complete.
        m_currentRegularRenderJobsBatchRegion = Platform::IntRectRegion();
        m_currentRegularRenderJobsBatchUnderPressure = false;
        m_parent->didRenderContent(renderedRect);
    }

    // Make sure we didn't alter state of the queues that should have been empty
    // before this method was called.
    ASSERT(m_visibleScrollJobs.empty());

    if (m_parent->shouldSuppressNonVisibleRegularRenderJobs() && !regionNotRendered.isEmpty())
        m_parent->updateTilesForScrollOrNotRenderedRegion(false /*checkLoading*/);
}

void RenderQueue::renderNonVisibleScrollJob()
{
    ASSERT(!m_nonVisibleScrollJobs.empty());

#if DEBUG_RENDER_QUEUE || DEBUG_RENDER_QUEUE_SORT
    // Start the time measurement.
    double time = WTF::currentTime();
    double elapsed;
#endif

    quickSort(&m_nonVisibleScrollJobs);

#if DEBUG_RENDER_QUEUE_SORT
    // Stop the time measurement.
    elapsed = WTF::currentTime() - time;
    Platform::logAlways(Platform::LogLevelCritical, "RenderQueue::renderNonVisibleScrollJob sort elapsed=%f", elapsed);
#endif

    RenderRect rect = m_nonVisibleScrollJobs[0];
    m_nonVisibleScrollJobs.erase(m_nonVisibleScrollJobs.begin());

    ASSERT(!rect.isCompleted());
    Platform::IntRect subRect = rect.rectForRendering();
    if (rect.isCompleted())
        m_nonVisibleScrollJobsCompleted.push_back(rect);
    else
        m_nonVisibleScrollJobs.insert(m_nonVisibleScrollJobs.begin(), rect);

    m_parent->render(subRect);

    // Record that it has now been rendered via a different type of job...
    clearRegularRenderJobs(subRect);

    // Make sure we didn't alter state of the queues that should have been empty
    // before this method was called.
    ASSERT(m_visibleScrollJobs.empty());

#if DEBUG_RENDER_QUEUE
    // Stop the time measurement.
    elapsed = WTF::currentTime() - time;
    Platform::logAlways(Platform::LogLevelCritical,
        "RenderQueue::renderNonVisibleScrollJob rect=%s elapsed=%f",
        subRect.toString().c_str(), elapsed);
#endif

    if (m_nonVisibleScrollJobs.empty())
        nonVisibleScrollJobsCompleted();
}

void RenderQueue::visibleScrollJobsCompleted(bool shouldBlit)
{
    // Now blit to the screen if we are done and get rid of the completed list!
    ASSERT(m_visibleScrollJobs.empty());
    m_visibleScrollJobsCompleted.clear();
    if (shouldBlit)
        m_parent->didRenderContent(m_parent->visibleContentsRect());
}

void RenderQueue::nonVisibleScrollJobsCompleted()
{
    // Get rid of the completed list!
    ASSERT(m_nonVisibleScrollJobs.empty());
    m_nonVisibleScrollJobsCompleted.clear();
}

} // namespace WebKit
} // namespace BlackBerry
