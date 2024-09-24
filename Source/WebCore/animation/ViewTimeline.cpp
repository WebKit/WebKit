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
#include "ViewTimeline.h"

#include "AnimationTimelinesController.h"
#include "CSSNumericFactory.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSUnits.h"
#include "CSSValuePool.h"
#include "CSSViewValue.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "Element.h"
#include "RenderBox.h"
#include "ScrollAnchoringController.h"
#include "StyleBuilderConverter.h"

namespace WebCore {

Ref<ViewTimeline> ViewTimeline::create(ViewTimelineOptions&& options)
{
    return adoptRef(*new ViewTimeline(WTFMove(options)));
}

Ref<ViewTimeline> ViewTimeline::create(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
{
    return adoptRef(*new ViewTimeline(name, axis, WTFMove(insets)));
}

Ref<ViewTimeline> ViewTimeline::createFromCSSValue(Style::BuilderState& builderState, const CSSViewValue& cssViewValue)
{
    auto axisValue = cssViewValue.axis();
    auto axis = axisValue ? fromCSSValueID<ScrollAxis>(axisValue->valueID()) : ScrollAxis::Block;

    auto convertInsetValue = [&](CSSValue* value) -> std::optional<Length> {
        if (!value)
            return std::nullopt;
        return Style::BuilderConverter::convertLengthOrAuto(builderState, *value);
    };

    auto startInset = convertInsetValue(cssViewValue.startInset().get());

    auto endInset = [&]() {
        if (auto endInsetValue = cssViewValue.endInset())
            return convertInsetValue(endInsetValue.get());
        return convertInsetValue(cssViewValue.startInset().get());
    }();

    return adoptRef(*new ViewTimeline(nullAtom(), axis, { startInset, endInset }));
}

// FIXME: compute offset values from options.
ViewTimeline::ViewTimeline(ViewTimelineOptions&& options)
    : ScrollTimeline(nullAtom(), options.axis)
    , m_subject(WTFMove(options.subject))
    , m_startOffset(CSSNumericFactory::px(0))
    , m_endOffset(CSSNumericFactory::px(0))
{
    if (m_subject)
        m_subject->protectedDocument()->ensureTimelinesController().addTimeline(*this);
}

ViewTimeline::ViewTimeline(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
    : ScrollTimeline(name, axis)
    , m_startOffset(CSSNumericFactory::px(0))
    , m_endOffset(CSSNumericFactory::px(0))
    , m_insets(WTFMove(insets))
{
}

void ViewTimeline::dump(TextStream& ts) const
{
    auto hasAxis = axis() != ScrollAxis::Block;
    auto hasEndInset = m_insets.end && m_insets.end != m_insets.start;
    auto hasStartInset = (m_insets.start && !m_insets.start->isAuto()) || (m_insets.start && m_insets.start->isAuto() && hasEndInset);

    ts << "view(";
    if (hasAxis)
        ts << axis();
    if (hasAxis && hasStartInset)
        ts << " ";
    if (hasStartInset)
        ts << *m_insets.start;
    if (hasStartInset && hasEndInset)
        ts << " ";
    if (hasEndInset)
        ts << *m_insets.end;
    ts << ")";
}

Ref<CSSValue> ViewTimeline::toCSSValue(const RenderStyle& style) const
{
    auto insetCSSValue = [&](const std::optional<Length>& inset) -> RefPtr<CSSValue> {
        if (!inset)
            return nullptr;
        return CSSPrimitiveValue::create(*inset, style);
    };

    return CSSViewValue::create(
        CSSPrimitiveValue::create(toCSSValueID(axis())),
        insetCSSValue(m_insets.start),
        insetCSSValue(m_insets.end)
    );
}

AnimationTimelinesController* ViewTimeline::controller() const
{
    if (m_subject)
        return &m_subject->protectedDocument()->ensureTimelinesController();
    return nullptr;
}

AnimationTimeline::ShouldUpdateAnimationsAndSendEvents ViewTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    if (m_subject && m_subject->isConnected())
        return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
    return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No;
}

Element* ViewTimeline::source() const
{
    if (auto* sourceRender = sourceRenderer())
        return sourceRender->element();
    return nullptr;
}

RenderBox* ViewTimeline::sourceRenderer() const
{
    if (!m_subject)
        return nullptr;

    CheckedPtr subjectRenderer = m_subject->renderer();
    if (!subjectRenderer)
        return { };

    // https://drafts.csswg.org/scroll-animations-1/#dom-scrolltimeline-source
    // Determine source renderer by looking for the nearest ancestor that establishes a scroll container
    return subjectRenderer->enclosingScrollableContainer();
}

ViewTimeline::Data ViewTimeline::computeViewTimelineData() const
{
    if (!m_subject)
        return { };

    Ref document = m_subject->document();

    CheckedPtr subjectRenderBox = dynamicDowncast<RenderBox>(m_subject->renderer());
    if (!subjectRenderBox)
        return { };

    auto* sourceScrollableArea = scrollableAreaForSourceRenderer(sourceRenderer(), document);
    if (!sourceScrollableArea)
        return { };

    // https://drafts.csswg.org/scroll-animations-1/#view-timeline-progress
    //
    // Progress (the current time) in a view progress timeline is calculated as: distance ÷ range where:
    //
    // - distance is the current scroll offset minus the scroll offset corresponding to the start of the
    //   cover range
    // - range is the scroll offset corresponding to the start of the cover range minus the scroll offset
    //   corresponding to the end of the cover range
    // TODO: take into account view-timeline-inset: https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-inset
    // TODO: take into account animation-range: https://drafts.csswg.org/scroll-animations-1/#animation-range

    float currentScrollOffset = axis() == ScrollAxis::Block ? sourceScrollableArea->scrollPosition().y() : sourceScrollableArea->scrollPosition().x();
    float scrollContainerSize = axis() == ScrollAxis::Block ? sourceScrollableArea->visibleHeight() : sourceScrollableArea->visibleWidth();

    auto offsetFromScroller = ScrollAnchoringController::computeOffsetFromScrollableArea(*subjectRenderBox, *sourceScrollableArea, ScrollAnchoringController::ShouldIncludeFrameViewLocation::No);
    float subjectOffset = axis() == ScrollAxis::Block ? offsetFromScroller.y() : offsetFromScroller.x();

    float subjectSize = axis() == ScrollAxis::Block ? subjectRenderBox->borderBoxRect().height() : subjectRenderBox->borderBoxRect().width();

    auto coverRangeStart = subjectOffset - currentScrollOffset;
    auto coverRangeEnd = coverRangeStart + subjectSize;

    return { scrollContainerSize, subjectOffset, currentScrollOffset, coverRangeEnd };
}

} // namespace WebCore
