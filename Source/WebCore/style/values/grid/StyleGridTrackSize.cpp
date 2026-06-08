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
#include "CSSGridTrackSize.h"
#include "CSSKeywordValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

template<> struct ToCSS<GridMinMaxFunctionParameters> { auto operator()(const GridMinMaxFunctionParameters&, const RenderStyle&) -> CSS::GridMinMaxFunctionParameters; };
template<> struct ToStyle<CSS::GridMinMaxFunctionParameters> { auto operator()(const CSS::GridMinMaxFunctionParameters&, const BuilderState&) -> GridMinMaxFunctionParameters; };

template<> struct ToCSS<GridFitContentFunctionParameters> { auto operator()(const GridFitContentFunctionParameters&, const RenderStyle&) -> CSS::GridFitContentFunctionParameters; };
template<> struct ToStyle<CSS::GridFitContentFunctionParameters> { auto operator()(const CSS::GridFitContentFunctionParameters&, const BuilderState&) -> GridFitContentFunctionParameters; };

auto ToCSS<GridMinMaxFunctionParameters>::operator()(const GridMinMaxFunctionParameters& value, const RenderStyle& style) -> CSS::GridMinMaxFunctionParameters
{
    return {
        .min = toCSS(value.min, style),
        .max = toCSS(value.max, style),
    };
}

auto ToStyle<CSS::GridMinMaxFunctionParameters>::operator()(const CSS::GridMinMaxFunctionParameters& value, const BuilderState& state) -> GridMinMaxFunctionParameters
{
    return {
        .min = toStyle(value.min, state),
        .max = toStyle(value.max, state),
    };
}

auto ToCSS<GridFitContentFunctionParameters>::operator()(const GridFitContentFunctionParameters& value, const RenderStyle& style) -> CSS::GridFitContentFunctionParameters
{
    return value.value.switchOnUsingSpecified(
        [&](const LengthPercentage<CSS::Nonnegative>& lengthPercentage) -> CSS::GridFitContentFunctionParameters {
            return {
                .value = toCSS(lengthPercentage, style),
            };
        }
    );
}

auto ToStyle<CSS::GridFitContentFunctionParameters>::operator()(const CSS::GridFitContentFunctionParameters& value, const BuilderState& state) -> GridFitContentFunctionParameters
{
    return {
        .value = toStyle(value.value, state),
    };
}

auto ToCSS<GridTrackSize>::operator()(const GridTrackSize& value, const RenderStyle& style) -> CSS::GridTrackSize
{
    return WTF::switchOn(value,
        [&](const GridTrackBreadth& breadth) -> CSS::GridTrackSize {
            return toCSS(breadth, style);
        },
        [&](const GridMinMaxFunction& function) -> CSS::GridTrackSize {
            return toCSS(function, style);
        },
        [&](const GridFitContentFunction& function) -> CSS::GridTrackSize {
            return toCSS(function, style);
        }
    );
}

auto ToStyle<CSS::GridTrackSize>::operator()(const CSS::GridTrackSize& value, const BuilderState& state) -> GridTrackSize
{
    return WTF::switchOn(value,
        [&](const CSS::GridTrackBreadth& breadth) -> GridTrackSize {
            return toStyle(breadth, state);
        },
        [&](const CSS::GridMinMaxFunction& function) -> GridTrackSize {
            return toStyle(function, state);
        },
        [&](const CSS::GridFitContentFunction& function) -> GridTrackSize {
            return toStyle(function, state);
        }
    );
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
            .parameters = {
                .value = Style::blend(from.fitContentTrackLength(), to.fitContentTrackLength(), context)
            }
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
