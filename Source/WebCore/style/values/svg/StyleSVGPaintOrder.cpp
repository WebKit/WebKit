/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleSVGPaintOrder.h"

#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

std::span<const PaintType, 3> SVGPaintOrder::paintTypes() const
{
    static constexpr std::array fillStrokeMarkers { PaintType::Fill, PaintType::Stroke, PaintType::Markers };
    static constexpr std::array fillMarkersStroke { PaintType::Fill, PaintType::Markers, PaintType::Stroke };
    static constexpr std::array strokeFillMarkers { PaintType::Stroke, PaintType::Fill, PaintType::Markers };
    static constexpr std::array strokeMarkersFill { PaintType::Stroke, PaintType::Markers, PaintType::Fill };
    static constexpr std::array markersFillStroke { PaintType::Markers, PaintType::Fill, PaintType::Stroke };
    static constexpr std::array markersStrokeFill { PaintType::Markers, PaintType::Stroke, PaintType::Fill };

    switch (m_type) {
    case Type::Normal:
    case Type::FillStrokeMarkers:
        return fillStrokeMarkers;
    case Type::FillMarkersStroke:
        return fillMarkersStroke;
    case Type::StrokeFillMarkers:
        return strokeFillMarkers;
    case Type::StrokeMarkersFill:
        return strokeMarkersFill;
    case Type::MarkersFillStroke:
        return markersFillStroke;
    case Type::MarkersStrokeFill:
        return markersStrokeFill;
    };
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

auto CSSValueConversion<SVGPaintOrder>::operator()(BuilderState& state, const CSSValue& value) -> SVGPaintOrder
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueFill:
            return CSS::Keyword::Fill { };
        case CSSValueStroke:
            return CSS::Keyword::Stroke { };
        case CSSValueMarkers:
            return CSS::Keyword::Markers { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue, 1>(state, value);
    if (!list)
        return CSS::Keyword::Normal { };

    switch (auto& first = list->item(0); first.valueID()) {
    case CSSValueFill:
        if (list->size() > 1) {
            switch (auto& second = list->item(1); second.valueID()) {
            case CSSValueMarkers:
                return { CSS::Keyword::Fill { }, CSS::Keyword::Markers { } };
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Normal { };
            }
        }
        return CSS::Keyword::Fill { };
    case CSSValueStroke:
        if (list->size() > 1) {
            switch (auto& second = list->item(1); second.valueID()) {
            case CSSValueMarkers:
                return { CSS::Keyword::Stroke { }, CSS::Keyword::Markers { } };
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Normal { };
            }
        }
        return CSS::Keyword::Stroke { };
    case CSSValueMarkers:
        if (list->size() > 1) {
            switch (auto& second = list->item(1); second.valueID()) {
            case CSSValueStroke:
                return { CSS::Keyword::Markers { }, CSS::Keyword::Stroke { } };
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Normal { };
            }
        }
        return CSS::Keyword::Markers { };
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }
}

} // namespace Style
} // namespace WebCore
