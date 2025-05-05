/*
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

#include "CSSPosition.h"
#include "CSSPropertyParserOptions.h"
#include <optional>
#include <wtf/OptionSet.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSValue;

namespace CSS {
struct PropertyParserState;
}

namespace CSSPropertyParserHelpers {

// MARK: <position> | <bg-position>
// https://drafts.csswg.org/css-values/#position

enum class AllowedPositionKeywords : uint8_t {
    AxisRelative = 1 << 0,
    FlowRelative = 1 << 1,
};

// MARK: <position> (CSSValue)
RefPtr<CSSValue> consumePosition(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-x> (CSSValue)
RefPtr<CSSValue> consumePositionX(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-y> (CSSValue)
RefPtr<CSSValue> consumePositionY(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <background-position-block> (CSSValue)
RefPtr<CSSValue> consumePositionBlock(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <background-position-inline> (CSSValue)
RefPtr<CSSValue> consumePositionInline(CSSParserTokenRange&, CSS::PropertyParserState&);


// MARK: <position> (unresolved)
std::optional<CSS::Position> consumePositionUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);
std::optional<CSS::PositionXY> consumePositionXYUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <bg-position> (unresolved)
std::optional<CSS::Position> consumeBackgroundPositionUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);
std::optional<CSS::PositionXY> consumeBackgroundPositionXYUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-x> (unresolved)
std::optional<CSS::PositionX> consumePositionXUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-y> (unresolved)
std::optional<CSS::PositionY> consumePositionYUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-block> (unresolved)
std::optional<CSS::PositionLogical> consumePositionBlockUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-inline> (unresolved)
std::optional<CSS::PositionLogical> consumePositionInlineUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);


// MARK: Subset / Special case parsers.

// NOTE: This is only used by the `<-webkit-radial-gradient()>` parser. Does not include axis or flow relative keywords.
std::optional<CSS::Position> consumeLegacyBackgroundPositionUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// NOTE: This is only used by the `<transform-origin>` parser. Does not include axis or flow relative keywords.
std::optional<CSS::PositionXY> consumeTransformOriginXY(CSSParserTokenRange&, CSS::PropertyParserState&);

// NOTE: This is only used by the `<horizontal-line-command>` parser. Does not include flow relative keywords.
std::optional<CSS::TwoComponentPositionHorizontal> consumeTwoComponentPositionHorizontalUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// NOTE: This is only used by the `<vertical-line-command>` parser. Does not include flow relative keywords.
std::optional<CSS::TwoComponentPositionVertical> consumeTwoComponentPositionVerticalUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
