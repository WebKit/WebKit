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

// MARK: <'text-indent'> consuming
// https://drafts.csswg.org/css-text-3/#text-indent-property
RefPtr<CSSValue> consumeTextIndent(CSSParserTokenRange&, const CSSParserContext&);

// MARK: <'text-transform'> consuming
// https://drafts.csswg.org/css-text-3/#text-transform-property
RefPtr<CSSValue> consumeTextTransform(CSSParserTokenRange&, const CSSParserContext&);

// MARK: <'hanging-punctuation'> consuming
// https://drafts.csswg.org/css-text-3/#propdef-hanging-punctuation
RefPtr<CSSValue> consumeHangingPunctuation(CSSParserTokenRange&, const CSSParserContext&);

// MARK: <'text-autospace'> consuming
// https://drafts.csswg.org/css-text-4/#text-autospace-property
RefPtr<CSSValue> consumeTextAutospace(CSSParserTokenRange&, const CSSParserContext&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
