/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#include "ContainerNodeInlines.h"
#include "Document.h"
#include "Element.h"
#include "KeyframeEffect.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerModelObject.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "StylableInlines.h"
#include "StyleSingleAnimationRange.h"
#include "WebAnimation.h"

#ifndef NDEBUG
#include "Settings.h"
#endif

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
    } else if (RefPtr scrollingElement = Ref { document }->scrollingElementForAPI()) {
        // Otherwise,
        // The scrollingElement of the Document associated with the Window that is the current global object.
        timeline->setSource(scrollingElement.get());
    }

    // 3. Set the axis property of timeline to the corresponding value from options.
    timeline->setAxis(options.axis);

    if (auto source = timeline->m_source.element()) {
        source->protectedDocument()->updateLayoutIgnorePendingStylesheets();
        timeline->cacheCurrentTime();
    }

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

Ref<ScrollTimeline> ScrollTimeline::createInactiveStyleOriginatedTimeline(const AtomString& name)
{
    auto timeline = adoptRef(*new ScrollTimeline(name, ScrollAxis::Block));
    timeline->m_isInactiveStyleOriginatedTimeline = true;
    return timeline;
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

Element* ScrollTimeline::bindingsSource() const
{
    return source();
}

Element* ScrollTimeline::source() const
{
    auto source = m_source.styleable();
    if (!source)
        return nullptr;

    switch (m_scroller) {
    case Scroller::Nearest: {
        if (CheckedPtr subjectRenderer = source->renderer()) {
            if (CheckedPtr nearestScrollableContainer = subjectRenderer->enclosingScrollableContainer()) {
                if (RefPtr nearestSource = nearestScrollableContainer->element()) {
                    Ref document = nearestSource->document();
                    RefPtr documentElement = document->documentElement();
                    if (nearestSource != documentElement)
                        return nearestSource.unsafeGet();
                    // RenderObject::enclosingScrollableContainer() will return the document element even in
                    // quirks mode, but the scrolling element in that case is the <body> element, so we must
                    // make sure to return Document::scrollingElement() in case the document element is
                    // returned by enclosingScrollableContainer() but it was not explicitly set as the source.
                    return &source->element == documentElement ? nearestSource.unsafeGet() : document->scrollingElement();
                }
            }
        }
        return nullptr;
    }
    case Scroller::Root:
        return source->element.protectedDocument()->scrollingElement();
    case Scroller::Self:
        return &source->element;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void ScrollTimeline::setSource(Element* source)
{
    if (source)
        setSource(Styleable::fromElement(*source));
    else {
        removeTimelineFromDocument(m_source.element().get());
        m_source = WeakStyleable();
    }
}

void ScrollTimeline::setSource(const Styleable& styleable)
{
    if (m_source == styleable)
        return;

    auto previousSource = m_source.element();
    m_source = styleable;

    if (previousSource && &previousSource->document() == &styleable.element.document())
        return;

    removeTimelineFromDocument(previousSource.get());

    styleable.element.protectedDocument()->ensureTimelinesController().addTimeline(*this);
}

void ScrollTimeline::removeTimelineFromDocument(Element* element)
{
    if (element) {
        if (CheckedPtr timelinesController = element->protectedDocument()->timelinesController())
            timelinesController->removeTimeline(*this);
    }
}

AnimationTimelinesController* ScrollTimeline::controller() const
{
    if (auto stylable = m_source.styleable())
        return &stylable->element.document().ensureTimelinesController();
    return nullptr;
}

ScrollTimeline::ResolvedScrollDirection ScrollTimeline::resolvedScrollDirection() const
{
    auto writingMode = [&] -> WritingMode {
        if (RefPtr source = this->source()) {
            if (CheckedPtr renderer = source->renderer())
                return renderer->style().writingMode();
        }

        return { RenderStyle::initialWritingMode(), RenderStyle::initialDirection(), RenderStyle::initialTextOrientation() };
    }();

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

    return { isVertical, isReversed };
}

auto ScrollTimeline::computeCurrentTimeData() const -> CurrentTimeData
{
    RefPtr source = this->source();
    if (!source)
        return { };
    CheckedPtr sourceScrollableArea = scrollableAreaForSourceRenderer(source->renderer(), source->document());
    if (!sourceScrollableArea)
        return { };
    auto scrollDirection = resolvedScrollDirection();
    float scrollOffset = scrollDirection.isVertical ? sourceScrollableArea->scrollOffset().y() : sourceScrollableArea->scrollOffset().x();
    float maxScrollOffset = scrollDirection.isVertical ? sourceScrollableArea->maximumScrollOffset().y() : sourceScrollableArea->maximumScrollOffset().x();
    // Chrome appears to clip the current time of a scroll timeline in the [0-100] range.
    // We match this behavior for compatibility reasons, see https://github.com/w3c/csswg-drafts/issues/11033.
    if (maxScrollOffset > 0)
        scrollOffset = std::clamp(scrollOffset, 0.f, maxScrollOffset);
    return { scrollOffset, maxScrollOffset };
}

void ScrollTimeline::cacheCurrentTime()
{
    auto previousMaxScrollOffset = m_cachedCurrentTimeData.maxScrollOffset;

    m_cachedCurrentTimeData = computeCurrentTimeData();

    if (previousMaxScrollOffset != m_cachedCurrentTimeData.maxScrollOffset) {
        for (auto& animation : m_animations)
            animation->progressBasedTimelineSourceDidChangeMetrics();
    }
}

AnimationTimeline::ShouldUpdateAnimationsAndSendEvents ScrollTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    cacheCurrentTime();
    auto source = m_source.styleable();
    if (source && source->element.isConnected())
        return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
    return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No;
}

void ScrollTimeline::updateCurrentTimeIfStale()
{
    // https://drafts.csswg.org/scroll-animations-1/#event-loop
    // We must update timelines that became stale in the process of updating the page rendering.
    // This function will be called during Page::updateRendering() after animations have been
    // updated, requestAnimationFrame callbacks have been serviced, styles have been updated
    // and resize observers have been run.
    // See https://github.com/w3c/csswg-drafts/issues/12120 about clarifying this.
    auto source = m_source.styleable();
    if (!source || m_animations.isEmpty())
        return;

    auto previousMaxScrollOffset = m_cachedCurrentTimeData.maxScrollOffset;
    cacheCurrentTime();
    if (previousMaxScrollOffset == m_cachedCurrentTimeData.maxScrollOffset)
        return;

    bool needsStyleUpdate = false;
    for (auto& animation : m_animations) {
        if (RefPtr effect = animation->keyframeEffect()) {
            effect->invalidate();
            needsStyleUpdate = true;
        }
    }

    if (needsStyleUpdate)
        source->element.protectedDocument()->updateStyleIfNeeded();
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

Style::SingleAnimationRange ScrollTimeline::defaultRange() const
{
    return Style::SingleAnimationRange::defaultForScrollTimeline();
}

ScrollTimeline::Data ScrollTimeline::computeTimelineData(UseCachedCurrentTime useCachedCurrentTime) const
{
    auto currentTimeData = useCachedCurrentTime == UseCachedCurrentTime::Yes ? m_cachedCurrentTimeData : computeCurrentTimeData();
    if (!currentTimeData.scrollOffset && !currentTimeData.maxScrollOffset)
        return { };
    return {
        currentTimeData.scrollOffset,
        0.f,
        currentTimeData.maxScrollOffset
    };
}

std::pair<WebAnimationTime, WebAnimationTime> ScrollTimeline::intervalForAttachmentRange(const Style::SingleAnimationRange& attachmentRange) const
{
    auto maxScrollOffset = m_cachedCurrentTimeData.maxScrollOffset;
    if (!maxScrollOffset)
        return { WebAnimationTime::fromPercentage(0), WebAnimationTime::fromPercentage(100) };

    auto attachmentRangeOrDefault = attachmentRange.isDefault() ? defaultRange() : attachmentRange;

    auto computedPercentageIfNecessary = [&](const auto& rangeOffset) {
        if (auto percentage = rangeOffset.tryPercentage())
            return percentage->value;
        return Style::evaluate<float>(rangeOffset, maxScrollOffset, Style::ZoomNeeded { }) / maxScrollOffset * 100;
    };

    return {
        WebAnimationTime::fromPercentage(computedPercentageIfNecessary(attachmentRangeOrDefault.start.offset())),
        WebAnimationTime::fromPercentage(computedPercentageIfNecessary(attachmentRangeOrDefault.end.offset()))
    };
}

std::optional<WebAnimationTime> ScrollTimeline::currentTime(UseCachedCurrentTime useCachedCurrentTime)
{
    // https://drafts.csswg.org/scroll-animations-1/#scroll-timeline-progress
    // Progress (the current time) for a scroll progress timeline is calculated as:
    // scroll offset ÷ (scrollable overflow size − scroll container size)
    auto data = computeTimelineData(useCachedCurrentTime);
    auto range = data.rangeEnd - data.rangeStart;
    if (!range)
        return { };

    auto scrollDirection = resolvedScrollDirection();
    auto distance = scrollDirection.isReversed ? data.rangeEnd - data.scrollOffset : data.scrollOffset - data.rangeStart;
    auto progress = distance / range;
    return WebAnimationTime::fromPercentage(progress * 100);
}

void ScrollTimeline::animationTimingDidChange(WebAnimation& animation)
{
    AnimationTimeline::animationTimingDidChange(animation);

    auto source = m_source.styleable();
    if (!source || !animation.pending() || animation.isEffectInvalidationSuspended())
        return;

    if (RefPtr page = source->element.protectedDocument()->page())
        page->scheduleRenderingUpdate(RenderingUpdateStep::Animations);
}

#if ENABLE(THREADED_ANIMATIONS)
bool ScrollTimeline::computeCanBeAccelerated() const
{
    RefPtr source = this->source();
    if (!source)
        return false;

    ASSERT(source->document().settings().threadedScrollDrivenAnimationsEnabled());

    CheckedPtr sourceScrollableArea = scrollableAreaForSourceRenderer(source->renderer(), source->document());
    return sourceScrollableArea && !!sourceScrollableArea->scrollingNodeID();
}

Ref<AcceleratedTimeline> ScrollTimeline::createAcceleratedRepresentation() const
{
    ASSERT(this->source());
    ASSERT(this->source()->document().settings().threadedScrollDrivenAnimationsEnabled());
    Ref source = *this->source();
    CheckedPtr sourceScrollableArea = scrollableAreaForSourceRenderer(source->renderer(), source->document());
    ASSERT(sourceScrollableArea);
    ASSERT(sourceScrollableArea->scrollingNodeID());

    ASSERT(duration());
    auto direction = resolvedScrollDirection();
    auto data = computeTimelineData();

    return AcceleratedTimeline::create(m_acceleratedTimelineIdentifier, {
        *sourceScrollableArea->scrollingNodeID(),
        *duration(),
        direction.isVertical,
        direction.isReversed,
        data.scrollOffset,
        data.rangeStart,
        data.rangeEnd
    });
}
#endif

TextStream& operator<<(TextStream& ts, const ScrollTimeline& timeline)
{
    return ts << timeline.name() << ' ' << timeline.axis();
}

} // namespace WebCore
