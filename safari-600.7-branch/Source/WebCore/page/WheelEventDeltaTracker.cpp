/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WheelEventDeltaTracker.h"

#include "PlatformWheelEvent.h"

namespace WebCore {

WheelEventDeltaTracker::WheelEventDeltaTracker()
    : m_isTrackingDeltas(false)
{
}

WheelEventDeltaTracker::~WheelEventDeltaTracker()
{
}

void WheelEventDeltaTracker::beginTrackingDeltas()
{
    m_recentWheelEventDeltas.clear();
    m_isTrackingDeltas = true;
}

void WheelEventDeltaTracker::endTrackingDeltas()
{
    m_isTrackingDeltas = false;
}

void WheelEventDeltaTracker::recordWheelEventDelta(const PlatformWheelEvent& event)
{
    m_recentWheelEventDeltas.append(FloatSize(event.deltaX(), event.deltaY()));
    if (m_recentWheelEventDeltas.size() > recentEventCount)
        m_recentWheelEventDeltas.removeFirst();
}

static bool deltaIsPredominantlyVertical(const FloatSize& delta)
{
    return fabs(delta.height()) > fabs(delta.width());
}

DominantScrollGestureDirection WheelEventDeltaTracker::dominantScrollGestureDirection() const
{
    bool allVertical = m_recentWheelEventDeltas.size();
    bool allHorizontal = m_recentWheelEventDeltas.size();

    for (const auto& delta : m_recentWheelEventDeltas) {
        bool isVertical = deltaIsPredominantlyVertical(delta);
        allVertical &= isVertical;
        allHorizontal &= !isVertical;
    }
    
    if (allVertical)
        return DominantScrollGestureDirection::Vertical;

    if (allHorizontal)
        return DominantScrollGestureDirection::Horizontal;
    
    return DominantScrollGestureDirection::None;
}

} // namespace WebCore
