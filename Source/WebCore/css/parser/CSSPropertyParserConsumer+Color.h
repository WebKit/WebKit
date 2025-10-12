/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSColorType.h>
#include <optional>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Color;
class CSSParserTokenRange;
class CSSValue;
class ScriptExecutionContext;
struct CSSParserContext;
enum CSSValueID : uint16_t;

namespace CSS {
struct Color;
struct DynamicRangeLimit;
struct PlatformColorResolutionState;
struct PropertyParserState;
}

namespace CSSPropertyParserHelpers {

// Options to augment color parsing.
struct CSSColorParsingOptions {
    OptionSet<CSS::ColorType> allowedColorTypes = { CSS::ColorType::Absolute, CSS::ColorType::Current, CSS::ColorType::System };
};

// MARK: Mode specific color settings.
bool isColorKeywordAllowed(CSSValueID, const CSSParserContext&);

// MARK: <color> consuming (unresolved)
std::optional<CSS::Color> consumeUnresolvedColor(CSSParserTokenRange&, CSS::PropertyParserState&, const CSSColorParsingOptions& = { });

// MARK: <color> consuming (CSSValue)
RefPtr<CSSValue> consumeColor(CSSParserTokenRange&, CSS::PropertyParserState&, const CSSColorParsingOptions& = { });

// MARK: <color> consuming (raw)
WebCore::Color consumeColorRaw(CSSParserTokenRange&, CSS::PropertyParserState&, const CSSColorParsingOptions&, CSS::PlatformColorResolutionState&);

// MARK: <color> parsing (raw)

// Parse with default options.
// NOTE: Callers must include CSSPropertyParserConsumer+ColorInlines.h to use this.
WebCore::Color parseColorRaw(const String&, const CSSParserContext&, ScriptExecutionContext&);

// Fast variant to be used when ScriptExecutionContext is expensive to obtain or when need to pass parsing options.
// If the result is invalid, callers should call parseColorRawGeneral().
// NOTE: Callers must include CSSPropertyParserConsumer+ColorInlines.h to use this.
WebCore::Color parseColorRawSimple(const String&, const CSSParserContext&);

// Parse with specific options.
WEBCORE_EXPORT WebCore::Color parseColorRawGeneral(const String&, const CSSParserContext&, ScriptExecutionContext&, const CSSColorParsingOptions&, CSS::PlatformColorResolutionState&);

// FIXME: All callers are not getting the right Settings, keyword resolution and calc resolution
// when using this function and should switch to parseColorRaw().
WEBCORE_EXPORT WebCore::Color deprecatedParseColorRawWithoutContext(const String&, const CSSColorParsingOptions& = { });

// MARK: <dynamic-range-limit> (unresolved)
std::optional<CSS::DynamicRangeLimit> consumeUnresolvedDynamicRangeLimit(CSSParserTokenRange&, CSS::PropertyParserState&);

// MARK: <dynamic-range-limit> (CSSValue)
RefPtr<CSSValue> consumeDynamicRangeLimit(CSSParserTokenRange&, CSS::PropertyParserState&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
