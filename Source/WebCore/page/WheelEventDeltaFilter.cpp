/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "PlatformWheelEvent.h"
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

bool WheelEventDeltaFilter::shouldApplyFilteringForEvent(const PlatformWheelEvent& event)
{
#if ENABLE(KINETIC_SCROLLING)
    // Maybe it's a per-platform decision about which event phases get filtered. Ideally we'd filter momentum events too (but that breaks some diagonal scrolling cases).
    // Also, ScrollingEffectsController should ask WheelEventDeltaFilter directly for the filtered velocity, rather than sending the velocity via PlatformWheelEvent.
    auto phase = event.phase();
    return phase == PlatformWheelEventPhase::Began || phase == PlatformWheelEventPhase::Changed;
#else
    return false;
#endif
}

bool WheelEventDeltaFilter::shouldIncludeVelocityForEvent(const PlatformWheelEvent& event)
{
#if ENABLE(KINETIC_SCROLLING)
    // Include velocity on momentum-began phases so that the momentum animator can use it.
    if (event.momentumPhase() == PlatformWheelEventPhase::Began)
        return true;
#endif
    return shouldApplyFilteringForEvent(event);
}

PlatformWheelEvent WheelEventDeltaFilter::eventCopyWithFilteredDeltas(const PlatformWheelEvent& event) const
{
    return event.copyWithDeltaAndVelocity(m_currentFilteredDelta, m_currentFilteredVelocity);
}

PlatformWheelEvent WheelEventDeltaFilter::eventCopyWithVelocity(const PlatformWheelEvent& event) const
{
    return event.copyWithVelocity(m_currentFilteredVelocity);
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

void BasicWheelEventDeltaFilter::updateFromEvent(const PlatformWheelEvent& event)
{
#if ENABLE(KINETIC_SCROLLING)
    switch (event.phase()) {
    case PlatformWheelEventPhase::MayBegin:
    case PlatformWheelEventPhase::Cancelled:
    case PlatformWheelEventPhase::Stationary:
    case PlatformWheelEventPhase::Ended:
        reset();
        break;

    case PlatformWheelEventPhase::None:
        break;

    case PlatformWheelEventPhase::Began:
    case PlatformWheelEventPhase::Changed:
        updateWithDelta(event.delta());
        break;
    }
#else
    m_currentFilteredDelta = event.delta();
#endif
}

constexpr size_t basicWheelEventDeltaFilterWindowSize = 3;

void BasicWheelEventDeltaFilter::updateWithDelta(FloatSize delta)
{
    m_currentFilteredDelta = delta;

    m_recentWheelEventDeltas.append(delta);
    if (m_recentWheelEventDeltas.size() > basicWheelEventDeltaFilterWindowSize)
        m_recentWheelEventDeltas.removeFirst();
    
    auto scrollAxis = dominantAxis();
    if (!scrollAxis)
        return;

    if (scrollAxis.value() == ScrollEventAxis::Vertical)
        m_currentFilteredDelta.setWidth(0);
    else if (scrollAxis.value() == ScrollEventAxis::Horizontal)
        m_currentFilteredDelta.setHeight(0);
}

void BasicWheelEventDeltaFilter::reset()
{
    m_recentWheelEventDeltas.clear();
    m_currentFilteredDelta = { };
    m_currentFilteredVelocity = { };
}

static inline bool deltaIsPredominantlyVertical(const FloatSize& delta)
{
    return fabs(delta.height()) > fabs(delta.width());
}

std::optional<ScrollEventAxis> BasicWheelEventDeltaFilter::dominantAxis() const
{
    bool allVertical = m_recentWheelEventDeltas.size();
    bool allHorizontal = m_recentWheelEventDeltas.size();

    for (const auto& delta : m_recentWheelEventDeltas) {
        bool isVertical = deltaIsPredominantlyVertical(delta);
        allVertical &= isVertical;
        allHorizontal &= !isVertical;
    }

    if (allVertical)
        return ScrollEventAxis::Vertical;

    if (allHorizontal)
        return ScrollEventAxis::Horizontal;

    return std::nullopt;
}

};
