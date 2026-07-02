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

#include <wtf/Forward.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSValue;

namespace CSS {
struct MaskBorderOutset;
struct MaskBorderRepeat;
struct MaskBorderSlice;
struct MaskBorderSource;
struct MaskBorderWidth;
struct MaskBorder;
struct PropertyParserState;
}

namespace CSSPropertyParserHelpers {

enum class MaskBorderSliceOverride : bool { None, AlwaysFill };

// <'clip'> = <rect()> | auto
// https://drafts.csswg.org/css-masking/#propdef-clip
RefPtr<CSSValue> consumeClip(CSSParserTokenRange&, CSS::PropertyParserState&);

// <'clip-path'> = none | <clip-source> | [ <basic-shape> || <geometry-box> ]
// https://drafts.csswg.org/css-masking/#propdef-clip-path
RefPtr<CSSValue> consumeClipPath(CSSParserTokenRange&, CSS::PropertyParserState&);

// <'mask-border-source'> = none | <image>
// https://drafts.csswg.org/css-masking-1/#propdef-mask-border-source
std::optional<CSS::MaskBorderSource> consumeUnresolvedMaskBorderSource(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeMaskBorderSource(CSSParserTokenRange&, CSS::PropertyParserState&);

// <'mask-border-outset'> = [ <length [0,∞]> | <number [0,∞]> ]{1,4}
// https://drafts.csswg.org/css-masking-1/#propdef-mask-border-outset
std::optional<CSS::MaskBorderOutset> consumeUnresolvedMaskBorderOutset(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeMaskBorderOutset(CSSParserTokenRange&, CSS::PropertyParserState&);

// <'mask-border-repeat'> = [ stretch | repeat | round | space ]{1,2}
// https://drafts.csswg.org/css-masking-1/#propdef-mask-border-repeat
std::optional<CSS::MaskBorderRepeat> consumeUnresolvedMaskBorderRepeat(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeMaskBorderRepeat(CSSParserTokenRange&, CSS::PropertyParserState&);

// <'mask-border-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
// https://drafts.csswg.org/css-masking-1/#propdef-mask-border-slice
std::optional<CSS::MaskBorderSlice> consumeUnresolvedMaskBorderSlice(CSSParserTokenRange&, CSS::PropertyParserState&, MaskBorderSliceOverride = MaskBorderSliceOverride::None);
RefPtr<CSSValue> consumeMaskBorderSlice(CSSParserTokenRange&, CSS::PropertyParserState&, MaskBorderSliceOverride = MaskBorderSliceOverride::None);

// <'mask-border-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
// https://drafts.csswg.org/css-masking-1/#propdef-mask-border-width
std::optional<CSS::MaskBorderWidth> consumeUnresolvedMaskBorderWidth(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeMaskBorderWidth(CSSParserTokenRange&, CSS::PropertyParserState&);

// <'mask-border'> = <'mask-border-source'> || <'mask-border-slice'> [ / <'mask-border-width'>? [ / <'mask-border-outset'> ]? ]? || <'mask-border-repeat'> || <'mask-border-mode'>
// https://drafts.csswg.org/css-masking-1/#propdef-mask-border
std::optional<CSS::MaskBorder> consumeUnresolvedMaskBorder(CSSParserTokenRange&, CSS::PropertyParserState&, MaskBorderSliceOverride = MaskBorderSliceOverride::None);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
