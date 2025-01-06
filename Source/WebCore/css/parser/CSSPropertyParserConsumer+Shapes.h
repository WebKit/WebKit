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
struct CSSParserContext;

namespace CSSPropertyParserHelpers {

enum class PathParsingOption : uint8_t {
    None = 0,
    RejectPathFillRule = 1 << 0,
    RejectPath = 1 << 1,
};

// <basic-shape> = <circle()> | <ellipse() | <inset()> | <path()> | <polygon()> | <rect()> | <shape()> | <xywh()>
// https://drafts.csswg.org/css-shapes/#typedef-basic-shape
RefPtr<CSSValue> consumeBasicShape(CSSParserTokenRange&, const CSSParserContext&, OptionSet<PathParsingOption>);

// <path()> = path( <'fill-rule'>? , <string> )
// https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-path
RefPtr<CSSValue> consumePath(CSSParserTokenRange&, const CSSParserContext&);

// <'shape-outside'> = none | [ <basic-shape> || <shape-box> ] | <image>
// https://drafts.csswg.org/css-shapes/#propdef-shape-outside
RefPtr<CSSValue> consumeShapeOutside(CSSParserTokenRange&, const CSSParserContext&);


} // namespace CSSPropertyParserHelpers
} // namespace WebCore
