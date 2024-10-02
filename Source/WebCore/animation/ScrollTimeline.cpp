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
#include "Document.h"
#include "DocumentInlines.h"
#include "Element.h"
#include "RenderLayerScrollableArea.h"
#include "RenderView.h"

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

ScrollTimeline::ScrollTimeline(ScrollTimelineOptions&& options)
    : m_source(WTFMove(options.source))
    , m_axis(options.axis)
{
    if (m_source)
        m_source->protectedDocument()->ensureTimelinesController().addTimeline(*this);
}

ScrollTimeline::ScrollTimeline(const AtomString& name, ScrollAxis axis)
    : m_axis(axis)
    , m_name(name)
{
}

ScrollTimeline::ScrollTimeline(Scroller scroller, ScrollAxis axis)
    : m_axis(axis)
    , m_scroller(scroller)
{
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
        return &m_source->protectedDocument()->ensureTimelinesController();
    return nullptr;
}

AnimationTimeline::ShouldUpdateAnimationsAndSendEvents ScrollTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    if (m_source && m_source->isConnected())
        return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
    return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No;
}

ScrollableArea* ScrollTimeline::scrollableAreaForSourceRenderer(RenderElement* renderer, Ref<Document> document)
{
    CheckedPtr renderBox = dynamicDowncast<RenderBox>(renderer);
    if (!renderBox)
        return nullptr;

    if (renderer->element() == document->documentElement())
        return &renderer->view().frameView();

    return (renderBox->canBeScrolledAndHasScrollableArea() && renderBox->hasLayer()) ? renderBox->layer()->scrollableArea() : nullptr;
}

ScrollTimeline::Data ScrollTimeline::computeScrollTimelineData() const
{
    if (!m_source)
        return { };

    auto* sourceScrollableArea = scrollableAreaForSourceRenderer(m_source->renderer(), m_source->document());
    if (!sourceScrollableArea)
        return { };

    // https://drafts.csswg.org/scroll-animations-1/#scroll-timeline-progress
    // Progress (the current time) for a scroll progress timeline is calculated as:
    // scroll offset ÷ (scrollable overflow size − scroll container size)

    float maxScrollOffset = axis() == ScrollAxis::Block ? sourceScrollableArea->maximumScrollOffset().y() : sourceScrollableArea->maximumScrollOffset().x();
    float scrollOffset = axis() == ScrollAxis::Block ? sourceScrollableArea->scrollOffset().y() : sourceScrollableArea->scrollOffset().x();

    return { maxScrollOffset, scrollOffset };
}

} // namespace WebCore
