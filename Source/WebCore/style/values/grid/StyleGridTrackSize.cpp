/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "StyleGridTrackSize.h"

#include "AnimationUtilities.h"
#include "CSSFunctionValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

auto CSSValueConversion<GridTrackSize>::operator()(BuilderState& state, const CSSValue& value) -> GridTrackSize
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return toStyleFromCSSValue<GridTrackSize::Breadth>(state, *primitiveValue);

    auto function = requiredListDowncast<CSSFunctionValue, CSSPrimitiveValue>(state, value);
    if (!function)
        return GridTrackSize::Breadth { 0_css_px };

    if (function->size() == 1) {
        RefPtr length = function->item(0);

        return GridTrackSize::FitContent {
            .parameters = toStyleFromCSSValue<GridTrackFitContentLength>(state, *length),
        };
    }

    RefPtr min = function->item(0);
    RefPtr max = function->item(1);

    return GridTrackSize::MinMax {
        .parameters = {
            .min = toStyleFromCSSValue<GridTrackBreadth>(state, *min),
            .max = toStyleFromCSSValue<GridTrackBreadth>(state, *max),
        }
    };
}

// MARK: - Blending

auto Blending<GridTrackSize>::blend(const GridTrackSize& from, const GridTrackSize& to, const BlendingContext& context) -> GridTrackSize
{
    if (from.type() != to.type())
        return context.progress < 0.5 ? from : to;

    switch (from.type()) {
    case GridTrackSize::Type::Breadth:
        return Style::blend(from.minTrackBreadth(), to.minTrackBreadth(), context);

    case GridTrackSize::Type::FitContent:
        return GridTrackSize::FitContent {
            .parameters = Style::blend(from.fitContentTrackLength(), to.fitContentTrackLength(), context),
        };

    case GridTrackSize::Type::MinMax:
        return GridTrackSize::MinMax {
            .parameters = {
                .min = Style::blend(from.minTrackBreadth(), to.minTrackBreadth(), context),
                .max = Style::blend(from.maxTrackBreadth(), to.maxTrackBreadth(), context),
            }
        };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const GridTrackSize& value)
{
    if (value.isBreadth())
        return ts << "size"_s;
    if (value.isMinMax())
        return ts << "minmax()"_s;
    if (value.isFitContent())
        return ts << "fit-content()"_s;
    return ts;
}

} // namespace Style
} // namespace WebCore
