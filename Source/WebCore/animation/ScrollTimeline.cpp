/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollTimeline.h"

#include "AnimationTimelinesController.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSScrollValue.h"
#include "CSSValuePool.h"
#include "DocumentInlines.h"
#include "Element.h"
#include "RenderLayerScrollableArea.h"
#include "RenderView.h"
#include "WebAnimation.h"

namespace WebCore {

Ref<ScrollTimeline> ScrollTimeline::create(ScrollTimelineOptions&& options)
{
    return adoptRef(*new ScrollTimeline(WTFMove(options)));
}

Ref<ScrollTimeline> ScrollTimeline::create(const AtomString& name, ScrollAxis axis)
{
    return adoptRef(*new ScrollTimeline(name, axis));
}

Ref<ScrollTimeline> ScrollTimeline::createFromCSSValue(const CSSScrollValue& cssScrollValue)
{
    auto scroller = [&]() {
        auto scrollerValue = cssScrollValue.scroller();
        if (!scrollerValue)
            return Scroller::Nearest;

        switch (scrollerValue->valueID()) {
        case CSSValueNearest:
            return Scroller::Nearest;
        case CSSValueRoot:
            return Scroller::Root;
        case CSSValueSelf:
            return Scroller::Self;
        default:
            ASSERT_NOT_REACHED();
            return Scroller::Nearest;
        }
    }();

    auto axisValue = cssScrollValue.axis();
    auto axis = axisValue ? fromCSSValueID<ScrollAxis>(axisValue->valueID()) : ScrollAxis::Block;

    return adoptRef(*new ScrollTimeline(scroller, axis));
}

// https://drafts.csswg.org/web-animations-2/#timelines
// For a monotonic timeline, there is no upper bound on current time, and
// timeline duration is unresolved. For a non-monotonic (e.g. scroll) timeline,
// the duration has a fixed upper bound. In this case, the timeline is a
// progress-based timeline, and its timeline duration is 100%.
ScrollTimeline::ScrollTimeline(ScrollTimelineOptions&& options)
    : AnimationTimeline(WebAnimationTime::fromPercentage(100))
    , m_source(WTFMove(options.source))
    , m_axis(options.axis)
{
    if (m_source)
        m_source->protectedDocument()->ensureTimelinesController().addTimeline(*this);
}

ScrollTimeline::ScrollTimeline(const AtomString& name, ScrollAxis axis)
    : ScrollTimeline()
{
    m_axis = axis;
    m_name = name;
}

ScrollTimeline::ScrollTimeline(Scroller scroller, ScrollAxis axis)
    : ScrollTimeline()
{
    m_axis = axis;
    m_scroller = scroller;
}

void ScrollTimeline::setSource(const Element* source)
{
    if (source == m_source)
        return;

    RefPtr previousSource = m_source.get();
    m_source = source;
    RefPtr newSource = m_source.get();

    if (previousSource && newSource && &previousSource->document() == &newSource->document())
        return;

    if (previousSource) {
        if (CheckedPtr timelinesController = previousSource->protectedDocument()->timelinesController())
            timelinesController->removeTimeline(*this);
    }

    if (newSource)
        newSource->protectedDocument()->ensureTimelinesController().addTimeline(*this);
}

void ScrollTimeline::dump(TextStream& ts) const
{
    auto hasScroller = m_scroller != Scroller::Nearest;
    auto hasAxis = m_axis != ScrollAxis::Block;

    ts << "scroll(";
    if (hasScroller)
        ts << (m_scroller == Scroller::Root ? "root" : "self");
    if (hasScroller && hasAxis)
        ts << " ";
    if (hasAxis)
        ts << m_axis;
    ts << ")";
}

Ref<CSSValue> ScrollTimeline::toCSSValue(const RenderStyle&) const
{
    auto scroller = [&]() {
        switch (m_scroller) {
        case Scroller::Nearest:
            return CSSValueNearest;
        case Scroller::Root:
            return CSSValueRoot;
        case Scroller::Self:
            return CSSValueSelf;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueNearest;
        }
    }();

    return CSSScrollValue::create(CSSPrimitiveValue::create(scroller), CSSPrimitiveValue::create(toCSSValueID(m_axis)));
}

AnimationTimelinesController* ScrollTimeline::controller() const
{
    if (m_source)
        return &m_source->document().ensureTimelinesController();
    return nullptr;
}

AnimationTimeline::ShouldUpdateAnimationsAndSendEvents ScrollTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    if (m_source && m_source->isConnected())
        return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
    return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No;
}

Element* ScrollTimeline::sourceElementForProgressCalculation() const
{
    switch (m_scroller) {
    case Scroller::Nearest: {
        CheckedPtr subjectRenderer = m_source->renderer();
        if (!subjectRenderer)
            return nullptr;
        return subjectRenderer->enclosingScrollableContainer()->element();
    }
    case Scroller::Root:
        return m_source->protectedDocument()->documentElement();
    case Scroller::Self:
        return m_source.get();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void ScrollTimeline::setTimelineScopeElement(const Element& element)
{
    m_timelineScopeElement = WeakPtr { &element };
}

ScrollableArea* ScrollTimeline::scrollableAreaForSourceRenderer(RenderElement* renderer, Ref<Document> document)
{
    CheckedPtr renderBox = dynamicDowncast<RenderBox>(renderer);
    if (!renderBox)
        return nullptr;

    if (renderer->element() == document->documentElement())
        return &renderer->view().frameView();

    return renderBox->hasLayer() ? renderBox->layer()->scrollableArea() : nullptr;
}

float ScrollTimeline::floatValueForOffset(const Length& offset, float maxValue)
{
    if (offset.isNormal() || offset.isAuto())
        return 0.f;
    return floatValueForLength(offset, maxValue);
}

TimelineRange ScrollTimeline::defaultRange() const
{
    return TimelineRange::defaultForScrollTimeline();
}

ScrollTimeline::Data ScrollTimeline::computeTimelineData(const TimelineRange& range) const
{
    if ((range.start.name != SingleTimelineRange::Name::Normal && range.start.name != SingleTimelineRange::Name::Omitted) || (range.end.name != SingleTimelineRange::Name::Normal && range.end.name != SingleTimelineRange::Name::Omitted))
        return { };

    RefPtr source = sourceElementForProgressCalculation();
    if (!source)
        return { };

    auto* sourceScrollableArea = scrollableAreaForSourceRenderer(source->renderer(), source->document());
    if (!sourceScrollableArea)
        return { };

    float maxScrollOffset = axis() == ScrollAxis::Block ? sourceScrollableArea->maximumScrollOffset().y() : sourceScrollableArea->maximumScrollOffset().x();
    float scrollOffset = axis() == ScrollAxis::Block ? sourceScrollableArea->scrollOffset().y() : sourceScrollableArea->scrollOffset().x();

    // Chrome appears to clip the current time of a scroll timeline in the [0-100] range.
    // We match this behavior for compatibility reasons, see https://github.com/w3c/csswg-drafts/issues/11033.
    if (maxScrollOffset > 0)
        scrollOffset = std::clamp(scrollOffset, 0.f, maxScrollOffset);

    return { scrollOffset, floatValueForOffset(range.start.offset, maxScrollOffset), floatValueForOffset(range.end.offset, maxScrollOffset) };
}

std::optional<WebAnimationTime> ScrollTimeline::currentTime(const TimelineRange& timelineRange)
{
    // https://drafts.csswg.org/scroll-animations-1/#scroll-timeline-progress
    // Progress (the current time) for a scroll progress timeline is calculated as:
    // scroll offset ÷ (scrollable overflow size − scroll container size)
    auto timelineRangeOrDefault = timelineRange.isDefault() ? defaultRange() : timelineRange;
    auto data = computeTimelineData(timelineRangeOrDefault);
    auto range = data.rangeEnd - data.rangeStart;
    if (!range)
        return std::nullopt;
    auto distance = data.scrollOffset - data.rangeStart;
    auto progress = distance / range;
    return WebAnimationTime::fromPercentage(progress * 100);
}

void ScrollTimeline::animationTimingDidChange(WebAnimation& animation)
{
    AnimationTimeline::animationTimingDidChange(animation);

    if (!m_source || !animation.pending() || animation.isEffectInvalidationSuspended())
        return;

    if (RefPtr page = m_source->protectedDocument()->page())
        page->scheduleRenderingUpdate(RenderingUpdateStep::Animations);
}

} // namespace WebCore
