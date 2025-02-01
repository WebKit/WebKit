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

#include "CSSKeywordColor.h"
#include "ColorSerialization.h"
#include "Document.h"
#include "ExtendedStyleColor.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "StyleAbsoluteColor.h"
#include "StyleColorLayers.h"
#include "StyleColorMix.h"
#include "StyleColorResolutionState.h"
#include "StyleContrastColor.h"
#include "StyleHexColor.h"
#include "StyleKeywordColor.h"
#include "StyleLightDarkColor.h"
#include "StyleRelativeColor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {


Color::Color()
{
    m_color.setCurrentColor();
}

Color::Color(WebCore::Color color)
    : m_color { WTFMove(color) }
{
}

Color::Color(SRGBA<uint8_t> color)
    : m_color { color }
{
}

Color::Color(Ref<ExtendedStyleColor>&& color)
{
    store(WTFMove(color));
}

Color::Color(ResolvedColor&& color)
{
    store(WTFMove(color));
}

Color::Color(CurrentColor&& color)
{
    store(WTFMove(color));
}

Color::Color(ColorMix&& colorMix)
{
    store(makeIndirectColor(WTFMove(colorMix)));
}

Color::Color(ContrastColor&& contrastColor)
{
    store(makeIndirectColor(WTFMove(contrastColor)));
}

Color::Color(ColorLayers&& colorLayers)
{
    store(makeIndirectColor(WTFMove(colorLayers)));
}

Color::Color(RelativeColor<RGBFunctionModernRelative>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<HSLFunctionModern>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<HWBFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<LabFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<LCHFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<OKLabFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<OKLCHFunction>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>&& relative)
{
    store(makeIndirectColor(WTFMove(relative)));
}

Color::Color(const Color& other)
{
    if (other.isExtendedStyleColor())
        other.extendedStyleColor().ref();

    m_color.m_colorAndFlags = other.m_color.m_colorAndFlags;
}

Color& Color::operator=(const Color& other)
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

Color::Color(Color&& other)
{
    if (other.isExtendedStyleColor()) {
        m_color.m_colorAndFlags = other.m_color.m_colorAndFlags;
        other.m_color.m_colorAndFlags = WebCore::Color::invalidColorAndFlags;
        return;
    }

    m_color = WTFMove(other.m_color);
}

Color& Color::operator=(Color&& other)
{
    if (this == &other)
        return *this;

    if (isExtendedStyleColor())
        extendedStyleColor().deref();

    m_color.m_colorAndFlags = other.m_color.m_colorAndFlags;
    other.m_color.m_colorAndFlags = WebCore::Color::invalidColorAndFlags;
    return *this;
}

Color::~Color()
{
    if (isExtendedStyleColor()) {
        extendedStyleColor().deref();
        m_color.m_colorAndFlags = WebCore::Color::invalidColorAndFlags;
    }
}

bool Color::operator==(const Color& other) const
{
    if (this == &other)
        return true;
    if (isResolvedColor() && other.isResolvedColor())
        return resolvedColor() == other.resolvedColor();
    if (isCurrentColor() && other.isCurrentColor())
        return true;
    if (isExtendedStyleColor() && other.isExtendedStyleColor())
        return extendedStyleColor() == other.extendedStyleColor();
    return false;
}

Color Color::currentColor()
{
    static LazyNeverDestroyed<WebCore::Color> color;
    static LazyNeverDestroyed<Color> styleColor;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        color.construct();
        color->setCurrentColor();
        styleColor.construct(Color { color });
    });
    return styleColor;
}

Color Color::invalidColor()
{
    return WebCore::Color { };
}

String Color::debugDescription() const
{
    TextStream ts;
    ts << *this;
    return ts.release();
}

WebCore::Color Color::resolveColor(const WebCore::Color& currentColor) const
{
    if (isResolvedColor())
        return m_color;
    if (isCurrentColor())
        return currentColor;
    ASSERT(isExtendedStyleColor());
    return protectedExtendedStyleColor()->resolveColor(currentColor);
}

bool Color::containsCurrentColor() const
{
    if (isCurrentColor())
        return true;
    if (isResolvedColor())
        return false;
    return protectedExtendedStyleColor()->containsCurrentColor();
}

bool containsCurrentColor(const Color& value)
{
    return value.containsCurrentColor();
}

bool Color::isCurrentColor() const
{
    return m_color.isCurrentColor();
}

bool Color::isExtendedStyleColor() const
{
    return m_color.isExtendedStyleColor();
}
void Color::store(ColorKind&& kind)
{
    if (std::holds_alternative<ResolvedColor>(kind))
        m_color = std::get<ResolvedColor>(kind).color;
    else if (std::holds_alternative<CurrentColor>(kind))
        m_color.setCurrentColor();
    else {
        ASSERT(std::holds_alternative<Ref<ExtendedStyleColor>>(kind));
        auto& extended = std::get<Ref<ExtendedStyleColor>>(kind);
        m_color.setExtendedStyleColor(WTFMove(extended));
    }
}

bool Color::isResolvedColor() const
{
    return !isCurrentColor() && !isExtendedStyleColor();
}

const WebCore::Color& Color::resolvedColor() const
{
    ASSERT(isResolvedColor());
    return m_color;
}

const ExtendedStyleColor& Color::extendedStyleColor() const
{
    ASSERT(isExtendedStyleColor());
    return *static_cast<ExtendedStyleColor*>(m_color.decodedOutOfLinePointer(m_color.m_colorAndFlags));
}

Ref<const ExtendedStyleColor> Color::protectedExtendedStyleColor() const
{
    return extendedStyleColor();
}

template<typename StyleColorType>
ColorKind Color::makeIndirectColor(StyleColorType&& colorType)
{
    return ExtendedStyleColor::create(WTFMove(colorType));
}

// MARK: - Serialization

String serializationForCSS(const Color& color)
{
    if (color.isCurrentColor())
        return "currentcolor"_s;
    if (color.isResolvedColor())
        return WebCore::serializationForCSS(color.resolvedColor());
    return serializationForCSS(color.protectedExtendedStyleColor());
}

void serializationForCSS(StringBuilder& builder, const Color& color)
{
    if (color.isCurrentColor())
        builder.append("currentcolor"_s);
    else if (color.isResolvedColor())
        builder.append(serializationForCSS(color.resolvedColor()));
    else
        serializationForCSS(builder, color.protectedExtendedStyleColor());
}

// MARK: - TextStream.

WTF::TextStream& operator<<(WTF::TextStream& out, const Color& color)
{
    out << "StyleColor[";
    if (color.isCurrentColor())
        out << "currentcolor";
    else if (color.isResolvedColor())
        out << color.resolvedColor();
    else
        out << color.protectedExtendedStyleColor();
    out << "]";
    return out;
}

bool Color::MarkableTraits::isEmptyValue(const Color& color)
{
    return color == Color::invalidColor();

}

Color Color::MarkableTraits::emptyValue()
{
    return Color::invalidColor();
}

// MARK: - Conversion

Color toStyleColor(const CSS::Color& value, ColorResolutionState& state)
{
    return WTF::switchOn(value, [&](const auto& color) { return toStyleColor(color, state); });

}

Color toStyleColor(const CSS::Color& value, Ref<const Document> document, const RenderStyle& style, const CSSToLengthConversionData& conversionData, ForVisitedLink forVisitedLink)
{
    auto resolutionState = ColorResolutionState {
        .document = document,
        .style = style,
        .conversionData = conversionData,
        .forVisitedLink = forVisitedLink
    };
    return toStyleColor(value, resolutionState);
}

Color toStyleColorWithResolvedCurrentColor(const CSS::Color& value, Ref<const Document> document, RenderStyle& style, const CSSToLengthConversionData& conversionData, ForVisitedLink forVisitedLink)
{
    // FIXME: 'currentcolor' should be resolved at use time to make it inherit correctly. https://bugs.webkit.org/show_bug.cgi?id=210005
    if (CSS::containsCurrentColor(value)) {
        // Color is an inherited property so depending on it effectively makes the property inherited.
        style.setHasExplicitlyInheritedProperties();
        style.setDisallowsFastPathInheritance();
    }

    return toStyleColor(value, document, style, conversionData, forVisitedLink);
}

auto ToCSS<Color>::operator()(const Color& value, const RenderStyle& style) -> CSS::Color
{
    return CSS::Color { CSS::ResolvedColor { style.colorResolvingCurrentColor(value) } };
}

auto ToStyle<CSS::Color>::operator()(const CSS::Color& value, const BuilderState& builderState, ForVisitedLink forVisitedLink) -> Color
{
    return toStyleColor(value, builderState.document(), builderState.style(), builderState.cssToLengthConversionData(), forVisitedLink);
}

auto ToStyle<CSS::Color>::operator()(const CSS::Color& value, const BuilderState& builderState) -> Color
{
    return toStyle(value, builderState, ForVisitedLink::No);
}


} // namespace Style

} // namespace WebCore
