/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSValue;

enum CSSValueID : uint16_t;

namespace CSS {
struct PropertyParserState;
}

namespace CSSPropertyParserHelpers {

enum class ValueType {
    Specified,
    Computed
};

// Take two keywords that make up a <position-area> and build a CSSValue that
// minimizes the serialization of position-area. The minimization done depends on
// the context where the value is used.
//
// For all contexts:
// * If one keyword is explicit about its axis, and the other keyword is span-all,
//   only keep the first keyword.
// * If returning a pair of keywords, order the block/X axis keyword before
//   the inline/Y axis keyword.
//
// For context where the computed value is used:
// * If one keyword is explicitly on the block/inline axis, and the other keyword
//   is explicitly on the opposite axis or axisless, remove the explicit-ness.
//   (e.g "block-start inline-end" becomes "start end")
//
// Returns null if the keywords aren't valid/compatible. Otherwise, return a
// CSSPrimitiveValue or CSSValuePair depending on if the keywords can be collapsed.
RefPtr<CSSValue> valueForPositionArea(CSSValueID, CSSValueID, ValueType);

// MARK: <'position-area'>
// https://drafts.csswg.org/css-anchor-position-1/#propdef-position-area
RefPtr<CSSValue> consumePositionArea(CSSParserTokenRange&, CSS::PropertyParserState&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
