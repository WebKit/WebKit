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

#include "AnimationTimeline.h"
#include "ScrollAxis.h"
#include "ScrollTimelineOptions.h"
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CSSScrollValue;
class Element;

class ScrollTimeline : public AnimationTimeline {
public:
    static Ref<ScrollTimeline> create(ScrollTimelineOptions&& = { });
    static Ref<ScrollTimeline> create(const AtomString&, ScrollAxis);
    static Ref<ScrollTimeline> createFromCSSValue(const CSSScrollValue&);

    Element* source() const { return m_source.get(); }

    ScrollAxis axis() const { return m_axis; }
    void setAxis(ScrollAxis axis) { m_axis = axis; }

    const AtomString& name() const { return m_name; }
    void setName(const AtomString& name) { m_name = name; }

    virtual Ref<CSSValue> toCSSValue() const;

protected:
    explicit ScrollTimeline(const AtomString&, ScrollAxis);

private:
    enum class Scroller : uint8_t { Nearest, Root, Self };

    explicit ScrollTimeline(ScrollTimelineOptions&& = { });
    explicit ScrollTimeline(Scroller, ScrollAxis);

    bool isScrollTimeline() const final { return true; }

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_source;
    ScrollAxis m_axis { ScrollAxis::Block };
    AtomString m_name;
    Scroller m_scroller { Scroller::Nearest };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_TIMELINE(ScrollTimeline, isScrollTimeline())
