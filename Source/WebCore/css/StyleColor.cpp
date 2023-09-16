/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "CSSColorMixSerialization.h"
#include "CSSResolvedColorMix.h"
#include "CSSUnresolvedColor.h"
#include "ColorNormalization.h"
#include "ColorSerialization.h"
#include "HashTools.h"
#include "RenderTheme.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

static std::optional<Color> resolveAbsoluteComponents(const StyleColorMix&);
static Color resolveColor(const StyleColorMix&, const Color& currentColor);

StyleColor::ColorKind StyleColor::copy(const StyleColor::ColorKind& other)
{
    return WTF::switchOn(other,
        [] (const Color& absoluteColor) -> StyleColor::ColorKind {
            return absoluteColor;
        },
        [] (const StyleCurrentColor& currentColor) -> StyleColor::ColorKind {
            return currentColor;
        },
        [] (const UniqueRef<StyleColorMix>& colorMix) -> StyleColor::ColorKind {
            return makeUniqueRef<StyleColorMix>(colorMix.get());
        }
    );
}

StyleColor::~StyleColor() = default;

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

bool StyleColor::containsCurrentColor(const CSSPrimitiveValue& value)
{
    if (StyleColor::isCurrentColor(value))
        return true;

    if (value.isUnresolvedColor())
        return value.unresolvedColor().containsCurrentColor();

    return false;
}

String StyleColor::debugDescription() const
{
    TextStream ts;
    ts << *this;
    return ts.release();
}

Color StyleColor::resolveColor(const Color& currentColor) const
{
    return WTF::switchOn(m_color,
        [&] (const Color& absoluteColor) -> Color {
            return absoluteColor;
        },
        [&] (const StyleCurrentColor&) -> Color {
            return currentColor;
        },
        [&] (const UniqueRef<StyleColorMix>& colorMix) -> Color {
            return WebCore::resolveColor(colorMix, currentColor);
        }
    );
}

bool StyleColor::containsCurrentColor() const
{
    return WTF::switchOn(m_color,
        [&] (const Color&) -> bool {
            return false;
        },
        [&] (const StyleCurrentColor&) -> bool {
            return true;
        },
        [&] (const UniqueRef<StyleColorMix>& colorMix) -> bool {
            return colorMix->mixComponents1.color.containsCurrentColor() || colorMix->mixComponents2.color.containsCurrentColor();
        }
    );
}

bool StyleColor::isCurrentColor() const
{
    return std::holds_alternative<StyleCurrentColor>(m_color);
}

bool StyleColor::isColorMix() const
{
    return std::holds_alternative<UniqueRef<StyleColorMix>>(m_color);
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

StyleColor::ColorKind StyleColor::resolveAbsoluteComponents(StyleColorMix&& colorMix)
{
    if (auto absoluteColor = WebCore::resolveAbsoluteComponents(colorMix))
        return { WTFMove(*absoluteColor) };
    return { makeUniqueRef<StyleColorMix>(WTFMove(colorMix)) };
}

// MARK: color-mix()

std::optional<Color> resolveAbsoluteComponents(const StyleColorMix& colorMix)
{
    if (!colorMix.mixComponents1.color.isAbsoluteColor() || !colorMix.mixComponents2.color.isAbsoluteColor())
        return std::nullopt;

    return mix(CSSResolvedColorMix {
        colorMix.colorInterpolationMethod,
        CSSResolvedColorMix::Component {
            colorMix.mixComponents1.color.absoluteColor(),
            colorMix.mixComponents1.percentage
        },
        CSSResolvedColorMix::Component {
            colorMix.mixComponents2.color.absoluteColor(),
            colorMix.mixComponents2.percentage
        }
    });
}

Color resolveColor(const StyleColorMix& colorMix, const Color& currentColor)
{
    return mix(CSSResolvedColorMix {
        colorMix.colorInterpolationMethod,
        CSSResolvedColorMix::Component {
            colorMix.mixComponents1.color.resolveColor(currentColor),
            colorMix.mixComponents1.percentage
        },
        CSSResolvedColorMix::Component {
            colorMix.mixComponents2.color.resolveColor(currentColor),
            colorMix.mixComponents2.percentage
        }
    });
}

// MARK: - Serialization

void serializationForCSS(StringBuilder& builder, const StyleColorMix& colorMix)
{
    serializationForCSSColorMix<StyleColorMix>(builder, colorMix);
}

void serializationForCSS(StringBuilder& builder, const StyleCurrentColor&)
{
    builder.append("currentcolor"_s);
}

void serializationForCSS(StringBuilder& builder, const StyleColor& color)
{
    WTF::switchOn(color.m_color,
        [&] (const Color& absoluteColor) {
            serializationForCSS(builder, absoluteColor);
        },
        [&] (const StyleCurrentColor& currentColor) {
            serializationForCSS(builder, currentColor);
        },
        [&] (const UniqueRef<StyleColorMix>& colorMix) {
            serializationForCSS(builder, colorMix);
        }
    );
}

String serializationForCSS(const StyleColorMix& colorMix)
{
    StringBuilder builder;
    serializationForCSS(builder, colorMix);
    return builder.toString();
}

String serializationForCSS(const StyleCurrentColor&)
{
    return "currentcolor"_s;
}

String serializationForCSS(const StyleColor& color)
{
    return WTF::switchOn(color.m_color,
        [&] (const Color& absoluteColor) {
            return serializationForCSS(absoluteColor);
        },
        [&] (const StyleCurrentColor& currentColor) {
            return serializationForCSS(currentColor);
        },
        [&] (const UniqueRef<StyleColorMix>& colorMix) {
            return serializationForCSS(colorMix);
        }
    );
}

// MARK: - TextStream.

static TextStream& operator<<(TextStream& ts, const StyleColorMix::Component& component)
{
    ts << component.color;
    if (component.percentage)
        ts << " " << *component.percentage << "%";
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleColorMix& colorMix)
{
    ts << "color-mix(";
    ts << "in " << colorMix.colorInterpolationMethod;
    ts << ", " << colorMix.mixComponents1;
    ts << ", " << colorMix.mixComponents2;
    ts << ")";

    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& out, const StyleCurrentColor&)
{
    out << "currentColor";
    return out;
}

WTF::TextStream& operator<<(WTF::TextStream& out, const StyleColor& color)
{
    out << "StyleColor[";

    WTF::switchOn(color.m_color,
        [&] (const Color& absoluteColor) {
            out << "absoluteColor(" << absoluteColor.debugDescription() << ")";
        },
        [&] (const StyleCurrentColor& currentColor) {
            out << currentColor;
        },
        [&] (const UniqueRef<StyleColorMix>& colorMix) {
            out << colorMix.get();
        }
    );

    out << "]";
    return out;
}

} // namespace WebCore
