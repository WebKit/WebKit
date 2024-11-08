/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "ColorSerialization.h"
#include "ExtendedStyleColor.h"
#include "HashTools.h"
#include "RenderTheme.h"
#include "StyleColorLayers.h"
#include "StyleColorMix.h"
#include "StyleContrastColor.h"
#include "StyleRelativeColor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {


StyleColor::StyleColor()
{
    m_color.setCurrentColor();
}

StyleColor::StyleColor(Color color)
    : m_color { WTFMove(color) }
{
}

StyleColor::StyleColor(SRGBA<uint8_t> color)
    : m_color { color }
{
}

StyleColor::StyleColor(Ref<ExtendedStyleColor>&& color)
{
    store(WTFMove(color));
}

StyleColor::StyleColor(StyleAbsoluteColor&& color)
{
    store(WTFMove(color));
}

StyleColor::StyleColor(StyleColorMix&& colorMix)
{
    store(makeIndirectColor(WTFMove(colorMix)));
}

StyleColor::StyleColor(StyleContrastColor&& contrastColor)
{
    store(makeIndirectColor(WTFMove(contrastColor)));
}

StyleColor::StyleColor(StyleColorLayers&& colorLayers)
{
    store(makeIndirectColor(WTFMove(colorLayers)));
}

StyleColor::StyleColor(StyleRelativeColor<RGBFunctionModernRelative>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<HSLFunctionModern>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<HWBFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<LabFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<LCHFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<OKLabFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<OKLCHFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(StyleRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

StyleColor::StyleColor(const StyleColor& other)
{
    if (other.isExtendedStyleColor())
        other.extendedStyleColor().ref();

    m_color.m_colorAndFlags = other.m_color.m_colorAndFlags;
}

StyleColor& StyleColor::operator=(const StyleColor& other)
{
    if (this == &other)
        return *this;

    if (other.isExtendedStyleColor())
        other.extendedStyleColor().ref();

    if (isExtendedStyleColor())
        extendedStyleColor().deref();

    m_color.m_colorAndFlags = other.m_color.m_colorAndFlags;
    return *this;
}

StyleColor::StyleColor(StyleColor&& other)
{
    if (other.isExtendedStyleColor()) {
        m_color.m_colorAndFlags = other.m_color.m_colorAndFlags;
        other.m_color.m_colorAndFlags = Color::invalidColorAndFlags;
        return;
    }

    m_color = WTFMove(other.m_color);
}

StyleColor& StyleColor::operator=(StyleColor&& other)
{
    if (this == &other)
        return *this;

    if (isExtendedStyleColor())
        extendedStyleColor().deref();

    m_color.m_colorAndFlags = other.m_color.m_colorAndFlags;
    other.m_color.m_colorAndFlags = Color::invalidColorAndFlags;
    return *this;
}

StyleColor::~StyleColor()
{
    if (isExtendedStyleColor()) {
        extendedStyleColor().deref();
        m_color.m_colorAndFlags = Color::invalidColorAndFlags;
    }
}

bool StyleColor::operator==(const StyleColor& other) const
{
    if (this == &other)
        return true;
    if (isAbsoluteColor() && other.isAbsoluteColor())
        return absoluteColor() == other.absoluteColor();
    if (isCurrentColor() && other.isCurrentColor())
        return true;
    if (isExtendedStyleColor() && other.isExtendedStyleColor())
        return extendedStyleColor() == other.extendedStyleColor();
    return false;
}

StyleColor StyleColor::currentColor()
{
    static LazyNeverDestroyed<Color> color;
    static LazyNeverDestroyed<StyleColor> styleColor;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        color.construct();
        color->setCurrentColor();
        styleColor.construct(StyleColor { color });
    });
    return styleColor;
}

StyleColor StyleColor::invalidColor()
{
    return Color { };
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
    if (isAbsoluteColor())
        return m_color;
    if (isCurrentColor())
        return currentColor;
    ASSERT(isExtendedStyleColor());
    return protectedExtendedStyleColor()->resolveColor(currentColor);
}

bool StyleColor::containsCurrentColor() const
{
    return isCurrentColor();
}

bool StyleColor::isCurrentColor() const
{
    return m_color.isCurrentColor();
}

bool StyleColor::isExtendedStyleColor() const
{
    return m_color.isExtendedStyleColor();
}

void StyleColor::store(ColorKind&& kind)
{
    if (std::holds_alternative<StyleAbsoluteColor>(kind))
        m_color = std::get<StyleAbsoluteColor>(kind).color;
    else if (std::holds_alternative<StyleCurrentColor>(kind))
        m_color.setCurrentColor();
    else {
        ASSERT(std::holds_alternative<Ref<ExtendedStyleColor>>(kind));
        auto& extended = std::get<Ref<ExtendedStyleColor>>(kind);
        m_color.setExtendedStyleColor(WTFMove(extended));
    }
}

bool StyleColor::isAbsoluteColor() const
{
    return !isCurrentColor() && !isExtendedStyleColor();
}

const Color& StyleColor::absoluteColor() const
{
    ASSERT(isAbsoluteColor());
    return m_color;
}

const ExtendedStyleColor& StyleColor::extendedStyleColor() const
{
    ASSERT(isExtendedStyleColor());
    return m_color.asExtendedStyleColor();
}

Ref<const ExtendedStyleColor> StyleColor::protectedExtendedStyleColor() const
{
    return extendedStyleColor();
}

template<typename StyleColorType>
ColorKind StyleColor::makeIndirectColor(StyleColorType&& colorType)
{
    return ExtendedStyleColor::create(WTFMove(colorType));
}

// MARK: - Serialization

String serializationForCSS(const StyleColor& color)
{
    if (color.isCurrentColor())
        return "currentcolor"_s;
    if (color.isAbsoluteColor())
        return WebCore::serializationForCSS(color.absoluteColor());
    return serializationForCSS(color.protectedExtendedStyleColor());
}

void serializationForCSS(StringBuilder& builder, const StyleColor& color)
{
    if (color.isCurrentColor())
        builder.append("currentcolor"_s);
    else if (color.isAbsoluteColor())
        builder.append(serializationForCSS(color.absoluteColor()));
    else
        serializationForCSS(builder, color.protectedExtendedStyleColor());
}

// MARK: - TextStream.

WTF::TextStream& operator<<(WTF::TextStream& out, const StyleColor& color)
{
    out << "StyleColor[";
    if (color.isCurrentColor())
        out << "currentcolor";
    else if (color.isAbsoluteColor())
        out << color.absoluteColor();
    else
        out << color.protectedExtendedStyleColor();
    out << "]";
    return out;
}

} // namespace WebCore
