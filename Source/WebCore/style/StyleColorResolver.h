/*
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/RenderStyle.h>

namespace WebCore {
namespace Style {

template<typename T> concept ImplementsVisitedLinkColor = requires {
    { T::visitedLinkColor(std::declval<const ComputedStyleProperties&>()) } -> std::same_as<const Color&>;
};

template<typename T> concept ImplementsColorResolvingCurrentColor = requires {
    { T::colorResolvingCurrentColor(std::declval<const ComputedStyleProperties&>()) } -> std::same_as<WebCore::Color>;
};

template<typename T> concept ImplementsVisitedLinkColorResolvingCurrentColor = requires {
    { T::visitedLinkColorResolvingCurrentColor(std::declval<const ComputedStyleProperties&>()) } -> std::same_as<WebCore::Color>;
};

template<typename T> concept ImplementsExcludesVisitedLinkColor = requires {
    { T::excludesVisitedLinkColor(std::declval<const WebCore::Color&>()) } -> std::same_as<bool>;
};

class ColorResolver {
public:
    explicit ColorResolver(const RenderStyle& style)
        : m_style { style.computedStyle() }
    {
    }

    explicit ColorResolver(const ComputedStyleProperties& style)
        : m_style { style }
    {
    }

    WebCore::Color colorApplyingColorFilter(const WebCore::Color&) const;

    // Resolves any references to `currentcolor` in the provided `Style::Color` to the current `ComputedStyle::color()` value.
    WebCore::Color colorResolvingCurrentColor(const Style::Color&) const;
    WebCore::Color colorResolvingCurrentColorApplyingColorFilter(const Style::Color&) const;

    // Resolves any references to `currentcolor` in the provided `Style::Color` to the current `ComputedStyle::visitedLinkColor()` value.
    WebCore::Color visitedLinkColorResolvingCurrentColor(const Style::Color&) const;
    WebCore::Color visitedLinkColorResolvingCurrentColorApplyingColorFilter(const Style::Color&) const;

protected:
    bool NODELETE visitedDependentShouldReturnUnvisitedLinkColor(OptionSet<PaintBehavior>) const;

    const CheckedRef<const ComputedStyleProperties> m_style;
};

// Property specialized resolver. Can perform visited link color specific resolutions that the non-specialized resolver cannot.
template<typename ColorTraits> class ColorPropertyResolver : public ColorResolver {
public:
    explicit ColorPropertyResolver(const RenderStyle& style)
        : ColorResolver { style }
    {
    }

    explicit ColorPropertyResolver(const ComputedStyleProperties& style)
        : ColorResolver { style }
    {
    }

    // Resolves any references to `currentcolor` in `Style::Color` returned by `ColorTraits::color()` to the current `ComputedStyle::color()` value.
    WebCore::Color colorResolvingCurrentColor() const;
    WebCore::Color colorResolvingCurrentColorApplyingColorFilter() const;

    // Resolves any references to `currentcolor` in `Style::Color` returned by `ColorTraits::visitedLinkColor()` to the current `ComputedStyle::visitedLinkColor()` value.
    WebCore::Color visitedLinkColorResolvingCurrentColor() const requires (ImplementsVisitedLinkColor<ColorTraits>);
    WebCore::Color visitedLinkColorResolvingCurrentColorApplyingColorFilter() const requires (ImplementsVisitedLinkColor<ColorTraits>);

    // Uses provided `PaintBehavior` options to resolve an appropriate color for the type of painting, combining both the color and visited link colors as needed.
    WebCore::Color visitedDependentColor(OptionSet<PaintBehavior> = { }) const;
    WebCore::Color visitedDependentColorApplyingColorFilter(OptionSet<PaintBehavior> = { }) const;
};

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::colorResolvingCurrentColor() const
{
    if constexpr (std::same_as<ColorTraits, ColorPropertyTraits<PropertyNameConstant<CSSPropertyColor>>>)
        return m_style->color();
    else if constexpr (ImplementsColorResolvingCurrentColor<ColorTraits>)
        return ColorTraits::colorResolvingCurrentColor(m_style);
    else
        return ColorTraits::color(m_style).resolveColor(m_style->color());
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::colorResolvingCurrentColorApplyingColorFilter() const
{
    if (m_style->appleColorFilter().isNone())
        return colorResolvingCurrentColor();
    return colorApplyingColorFilter(colorResolvingCurrentColor());
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedLinkColorResolvingCurrentColor() const
    requires (ImplementsVisitedLinkColor<ColorTraits>)
{
    if constexpr (std::same_as<ColorTraits, ColorPropertyTraits<PropertyNameConstant<CSSPropertyColor>>>)
        return m_style->visitedLinkColor();
    else if constexpr (ImplementsVisitedLinkColorResolvingCurrentColor<ColorTraits>)
        return ColorTraits::visitedLinkColorResolvingCurrentColor(m_style);
    else
        return ColorTraits::visitedLinkColor(m_style).resolveColor(m_style->visitedLinkColor());
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedLinkColorResolvingCurrentColorApplyingColorFilter() const requires (ImplementsVisitedLinkColor<ColorTraits>)
{
    if (m_style->appleColorFilter().isNone())
        return visitedLinkColorResolvingCurrentColor();
    return colorApplyingColorFilter(visitedLinkColorResolvingCurrentColor());
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedDependentColor(OptionSet<PaintBehavior> paintBehavior) const
{
    auto unvisitedLinkColor = colorResolvingCurrentColor();

    if constexpr (!ImplementsVisitedLinkColor<ColorTraits>) {
        return unvisitedLinkColor;
    } else {
        if (visitedDependentShouldReturnUnvisitedLinkColor(paintBehavior))
            return unvisitedLinkColor;

        auto visitedLinkColor = visitedLinkColorResolvingCurrentColor();

        if constexpr (ImplementsExcludesVisitedLinkColor<ColorTraits>) {
            if (ColorTraits::excludesVisitedLinkColor(visitedLinkColor))
                return unvisitedLinkColor;
        }

        // Take the alpha from the unvisited color, but get the RGB values from the visited color.
        return visitedLinkColor.colorWithAlpha(unvisitedLinkColor.alphaAsFloat());
    }
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedDependentColorApplyingColorFilter(OptionSet<PaintBehavior> paintBehavior) const
{
    if (m_style->appleColorFilter().isNone())
        return visitedDependentColor(paintBehavior);
    return colorApplyingColorFilter(visitedDependentColor(paintBehavior));
}

} // namespace Style
} // namespace WebCore
