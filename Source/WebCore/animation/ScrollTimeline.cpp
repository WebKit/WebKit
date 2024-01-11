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

#include "CSSPrimitiveValueMappings.h"
#include "CSSScrollValue.h"
#include "CSSValuePool.h"
#include "Element.h"

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

Ref<CSSValue> ScrollTimeline::toCSSValue() const
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

} // namespace WebCore
