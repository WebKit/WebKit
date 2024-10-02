/*
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <optional>

namespace WebCore {

class CSSToLengthConversionData;
class CSSValue;
class FontCascade;
class FontCascadeDescription;
class FontSelectionValue;
class ScriptExecutionContext;

struct FontSizeAdjust;

template<typename> class FontTaggedSettings;
using FontFeatureSettings = FontTaggedSettings<int>;
using FontVariationSettings = FontTaggedSettings<float>;

namespace CSSPropertyParserHelpers {
struct UnresolvedFont;
}

namespace Style {

FontSelectionValue fontWeightFromCSSValueDeprecated(const CSSValue&);
FontSelectionValue fontWeightFromCSSValue(const CSSValue&, const CSSToLengthConversionData&);

FontSelectionValue fontStretchFromCSSValueDeprecated(const CSSValue&);
FontSelectionValue fontStretchFromCSSValue(const CSSValue&, const CSSToLengthConversionData&);

FontSelectionValue fontStyleAngleFromCSSValueDeprecated(const CSSValue&);
FontSelectionValue fontStyleAngleFromCSSValue(const CSSValue&, const CSSToLengthConversionData&);

std::optional<FontSelectionValue> fontStyleFromCSSValueDeprecated(const CSSValue&);
std::optional<FontSelectionValue> fontStyleFromCSSValue(const CSSValue&, const CSSToLengthConversionData&);

FontFeatureSettings fontFeatureSettingsFromCSSValue(const CSSValue&, const CSSToLengthConversionData&);
FontVariationSettings fontVariationSettingsFromCSSValue(const CSSValue&, const CSSToLengthConversionData&);
FontSizeAdjust fontSizeAdjustFromCSSValue(const CSSValue&, const CSSToLengthConversionData&);

std::optional<FontCascade> resolveForUnresolvedFont(const CSSPropertyParserHelpers::UnresolvedFont&, FontCascadeDescription&&, ScriptExecutionContext&);

} // namespace Style
} // namespace WebCore
