/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "CSSParserMode.h"
#include <wtf/Forward.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSValue;
struct CSSParserContext;
enum CSSValueID : uint16_t;

namespace CSSPropertyParserHelpers {

// https://drafts.csswg.org/css-counter-styles-3/

// MARK: <counter-style> consumer
RefPtr<CSSValue> consumeCounterStyle(CSSParserTokenRange&, const CSSParserContext&);

// MARK: @counter-style descriptor consumers
AtomString consumeCounterStyleNameInPrelude(CSSParserTokenRange&, CSSParserMode = CSSParserMode::HTMLStandardMode);
RefPtr<CSSValue> consumeCounterStyleName(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterStyleSymbol(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterStyleSystem(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterStyleNegative(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterStyleRange(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterStylePad(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterStyleSymbols(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterStyleAdditiveSymbols(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterStyleSpeakAs(CSSParserTokenRange&, const CSSParserContext&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
