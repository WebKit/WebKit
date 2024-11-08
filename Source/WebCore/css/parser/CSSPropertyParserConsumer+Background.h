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
struct BorderRadius;
}

class CSSParserTokenRange;
class CSSValue;
enum CSSPropertyID : uint16_t;
struct CSSParserContext;

namespace CSSPropertyParserHelpers {

// MARK: - Border Radius

// <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
// https://drafts.csswg.org/css-backgrounds/#propdef-border-radius
std::optional<CSS::BorderRadius> consumeUnresolvedBorderRadius(CSSParserTokenRange&, const CSSParserContext&);

// Non-standard -webkit-border-radius.
std::optional<CSS::BorderRadius> consumeUnresolvedWebKitBorderRadius(CSSParserTokenRange&, const CSSParserContext&);

// <'border-[top|bottom]-[left|right]-radius,'> = <length-percentage [0,∞]>{1,2}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-top-left-radius
RefPtr<CSSValue> consumeBorderRadiusCorner(CSSParserTokenRange&, const CSSParserContext&);

// MARK: - Border Image

// <'border-image-repeat> = [ stretch | repeat | round | space ]{1,2}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-repeat
RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange&, const CSSParserContext&);

// <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice
RefPtr<CSSValue> consumeBorderImageSlice(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID currentProperty);

// <'border-image-outset'> = [ <length [0,∞]> | <number [0,∞]> ]{1,4}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-outset
RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange&, const CSSParserContext&);

// <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width
RefPtr<CSSValue> consumeBorderImageWidth(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID currentProperty);

// https://drafts.csswg.org/css-backgrounds/#border-image
bool consumeBorderImageComponents(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID currentProperty, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);

// MARK: - Border Style

// <'border-*-width'> = <line-width>
// https://drafts.csswg.org/css-backgrounds/#propdef-border-top-width
RefPtr<CSSValue> consumeBorderWidth(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID currentShorthand);

// <'border-*-width'> = <line-width>
// https://drafts.csswg.org/css-backgrounds/#propdef-border-top-width
RefPtr<CSSValue> consumeBorderColor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID currentShorthand);

// MARK: - Background Clip

// <single-background-clip> = <visual-box>
// https://drafts.csswg.org/css-backgrounds/#propdef-background-clip
RefPtr<CSSValue> consumeSingleBackgroundClip(CSSParserTokenRange&, const CSSParserContext&);

// <'background-clip'> = <visual-box>#
// https://drafts.csswg.org/css-backgrounds/#propdef-background-clip
RefPtr<CSSValue> consumeBackgroundClip(CSSParserTokenRange&, const CSSParserContext&);

// MARK: - Background Size

// <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
// https://drafts.csswg.org/css-backgrounds/#background-size
RefPtr<CSSValue> consumeSingleBackgroundSize(CSSParserTokenRange&, const CSSParserContext&);

// Non-standard.
RefPtr<CSSValue> consumeSingleWebkitBackgroundSize(CSSParserTokenRange&, const CSSParserContext&);

// <single-mask-size> = <bg-size>
// https://drafts.fxtf.org/css-masking/#the-mask-size
RefPtr<CSSValue> consumeSingleMaskSize(CSSParserTokenRange&, const CSSParserContext&);

// MARK: - Background Repeat

// <repeat-style> = repeat-x | repeat-y | [repeat | space | round | no-repeat]{1,2}
// https://drafts.csswg.org/css-backgrounds/#typedef-repeat-style
RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange&, const CSSParserContext&);

// MARK: - Shadows

// <'box-shadow'> = none | <shadow>#
// https://drafts.csswg.org/css-backgrounds/#propdef-box-shadow
RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange&, const CSSParserContext&);

// MARK: - Reflection (non-standard)

// Non-standard addition.
RefPtr<CSSValue> consumeReflect(CSSParserTokenRange&, const CSSParserContext&);

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
        return completeQuad<Container>(WTFMove(*optionals[0]));

    if (!optionals[2])
        return completeQuad<Container>(WTFMove(*optionals[0]), WTFMove(*optionals[1]));

    if (!optionals[3])
        return completeQuad<Container>(WTFMove(*optionals[0]), WTFMove(*optionals[1]), WTFMove(*optionals[2]));

    return Container { WTFMove(*optionals[0]), WTFMove(*optionals[1]), WTFMove(*optionals[2]), WTFMove(*optionals[3]) };
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
