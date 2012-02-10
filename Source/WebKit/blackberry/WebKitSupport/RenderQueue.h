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

#ifndef RenderQueue_h
#define RenderQueue_h

#include <BlackBerryPlatformIntRectRegion.h>
#include <BlackBerryPlatformPrimitives.h>
#include <vector>

namespace BlackBerry {
namespace WebKit {

class BackingStorePrivate;

typedef std::vector<Platform::IntRect> IntRectList;

enum SortDirection {
    LeftToRight = 0,
    RightToLeft,
    TopToBottom,
    BottomToTop,
    NumSortDirections
};

class RenderRect : public Platform::IntRect {
public:
    RenderRect() { }
    RenderRect(const Platform::IntPoint& location, const Platform::IntSize&, int splittingFactor);
    RenderRect(int x, int y, int width, int height, int splittingFactor);
    RenderRect(const Platform::IntRect&);

    Platform::IntRect rectForRendering();

    bool isCompleted() const { return !m_subRects.size(); }

    const IntRectList& subRects() const { return m_subRects; }
    void updateSortDirection(SortDirection primary, SortDirection secondary);

private:
    void initialize(int splittingFactor);
    void split();
    void quickSort();
    int m_splittingFactor;
    IntRectList m_subRects;
    SortDirection m_primarySortDirection;
    SortDirection m_secondarySortDirection;
};

typedef std::vector<RenderRect> RenderRectList;

class RenderQueue {
public:
    enum JobType { VisibleZoom, VisibleScroll, RegularRender, NonVisibleScroll };
    RenderQueue(BackingStorePrivate*);

    void reset();
    RenderRect convertToRenderRect(const Platform::IntRect&) const;

    bool isEmpty(bool shouldPerformRegularRenderJobs = true) const;

    bool hasCurrentRegularRenderJob() const;
    bool hasCurrentVisibleZoomJob() const;
    bool hasCurrentVisibleScrollJob() const;
    bool isCurrentVisibleScrollJob(const Platform::IntRect&) const;
    bool isCurrentVisibleScrollJobCompleted(const Platform::IntRect&) const;
    bool isCurrentRegularRenderJob(const Platform::IntRect&) const;

    bool currentRegularRenderJobBatchUnderPressure() const;
    void setCurrentRegularRenderJobBatchUnderPressure(bool);

    void eventQueueCycled();

    void addToQueue(JobType, const Platform::IntRect&);
    void addToQueue(JobType, const IntRectList&);

    void updateSortDirection(int lastDeltaX, int lastDeltaY);
    void visibleContentChanged(const Platform::IntRect&);
    void clear(const Platform::IntRectRegion&, bool clearRegularRenderJobs);
    void clear(const Platform::IntRect&, bool clearRegularRenderJobs);
    void clearRegularRenderJobs(const Platform::IntRect&);
    void clearVisibleZoom();
    bool regularRenderJobsPreviouslyAttemptedButNotRendered(const Platform::IntRect&);
    Platform::IntRectRegion regularRenderJobsNotRenderedRegion() const { return m_regularRenderJobsNotRenderedRegion; }

    void render(bool shouldPerformRegularRenderJobs = true);
    void renderAllCurrentRegularRenderJobs();

private:
    void startRegularRenderJobBatchIfNeeded();

    // Render an item from the queue.
    void renderVisibleZoomJob();
    void renderVisibleScrollJob();
    void renderRegularRenderJob();
    void renderNonVisibleScrollJob();

    // Methods to handle a completed set of scroll jobs.
    void visibleScrollJobsCompleted(bool shouldBlit);
    void nonVisibleScrollJobsCompleted();

    // Internal method to add to the various queues.
    void addToRegularQueue(const Platform::IntRect&);
    void addToScrollZoomQueue(const RenderRect&, RenderRectList* queue);
    void quickSort(RenderRectList*);

    // The splitting factor for render rects.
    int splittingFactor(const Platform::IntRect&) const;

    BackingStorePrivate* m_parent;

    // The highest priority queue.
    RenderRectList m_visibleZoomJobs;
    RenderRectList m_visibleScrollJobs;
    RenderRectList m_visibleScrollJobsCompleted;
    // The lowest priority queue.
    RenderRectList m_nonVisibleScrollJobs;
    RenderRectList m_nonVisibleScrollJobsCompleted;
    // The regular render jobs are in the middle.
    Platform::IntRectRegion m_regularRenderJobsRegion;
    IntRectList m_currentRegularRenderJobsBatch;
    Platform::IntRectRegion m_currentRegularRenderJobsBatchRegion;
    bool m_rectsAddedToRegularRenderJobsInCurrentCycle;
    bool m_currentRegularRenderJobsBatchUnderPressure;

    // Holds the region of the page that we attempt to render, but the
    // backingstore was not in the right place at the time. This will
    // be checked before we try to restore a tile to it's last rendered
    // place.
    Platform::IntRectRegion m_regularRenderJobsNotRenderedRegion;

    SortDirection m_primarySortDirection;
    SortDirection m_secondarySortDirection;
};

} // namespace WebKit
} // namespace BlackBerry

#endif // RenderQueue_h
