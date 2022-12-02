/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 * Copyright (C) 2016, 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleColor.h"

#include "ColorSerialization.h"
#include "HashTools.h"
#include "RenderTheme.h"

namespace WebCore {

String serializationForCSS(const StyleColor& color)
{
    if (color.isAbsoluteColor())
        return serializationForCSS(color.absoluteColor());

    if (color.isCurrentColor())
        return "currentcolor"_s;

    ASSERT_NOT_REACHED();
    return { };
}

Color StyleColor::colorFromAbsoluteKeyword(CSSValueID keyword)
{
    // TODO: maybe it should be a constexpr map for performance.
    ASSERT(StyleColor::isAbsoluteColorKeyword(keyword));
    if (const char* valueName = nameLiteral(keyword)) {
        if (auto namedColor = findColor(valueName, strlen(valueName)))
            return asSRGBA(PackedColor::ARGB { namedColor->ARGBValue });
    }
    ASSERT_NOT_REACHED();
    return { };
}

Color StyleColor::colorFromKeyword(CSSValueID keyword , OptionSet<StyleColorOptions> options)
{
    if (isAbsoluteColorKeyword(keyword))
        return colorFromAbsoluteKeyword(keyword);

    return RenderTheme::singleton().systemColor(keyword, options);
}

static bool isVGAPaletteColor(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#named-colors
    // "16 of CSS’s named colors come from the VGA palette originally, and were then adopted into HTML"
    return (id >= CSSValueAqua && id <= CSSValueGrey);
}

static bool isNonVGANamedColor(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#named-colors
    return id >= CSSValueAliceblue && id <= CSSValueYellowgreen;
}

bool StyleColor::isAbsoluteColorKeyword(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#typedef-absolute-color
    return isVGAPaletteColor(id) || isNonVGANamedColor(id) || id == CSSValueAlpha || id == CSSValueTransparent;
}

bool StyleColor::isSystemColorKeyword(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#css-system-colors
    return (id >= CSSValueCanvas && id <= CSSValueInternalDocumentTextColor) || id == CSSValueText || isDeprecatedSystemColorKeyword(id);
}

bool StyleColor::isDeprecatedSystemColorKeyword(CSSValueID id)
{
    // https://drafts.csswg.org/css-color-4/#deprecated-system-colors
    return (id >= CSSValueActiveborder && id <= CSSValueWindowtext) || id == CSSValueMenu;
}

bool StyleColor::isColorKeyword(CSSValueID id, OptionSet<CSSColorType> allowedColorTypes)
{
    return (allowedColorTypes.contains(CSSColorType::Absolute) && isAbsoluteColorKeyword(id))
        || (allowedColorTypes.contains(CSSColorType::Current) && isCurrentColorKeyword(id))
        || (allowedColorTypes.contains(CSSColorType::System) && isSystemColorKeyword(id));
}

WTF::TextStream& operator<<(WTF::TextStream& out, const StyleColor& v)
{
    out << "StyleColor[";
    if (v.isAbsoluteColor()) {
        out << "absoluteColor(";
        out << v.absoluteColor().debugDescription();
        out << ")";
    } else if (v.isCurrentColor())
        out << "currentColor";
    
    out << "]";
    return out;
}

String StyleColor::debugDescription() const
{
    TextStream ts;
    ts << *this;
    return ts.release();
}

Color StyleColor::resolveColor(const Color& currentColor) const
{
    if (isAbsoluteColor())
        return absoluteColor();

    if (isCurrentColor())
        return currentColor;

    return { };
}

bool StyleColor::isCurrentColor() const
{
    return std::holds_alternative<CurrentColor>(m_color);
}

bool StyleColor::isAbsoluteColor() const
{
    return std::holds_alternative<Color>(m_color);
}

const Color& StyleColor::absoluteColor() const
{
    ASSERT(isAbsoluteColor());
    return std::get<Color>(m_color);
}

} // namespace WebCore
