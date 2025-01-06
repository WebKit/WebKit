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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSKeywordColor.h"

#include "CSSColorType.h"
#include "CSSPlatformColorResolutionState.h"
#include "CSSValueKeywords.h"
#include "HashTools.h"
#include "RenderTheme.h"
#include "StyleColorOptions.h"
#include <wtf/OptionSet.h>

namespace WebCore {
namespace CSS {

static bool isVGAPaletteColor(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#named-colors
    // "16 of CSS’s named colors come from the VGA palette originally, and were then adopted into HTML"
    return id >= CSSValueAqua && id <= CSSValueGrey;
}

static bool isNonVGANamedColor(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#named-colors
    return id >= CSSValueAliceblue && id <= CSSValueYellowgreen;
}

bool isAbsoluteColorKeyword(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#typedef-absolute-color
    return isVGAPaletteColor(id) || isNonVGANamedColor(id) || id == CSSValueAlpha || id == CSSValueTransparent;
}

bool isCurrentColorKeyword(CSSValueID id)
{
    return id == CSSValueCurrentcolor;
}

bool isSystemColorKeyword(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#css-system-colors
    return (id >= CSSValueCanvas && id <= CSSValueInternalDocumentTextColor) || id == CSSValueText || isDeprecatedSystemColorKeyword(id);
}

bool isDeprecatedSystemColorKeyword(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#deprecated-system-colors
    return (id >= CSSValueActiveborder && id <= CSSValueWindowtext) || id == CSSValueMenu;
}

bool isColorKeyword(CSSValueID id, OptionSet<ColorType> allowedColorTypes)
{
    return (allowedColorTypes.contains(ColorType::Absolute) && isAbsoluteColorKeyword(id))
        || (allowedColorTypes.contains(ColorType::Current) && isCurrentColorKeyword(id))
        || (allowedColorTypes.contains(ColorType::System) && isSystemColorKeyword(id));
}

bool isColorKeyword(CSSValueID id)
{
    return isAbsoluteColorKeyword(id) || isCurrentColorKeyword(id) || isSystemColorKeyword(id);
}

WebCore::Color colorFromAbsoluteKeyword(CSSValueID keyword)
{
    ASSERT(isAbsoluteColorKeyword(keyword));

    // TODO: Investigate if this should be a constexpr map for performance.

    if (auto valueName = nameLiteral(keyword)) {
        if (auto namedColor = findColor(valueName.characters(), valueName.length()))
            return asSRGBA(PackedColor::ARGB { namedColor->ARGBValue });
    }
    ASSERT_NOT_REACHED();
    return { };
}

WebCore::Color colorFromKeyword(CSSValueID keyword, OptionSet<StyleColorOptions> options)
{
    if (isAbsoluteColorKeyword(keyword))
        return colorFromAbsoluteKeyword(keyword);

    return RenderTheme::singleton().systemColor(keyword, options);
}

WebCore::Color createColor(const KeywordColor& unresolved, PlatformColorResolutionState& state)
{
    switch (unresolved.valueID) {
    case CSSValueInternalDocumentTextColor:
        return state.internalDocumentTextColor();
    case CSSValueWebkitLink:
        return state.forVisitedLink == Style::ForVisitedLink::Yes ? state.webkitLinkVisited() : state.webkitLink();
    case CSSValueWebkitActivelink:
        return state.webkitActiveLink();
    case CSSValueWebkitFocusRingColor:
        return state.webkitFocusRingColor();
    case CSSValueCurrentcolor:
        return state.currentColor();
    default:
        return colorFromKeyword(unresolved.valueID, state.keywordOptions);
    }
}

bool containsCurrentColor(const KeywordColor& unresolved)
{
    return isCurrentColorKeyword(unresolved.valueID);
}

bool containsColorSchemeDependentColor(const KeywordColor& unresolved)
{
    return isSystemColorKeyword(unresolved.valueID);
}

void Serialize<KeywordColor>::operator()(StringBuilder& builder, const KeywordColor& value)
{
    builder.append(nameLiteralForSerialization(value.valueID));
}

} // namespace CSS
} // namespace WebCore
