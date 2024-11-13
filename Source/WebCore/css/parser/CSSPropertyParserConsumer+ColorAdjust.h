/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
struct ColorScheme;
}

class CSSValue;
class CSSParserTokenRange;
struct CSSParserContext;

namespace CSSPropertyParserHelpers {

#if ENABLE(DARK_MODE_CSS)

// <'color-scheme'> = normal | [ light | dark | <custom-ident> ]+ && only?
// https://drafts.csswg.org/css-color-adjust/#propdef-color-scheme

// MARK: <'color-scheme'> consuming (unresolved)
std::optional<CSS::ColorScheme> consumeUnresolvedColorScheme(CSSParserTokenRange&, const CSSParserContext&);

// MARK: <'color-scheme'> parsing (unresolved)
std::optional<CSS::ColorScheme> parseUnresolvedColorScheme(const String&, const CSSParserContext&);

// MARK: <'color-scheme'> consuming (CSSValue)
RefPtr<CSSValue> consumeColorScheme(CSSParserTokenRange&, const CSSParserContext&);

#endif // ENABLE(DARK_MODE_CSS)

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
