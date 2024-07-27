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

#include "CSSParserFastPaths.h"
#include "CSSUnresolvedColorResolutionContext.h"
#include "StyleColor.h"
#include <optional>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Color;
class CSSPrimitiveValue;
class CSSParserTokenRange;
class CSSUnresolvedColor;

struct CSSParserContext;

namespace CSSPropertyParserHelpers {

// Options to augment color parsing.
struct CSSColorParsingOptions {
    bool acceptQuirkyColors = false;
    OptionSet<StyleColor::CSSColorType> allowedColorTypes = { StyleColor::CSSColorType::Absolute, StyleColor::CSSColorType::Current, StyleColor::CSSColorType::System };
};

// MARK: <color> consuming (unresolved)
std::optional<CSSUnresolvedColor> consumeUnresolvedColor(CSSParserTokenRange&, const CSSParserContext&, const CSSColorParsingOptions& = { });

// MARK: <color> consuming (CSSPrimitiveValue)
RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange&, const CSSParserContext&, const CSSColorParsingOptions& = { });

// MARK: <color> consuming (raw)
Color consumeColorRaw(CSSParserTokenRange&, const CSSParserContext&, const CSSColorParsingOptions&, const CSSUnresolvedColorResolutionContext& = { });

// MARK: <color> parsing (raw)
Color parseColorRawSlow(const String&, const CSSParserContext&, const CSSColorParsingOptions&, const CSSUnresolvedColorResolutionContext& = { });

template<typename F> Color parseColorRaw(const String& string, const CSSParserContext& context, F&& lazySlowPathOptionsFunctor)
{
    bool strict = !isQuirksModeBehavior(context.mode);
    if (auto color = CSSParserFastPaths::parseSimpleColor(string, strict))
        return *color;

    // To avoid doing anything unnecessary before the fast path can run, callers bundle up
    // a functor to generate the slow path parameters.
    auto [options, eagerResolutionContext, eagerResolutionDelegate] = lazySlowPathOptionsFunctor();

    // If a delegate is provided, hook it up to the context here. By having it live on the stack,
    // we avoid allocating it.
    if (eagerResolutionDelegate)
        eagerResolutionContext.delegate = &eagerResolutionDelegate.value();

    return parseColorRawSlow(string, context, options, eagerResolutionContext);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
