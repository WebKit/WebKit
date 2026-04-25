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

namespace WebCore {

class CSSParserTokenRange;
class CSSValue;

namespace CSS {
struct PropertyParserState;
}

namespace CSSPropertyParserHelpers {

// MARK: <position> | <bg-position>
// https://drafts.csswg.org/css-values/#position

// MARK: <position> (CSSValue)
RefPtr<CSSValue> consumePosition(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-x> (CSSValue)
RefPtr<CSSValue> consumePositionX(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-y> (CSSValue)
RefPtr<CSSValue> consumePositionY(CSSParserTokenRange&, CSS::PropertyParserState&);


// MARK: <position> (unresolved)
std::optional<CSS::Position> consumePositionUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <bg-position> (unresolved)
std::optional<CSS::Position> consumeBackgroundPositionUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-x> (unresolved)
std::optional<CSS::PositionX> consumePositionXUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <position-y> (unresolved)
std::optional<CSS::PositionY> consumePositionYUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);


// MARK: Subset / Special case parsers.

// NOTE: This is only used by the `<-webkit-radial-gradient()>` and `<transform-origin>` parsers.
std::optional<CSS::Position> consumeOneOrTwoComponentPositionUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// NOTE: This is only used by the `<horizontal-line-command>` parser
std::optional<CSS::TwoComponentPositionHorizontal> consumeTwoComponentPositionHorizontalUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

// NOTE: This is only used by the `<vertical-line-command>` parser
std::optional<CSS::TwoComponentPositionVertical> consumeTwoComponentPositionVerticalUnresolved(CSSParserTokenRange&, CSS::PropertyParserState&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
