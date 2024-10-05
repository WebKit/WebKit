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
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Timeline.h"
#include "CSSTokenizer.h"
#include "CSSUnits.h"
#include "CSSValueList.h"
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

static std::optional<Length> lengthForInset(std::variant<RefPtr<CSSNumericValue>, RefPtr<CSSKeywordValue>> inset)
{
    // TODO: Need to test this
    if (auto* numericInset = std::get_if<RefPtr<CSSNumericValue>>(&inset)) {
        if (RefPtr insetValue = dynamicDowncast<CSSUnitValue>(*numericInset)) {
            if (auto length = insetValue->convertTo(CSSUnitType::CSS_PX))
                return Length(length->value(), LengthType::Fixed);
            return { };
        }
    }
    ASSERT(std::holds_alternative<RefPtr<CSSKeywordValue>>(inset));
    return { };
}

static Length lengthForInsetCSSValue(RefPtr<CSSPrimitiveValue> value)
{
    // TODO: Figure out how to make this work when provided calc value
    if (!value || value->isCalculated())
        return { };
    if (value->valueID() == CSSValueAuto)
        return { };
    if (value->isPercentage())
        return Length(value->resolveAsPercentageNoConversionDataRequired(), LengthType::Percent);
    if (value->isPx())
        return Length(value->resolveAsLengthNoConversionDataRequired(), LengthType::Fixed);

    ASSERT_NOT_REACHED();
    return { };
}

static ViewTimelineInsets insetsFromOptions(const std::variant<String, Vector<std::variant<RefPtr<CSSNumericValue>, RefPtr<CSSKeywordValue>>>> inset, CSSParserContext context)
{
    if (auto* insetString = std::get_if<String>(&inset)) {
        if (insetString->isEmpty())
            return { };
        CSSTokenizer tokenizer(*insetString);
        auto tokenRange = tokenizer.tokenRange();
        tokenRange.consumeWhitespace();
        auto consumedInset = CSSPropertyParserHelpers::consumeViewTimelineInsetListItem(tokenRange, context);
        if (auto insetPair = dynamicDowncast<CSSValuePair>(consumedInset))
            return { lengthForInsetCSSValue(dynamicDowncast<CSSPrimitiveValue>(insetPair->protectedFirst())), lengthForInsetCSSValue(dynamicDowncast<CSSPrimitiveValue>(insetPair->protectedSecond())) };
        return { lengthForInsetCSSValue(dynamicDowncast<CSSPrimitiveValue>(consumedInset)), std::nullopt };
    }
    auto insetList = std::get<Vector<std::variant<RefPtr<CSSNumericValue>, RefPtr<CSSKeywordValue>>>>(inset);

    if (!insetList.size())
        return { };
    if (insetList.size() == 2)
        return { lengthForInset(insetList.at(0)), lengthForInset(insetList.at(1)) };
    return { lengthForInset(insetList.at(0)), std::nullopt };
}

ViewTimeline::ViewTimeline(ViewTimelineOptions&& options)
    : ScrollTimeline(nullAtom(), options.axis)
    , m_subject(WTFMove(options.subject))
{
    if (m_subject) {
        auto document = m_subject->protectedDocument();
        document->ensureTimelinesController().addTimeline(*this);
        m_insets = insetsFromOptions(options.inset, document->cssParserContext());
    }
}

ViewTimeline::ViewTimeline(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
    : ScrollTimeline(name, axis)
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
    // - range is the scroll offset corresponding to the end of the cover range minus the scroll offset
    //   corresponding to the start of the cover range
    // TODO: take into account animation-range: https://drafts.csswg.org/scroll-animations-1/#animation-range
    // TODO: investigate best way to compute subjectOffset, as offsetTop uses offsetParent(), not the containing scroller
    // TODO: view timeline progress calculation: (currentScrollOffset - coverRangeStart) / (coverRangeEnd - coverRangeStart)

    float currentScrollOffset = axis() == ScrollAxis::Block ? sourceScrollableArea->scrollPosition().y() : sourceScrollableArea->scrollPosition().x();
    float scrollContainerSize = axis() == ScrollAxis::Block ? sourceScrollableArea->visibleHeight() : sourceScrollableArea->visibleWidth();

    float subjectOffset = axis() == ScrollAxis::Block ? subjectRenderBox->offsetTop() : subjectRenderBox->offsetLeft();

    float subjectSize = axis() == ScrollAxis::Block ? subjectRenderBox->borderBoxRect().height() : subjectRenderBox->borderBoxRect().width();

    auto insetStart = m_insets.start.value_or(Length());
    auto insetEnd = m_insets.end.value_or(insetStart);

    auto coverRangeStart = subjectOffset - scrollContainerSize + insetEnd.value();
    auto coverRangeEnd = subjectOffset + subjectSize - insetStart.value();

    return { currentScrollOffset, coverRangeStart, coverRangeEnd };
}

const CSSNumericValue& ViewTimeline::startOffset() const
{
    auto data = computeViewTimelineData();
    return CSSNumericFactory::px(data.coverRangeStart);
}

const CSSNumericValue& ViewTimeline::endOffset() const
{
    auto data = computeViewTimelineData();
    return CSSNumericFactory::px(data.coverRangeEnd);
}

} // namespace WebCore
