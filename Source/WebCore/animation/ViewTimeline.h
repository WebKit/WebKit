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

#pragma once

#include <WebCore/CSSNumericValue.h>
#include <WebCore/CSSPrimitiveValue.h>
#include <WebCore/ScrollTimeline.h>
#include <WebCore/StyleViewTimelineInsets.h>
#include <WebCore/Styleable.h>
#include <WebCore/ViewTimelineOptions.h>
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Style {
class BuilderState;
enum class SingleAnimationRangeName : uint8_t;
}

class Element;
class StickyPositionViewportConstraints;

struct StickinessAdjustmentData {
    bool operator==(const StickinessAdjustmentData& other) const = default;

    enum class StickinessLocation {
        BeforeEntry,
        DuringEntry,
        WhileContained,
        DuringExit,
        AfterExit
    };

    float entryDistanceAdjustment() const;
    float exitDistanceAdjustment() const;
    float rangeStartAdjustment() const;
    float rangeEndAdjustment() const;

    static StickinessAdjustmentData computeStickinessAdjustmentData(const StickyPositionViewportConstraints&, ScrollTimeline::ResolvedScrollDirection, float scrollContainerSize, float subjectSize, float subjectOffset);

    float stickyTopOrLeftAdjustment { 0 };
    StickinessLocation topOrLeftAdjustmentLocation { StickinessLocation::WhileContained };
    float stickyBottomOrRightAdjustment { 0 };
    StickinessLocation bottomOrRightAdjustmentLocation { StickinessLocation::WhileContained };
};

class ViewTimeline final : public ScrollTimeline {
public:
    static ExceptionOr<Ref<ViewTimeline>> create(Document&, ViewTimelineOptions&& = { });
    static Ref<ViewTimeline> create(const AtomString&, ScrollAxis, const Style::ViewTimelineInsetItem&);

    const Element* subject() const;
    const WeakStyleable subjectStyleable() const { return m_subject; }
    void setSubject(Element*);
    void setSubject(const Styleable&);

    const Style::ViewTimelineInsetItem& insets() const { return m_insets; }
    void setInsets(const Style::ViewTimelineInsetItem& insets) { m_insets = insets; }

    Ref<CSSNumericValue> startOffset() const;
    Ref<CSSNumericValue> endOffset() const;

    AnimationTimeline::ShouldUpdateAnimationsAndSendEvents documentWillUpdateAnimationsAndSendEvents() override;
    AnimationTimelinesController* controller() const override;

    const RenderBox* sourceScrollerRenderer() const;
    CheckedPtr<const RenderElement> stickyContainer() const;
    Element* bindingsSource() const override;
    Element* source() const override;
    Style::SingleAnimationRange defaultRange() const final;

    std::pair<WebAnimationTime, WebAnimationTime> intervalForAttachmentRange(const Style::SingleAnimationRange&) const final;
    std::pair<double, double> offsetIntervalForAttachmentRange(const Style::SingleAnimationRange&) const;
    std::pair<double, double> offsetIntervalForTimelineRangeName(Style::SingleAnimationRangeName) const;

private:
    ScrollTimeline::Data computeTimelineData(UseCachedCurrentTime = UseCachedCurrentTime::Yes) const final;
    std::pair<double, double> intervalForTimelineRangeName(const ScrollTimeline::Data&, Style::SingleAnimationRangeName) const;
    template<typename F> double mapOffsetToTimelineRange(const ScrollTimeline::Data&, Style::SingleAnimationRangeName, F&&) const;

    explicit ViewTimeline(ScrollAxis);
    explicit ViewTimeline(const AtomString&, ScrollAxis, const Style::ViewTimelineInsetItem&);

    bool isViewTimeline() const final { return true; }

    struct CurrentTimeData {
        float scrollOffset { 0 };
        float scrollContainerSize { 0 };
        float subjectOffset { 0 };
        float subjectSize { 0 };
        float insetStart { 0 };
        float insetEnd { 0 };
        StickinessAdjustmentData stickinessData { };
    };

    void cacheCurrentTime();

    struct SpecifiedViewTimelineInsets {
        RefPtr<CSSPrimitiveValue> start;
        RefPtr<CSSPrimitiveValue> end;
    };

    ExceptionOr<SpecifiedViewTimelineInsets> validateSpecifiedInsets(const ViewTimelineInsetValue, const Document&);

    WeakStyleable m_subject;
    std::optional<SpecifiedViewTimelineInsets> m_specifiedInsets;
    Style::ViewTimelineInsetItem m_insets;
    CurrentTimeData m_cachedCurrentTimeData { };
};

WTF::TextStream& operator<<(WTF::TextStream&, const StickinessAdjustmentData&);
WTF::TextStream& operator<<(WTF::TextStream&, const StickinessAdjustmentData::StickinessLocation&);
WTF::TextStream& operator<<(WTF::TextStream&, const ViewTimeline&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_TIMELINE(ViewTimeline, isViewTimeline())
