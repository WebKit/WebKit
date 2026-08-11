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
#include "ViewTimeline.h"

#include "AnimationTimelinesController.h"
#include "CSSKeywordValueInlines.h"
#include "CSSNumericFactory.h"
#include "Document.h"
#include "Element.h"
#include "FloatQuad.h"
#include "LegacyRenderSVGModelObject.h"
#include "RenderBlock.h"
#include "RenderBoxModelObject.h"
#include "RenderElementInlines.h"
#include "RenderLayerScrollableArea.h"
#include "RenderSVGModelObject.h"
#include "ScrollAnchoringController.h"
#include "ScrollingConstraints.h"
#include "StyleableInlines.h"
#include "StyleBuilderState.h"
#include "StyleKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StyleScrollPadding.h"
#include "StyleSingleAnimationRange.h"
#include "ViewTimelineOptions.h"
#include "WebAnimation.h"

namespace WebCore {

ExceptionOr<Ref<ViewTimeline>> ViewTimeline::create(Document& document, ViewTimelineOptions&& options)
{
    auto insets = validateViewTimelineInset(WTF::move(options.inset), document);
    if (!insets)
        return Exception { ExceptionCode::TypeError };

    auto viewTimeline = ViewTimeline::create(nullAtom(), options.axis, WTF::move(*insets), Style::ZoomFactor::none());

    viewTimeline->setSubject(options.subject.get());
    if (auto subject = options.subject)
        protect(subject->document())->updateLayoutIgnorePendingStylesheets();
    viewTimeline->cacheCurrentTime();

    return viewTimeline;
}

Ref<ViewTimeline> ViewTimeline::create(const AtomString& name, ScrollAxis axis, const Style::ViewTimelineInsetItem& insets, const Style::ZoomFactor& usedZoomForLength)
{
    return adoptRef(*new ViewTimeline(name, axis, insets, usedZoomForLength));
}

ViewTimeline::ViewTimeline(const AtomString& name, ScrollAxis axis, const Style::ViewTimelineInsetItem& insets, const Style::ZoomFactor& usedZoomForLength)
    : ScrollTimeline(name, axis)
    , m_insets({ .insets = insets, .zoom = usedZoomForLength })
{
}

const Element* ViewTimeline::subject() const
{
    if (auto subject = m_subject.styleable())
        return &subject->element;
    return nullptr;
}

void ViewTimeline::setSubject(Element* subject)
{
    if (subject)
        setSubject(Styleable::fromElement(*subject));
    else {
        removeTimelineFromDocument(protect(m_subject.element().get()));
        m_subject = WeakStyleable();
    }
}

void ViewTimeline::setSubject(const Styleable& styleable)
{
    if (m_subject == styleable)
        return;

    auto previousSubject = m_subject.element();
    m_subject = styleable;

    if (previousSubject && &previousSubject->document() == &styleable.element.document())
        return;

    removeTimelineFromDocument(protect(previousSubject.get()));

    protect(styleable.element.document())->ensureTimelinesController().addTimeline(*this);
}

AnimationTimelinesController* ViewTimeline::controller() const
{
    if (auto subject = m_subject.styleable())
        return &protect(subject->element.document())->ensureTimelinesController();
    return nullptr;
}

StickinessAdjustmentData StickinessAdjustmentData::computeStickinessAdjustmentData(const StickyPositionViewportConstraints& constraints, ScrollTimeline::ResolvedScrollDirection scrollDirection, float scrollContainerSize, float subjectSize, float subjectOffset)
{
    // For a sticky container, determine the amount of adjustment that is possible, which is the distance from the edge of the sticky container
    // to the edge of its containing block. We also need to determine where the subject element is relative to the scroller when the stickiness
    // occurs, so that we can properly adjust the start and end of the range, as well as for a specific animation-range.

    StickinessAdjustmentData data;

    auto computeSubjectStickinessLocation = [] (float stickyBoxStuckPosition, float stickyBoxStaticPosition, float scrollContainerSize, float subjectSize, float subjectOffset) {
        float subjectPositionInScroller = stickyBoxStuckPosition + subjectOffset - stickyBoxStaticPosition;
        if (subjectPositionInScroller > scrollContainerSize)
            return StickinessLocation::BeforeEntry;
        if (subjectPositionInScroller < 0 && subjectPositionInScroller + subjectSize > scrollContainerSize)
            return StickinessLocation::WhileCovering;
        if (subjectPositionInScroller + subjectSize > scrollContainerSize)
            return StickinessLocation::DuringEntry;
        if (subjectPositionInScroller + subjectSize < 0)
            return StickinessLocation::AfterExit;
        if (subjectPositionInScroller < 0)
            return StickinessLocation::DuringExit;
        return StickinessLocation::WhileContained;
    };

    if (scrollDirection.isVertical) {
        if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeTop)) {
            data.stickyTopOrLeftAdjustment = constraints.containingBlockRect().maxY() - constraints.stickyBoxRect().maxY();
            data.topOrLeftAdjustmentLocation = computeSubjectStickinessLocation(constraints.topOffset(), constraints.stickyBoxRect().y(), scrollContainerSize, subjectSize, subjectOffset);
        }
        if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeBottom)) {
            data.stickyBottomOrRightAdjustment = constraints.containingBlockRect().y() - constraints.stickyBoxRect().y();
            data.bottomOrRightAdjustmentLocation = computeSubjectStickinessLocation(scrollContainerSize - constraints.bottomOffset() - constraints.stickyBoxRect().height(), constraints.stickyBoxRect().y(), scrollContainerSize, subjectSize, subjectOffset);
        }
    } else {
        if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeLeft)) {
            data.stickyTopOrLeftAdjustment = constraints.containingBlockRect().maxX() - constraints.stickyBoxRect().maxX();
            data.topOrLeftAdjustmentLocation = computeSubjectStickinessLocation(constraints.leftOffset(), constraints.stickyBoxRect().x(), scrollContainerSize, subjectSize, subjectOffset);
        }
        if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeRight)) {
            data.stickyBottomOrRightAdjustment = constraints.containingBlockRect().x() - constraints.stickyBoxRect().x();
            data.bottomOrRightAdjustmentLocation = computeSubjectStickinessLocation(scrollContainerSize - constraints.rightOffset() - constraints.stickyBoxRect().width(), constraints.stickyBoxRect().x(), scrollContainerSize, subjectSize, subjectOffset);
        }
    }
    return data;
}

float StickinessAdjustmentData::entryDistanceAdjustment() const
{
    float entryDistanceAdjustment = 0;
    if (topOrLeftAdjustmentLocation == StickinessLocation::DuringEntry || topOrLeftAdjustmentLocation == StickinessLocation::WhileCovering)
        entryDistanceAdjustment += stickyTopOrLeftAdjustment;
    if (bottomOrRightAdjustmentLocation == StickinessLocation::DuringEntry || bottomOrRightAdjustmentLocation == StickinessLocation::WhileCovering)
        entryDistanceAdjustment -= stickyBottomOrRightAdjustment;
    return entryDistanceAdjustment;
}

float StickinessAdjustmentData::exitDistanceAdjustment() const
{
    float exitDistanceAdjustment = 0;
    if (topOrLeftAdjustmentLocation == StickinessLocation::DuringExit || topOrLeftAdjustmentLocation == StickinessLocation::WhileCovering)
        exitDistanceAdjustment += stickyTopOrLeftAdjustment;
    if (bottomOrRightAdjustmentLocation == StickinessLocation::DuringExit || bottomOrRightAdjustmentLocation == StickinessLocation::WhileCovering)
        exitDistanceAdjustment -= stickyBottomOrRightAdjustment;
    return exitDistanceAdjustment;
}

float StickinessAdjustmentData::rangeStartAdjustment() const
{
    auto rangeStartAdjustment = 0;
    if (topOrLeftAdjustmentLocation == StickinessLocation::BeforeEntry)
        rangeStartAdjustment += stickyTopOrLeftAdjustment;
    if (bottomOrRightAdjustmentLocation != StickinessLocation::BeforeEntry)
        rangeStartAdjustment += stickyBottomOrRightAdjustment;
    return rangeStartAdjustment;
}

float StickinessAdjustmentData::rangeEndAdjustment() const
{
    auto rangeEndAdjustment = 0;
    if (topOrLeftAdjustmentLocation != StickinessLocation::AfterExit)
        rangeEndAdjustment += stickyTopOrLeftAdjustment;
    if (bottomOrRightAdjustmentLocation == StickinessLocation::AfterExit)
        rangeEndAdjustment += stickyBottomOrRightAdjustment;
    return rangeEndAdjustment;
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
        auto subject = m_subject.styleable();
        if (!subject)
            return { };

        CheckedPtr subjectRenderer = subject->renderer();
        if (!subjectRenderer)
            return { };

        CheckedPtr sourceRenderer = sourceScrollerRenderer();
        CheckedPtr sourceScrollableArea = scrollableAreaForSourceRenderer(sourceRenderer.get(), protect(subject->element.document()));
        if (!sourceScrollableArea)
            return { };

        auto scrollDirection = resolvedScrollDirection();
        float scrollOffset = scrollDirection.isVertical ? sourceScrollableArea->scrollOffset().y() : sourceScrollableArea->scrollOffset().x();
        float maxScrollOffset = scrollDirection.isVertical ? sourceScrollableArea->maximumScrollOffset().y() : sourceScrollableArea->maximumScrollOffset().x();
        float scrollContainerSize = scrollDirection.isVertical ? sourceScrollableArea->visibleHeight() : sourceScrollableArea->visibleWidth();

        // https://drafts.csswg.org/scroll-animations-1/#view-timelines-ranges
        // Transforms and sticky position offsets are ignored, but relative and absolute positioning are accounted for.
        OptionSet<MapCoordinatesMode> options { MapCoordinatesMode::IgnoreStickyOffsets };
        auto subjectOffsetFromSource = subjectRenderer->localToContainerPoint(pointForLocalToContainer(*sourceScrollableArea), sourceRenderer.get(), options);
        float subjectOffset = scrollDirection.isVertical ? subjectOffsetFromSource.y() : subjectOffsetFromSource.x();

        // Ensure borders are subtracted.
        auto scrollerPaddingBoxOrigin = sourceRenderer->paddingBoxRect().location();
        subjectOffset -= scrollDirection.isVertical ? scrollerPaddingBoxOrigin.y() : scrollerPaddingBoxOrigin.x();

        auto subjectBounds = [&] -> FloatSize {
            // For an SVG subject, map its local box through the SVG transform chain so the size stays
            // consistent with the (already transform-aware) offset, e.g. a rotated <foreignObject>.
            auto svgLocalBounds = [&]() -> std::optional<FloatRect> {
                if (auto* subjectRenderSVGModelObject = dynamicDowncast<RenderSVGModelObject>(subjectRenderer.get()))
                    return subjectRenderSVGModelObject->borderBoxRectEquivalent();
                if (subjectRenderer->isRenderOrLegacyRenderSVGForeignObject() || is<LegacyRenderSVGModelObject>(subjectRenderer.get()))
                    return subjectRenderer->objectBoundingBox();
                return std::nullopt;
            }();
            if (svgLocalBounds)
                return subjectRenderer->localToContainerQuad(FloatQuad { *svgLocalBounds }, sourceRenderer.get(), options).boundingBox().size();
            if (CheckedPtr subjectRenderBoxModelObject = dynamicDowncast<RenderBoxModelObject>(subjectRenderer.get()))
                return subjectRenderBoxModelObject->borderBoundingBox().size();
            return { };
        }();

        auto subjectSize = scrollDirection.isVertical ? subjectBounds.height() : subjectBounds.width();

        auto scrollPaddingStart = [&] {
            CheckedRef style = sourceRenderer->style();
            return Style::evaluate<float>(scrollDirection.isVertical ? style->scrollPaddingTop() : style->scrollPaddingLeft(), scrollContainerSize, style->usedZoomForLength());
        };
        auto scrollPaddingEnd = [&] {
            CheckedRef style = sourceRenderer->style();
            return Style::evaluate<float>(scrollDirection.isVertical ? style->scrollPaddingBottom() : style->scrollPaddingRight(), scrollContainerSize, style->usedZoomForLength());
        };

        float insetStart = 0;
        float insetEnd = 0;

        if (m_insets.insets.start().isAuto())
            insetStart = scrollPaddingStart();
        else
            insetStart = Style::evaluate<float>(m_insets.insets.start(), scrollContainerSize, m_insets.zoom);

        if (m_insets.insets.end().isAuto())
            insetEnd = scrollPaddingEnd();
        else
            insetEnd = Style::evaluate<float>(m_insets.insets.end(), scrollContainerSize, m_insets.zoom);

        StickinessAdjustmentData stickyData;
        if (CheckedPtr stickyContainer = dynamicDowncast<RenderBoxModelObject>(this->stickyContainer().get())) {
            FloatRect constrainingRect = stickyContainer->constrainingRectForStickyPosition();
            StickyPositionViewportConstraints constraints;
            stickyContainer->computeStickyPositionConstraints(constraints, constrainingRect);
            stickyData = StickinessAdjustmentData::computeStickinessAdjustmentData(constraints, scrollDirection, scrollContainerSize, subjectSize, subjectOffset);
        }

        return {
            scrollOffset,
            maxScrollOffset,
            scrollContainerSize,
            subjectOffset,
            subjectSize,
            insetStart,
            insetEnd,
            stickyData
        };
    }();

    auto metricsChanged = previousCurrentTimeData.scrollContainerSize != m_cachedCurrentTimeData.scrollContainerSize
        || previousCurrentTimeData.subjectOffset != m_cachedCurrentTimeData.subjectOffset
        || previousCurrentTimeData.subjectSize != m_cachedCurrentTimeData.subjectSize
        || previousCurrentTimeData.insetStart != m_cachedCurrentTimeData.insetStart
        || previousCurrentTimeData.insetEnd != m_cachedCurrentTimeData.insetEnd
        || previousCurrentTimeData.stickinessData != m_cachedCurrentTimeData.stickinessData;

    if (metricsChanged)
        sourceMetricsDidChange();
}

WebAnimationTime ViewTimeline::epsilon() const
{
    if (!m_cachedCurrentTimeData.subjectSize)
        return WebAnimationTime::fromPercentage(0);
    // The metrics reported for the subject and scroll container can be the subject of multiple conversions
    // along the way, so we compute a percentage value that can be used in WebAnimation::currentTime() to round
    // values around the 0% and 100% thresholds. To that end, we'll allow for a 0.1pt tolerance.
    float pointTolerance = 0.1;
    return WebAnimationTime::fromPercentage(pointTolerance / m_cachedCurrentTimeData.subjectSize * 100);
}

AnimationTimeline::ShouldUpdateAnimationsAndSendEvents ViewTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    cacheCurrentTime();
    if (m_subject.element() && m_subject.element()->isConnected())
        return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
    return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No;
}

Style::SingleAnimationRange ViewTimeline::defaultRange() const
{
    return Style::SingleAnimationRange::defaultForViewTimeline();
}

RefPtr<Element> ViewTimeline::bindingsSource() const
{
    if (auto subject = m_subject.styleable())
        protect(subject->element.document())->updateStyleIfNeeded();
    return ScrollTimeline::bindingsSource();
}

RefPtr<Element> ViewTimeline::source() const
{
    if (CheckedPtr sourceRender = sourceScrollerRenderer())
        return sourceRender->element();
    return nullptr;
}

const RenderBox* ViewTimeline::sourceScrollerRenderer() const
{
    auto subject = m_subject.styleable();
    if (!subject)
        return nullptr;

    CheckedPtr subjectRenderer = subject->renderer();
    if (!subjectRenderer)
        return { };

    // https://drafts.csswg.org/scroll-animations-1/#dom-scrolltimeline-source
    // Determine source renderer by looking for the nearest ancestor that establishes a scroll container
    return subjectRenderer->enclosingScrollableContainer();
}

CheckedPtr<const RenderElement> ViewTimeline::stickyContainer() const
{
    auto subject = m_subject.styleable();
    if (!subject)
        return nullptr;

    CheckedPtr renderer = subject->renderer();

    CheckedPtr scrollerRenderer = sourceScrollerRenderer();
    while (renderer && renderer.get() != scrollerRenderer) {
        if (renderer->isStickilyPositioned())
            return renderer;
        renderer = renderer->containingBlock();
    }
    return nullptr;
}

ScrollTimeline::Data ViewTimeline::computeTimelineData(UseCachedCurrentTime) const
{
    // FIXME: account for UseCachedCurrentTime parameter.
    if (!m_cachedCurrentTimeData.scrollOffset && !m_cachedCurrentTimeData.scrollContainerSize)
        return { };

    auto rangeStart = m_cachedCurrentTimeData.subjectOffset - m_cachedCurrentTimeData.scrollContainerSize;
    auto range = m_cachedCurrentTimeData.subjectSize + m_cachedCurrentTimeData.scrollContainerSize;
    auto rangeEnd = rangeStart + range;

    return {
        m_cachedCurrentTimeData.scrollOffset,
        rangeStart + m_cachedCurrentTimeData.insetEnd + m_cachedCurrentTimeData.stickinessData.rangeStartAdjustment(),
        rangeEnd - m_cachedCurrentTimeData.insetStart + m_cachedCurrentTimeData.stickinessData.rangeEndAdjustment()
    };
}

std::pair<double, double> ViewTimeline::intervalForTimelineRangeName(const ScrollTimeline::Data& data, const Style::SingleAnimationRangeName name) const
{
    auto subjectRangeStart = [&]() -> double {
        switch (name) {
        case Style::SingleAnimationRangeName::Normal:
        case Style::SingleAnimationRangeName::Omitted:
        case Style::SingleAnimationRangeName::Cover:
        case Style::SingleAnimationRangeName::EntryCrossing:
            return data.rangeStart;
        case Style::SingleAnimationRangeName::Scroll:
            return 0.0;
        case Style::SingleAnimationRangeName::Entry:
            // https://drafts.csswg.org/scroll-animations-1/#valdef-animation-timeline-range-entry
            // 0% is equivalent to 0% of the cover range.
            return intervalForTimelineRangeName(data, Style::SingleAnimationRangeName::Cover).first;
        case Style::SingleAnimationRangeName::Contain:
            return data.rangeStart + m_cachedCurrentTimeData.subjectSize + m_cachedCurrentTimeData.stickinessData.entryDistanceAdjustment();
        case Style::SingleAnimationRangeName::Exit:
            // https://drafts.csswg.org/scroll-animations-1/#valdef-animation-timeline-range-exit
            // 0% is equivalent to 100% of the contain range.
            return intervalForTimelineRangeName(data, Style::SingleAnimationRangeName::Contain).second;
        case Style::SingleAnimationRangeName::ExitCrossing:
            return data.rangeEnd - m_cachedCurrentTimeData.subjectSize - m_cachedCurrentTimeData.stickinessData.exitDistanceAdjustment();
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return 0.0;
    }();

    auto subjectRangeEnd = [&]() -> double {
        switch (name) {
        case Style::SingleAnimationRangeName::Normal:
        case Style::SingleAnimationRangeName::Omitted:
        case Style::SingleAnimationRangeName::Cover:
        case Style::SingleAnimationRangeName::ExitCrossing:
            return data.rangeEnd;
        case Style::SingleAnimationRangeName::Scroll:
            return m_cachedCurrentTimeData.maxScrollOffset;
        case Style::SingleAnimationRangeName::Exit:
            // https://drafts.csswg.org/scroll-animations-1/#valdef-animation-timeline-range-exit
            // 100% is equivalent to 100% of the cover range.
            return intervalForTimelineRangeName(data, Style::SingleAnimationRangeName::Cover).second;
        case Style::SingleAnimationRangeName::Contain:
            return data.rangeEnd - m_cachedCurrentTimeData.subjectSize - m_cachedCurrentTimeData.stickinessData.exitDistanceAdjustment();
        case Style::SingleAnimationRangeName::Entry:
            // https://drafts.csswg.org/scroll-animations-1/#valdef-animation-timeline-range-entry
            // 100% is equivalent to 0% of the contain range.
            return intervalForTimelineRangeName(data, Style::SingleAnimationRangeName::Contain).first;
        case Style::SingleAnimationRangeName::EntryCrossing:
            return data.rangeStart + m_cachedCurrentTimeData.subjectSize + m_cachedCurrentTimeData.stickinessData.entryDistanceAdjustment();
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return 0.0;
    }();

    if (subjectRangeEnd < subjectRangeStart)
        std::swap(subjectRangeStart, subjectRangeEnd);

    return { subjectRangeStart, subjectRangeEnd };
}

template<typename F> double ViewTimeline::mapOffsetToTimelineRange(const ScrollTimeline::Data& data, const Style::SingleAnimationRangeName name, F&& valueWithinSubjectRange) const
{
    auto timelineRange = data.rangeEnd - data.rangeStart;
    ASSERT(timelineRange);
    auto [subjectRangeStart, subjectRangeEnd] = intervalForTimelineRangeName(data, name);
    auto subjectRange = subjectRangeEnd - subjectRangeStart;
    auto positionWithinContainer = subjectRangeStart + valueWithinSubjectRange(subjectRange);
    auto positionWithinTimelineRange = positionWithinContainer - data.rangeStart;
    return positionWithinTimelineRange / timelineRange;
}

std::pair<double, double> ViewTimeline::offsetIntervalForTimelineRangeName(const Style::SingleAnimationRangeName name) const
{
    auto data = computeTimelineData();
    auto computeOffset = [&](double offset) {
        return mapOffsetToTimelineRange(data, name, [&](const float& subjectRange) {
            return offset * subjectRange;
        });
    };
    return { computeOffset(0), computeOffset(1) };
}

std::pair<double, double> ViewTimeline::offsetIntervalForAttachmentRange(const ResolvableTimelineRange& resolvableTimelineRange) const
{
    auto data = computeTimelineData();
    auto timelineRange = data.rangeEnd - data.rangeStart;
    ASSERT(timelineRange);

    auto offsetForSingleTimelineRange = [&](const auto& edge, auto zoom) {
        auto [conversionRangeStart, conversionRangeEnd] = intervalForTimelineRangeName(data, edge.name());
        auto conversionRange = conversionRangeEnd - conversionRangeStart;
        auto convertedValue = Style::evaluate<float>(edge.offset(), conversionRange, zoom);
        auto position = conversionRangeStart + convertedValue;
        return (position - data.rangeStart) / timelineRange;
    };

    return {
        offsetForSingleTimelineRange(resolvableTimelineRange.start, resolvableTimelineRange.startZoom),
        offsetForSingleTimelineRange(resolvableTimelineRange.end, resolvableTimelineRange.endZoom)
    };
}

std::pair<WebAnimationTime, WebAnimationTime> ViewTimeline::intervalForAttachmentRange(const ResolvableTimelineRange& resolvableTimelineRange) const
{
    // https://drafts.csswg.org/scroll-animations-1/#view-timelines-ranges
    auto data = computeTimelineData();
    auto timelineRange = data.rangeEnd - data.rangeStart;
    if (!timelineRange)
        return { WebAnimationTime::fromPercentage(0), WebAnimationTime::fromPercentage(100) };

    auto computeTime = [&](const auto& edge, auto zoom) {
        auto mappedOffset = mapOffsetToTimelineRange(data, edge.name(), [&](const float& subjectRange) {
            return Style::evaluate<float>(edge.offset(), subjectRange, zoom);
        });
        return WebAnimationTime::fromPercentage(mappedOffset * 100);
    };

    if (resolvableTimelineRange.isDefault()) {
        auto range = defaultRange();
        return {
            computeTime(range.start, Style::ZoomFactor::none()),
            computeTime(range.end, Style::ZoomFactor::none()),
        };
    }

    return {
        computeTime(resolvableTimelineRange.start, resolvableTimelineRange.startZoom),
        computeTime(resolvableTimelineRange.end, resolvableTimelineRange.endZoom),
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

bool ViewTimeline::matchesAnonymousViewFunctionForSubject(const Style::ViewFunction& viewFunction, const Style::ZoomFactor& usedZoomForLength, const Styleable& subject) const
{
    return isStyleOriginated()
        && name().isEmpty()
        && m_insets.insets == viewFunction->insets
        && m_insets.zoom == usedZoomForLength
        && axis() == viewFunction->axis
        && m_subject.styleable() == subject;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const StickinessAdjustmentData& stickiness)
{
    ts << "[ TopOrLeftAdjustment: "_s << stickiness.stickyTopOrLeftAdjustment << ", TopOrLeftLocation: "_s << stickiness.topOrLeftAdjustmentLocation << ", BottomOrRightAdjustment: "_s << stickiness.stickyBottomOrRightAdjustment << ", BottomOrRightLocation: "_s << stickiness.bottomOrRightAdjustmentLocation << " ]"_s;
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const StickinessAdjustmentData::StickinessLocation& stickiness)
{
    switch (stickiness) {
    case StickinessAdjustmentData::StickinessLocation::BeforeEntry: ts << "BeforeEntry"_s; break;
    case StickinessAdjustmentData::StickinessLocation::DuringEntry: ts << "DuringEntry"_s; break;
    case StickinessAdjustmentData::StickinessLocation::WhileContained: ts << "WhileContained"_s; break;
    case StickinessAdjustmentData::StickinessLocation::WhileCovering: ts << "WhileCovering"_s; break;
    case StickinessAdjustmentData::StickinessLocation::DuringExit: ts << "DuringExit"_s; break;
    case StickinessAdjustmentData::StickinessLocation::AfterExit: ts << "AfterExit"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const ViewTimeline& timeline)
{
    return ts << timeline.name() << ' ' << timeline.axis() << ' ' << timeline.insets().insets;
}

} // namespace WebCore
