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

#include <WebCore/AnimationTimeline.h>
#include <WebCore/Element.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/ScrollAxis.h>
#include <WebCore/ScrollTimelineOptions.h>
#include <WebCore/Styleable.h>
#include <wtf/Ref.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AnimationTimelinesController;
class Document;
class Element;
class RenderStyle;
class ScrollableArea;

class ScrollTimeline : public AnimationTimeline {
public:
    static Ref<ScrollTimeline> create(Document&, ScrollTimelineOptions&& = { });
    static Ref<ScrollTimeline> create(const AtomString&, ScrollAxis);
    static Ref<ScrollTimeline> create(Scroller, ScrollAxis);
    static Ref<ScrollTimeline> createInactiveStyleOriginatedTimeline(const AtomString& name);

    const WeakStyleable& sourceStyleable() const { return m_source; }
    virtual Element* bindingsSource() const;
    virtual Element* source() const;
    void setSource(Element*);
    void setSource(const Styleable&);

    ScrollAxis axis() const { return m_axis; }
    void setAxis(ScrollAxis axis) { m_axis = axis; }

    const AtomString& name() const { return m_name; }
    void setName(const AtomString& name) { m_name = name; }

    bool isInactiveStyleOriginatedTimeline() const { return m_isInactiveStyleOriginatedTimeline; }

    AnimationTimeline::ShouldUpdateAnimationsAndSendEvents documentWillUpdateAnimationsAndSendEvents() override;
    void updateCurrentTimeIfStale();

    AnimationTimelinesController* controller() const override;

    std::optional<WebAnimationTime> currentTime(UseCachedCurrentTime = UseCachedCurrentTime::Yes) override;
    Style::SingleAnimationRange defaultRange() const override;
    WeakPtr<Element, WeakPtrImplWithEventTargetData> timelineScopeDeclaredElement() const { return m_timelineScopeElement; }
    void setTimelineScopeElement(const Element&);
    void clearTimelineScopeDeclaredElement() { m_timelineScopeElement = nullptr; }

    virtual std::pair<WebAnimationTime, WebAnimationTime> intervalForAttachmentRange(const Style::SingleAnimationRange&) const;

    void removeTimelineFromDocument(Element*);

    struct ResolvedScrollDirection {
        bool isVertical;
        bool isReversed;
    };

protected:
    explicit ScrollTimeline(const AtomString&, ScrollAxis);

    struct Data {
        float scrollOffset { 0 };
        float rangeStart { 0 };
        float rangeEnd { 0 };
    };
    virtual Data computeTimelineData(UseCachedCurrentTime = UseCachedCurrentTime::Yes) const;

    static ScrollableArea* scrollableAreaForSourceRenderer(const RenderElement*, Document&);

    ResolvedScrollDirection resolvedScrollDirection() const;

private:
    explicit ScrollTimeline();
    explicit ScrollTimeline(Scroller, ScrollAxis);

    bool isScrollTimeline() const final { return true; }
#if ENABLE(THREADED_ANIMATIONS)
    bool computeCanBeAccelerated() const final;
    Ref<AcceleratedTimeline> createAcceleratedRepresentation() const final;
#endif

    void animationTimingDidChange(WebAnimation&) override;

    struct CurrentTimeData {
        float scrollOffset { 0 };
        float maxScrollOffset { 0 };
    };

    CurrentTimeData computeCurrentTimeData() const;
    void cacheCurrentTime();

    WeakStyleable m_source;
    ScrollAxis m_axis { ScrollAxis::Block };
    AtomString m_name;
    Scroller m_scroller { Scroller::Self };
    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_timelineScopeElement;
    CurrentTimeData m_cachedCurrentTimeData { };
    bool m_isInactiveStyleOriginatedTimeline { false };
};

WTF::TextStream& operator<<(WTF::TextStream&, const ScrollTimeline&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_TIMELINE(ScrollTimeline, isScrollTimeline())
