/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include <array>
#include <optional>

namespace WebCore {

namespace CSS {
struct BorderImageComponents;
struct BorderRadius;
struct PropertyParserState;
}

class CSSParserTokenRange;
class CSSValue;
enum CSSPropertyID : uint16_t;

namespace CSSPropertyParserHelpers {

// Default value of the `fill` parameter for `border-image-slice`.
enum class BorderImageSliceFillDefault : bool { No, Yes };

// Legacy behavior needed by -webkit-border-image that makes fixed border slices also set the border widths.
enum class BorderImageWidthOverridesWidthForLength : bool { No, Yes };

// MARK: - Border Radius

// <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
// https://drafts.csswg.org/css-backgrounds/#propdef-border-radius
std::optional<CSS::BorderRadius> consumeUnresolvedBorderRadius(CSSParserTokenRange&, CSS::PropertyParserState&);

// Non-standard -webkit-border-radius.
std::optional<CSS::BorderRadius> consumeUnresolvedWebKitBorderRadius(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: - Border Image

// <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice
RefPtr<CSSValue> consumeBorderImageSlice(CSSParserTokenRange&, CSS::PropertyParserState&, BorderImageSliceFillDefault = BorderImageSliceFillDefault::No);

// <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width
RefPtr<CSSValue> consumeBorderImageWidth(CSSParserTokenRange&, CSS::PropertyParserState&, BorderImageWidthOverridesWidthForLength = BorderImageWidthOverridesWidthForLength::No);

// https://drafts.csswg.org/css-backgrounds/#border-image
std::optional<CSS::BorderImageComponents> consumeBorderImageComponents(CSSParserTokenRange&, CSS::PropertyParserState&, BorderImageSliceFillDefault = BorderImageSliceFillDefault::No, BorderImageWidthOverridesWidthForLength = BorderImageWidthOverridesWidthForLength::No);

// MARK: - Background Size

// <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
// https://drafts.csswg.org/css-backgrounds/#background-size
RefPtr<CSSValue> consumeSingleBackgroundSize(CSSParserTokenRange&, CSS::PropertyParserState&);

// Non-standard.
RefPtr<CSSValue> consumeSingleWebkitBackgroundSize(CSSParserTokenRange&, CSS::PropertyParserState&);

// <single-mask-size> = <bg-size>
// https://drafts.fxtf.org/css-masking/#the-mask-size
RefPtr<CSSValue> consumeSingleMaskSize(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: - Background Repeat

// <repeat-style> = repeat-x | repeat-y | [repeat | space | round | no-repeat]{1,2}
// https://drafts.csswg.org/css-backgrounds/#typedef-repeat-style
RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: - Shadows

// <'box-shadow'> = none | <shadow>#
// https://drafts.csswg.org/css-backgrounds/#propdef-box-shadow
RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange&, CSS::PropertyParserState&);

// Non-standard.
RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: - Reflection (non-standard)

// Non-standard addition.
RefPtr<CSSValue> consumeWebkitBoxReflect(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: Utilities for filling in rects / quads in the "margin" form.

// - if only 1 value, `a`, is provided, set top, bottom, right & left to `a`.
// - if only 2 values, `a` and `b` are provided, set top & bottom to `a`, right & left to `b`.
// - if only 3 values, `a`, `b`, and `c` are provided, set top to `a`, right to `b`, bottom to `c`, & left to `b`.

template<typename Container, typename T> Container completeQuad(T a)
{
    return Container { a, a, a, a };
}

template<typename Container, typename T> Container completeQuad(T a, T b)
{
    return Container { a, b, a, b };
}

template<typename Container, typename T> Container completeQuad(T a, T b, T c)
{
    return Container { a, b, c, b };
}

template<typename Container, typename T> Container completeQuadFromArray(std::array<std::optional<T>, 4> optionals)
{
    ASSERT(optionals[0].has_value());

    if (!optionals[1])
        return completeQuad<Container>(WTF::move(*optionals[0]));

    if (!optionals[2])
        return completeQuad<Container>(WTF::move(*optionals[0]), WTF::move(*optionals[1]));

    if (!optionals[3])
        return completeQuad<Container>(WTF::move(*optionals[0]), WTF::move(*optionals[1]), WTF::move(*optionals[2]));

    return Container { WTF::move(*optionals[0]), WTF::move(*optionals[1]), WTF::move(*optionals[2]), WTF::move(*optionals[3]) };
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
