/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "FreeformGradient.h"

#include "ColorConversion.h"
#include "ColorSpace.h"
#include "NativeImage.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FreeformGradient);

static ResolvedPremultiplied<ColorComponents<float, 4>> premultipliedComponents(const ColorComponents<float, 4>& components, ColorSpaceName colorSpace)
{
    // Multiplying a hue by alpha would be wrong, but the renderers only convert to RGB color spaces.
    ASSERT_UNUSED(colorSpace, callWithColorType<float>(colorSpace, []<typename ColorType> { return UsesRGBModel<ColorType>; }));

    auto [red, green, blue, alpha] = components;
    return Premultiplied { resolvedColor(ColorComponents<float, 4> { red * alpha, green * alpha, blue * alpha, alpha }) };
}

Ref<FreeformGradient> FreeformGradient::create(ColorInterpolationMethod colorInterpolationMethod, Vector<Stop>&& stops)
{
    return adoptRef(*new FreeformGradient(colorInterpolationMethod, WTF::move(stops)));
}

std::optional<Ref<FreeformGradient>> FreeformGradient::tryCreate(ColorInterpolationMethod colorInterpolationMethod, Vector<Stop>&& stops)
{
    if (stops.isEmpty())
        return std::nullopt;

    for (auto& stop : stops) {
        if (!std::isfinite(stop.position.x()) || !std::isfinite(stop.position.y()) || !std::isfinite(stop.weight))
            return std::nullopt;
    }

    return create(colorInterpolationMethod, WTF::move(stops));
}

FreeformGradient::FreeformGradient(ColorInterpolationMethod colorInterpolationMethod, Vector<Stop>&& stops)
    : m_colorInterpolationMethod { colorInterpolationMethod }
    , m_stops { WTF::move(stops) }
{
    RELEASE_ASSERT(!m_stops.isEmpty());
}

FreeformGradient::~FreeformGradient()
{
    for (CheckedRef observer : m_observers)
        observer->willDestroyFreeformGradient(*this);
}

std::optional<ColorSpaceName> FreeformGradient::renderingColorSpace(const ColorSpace& colorSpace)
{
    if (colorSpace == ColorSpace::SRGB())
        return ColorSpaceName::SRGB;
    if (colorSpace == ColorSpace::LinearSRGB())
        return ColorSpaceName::LinearSRGB;
#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
    if (colorSpace == ColorSpace::ExtendedSRGB())
        return ColorSpaceName::ExtendedSRGB;
    if (colorSpace == ColorSpace::ExtendedLinearSRGB())
        return ColorSpaceName::ExtendedLinearSRGB;
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    if (colorSpace == ColorSpace::DisplayP3())
        return ColorSpaceName::DisplayP3;
    if (colorSpace == ColorSpace::LinearDisplayP3())
        return ColorSpaceName::LinearDisplayP3;
    if (colorSpace == ColorSpace::ExtendedDisplayP3())
        return ColorSpaceName::ExtendedDisplayP3;
    if (colorSpace == ColorSpace::ExtendedLinearDisplayP3())
        return ColorSpaceName::ExtendedLinearDisplayP3;
#endif
    return std::nullopt;
}

ResolvedPremultiplied<ColorComponents<float, 4>> FreeformGradient::premultipliedColor(FloatPoint, ColorSpaceName colorSpace) const
{
    // FIXME: Blend the colors of the stops. For now, this is the color of the first stop everywhere.
    return premultipliedComponents(m_stops[0].color.toResolvedColorComponentsInColorSpace(colorSpace), colorSpace);
}

RefPtr<NativeImage> FreeformGradient::renderedImage(const IntSize& pixelSize, float scale, const ColorSpace& colorSpace) const
{
    Locker locker { m_renderedImageLock };
    RefPtr renderedImage = m_renderedImage;
    if (!renderedImage || m_renderedImagePixelSize != pixelSize || m_renderedImageScale != scale || renderedImage->colorSpace() != colorSpace)
        return nullptr;
    return renderedImage;
}

void FreeformGradient::setRenderedImage(const IntSize& pixelSize, float scale, Ref<NativeImage>&& image) const
{
    Locker locker { m_renderedImageLock };
    m_renderedImagePixelSize = pixelSize;
    m_renderedImageScale = scale;
    m_renderedImage = WTF::move(image);
}

TextStream& operator<<(TextStream& ts, const FreeformGradient::Stop& stop)
{
    return ts << stop.color << " weight "_s << stop.weight << " at "_s << stop.position;
}

TextStream& operator<<(TextStream& ts, const FreeformGradient& gradient)
{
    ts.dumpProperty("color-interpolation-method"_s, gradient.colorInterpolationMethod());
    ts.dumpProperty("stops"_s, gradient.stops());
    return ts;
}

} // namespace WebCore
