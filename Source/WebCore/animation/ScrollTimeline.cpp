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
#include "DocumentInlines.h"
#include "Element.h"
#include "RenderLayerScrollableArea.h"
#include "RenderView.h"
#include "WebAnimation.h"

namespace WebCore {

Ref<ScrollTimeline> ScrollTimeline::create(Document& document, ScrollTimelineOptions&& options)
{
    // https://drafts.csswg.org/scroll-animations-1/#dom-scrolltimeline-scrolltimeline

    // 1. Let timeline be the new ScrollTimeline object.
    auto timeline = adoptRef(*new ScrollTimeline);

    // 2. Set the source of timeline to:
    if (auto optionalSource = options.source) {
        // If the source member of options is present,
        // The source member of options.
        timeline->setSource(optionalSource->get());
    } else if (RefPtr scrollingElement = Ref { document }->scrollingElement()) {
        // Otherwise,
        // The scrollingElement of the Document associated with the Window that is the current global object.
        timeline->setSource(scrollingElement.get());
    }

    // 3. Set the axis property of timeline to the corresponding value from options.
    timeline->setAxis(options.axis);

    if (timeline->m_source)
        timeline->cacheCurrentTime();

    return timeline;
}

Ref<ScrollTimeline> ScrollTimeline::create(const AtomString& name, ScrollAxis axis)
{
    return adoptRef(*new ScrollTimeline(name, axis));
}

Ref<ScrollTimeline> ScrollTimeline::create(Scroller scroller, ScrollAxis axis)
{
    return adoptRef(*new ScrollTimeline(scroller, axis));
}

// https://drafts.csswg.org/web-animations-2/#timelines
// For a monotonic timeline, there is no upper bound on current time, and
// timeline duration is unresolved. For a non-monotonic (e.g. scroll) timeline,
// the duration has a fixed upper bound. In this case, the timeline is a
// progress-based timeline, and its timeline duration is 100%.
ScrollTimeline::ScrollTimeline()
    : AnimationTimeline(WebAnimationTime::fromPercentage(100))
{
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

Element* ScrollTimeline::source() const
{
    if (!m_source)
        return nullptr;

    switch (m_scroller) {
    case Scroller::Nearest: {
        if (CheckedPtr subjectRenderer = m_source->renderer()) {
            if (CheckedPtr nearestScrollableContainer = subjectRenderer->enclosingScrollableContainer()) {
                if (RefPtr nearestSource = nearestScrollableContainer->element()) {
                    auto document = nearestSource->protectedDocument();
                    RefPtr documentElement = document->documentElement();
                    if (nearestSource != documentElement)
                        return nearestSource.get();
                    // RenderObject::enclosingScrollableContainer() will return the document element even in
                    // quirks mode, but the scrolling element in that case is the <body> element, so we must
                    // make sure to return Document::scrollingElement() in case the document element is
                    // returned by enclosingScrollableContainer() but it was not explicitly set as the source.
                    return m_source.get() == documentElement ? nearestSource.get() : document->scrollingElement();
                }
            }
        }
        return nullptr;
    }
    case Scroller::Root:
        return m_source->protectedDocument()->scrollingElement();
    case Scroller::Self:
        return m_source.get();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
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

AnimationTimelinesController* ScrollTimeline::controller() const
{
    if (m_source)
        return &m_source->document().ensureTimelinesController();
    return nullptr;
}

std::optional<ScrollTimeline::ResolvedScrollDirection> ScrollTimeline::resolvedScrollDirection() const
{
    RefPtr source = this->source();
    if (!source)
        return { };

    CheckedPtr renderer = source->renderer();
    if (!renderer)
        return { };

    auto writingMode = renderer->style().writingMode();

    auto isVertical = [&] {
        switch (m_axis) {
        case ScrollAxis::Block:
            // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-block
            // Specifies to use the measure of progress along the block axis of the scroll container.
            // https://drafts.csswg.org/css-writing-modes-4/#block-axis
            // The axis in the block dimension, i.e. the vertical axis in horizontal writing modes and
            // the horizontal axis in vertical writing modes.
            return writingMode.isHorizontal();
        case ScrollAxis::Inline:
            // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-inline
            // Specifies to use the measure of progress along the inline axis of the scroll container.
            // https://drafts.csswg.org/css-writing-modes-4/#inline-axis
            // The axis in the inline dimension, i.e. the horizontal axis in horizontal writing modes and
            // the vertical axis in vertical writing modes.
            return writingMode.isVertical();
        case ScrollAxis::X:
            // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-x
            // Specifies to use the measure of progress along the horizontal axis of the scroll container.
            return false;
        case ScrollAxis::Y:
            // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-y
            // Specifies to use the measure of progress along the vertical axis of the scroll container.
            return true;
        }
        ASSERT_NOT_REACHED();
        return true;
    }();

    auto isReversed = (isVertical && !writingMode.isAnyTopToBottom()) || (!isVertical && !writingMode.isAnyLeftToRight());

    return { { isVertical, isReversed } };
}

void ScrollTimeline::cacheCurrentTime()
{
    auto previousMaxScrollOffset = m_cachedCurrentTimeData.maxScrollOffset;

    m_cachedCurrentTimeData = [&] -> CurrentTimeData {
        RefPtr source = this->source();
        if (!source)
            return { };
        auto* sourceScrollableArea = scrollableAreaForSourceRenderer(source->renderer(), source->document());
        if (!sourceScrollableArea)
            return { };
        auto scrollDirection = resolvedScrollDirection();
        if (!scrollDirection)
            return { };

        float scrollOffset = scrollDirection->isVertical ? sourceScrollableArea->scrollOffset().y() : sourceScrollableArea->scrollOffset().x();
        float maxScrollOffset = scrollDirection->isVertical ? sourceScrollableArea->maximumScrollOffset().y() : sourceScrollableArea->maximumScrollOffset().x();
        // Chrome appears to clip the current time of a scroll timeline in the [0-100] range.
        // We match this behavior for compatibility reasons, see https://github.com/w3c/csswg-drafts/issues/11033.
        if (maxScrollOffset > 0)
            scrollOffset = std::clamp(scrollOffset, 0.f, maxScrollOffset);
        return { scrollOffset, maxScrollOffset };
    }();

    if (previousMaxScrollOffset != m_cachedCurrentTimeData.maxScrollOffset) {
        for (auto& animation : m_animations)
            animation->progressBasedTimelineSourceDidChangeMetrics();
    }
}

AnimationTimeline::ShouldUpdateAnimationsAndSendEvents ScrollTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    cacheCurrentTime();
    if (m_source && m_source->isConnected())
        return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
    return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No;
}

void ScrollTimeline::setTimelineScopeElement(const Element& element)
{
    m_timelineScopeElement = WeakPtr { &element };
}

ScrollableArea* ScrollTimeline::scrollableAreaForSourceRenderer(const RenderElement* renderer, Document& document)
{
    CheckedPtr renderBox = dynamicDowncast<RenderBox>(renderer);
    if (!renderBox)
        return nullptr;

    if (renderer->element() == Ref { document }->scrollingElement())
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

ScrollTimeline::Data ScrollTimeline::computeTimelineData() const
{
    if (!m_cachedCurrentTimeData.scrollOffset && !m_cachedCurrentTimeData.maxScrollOffset)
        return { };
    return {
        m_cachedCurrentTimeData.scrollOffset,
        0.f,
        m_cachedCurrentTimeData.maxScrollOffset
    };
}

std::pair<WebAnimationTime, WebAnimationTime> ScrollTimeline::intervalForAttachmentRange(const TimelineRange& attachmentRange) const
{
    auto maxScrollOffset = m_cachedCurrentTimeData.maxScrollOffset;
    if (!maxScrollOffset)
        return { WebAnimationTime::fromPercentage(0), WebAnimationTime::fromPercentage(100) };

    auto attachmentRangeOrDefault = attachmentRange.isDefault() ? defaultRange() : attachmentRange;

    auto computedPercentageIfNecessary = [&](const Length& length) {
        if (length.isPercent())
            return length.value();
        return floatValueForOffset(length, maxScrollOffset) / maxScrollOffset * 100;
    };

    return {
        WebAnimationTime::fromPercentage(computedPercentageIfNecessary(attachmentRangeOrDefault.start.offset)),
        WebAnimationTime::fromPercentage(computedPercentageIfNecessary(attachmentRangeOrDefault.end.offset))
    };
}

std::optional<WebAnimationTime> ScrollTimeline::currentTime()
{
    // https://drafts.csswg.org/scroll-animations-1/#scroll-timeline-progress
    // Progress (the current time) for a scroll progress timeline is calculated as:
    // scroll offset ÷ (scrollable overflow size − scroll container size)
    auto data = computeTimelineData();
    auto range = data.rangeEnd - data.rangeStart;
    if (!range)
        return { };

    auto scrollDirection = resolvedScrollDirection();
    if (!scrollDirection)
        return { };

    auto distance = scrollDirection->isReversed ? data.rangeEnd - data.scrollOffset : data.scrollOffset - data.rangeStart;
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

TextStream& operator<<(TextStream& ts, Scroller scroller)
{
    switch (scroller) {
    case Scroller::Nearest: ts << "nearest"; break;
    case Scroller::Root: ts << "root"; break;
    case Scroller::Self: ts << "self"; break;
    }
    return ts;
}

} // namespace WebCore
