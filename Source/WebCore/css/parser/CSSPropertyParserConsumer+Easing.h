/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSEasingFunction.h"

namespace WebCore {

class CSSParserTokenRange;
class CSSToLengthConversionData;
class CSSValue;
class TimingFunction;

struct CSSParserContext;

namespace CSSPropertyParserHelpers {

// <easing-function> = linear | ease | ease-in | ease-out | ease-in-out | step-start | step-end | <linear()> | <cubic-bezier()> | <steps()>
// NOTE: also includes non-standard <spring()>.
// https://drafts.csswg.org/css-easing/#typedef-easing-function

// MARK: <easing-function> consuming (unresolved)
std::optional<CSS::EasingFunction> consumeUnresolvedEasingFunction(CSSParserTokenRange&, const CSSParserContext&);

// MARK: <easing-function> consuming (CSSValue)
RefPtr<CSSValue> consumeEasingFunction(CSSParserTokenRange&, const CSSParserContext&);

// MARK: <easing-function> parsing (raw)
RefPtr<TimingFunction> parseEasingFunction(const String&, const CSSParserContext&, const CSSToLengthConversionData&);
RefPtr<TimingFunction> parseEasingFunctionDeprecated(const String&, const CSSParserContext&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
