/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "WheelEventDeltaFilter.h"

#include "FloatSize.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

#if PLATFORM(MAC)
#include "WheelEventDeltaFilterMac.h"
#endif

namespace WebCore {
    
WheelEventDeltaFilter::WheelEventDeltaFilter() = default;

WheelEventDeltaFilter::~WheelEventDeltaFilter() = default;

std::unique_ptr<WheelEventDeltaFilter> WheelEventDeltaFilter::create()
{
#if PLATFORM(MAC)
    return makeUnique<WheelEventDeltaFilterMac>();
#else
    return makeUnique<BasicWheelEventDeltaFilter>();
#endif
}

bool WheelEventDeltaFilter::isFilteringDeltas() const
{
    return m_isFilteringDeltas;
}

FloatSize WheelEventDeltaFilter::filteredDelta() const
{
    return m_currentFilteredDelta;
}

FloatSize WheelEventDeltaFilter::filteredVelocity() const
{
    return m_currentFilteredVelocity;
}

BasicWheelEventDeltaFilter::BasicWheelEventDeltaFilter()
    : WheelEventDeltaFilter()
{
}

const size_t basicWheelEventDeltaFilterWindowSize = 3;

void BasicWheelEventDeltaFilter::updateFromDelta(const FloatSize& delta)
{
    m_currentFilteredDelta = delta;
    if (!m_isFilteringDeltas)
        return;
    
    m_recentWheelEventDeltas.append(delta);
    if (m_recentWheelEventDeltas.size() > basicWheelEventDeltaFilterWindowSize)
        m_recentWheelEventDeltas.removeFirst();
    
    DominantScrollGestureDirection scrollDirection = dominantScrollGestureDirection();
    if (scrollDirection == DominantScrollGestureDirection::Vertical)
        m_currentFilteredDelta.setWidth(0);
    else if (scrollDirection == DominantScrollGestureDirection::Horizontal)
        m_currentFilteredDelta.setHeight(0);
}

void BasicWheelEventDeltaFilter::beginFilteringDeltas()
{
    m_recentWheelEventDeltas.clear();
    m_isFilteringDeltas = true;
}

void BasicWheelEventDeltaFilter::endFilteringDeltas()
{
    m_currentFilteredDelta = FloatSize(0, 0);
    m_isFilteringDeltas = false;
}

static inline bool deltaIsPredominantlyVertical(const FloatSize& delta)
{
    return fabs(delta.height()) > fabs(delta.width());
}

DominantScrollGestureDirection BasicWheelEventDeltaFilter::dominantScrollGestureDirection() const
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

};
