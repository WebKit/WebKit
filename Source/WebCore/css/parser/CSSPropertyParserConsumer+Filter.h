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

#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

namespace CSS {
struct AppleColorFilter;
struct Filter;
struct PropertyParserState;
}

namespace Style {
struct Filter;
}

class CSSParserTokenRange;
class CSSValue;
class Document;
class FilterOperations;
class RenderStyle;
struct CSSParserContext;

namespace CSSPropertyParserHelpers {

// https://drafts.fxtf.org/filter-effects/#FilterProperty

// MARK: <'filter'> consuming (CSSValue)
RefPtr<CSSValue> consumeFilter(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <'-apple-color-filter'> consuming (CSSValue)
RefPtr<CSSValue> consumeAppleColorFilter(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <'filter'> consuming (unresolved)
std::optional<CSS::Filter> consumeUnresolvedFilter(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <'apple-color-filter'> consuming (unresolved)
std::optional<CSS::AppleColorFilter> consumeUnresolvedAppleColorFilter(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <'filter'> parsing (raw)
std::optional<Style::Filter> parseFilterValueListOrNoneRaw(const String&, const CSSParserContext&, const Document&, RenderStyle&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
