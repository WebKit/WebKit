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
#include "CSSPropertyParserConsumer+Timeline.h"
#include "Document.h"
#include "Element.h"
#include "LegacyRenderSVGModelObject.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderInline.h"
#include "RenderLayerScrollableArea.h"
#include "RenderSVGModelObject.h"
#include "ScrollAnchoringController.h"
#include "StyleBuilderConverter.h"
#include "StyleScrollPadding.h"
#include "WebAnimation.h"

namespace WebCore {

static bool isValidInset(RefPtr<CSSPrimitiveValue>& inset)
{
    return !inset || inset->valueID() == CSSValueAuto || inset->isLength() || inset->isPercentage();
}

ExceptionOr<Ref<ViewTimeline>> ViewTimeline::create(Document& document, ViewTimelineOptions&& options)
{
    auto viewTimeline = adoptRef(*new ViewTimeline(options.axis));

    auto specifiedInsetsOrException = viewTimeline->validateSpecifiedInsets(options.inset, document);
    if (specifiedInsetsOrException.hasException())
        return specifiedInsetsOrException.releaseException();

    auto specifiedInsets = specifiedInsetsOrException.releaseReturnValue();
    if (!isValidInset(specifiedInsets.start) || !isValidInset(specifiedInsets.end))
        return Exception { ExceptionCode::TypeError };

    viewTimeline->m_specifiedInsets = WTFMove(specifiedInsets);
    viewTimeline->setSubject(options.subject.get());
    viewTimeline->cacheCurrentTime();

    return viewTimeline;
}

Ref<ViewTimeline> ViewTimeline::create(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
{
    return adoptRef(*new ViewTimeline(name, axis, WTFMove(insets)));
}

ViewTimeline::ViewTimeline(ScrollAxis axis)
    : ScrollTimeline(nullAtom(), axis)
{
}

ViewTimeline::ViewTimeline(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
    : ScrollTimeline(name, axis)
    , m_insets(WTFMove(insets))
{
}

ExceptionOr<ViewTimeline::SpecifiedViewTimelineInsets> ViewTimeline::validateSpecifiedInsets(const ViewTimelineInsetValue inset, const Document& document)
{
    // https://drafts.csswg.org/scroll-animations-1/#dom-viewtimeline-viewtimeline

    // FIXME: note that we use CSSKeywordish instead of CSSKeywordValue to match Chrome,
    // issue being tracked at https://github.com/w3c/csswg-drafts/issues/11477.

    // If a DOMString value is provided as an inset, parse it as a <'view-timeline-inset'> value;
    if (auto* insetString = std::get_if<String>(&inset)) {
        if (insetString->isEmpty())
            return Exception { ExceptionCode::TypeError };
        CSSTokenizer tokenizer(*insetString);
        auto tokenRange = tokenizer.tokenRange();
        tokenRange.consumeWhitespace();
        auto consumedInset = CSSPropertyParserHelpers::consumeViewTimelineInsetListItem(tokenRange, Ref { document }->cssParserContext());
        if (!consumedInset)
            return Exception { ExceptionCode::TypeError };

        if (auto insetPair = dynamicDowncast<CSSValuePair>(consumedInset)) {
            return { {
                dynamicDowncast<CSSPrimitiveValue>(insetPair->protectedFirst()),
                dynamicDowncast<CSSPrimitiveValue>(insetPair->protectedSecond())
            } };
        } else
            return { { dynamicDowncast<CSSPrimitiveValue>(consumedInset), nullptr } };
    }

    auto cssPrimitiveValueForCSSNumericValue = [&](RefPtr<CSSNumericValue> numericValue) -> ExceptionOr<RefPtr<CSSPrimitiveValue>> {
        if (RefPtr insetValue = dynamicDowncast<CSSUnitValue>(*numericValue))
            return dynamicDowncast<CSSPrimitiveValue>(insetValue->toCSSValue());
        return nullptr;
    };

    auto cssPrimitiveValueForCSSKeywordValue = [&](RefPtr<CSSKeywordValue> keywordValue) -> ExceptionOr<RefPtr<CSSPrimitiveValue>> {
        if (keywordValue->value() != "auto"_s)
            return Exception { ExceptionCode::TypeError };
        return nullptr;
    };

    auto cssPrimitiveValueForIndividualInset = [&](ViewTimelineIndividualInset individualInset) -> ExceptionOr<RefPtr<CSSPrimitiveValue>> {
        if (auto* numericInset = std::get_if<RefPtr<CSSNumericValue>>(&individualInset))
            return cssPrimitiveValueForCSSNumericValue(*numericInset);
        if (auto* stringInset = std::get_if<String>(&individualInset))
            return cssPrimitiveValueForCSSKeywordValue(CSSKeywordValue::rectifyKeywordish(*stringInset));
        ASSERT(std::holds_alternative<RefPtr<CSSKeywordValue>>(individualInset));
        return cssPrimitiveValueForCSSKeywordValue(CSSKeywordValue::rectifyKeywordish(std::get<RefPtr<CSSKeywordValue>>(individualInset)));
    };

    // if a sequence is provided, the first value represents the start inset and the second value represents the end inset.
    // If the sequence has only one value, it is duplicated. If it has zero values or more than two values, or if it contains
    // a CSSKeywordValue whose value is not "auto", throw a TypeError.
    auto insetList = std::get<Vector<ViewTimelineIndividualInset>>(inset);
    auto numberOfInsets = insetList.size();

    if (!numberOfInsets || numberOfInsets > 2)
        return Exception { ExceptionCode::TypeError };

    auto startInsetOrException = cssPrimitiveValueForIndividualInset(insetList.at(0));
    if (startInsetOrException.hasException())
        return startInsetOrException.releaseException();
    auto startInset = startInsetOrException.releaseReturnValue();

    if (numberOfInsets == 1)
        return { { startInset, startInset } };

    auto endInsetOrException = cssPrimitiveValueForIndividualInset(insetList.at(1));
    if (endInsetOrException.hasException())
        return endInsetOrException.releaseException();
    auto endInset = endInsetOrException.releaseReturnValue();
    return { { startInset, endInset } };
}

void ViewTimeline::setSubject(const Element* subject)
{
    if (subject == m_subject)
        return;

    RefPtr previousSubject = m_subject.get();
    m_subject = subject;
    RefPtr newSubject = m_subject.get();

    if (previousSubject && newSubject && &previousSubject->document() == &newSubject->document())
        return;

    if (previousSubject) {
        if (CheckedPtr timelinesController = previousSubject->protectedDocument()->timelinesController())
            timelinesController->removeTimeline(*this);
    }

    if (newSubject)
        newSubject->protectedDocument()->ensureTimelinesController().addTimeline(*this);
}

AnimationTimelinesController* ViewTimeline::controller() const
{
    if (m_subject)
        return &m_subject->document().ensureTimelinesController();
    return nullptr;
}

void ViewTimeline::cacheCurrentTime()
{
    auto previousCurrentTimeData = m_cachedCurrentTimeData;

    auto pointForLocalToContainer = [](const ScrollableArea& area) -> FloatPoint {
        // For subscrollers we need to ajust the point fed into localToContainerPoint as
        // the returned value can be outside of the scroller.
        if (is<RenderLayerScrollableArea>(area))
            return area.scrollOffset();
        return { };
    };

    m_cachedCurrentTimeData = [&] -> CurrentTimeData {
        if (!m_subject)
            return { };

        CheckedPtr subjectRenderer = m_subject->renderer();
        if (!subjectRenderer)
            return { };

        CheckedPtr sourceRenderer = sourceScrollerRenderer();
        auto* sourceScrollableArea = scrollableAreaForSourceRenderer(sourceRenderer.get(), m_subject->document());
        if (!sourceScrollableArea)
            return { };

        auto scrollDirection = resolvedScrollDirection();
        if (!scrollDirection)
            return { };

        float scrollOffset = scrollDirection->isVertical ? sourceScrollableArea->scrollOffset().y() : sourceScrollableArea->scrollOffset().x();
        float scrollContainerSize = scrollDirection->isVertical ? sourceScrollableArea->visibleHeight() : sourceScrollableArea->visibleWidth();

        // https://drafts.csswg.org/scroll-animations-1/#view-timelines-ranges
        // Transforms are ignored, but relative and absolute positioning are accounted for.
        OptionSet<MapCoordinatesMode> excludeTransforms { };
        auto subjectOffsetFromSource = subjectRenderer->localToContainerPoint(pointForLocalToContainer(*sourceScrollableArea), sourceRenderer.get(), excludeTransforms);
        float subjectOffset = scrollDirection->isVertical ? subjectOffsetFromSource.y() : subjectOffsetFromSource.x();

        // Ensure borders are subtracted.
        auto scrollerPaddingBoxOrigin = sourceRenderer->paddingBoxRect().location();
        subjectOffset -= scrollDirection->isVertical ? scrollerPaddingBoxOrigin.y() : scrollerPaddingBoxOrigin.x();

        auto subjectBounds = [&] -> FloatSize {
            if (CheckedPtr subjectRenderBox = dynamicDowncast<RenderBox>(subjectRenderer.get()))
                return subjectRenderBox->contentBoxRect().size();
            if (CheckedPtr subjectRenderInline = dynamicDowncast<RenderInline>(subjectRenderer.get()))
                return subjectRenderInline->borderBoundingBox().size();
            if (CheckedPtr subjectRenderSVGModelObject = dynamicDowncast<RenderSVGModelObject>(subjectRenderer.get()))
                return subjectRenderSVGModelObject->borderBoxRectEquivalent().size();
            if (is<LegacyRenderSVGModelObject>(subjectRenderer.get()))
                return subjectRenderer->objectBoundingBox().size();
            return { };
        }();

        auto subjectSize = scrollDirection->isVertical ? subjectBounds.height() : subjectBounds.width();

        if (m_specifiedInsets) {
            RefPtr subject { m_subject.get() };
            auto computedInset = [&](const RefPtr<CSSPrimitiveValue>& specifiedInset) -> std::optional<Length> {
                if (specifiedInset)
                    return SingleTimelineRange::lengthForCSSValue(specifiedInset, subject);
                return { };
            };
            m_insets = { computedInset(m_specifiedInsets->start), computedInset(m_specifiedInsets->end) };
        }

        enum class PaddingEdge : bool { Start, End };
        auto scrollPadding = [&](PaddingEdge edge) {
            auto& style = sourceRenderer->style();
            if (edge == PaddingEdge::Start)
                return scrollDirection->isVertical ? style.scrollPaddingTop() : style.scrollPaddingLeft();
            return scrollDirection->isVertical ? style.scrollPaddingBottom() : style.scrollPaddingRight();
        };

        bool hasInsetsStart = m_insets.start.has_value();
        bool hasInsetsEnd = m_insets.end.has_value();

        float insetStart = 0;
        float insetEnd = 0;
        if (hasInsetsStart && hasInsetsEnd) {
            insetStart = floatValueForOffset(*m_insets.start, scrollContainerSize);
            insetEnd = floatValueForOffset(*m_insets.end, scrollContainerSize);
        } else if (hasInsetsStart) {
            insetStart = floatValueForOffset(*m_insets.start, scrollContainerSize);
            insetEnd = insetStart;
        } else if (hasInsetsEnd) {
            insetStart = Style::evaluate(scrollPadding(PaddingEdge::Start), scrollContainerSize);
            insetEnd = floatValueForOffset(*m_insets.end, scrollContainerSize);
        } else {
            insetStart = Style::evaluate(scrollPadding(PaddingEdge::Start), scrollContainerSize);
            insetEnd = Style::evaluate(scrollPadding(PaddingEdge::End), scrollContainerSize);
        }

        return {
            scrollOffset,
            scrollContainerSize,
            subjectOffset,
            subjectSize,
            insetStart,
            insetEnd
        };
    }();

    auto metricsChanged = previousCurrentTimeData.scrollContainerSize != m_cachedCurrentTimeData.scrollContainerSize
        || previousCurrentTimeData.subjectOffset != m_cachedCurrentTimeData.subjectOffset
        || previousCurrentTimeData.subjectSize != m_cachedCurrentTimeData.subjectSize
        || previousCurrentTimeData.insetStart != m_cachedCurrentTimeData.insetStart
        || previousCurrentTimeData.insetEnd != m_cachedCurrentTimeData.insetEnd;

    if (metricsChanged) {
        for (auto& animation : m_animations)
            animation->progressBasedTimelineSourceDidChangeMetrics();
    }
}

AnimationTimeline::ShouldUpdateAnimationsAndSendEvents ViewTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    cacheCurrentTime();
    if (m_subject && m_subject->isConnected())
        return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
    return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No;
}

TimelineRange ViewTimeline::defaultRange() const
{
    return TimelineRange::defaultForViewTimeline();
}

Element* ViewTimeline::source() const
{
    if (CheckedPtr sourceRender = sourceScrollerRenderer())
        return sourceRender->element();
    return nullptr;
}

const RenderBox* ViewTimeline::sourceScrollerRenderer() const
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

ScrollTimeline::Data ViewTimeline::computeTimelineData() const
{
    if (!m_cachedCurrentTimeData.scrollOffset && !m_cachedCurrentTimeData.scrollContainerSize)
        return { };

    auto rangeStart = m_cachedCurrentTimeData.subjectOffset - m_cachedCurrentTimeData.scrollContainerSize;
    auto range = m_cachedCurrentTimeData.subjectSize + m_cachedCurrentTimeData.scrollContainerSize;
    auto rangeEnd = rangeStart + range;

    return {
        m_cachedCurrentTimeData.scrollOffset,
        rangeStart + m_cachedCurrentTimeData.insetEnd,
        rangeEnd - m_cachedCurrentTimeData.insetStart
    };
}

std::pair<WebAnimationTime, WebAnimationTime> ViewTimeline::intervalForAttachmentRange(const TimelineRange& attachmentRange) const
{
    // https://drafts.csswg.org/scroll-animations-1/#view-timelines-ranges
    auto data = computeTimelineData();
    auto timelineRange = data.rangeEnd - data.rangeStart;
    if (!timelineRange)
        return { WebAnimationTime::fromPercentage(0), WebAnimationTime::fromPercentage(100) };

    auto subjectRangeStartForName = [&](SingleTimelineRange::Name name) {
        switch (name) {
        case SingleTimelineRange::Name::Normal:
        case SingleTimelineRange::Name::Omitted:
        case SingleTimelineRange::Name::Cover:
        case SingleTimelineRange::Name::Entry:
        case SingleTimelineRange::Name::EntryCrossing:
            return data.rangeStart;
        case SingleTimelineRange::Name::Contain:
            return data.rangeStart + m_cachedCurrentTimeData.subjectSize;
        case SingleTimelineRange::Name::Exit:
        case SingleTimelineRange::Name::ExitCrossing:
            return m_cachedCurrentTimeData.subjectOffset - m_cachedCurrentTimeData.insetEnd;
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return 0.f;
    };

    auto subjectRangeEndForName = [&](SingleTimelineRange::Name name) {
        switch (name) {
        case SingleTimelineRange::Name::Normal:
        case SingleTimelineRange::Name::Omitted:
        case SingleTimelineRange::Name::Cover:
        case SingleTimelineRange::Name::Exit:
        case SingleTimelineRange::Name::ExitCrossing:
            return data.rangeEnd;
        case SingleTimelineRange::Name::Contain:
            return m_cachedCurrentTimeData.subjectOffset - m_cachedCurrentTimeData.insetEnd;
        case SingleTimelineRange::Name::Entry:
        case SingleTimelineRange::Name::EntryCrossing:
            return data.rangeStart + m_cachedCurrentTimeData.subjectSize;
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return 0.f;
    };

    auto computeTime = [&](const SingleTimelineRange& rangeToConvert) {
        auto subjectRangeStart = subjectRangeStartForName(rangeToConvert.name);
        auto subjectRangeEnd = subjectRangeEndForName(rangeToConvert.name);
        if (subjectRangeEnd < subjectRangeStart)
            std::swap(subjectRangeStart, subjectRangeEnd);
        auto subjectRange = subjectRangeEnd - subjectRangeStart;

        auto& length = rangeToConvert.offset;
        auto valueWithinSubjectRange = floatValueForOffset(length, subjectRange);
        auto positionWithinContainer = subjectRangeStart + valueWithinSubjectRange;
        auto positionWithinTimelineRange = positionWithinContainer - data.rangeStart;
        auto offsetWithinTimelineRange = positionWithinTimelineRange / timelineRange;
        return WebAnimationTime::fromPercentage(offsetWithinTimelineRange * 100);
    };

    auto attachmentRangeOrDefault = attachmentRange.isDefault() ? defaultRange() : attachmentRange;
    return {
        computeTime(attachmentRangeOrDefault.start),
        computeTime(attachmentRangeOrDefault.end),
    };
}

Ref<CSSNumericValue> ViewTimeline::startOffset() const
{
    return CSSNumericFactory::px(computeTimelineData().rangeStart);
}

Ref<CSSNumericValue> ViewTimeline::endOffset() const
{
    return CSSNumericFactory::px(computeTimelineData().rangeEnd);
}

} // namespace WebCore
