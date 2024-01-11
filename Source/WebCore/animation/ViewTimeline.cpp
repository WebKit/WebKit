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

#include "CSSNumericFactory.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSUnits.h"
#include "CSSValuePool.h"
#include "CSSViewValue.h"
#include "Element.h"

namespace WebCore {

Ref<ViewTimeline> ViewTimeline::create(ViewTimelineOptions&& options)
{
    return adoptRef(*new ViewTimeline(WTFMove(options)));
}

Ref<ViewTimeline> ViewTimeline::create(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
{
    return adoptRef(*new ViewTimeline(name, axis, WTFMove(insets)));
}

Ref<ViewTimeline> ViewTimeline::createFromCSSValue(const CSSViewValue& cssViewValue)
{
    auto axisValue = cssViewValue.axis();
    auto axis = axisValue ? fromCSSValueID<ScrollAxis>(axisValue->valueID()) : ScrollAxis::Block;

    auto convertInsetValue = [](CSSValue* value) -> std::optional<Length> {
        if (!value)
            return std::nullopt;

        if (value->valueID() == CSSValueAuto)
            return Length();

        auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        if (primitiveValue.isPercentage())
            return Length(primitiveValue.doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
        if (primitiveValue.isLength())
            return Length(primitiveValue.doubleValue(CSSUnitType::CSS_PX), LengthType::Fixed);

        ASSERT_NOT_REACHED();
        return std::nullopt;
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
}

ViewTimeline::ViewTimeline(const AtomString& name, ScrollAxis axis, ViewTimelineInsets&& insets)
    : ScrollTimeline(name, axis)
    , m_startOffset(CSSNumericFactory::px(0))
    , m_endOffset(CSSNumericFactory::px(0))
    , m_insets(WTFMove(insets))
{
}

Ref<CSSValue> ViewTimeline::toCSSValue() const
{
    auto insetCSSValue = [](const std::optional<Length>& inset) -> RefPtr<CSSValue> {
        if (!inset)
            return nullptr;
        return CSSPrimitiveValue::create(*inset);
    };

    return CSSViewValue::create(
        CSSPrimitiveValue::create(toCSSValueID(axis())),
        insetCSSValue(m_insets.start),
        insetCSSValue(m_insets.end)
    );
}

} // namespace WebCore
