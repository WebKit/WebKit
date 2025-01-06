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
#include "WebAnimation.h"

namespace WebCore {

Ref<ViewTimeline> ViewTimeline::create(ViewTimelineOptions&& options)
{
    return adoptRef(*new ViewTimeline(WTFMove(options)));
}

Ref<ViewTimeline> ViewTimeline::create(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
{
    return adoptRef(*new ViewTimeline(name, axis, WTFMove(insets)));
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

static ViewTimelineInsets insetsFromOptions(const std::variant<String, Vector<std::variant<RefPtr<CSSNumericValue>, RefPtr<CSSKeywordValue>>>> inset, RefPtr<Element> element)
{
    if (auto* insetString = std::get_if<String>(&inset)) {
        if (insetString->isEmpty())
            return { };
        CSSTokenizer tokenizer(*insetString);
        auto tokenRange = tokenizer.tokenRange();
        tokenRange.consumeWhitespace();
        auto consumedInset = CSSPropertyParserHelpers::consumeViewTimelineInsetListItem(tokenRange, element->protectedDocument()->cssParserContext());
        if (auto insetPair = dynamicDowncast<CSSValuePair>(consumedInset)) {
            return {
                SingleTimelineRange::lengthForCSSValue(dynamicDowncast<CSSPrimitiveValue>(insetPair->protectedFirst()), element),
                SingleTimelineRange::lengthForCSSValue(dynamicDowncast<CSSPrimitiveValue>(insetPair->protectedSecond()), element)
            };
        }
        return {
            SingleTimelineRange::lengthForCSSValue(dynamicDowncast<CSSPrimitiveValue>(consumedInset), element),
            std::nullopt
        };
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
        m_insets = insetsFromOptions(options.inset, RefPtr { m_subject.get() });
        cacheCurrentTime();
    }
}

ViewTimeline::ViewTimeline(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
    : ScrollTimeline(name, axis)
    , m_insets(WTFMove(insets))
{
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
        auto* sourceScrollableArea = scrollableAreaForSourceRenderer(sourceScrollerRenderer(), m_subject->document());
        if (!sourceScrollableArea)
            return { };
        auto scrollDirection = resolvedScrollDirection();
        if (!scrollDirection)
            return { };

        float scrollOffset = scrollDirection->isVertical ? sourceScrollableArea->scrollOffset().y() : sourceScrollableArea->scrollOffset().x();
        float scrollContainerSize = scrollDirection->isVertical ? sourceScrollableArea->visibleHeight() : sourceScrollableArea->visibleWidth();
        auto subjectOffsetFromSource = subjectRenderer->localToContainerPoint(pointForLocalToContainer(*sourceScrollableArea), sourceScrollerRenderer());
        float subjectOffset = scrollDirection->isVertical ? subjectOffsetFromSource.y() : subjectOffsetFromSource.x();
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

        auto insetStartLength = m_insets.start.value_or(Length());
        auto insetEndLength = m_insets.start.value_or(insetStartLength);
        auto insetStart = floatValueForOffset(insetStartLength, scrollContainerSize);
        auto insetEnd = floatValueForOffset(insetEndLength, scrollContainerSize);

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
    if (auto* sourceRender = sourceScrollerRenderer())
        return sourceRender->element();
    return nullptr;
}

RenderBox* ViewTimeline::sourceScrollerRenderer() const
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
        rangeStart + m_cachedCurrentTimeData.insetStart,
        rangeEnd - m_cachedCurrentTimeData.insetEnd
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

Ref<CSSNumericValue> ViewTimeline::startOffset()
{
    return CSSNumericFactory::px(computeTimelineData().rangeStart);
}

Ref<CSSNumericValue> ViewTimeline::endOffset()
{
    return CSSNumericFactory::px(computeTimelineData().rangeEnd);
}

} // namespace WebCore
