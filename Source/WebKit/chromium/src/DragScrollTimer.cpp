/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DragScrollTimer.h"

#include "FrameView.h"

using namespace WebCore;

namespace WebKit {

// Computes the distance from a point outside a rect to the nearest edge of the rect.
static IntSize distanceToRect(const IntPoint& point, const IntRect& rect)
{
    int dx = 0, dy = 0;
    if (point.x() < rect.x())
        dx = point.x() - rect.x();
    else if (rect.maxX() < point.x())
        dx = point.x() - rect.maxX();
    if (point.y() < rect.y())
        dy = point.y() - rect.y();
    else if (rect.maxY() < point.y())
        dy = point.y() - rect.maxY();
    return IntSize(dx, dy);
}

DragScrollTimer::DragScrollTimer()
    : m_timer(this, &DragScrollTimer::fired)
    , m_view(0)
    , m_scrolling(false)
{
}

DragScrollTimer::~DragScrollTimer()
{
    // We do this for detecting dead object earlier
    stop(); 
}

void DragScrollTimer::stop()
{
    m_timer.stop();
    m_view = 0;
    m_scrolling = false;
}

void DragScrollTimer::scroll()
{
    m_view->scrollBy(m_lastDistance);
    m_scrolling = true;
}

void DragScrollTimer::update()
{
    if (shouldScroll())
        scroll();
    else
        stop();
}

void DragScrollTimer::triggerScroll(FrameView* view, const WebPoint& location)
{
    if (!view)
        return;

    // Approximates Safari
    static const double scrollStartDelay = 0.2;

    m_view = view;
    m_lastDistance = scrollDistanceFor(view, location);

    if (m_scrolling)
        update();
    else if (shouldScroll() && !m_timer.isActive())
        m_timer.startOneShot(scrollStartDelay);
}

IntSize DragScrollTimer::scrollDistanceFor(FrameView* view, const WebPoint& location) const
{
    static const int scrollMargin = 30;

    IntRect bounds(0, 0, view->visibleWidth(), view->visibleHeight());
    if (!bounds.contains(location))
        return IntSize(0, 0); // The location is outside the border belt.

    bounds.setY(bounds.y() + scrollMargin);
    bounds.setHeight(bounds.height() - scrollMargin * 2);
    bounds.setX(bounds.x() + scrollMargin);
    bounds.setWidth(bounds.width() - scrollMargin * 2);

    if (bounds.contains(location))
        return IntSize(0, 0); // The location is inside the border belt.

    // The location is over the border belt.
    return distanceToRect(location, bounds);
}

} // namespace WebKit
