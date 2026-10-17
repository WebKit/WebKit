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

#include <WebCore/Color.h>
#include <WebCore/DocumentEnums.h>
#include <WebCore/StyleAppleColorFilter.h>
#include <WebCore/StyleComputedStyle.h>
#include <WebCore/StyleResolvedColors.h>

namespace WebCore {
namespace Style {

template<typename T> concept ImplementsVisitedLinkColor = requires {
    { T::visitedLinkColor(std::declval<const ComputedStyleProperties&>()) } -> std::same_as<const Color&>;
};

template<typename T> concept ImplementsResolveColor = requires {
    { T::resolveColor(std::declval<const ComputedStyleProperties&>(), std::declval<StyleColorOptionsPurpose>()) } -> std::same_as<WebCore::Color>;
};

template<typename T> concept ImplementsResolveVisitedLinkColor = requires {
    { T::resolveVisitedLinkColor(std::declval<const ComputedStyleProperties&>(), std::declval<StyleColorOptionsPurpose>()) } -> std::same_as<WebCore::Color>;
};

template<typename T> concept ImplementsExcludesVisitedLinkColor = requires {
    { T::excludesVisitedLinkColor(std::declval<const WebCore::Color&>()) } -> std::same_as<bool>;
};

class ColorResolver {
public:
    explicit ColorResolver(const Style::ComputedStyle& style)
        : m_style { style }
    {
    }

    explicit ColorResolver(const ComputedStyleProperties& style)
        : m_style { style }
    {
    }

    // Resolves any references to `currentcolor` in the provided `Style::Color` to the current `ComputedStyle::color()`.
    WebCore::Color resolveColor(const Style::Color&, StyleColorOptionsPurpose) const;
    // Similar to `resolveColor`, but resolves `currentcolor` to `ComputedStyle::visitedLinkColor()`.
    WebCore::Color resolveVisitedLinkColor(const Style::Color&, StyleColorOptionsPurpose) const;

    // Shorthand for resolving for ComputedStyle purpose.
    // FIXME: keeping the old name since it's widely used in the codebase, but should rename it.
    WebCore::Color colorResolvingCurrentColor(const Style::Color&) const;
    WebCore::Color visitedLinkColorResolvingCurrentColor(const Style::Color&) const;

    // Shorthand for resolving for Rendering purpose, then applying -apple-color-filter.
    // FIXME: keeping the old name since it's widely used in the codebase, but should rename it.
    WebCore::Color colorResolvingCurrentColorApplyingColorFilter(const Style::Color&) const;
    WebCore::Color visitedLinkColorResolvingCurrentColorApplyingColorFilter(const Style::Color&) const;

    WebCore::Color colorApplyingColorFilter(const WebCore::Color&) const;

    bool LEGACY_NODELETE visitedDependentShouldReturnUnvisitedLinkColor(OptionSet<PaintBehavior>) const;

protected:
    const CheckedRef<const ComputedStyleProperties> m_style;
};

// Property specialized resolver. Can perform visited link color specific resolutions that the non-specialized resolver cannot.
template<typename ColorTraits> class ColorPropertyResolver : public ColorResolver {
public:
    explicit ColorPropertyResolver(const Style::ComputedStyle& style)
        : ColorResolver { style }
    {
    }

    explicit ColorPropertyResolver(const ComputedStyleProperties& style)
        : ColorResolver { style }
    {
    }

    // Resolves any references to `currentcolor` in `Style::Color` returned by `ColorTraits::color()` to the current `ComputedStyle::color()` value.
    WebCore::Color resolveColor(StyleColorOptionsPurpose) const;
    // Resolves any references to `currentcolor` in `Style::Color` returned by `ColorTraits::visitedLinkColor()` to the current `ComputedStyle::visitedLinkColor()` value.
    WebCore::Color resolveVisitedLinkColor(StyleColorOptionsPurpose) const requires (ImplementsVisitedLinkColor<ColorTraits>);
    // Uses provided `PaintBehavior` options to resolve an appropriate color for the type of painting, combining both the color and visited link colors as needed.
    WebCore::Color visitedDependentResolveColor(OptionSet<PaintBehavior>, StyleColorOptionsPurpose) const;

    // Shorthand for resolving for ComputedStyle purpose.
    // FIXME: keeping the old name since it's widely used in the codebase, but should rename it.
    WebCore::Color colorResolvingCurrentColor() const;
    WebCore::Color visitedLinkColorResolvingCurrentColor() const requires (ImplementsVisitedLinkColor<ColorTraits>);
    WebCore::Color visitedDependentColor(OptionSet<PaintBehavior> = { }) const;

    // Shorthand for resolving for Rendering purpose, then applying -apple-color-filter.
    // FIXME: keeping the old name since it's widely used in the codebase, but should rename it.
    WebCore::Color colorResolvingCurrentColorApplyingColorFilter() const;
    WebCore::Color visitedLinkColorResolvingCurrentColorApplyingColorFilter() const requires (ImplementsVisitedLinkColor<ColorTraits>);
    WebCore::Color visitedDependentColorApplyingColorFilter(OptionSet<PaintBehavior> = { }) const;
};

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::resolveColor(StyleColorOptionsPurpose purpose) const
{
    if constexpr (std::same_as<ColorTraits, ColorPropertyTraits<PropertyNameConstant<CSSPropertyID::Color>>>) {
        switch (purpose) {
        case StyleColorOptionsPurpose::ComputedStyle:
            return m_style->color();
        case StyleColorOptionsPurpose::Rendering:
            return m_style->usedColor();
        }

        ASSERT_NOT_REACHED();
        return m_style->color();
    } else if constexpr (ImplementsResolveColor<ColorTraits>)
        return ColorTraits::resolveColor(m_style, purpose);
    else
        return ColorTraits::color(m_style).resolveColor(ResolvedColors::fromStyle(m_style, purpose));
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::colorResolvingCurrentColor() const
{
    return resolveColor(StyleColorOptionsPurpose::ComputedStyle);
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::colorResolvingCurrentColorApplyingColorFilter() const
{
    if (m_style->appleColorFilter().isNone())
        return resolveColor(StyleColorOptionsPurpose::Rendering);
    return colorApplyingColorFilter(resolveColor(StyleColorOptionsPurpose::Rendering));
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::resolveVisitedLinkColor(StyleColorOptionsPurpose purpose) const
    requires (ImplementsVisitedLinkColor<ColorTraits>)
{
    if constexpr (std::same_as<ColorTraits, ColorPropertyTraits<PropertyNameConstant<CSSPropertyID::Color>>>) {
        switch (purpose) {
        case StyleColorOptionsPurpose::ComputedStyle:
            return m_style->visitedLinkColor();
        case StyleColorOptionsPurpose::Rendering:
            return m_style->usedVisitedLinkColor();
        }

        ASSERT_NOT_REACHED();
        return m_style->visitedLinkColor();
    } else if constexpr (ImplementsResolveVisitedLinkColor<ColorTraits>)
        return ColorTraits::resolveVisitedLinkColor(m_style, purpose);
    else
        return ColorTraits::visitedLinkColor(m_style).resolveColor(ResolvedColors::fromVisitedLinkStyle(m_style, purpose));
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedLinkColorResolvingCurrentColor() const
    requires (ImplementsVisitedLinkColor<ColorTraits>)
{
    return resolveVisitedLinkColor(StyleColorOptionsPurpose::ComputedStyle);
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedLinkColorResolvingCurrentColorApplyingColorFilter() const requires (ImplementsVisitedLinkColor<ColorTraits>)
{
    if (m_style->appleColorFilter().isNone())
        return resolveVisitedLinkColor(StyleColorOptionsPurpose::Rendering);
    return colorApplyingColorFilter(resolveVisitedLinkColor(StyleColorOptionsPurpose::Rendering));
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedDependentResolveColor(OptionSet<PaintBehavior> paintBehavior, StyleColorOptionsPurpose purpose) const
{
    auto unvisitedLinkColor = resolveColor(purpose);

    if constexpr (!ImplementsVisitedLinkColor<ColorTraits>)
        return unvisitedLinkColor;
    else {
        if (visitedDependentShouldReturnUnvisitedLinkColor(paintBehavior))
            return unvisitedLinkColor;

        auto visitedLinkColor = resolveVisitedLinkColor(purpose);

        if constexpr (ImplementsExcludesVisitedLinkColor<ColorTraits>) {
            if (ColorTraits::excludesVisitedLinkColor(visitedLinkColor))
                return unvisitedLinkColor;
        }

        // Take the alpha from the unvisited color, but get the RGB values from the visited color.
        return visitedLinkColor.colorWithAlpha(unvisitedLinkColor.alphaAsFloat());
    }
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedDependentColor(OptionSet<PaintBehavior> paintBehavior) const
{
    return visitedDependentResolveColor(paintBehavior, StyleColorOptionsPurpose::ComputedStyle);
}

template<typename ColorTraits>
WebCore::Color ColorPropertyResolver<ColorTraits>::visitedDependentColorApplyingColorFilter(OptionSet<PaintBehavior> paintBehavior) const
{
    if (m_style->appleColorFilter().isNone())
        return visitedDependentResolveColor(paintBehavior, StyleColorOptionsPurpose::Rendering);
    return colorApplyingColorFilter(visitedDependentResolveColor(paintBehavior, StyleColorOptionsPurpose::Rendering));
}

} // namespace Style
} // namespace WebCore
