/*
// Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

class CSSParserTokenRange;
class CSSToLengthConversionData;
class CSSValue;
struct CSSParserContext;

namespace CSS {
struct PropertyParserState;
}

namespace Style {
struct Transform;
}

namespace CSSPropertyParserHelpers {

// MARK: <rotate3d()> consuming (CSSValue)
RefPtr<CSSValue> consumeRotate3dFunction(CSSParserTokenRange&, CSS::PropertyParserState&);
// MARK: <translate()> consuming (CSSValue)
RefPtr<CSSValue> consumeTranslateFunction(CSSParserTokenRange&, CSS::PropertyParserState&);
// MARK: <translate3d()> consuming (CSSValue)
RefPtr<CSSValue> consumeTranslate3dFunction(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <'translate'> consuming (CSSValue)
RefPtr<CSSValue> consumeTranslate(CSSParserTokenRange&, CSS::PropertyParserState&);
// MARK: <'scale'> consuming (CSSValue)
RefPtr<CSSValue> consumeScale(CSSParserTokenRange&, CSS::PropertyParserState&);
// MARK: <'rotate'> consuming (CSSValue)
RefPtr<CSSValue> consumeRotate(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <'transform'> parsing (raw)
std::optional<Style::Transform> parseTransformRaw(const String&, const CSSParserContext&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
