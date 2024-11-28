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

#include "CSSUnresolvedColor.h"
#include "HashTools.h"
#include "RenderTheme.h"
#include "StyleColorLayers.h"
#include "StyleColorMix.h"
#include "StyleContrastColor.h"
#include "StyleRelativeColor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

StyleColor::StyleColor(StyleColor::ColorKind&& color)
    : m_color { WTFMove(color) }
{
}

StyleColor::StyleColor()
    : m_color { StyleCurrentColor { } }
{
}

StyleColor::StyleColor(Color color)
    : m_color { StyleAbsoluteColor { WTFMove(color) } }
{
}

StyleColor::StyleColor(SRGBA<uint8_t> color)
    : m_color { StyleAbsoluteColor { Color { color } } }
{
}

StyleColor::StyleColor(StyleAbsoluteColor&& color)
    : m_color { WTFMove(color) }
{
}

StyleColor::StyleColor(StyleCurrentColor&& color)
    : m_color { WTFMove(color) }
{
}

StyleColor::StyleColor(StyleColorLayers&& colorLayers)
    : m_color { makeIndirectColor(WTFMove(colorLayers)) }
{
}

StyleColor::StyleColor(StyleColorMix&& colorMix)
    : m_color { makeIndirectColor(WTFMove(colorMix)) }
{
}

StyleColor::StyleColor(StyleContrastColor&& contrastColor)
    : m_color { makeIndirectColor(WTFMove(contrastColor)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<RGBFunctionModernRelative>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<HSLFunctionModern>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<HWBFunction>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<LabFunction>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<LCHFunction>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<OKLabFunction>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<OKLCHFunction>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>&& relative)
    : m_color { makeIndirectColor(WTFMove(relative)) }
{
}

StyleColor::StyleColor(const StyleColor& other)
    : m_color { copy(other.m_color) }
{
}

StyleColor& StyleColor::operator=(const StyleColor& other)
{
    m_color = copy(other.m_color);
    return *this;
}

StyleColor::StyleColor(StyleColor&&) = default;
StyleColor& StyleColor::operator=(StyleColor&&) = default;
StyleColor::~StyleColor() = default;

bool StyleColor::operator==(const StyleColor& other) const
{
    return m_color == other.m_color;
}

// This helper allows us to treat all the alternatives in ColorKind
// as const references, pretending the UniqueRefs don't exist.
template<typename... F>
decltype(auto) StyleColor::visit(const StyleColor::ColorKind& color, F&&... f)
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
    return WTF::switchOn(color,
        [&](const StyleAbsoluteColor& absoluteColor) {
            return visitor(absoluteColor);
        },
        [&](const StyleCurrentColor& currentColor) {
            return visitor(currentColor);
        },
        [&](const UniqueRef<StyleColorLayers>& colorLayers) {
            return visitor(colorLayers.get());
        },
        [&](const UniqueRef<StyleColorMix>& colorMix) {
            return visitor(colorMix.get());
        },
        [&](const UniqueRef<StyleContrastColor>& contrastColor) {
            return visitor(contrastColor.get());
        },
        [&]<typename Descriptor>(const UniqueRef<StyleRelativeColor<Descriptor>>& relativeColor) {
            return visitor(relativeColor.get());
        }
    );
}

StyleColor StyleColor::currentColor()
{
    return StyleColor { StyleCurrentColor { } };
}

StyleColor::ColorKind StyleColor::copy(const StyleColor::ColorKind& other)
{
    return StyleColor::visit(other,
        [] (const StyleAbsoluteColor& absoluteColor) -> StyleColor::ColorKind {
            return StyleAbsoluteColor { absoluteColor.color };
        },
        [] (const StyleCurrentColor&) -> StyleColor::ColorKind {
            return StyleCurrentColor { };
        },
        [] (const StyleColorLayers& colorLayers) -> StyleColor::ColorKind {
            return makeUniqueRef<StyleColorLayers>(colorLayers);
        },
        [] (const StyleColorMix& colorMix) -> StyleColor::ColorKind {
            return makeUniqueRef<StyleColorMix>(colorMix);
        },
        [] (const StyleContrastColor& contrastColor) -> StyleColor::ColorKind {
            return makeUniqueRef<StyleContrastColor>(contrastColor);
        },
        []<typename Descriptor>(const StyleRelativeColor<Descriptor>& relativeColor) -> StyleColor::ColorKind {
            return makeUniqueRef<StyleRelativeColor<Descriptor>>(relativeColor);
        }
    );
}


Color StyleColor::colorFromAbsoluteKeyword(CSSValueID keyword)
{
    // TODO: maybe it should be a constexpr map for performance.
    ASSERT(StyleColor::isAbsoluteColorKeyword(keyword));
    if (auto valueName = nameLiteral(keyword)) {
        if (auto namedColor = findColor(valueName.characters(), valueName.length()))
            return asSRGBA(PackedColor::ARGB { namedColor->ARGBValue });
    }
    ASSERT_NOT_REACHED();
    return { };
}

Color StyleColor::colorFromKeyword(CSSValueID keyword, OptionSet<StyleColorOptions> options)
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

bool StyleColor::containsColorSchemeDependentColor(const CSSPrimitiveValue& value)
{
    if (StyleColor::isSystemColorKeyword(value.valueID()))
        return true;

    if (value.isUnresolvedColor())
        return value.unresolvedColor().containsColorSchemeDependentColor();

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
    return StyleColor::visit(m_color,
        [&](const auto& kind) {
            return WebCore::resolveColor(kind, currentColor);
        }
    );
}

bool StyleColor::containsCurrentColor() const
{
    return StyleColor::visit(m_color,
        [](const auto& kind) {
            return WebCore::containsCurrentColor(kind);
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

bool StyleColor::isContrastColor() const
{
    return std::holds_alternative<UniqueRef<StyleContrastColor>>(m_color);
}

bool StyleColor::isRelativeColor() const
{
    return std::holds_alternative<UniqueRef<StyleRelativeColor<RGBFunctionModernRelative>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<HSLFunctionModern>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<HWBFunction>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<LabFunction>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<LCHFunction>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<OKLabFunction>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<OKLCHFunction>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>>>(m_color)
        || std::holds_alternative<UniqueRef<StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>>>(m_color);
}

bool StyleColor::isAbsoluteColor() const
{
    return std::holds_alternative<StyleAbsoluteColor>(m_color);
}

const Color& StyleColor::absoluteColor() const
{
    ASSERT(isAbsoluteColor());
    return std::get<StyleAbsoluteColor>(m_color).color;
}

template<typename StyleColorType>
StyleColor::ColorKind StyleColor::makeIndirectColor(StyleColorType&& colorType)
{
    return { makeUniqueRef<StyleColorType>(WTFMove(colorType)) };
}

// MARK: - Serialization

String serializationForCSS(const StyleColor& color)
{
    return StyleColor::visit(color.m_color,
        [](const auto& kind) {
            return WebCore::serializationForCSS(kind);
        }
    );
}

void serializationForCSS(StringBuilder& builder, const StyleColor& color)
{
    return StyleColor::visit(color.m_color,
        [&](const auto& kind) {
            WebCore::serializationForCSS(builder, kind);
        }
    );
}

// MARK: - TextStream.

TextStream& operator<<(TextStream& out, const StyleColor& color)
{
    out << "StyleColor[";

    StyleColor::visit(color.m_color,
        [&](const auto& kind) {
            out << kind;
        }
    );

    out << "]";
    return out;
}

TextStream& operator<<(TextStream& out, StyleColorOptions colorOptions)
{
    switch (colorOptions) {
    case StyleColorOptions::ForVisitedLink: out << "ForVisitedLink"; break;
    case StyleColorOptions::UseSystemAppearance: out << "UseSystemAppearance"; break;
    case StyleColorOptions::UseDarkAppearance: out << "UseDarkAppearance"; break;
    case StyleColorOptions::UseElevatedUserInterfaceLevel: out << "UseElevatedUserInterfaceLevel"; break;
    }
    return out;
}

} // namespace WebCore
