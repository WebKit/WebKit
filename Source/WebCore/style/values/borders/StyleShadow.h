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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatRect.h"
#include "LayoutRect.h"
#include "WritingMode.h"
#include <wtf/FixedVector.h>

namespace WebCore {
namespace Style {

enum class ShadowStyle : bool { Normal, Inset };

template<typename T> concept Shadow = requires(const T& shadow) {
    { shadowStyle(shadow) } -> std::same_as<ShadowStyle>;
    { isInset(shadow) } -> std::same_as<bool>;
    { paintingSpread(shadow) } -> std::same_as<LayoutUnit>;
};

template<typename ShadowType> using ShadowList = CommaSeparatedFixedVector<ShadowType>;
template<typename ShadowType> struct Shadows : ListOrNone<ShadowList<ShadowType>> { using ListOrNone<ShadowList<ShadowType>>::ListOrNone; };

LayoutUnit paintingExtent(Shadow auto const& shadow)
{
    // Blurring uses a Gaussian function whose std. deviation is m_radius/2, and which in theory
    // extends to infinity. In 8-bit contexts, however, rounding causes the effect to become
    // undetectable at around 1.4x the radius.
    constexpr const float radiusExtentMultiplier = 1.4;
    return LayoutUnit { ceilf(shadow.blur.value * radiusExtentMultiplier) };
}

LayoutUnit paintingExtentAndSpread(Shadow auto const& shadow)
{
    return paintingExtent(shadow) + paintingSpread(shadow);
}

template<Shadow ShadowType> auto shadowOutsetExtent(const Shadows<ShadowType>& shadows) -> LayoutBoxExtent
{
    LayoutUnit top;
    LayoutUnit right;
    LayoutUnit bottom;
    LayoutUnit left;

    for (const auto& shadow : shadows) {
        if (isInset(shadow))
            continue;

        auto extentAndSpread = paintingExtentAndSpread(shadow);

        left = std::min<LayoutUnit>(left, LayoutUnit(shadow.location.x().value) - extentAndSpread);
        right = std::max<LayoutUnit>(right, LayoutUnit(shadow.location.x().value) + extentAndSpread);
        top = std::min<LayoutUnit>(top, LayoutUnit(shadow.location.y().value) - extentAndSpread);
        bottom = std::max<LayoutUnit>(bottom, LayoutUnit(shadow.location.y().value) + extentAndSpread);
    }

    return { top, right, bottom, left };
}

template<Shadow ShadowType> auto shadowInsetExtent(const Shadows<ShadowType>& shadows) -> LayoutBoxExtent
{
    LayoutUnit top;
    LayoutUnit right;
    LayoutUnit bottom;
    LayoutUnit left;

    for (const auto& shadow : shadows) {
        if (!isInset(shadow))
            continue;

        auto extentAndSpread = paintingExtentAndSpread(shadow);

        top = std::max<LayoutUnit>(top, LayoutUnit(shadow.location.y().value) + extentAndSpread);
        right = std::min<LayoutUnit>(right, LayoutUnit(shadow.location.x().value) - extentAndSpread);
        bottom = std::min<LayoutUnit>(bottom, LayoutUnit(shadow.location.y().value) - extentAndSpread);
        left = std::max<LayoutUnit>(left, LayoutUnit(shadow.location.x().value) + extentAndSpread);
    }

    return { top, right, bottom, left };
}

template<Shadow ShadowType> auto shadowHorizontalExtent(const Shadows<ShadowType>& shadows) -> std::pair<LayoutUnit, LayoutUnit>
{
    LayoutUnit left = 0;
    LayoutUnit right = 0;

    for (const auto& shadow : shadows) {
        if (isInset(shadow))
            continue;

        auto extentAndSpread = paintingExtentAndSpread(shadow);

        left = std::min<LayoutUnit>(left, LayoutUnit(shadow.location.x().value) - extentAndSpread);
        right = std::max<LayoutUnit>(right, LayoutUnit(shadow.location.x().value) + extentAndSpread);
    }

    return { left, right };
}

template<Shadow ShadowType> auto shadowVerticalExtent(const Shadows<ShadowType>& shadows) -> std::pair<LayoutUnit, LayoutUnit>
{
    LayoutUnit top = 0;
    LayoutUnit bottom = 0;

    for (const auto& shadow : shadows) {
        if (isInset(shadow))
            continue;

        auto extentAndSpread = paintingExtentAndSpread(shadow);

        // FIXME: Why does this do a static cast to `int` but all of the other "extent" functions in this file do not?
        top = std::min<LayoutUnit>(top, LayoutUnit(static_cast<int>(shadow.location.y().value)) - extentAndSpread);
        bottom = std::max<LayoutUnit>(bottom, LayoutUnit(static_cast<int>(shadow.location.y().value)) + extentAndSpread);
    }

    return { top, bottom };
}

template<Shadow ShadowType> auto shadowBlockDirectionExtent(const Shadows<ShadowType>& shadows, WritingMode writingMode) -> std::pair<LayoutUnit, LayoutUnit>
{
    return writingMode.isHorizontal() ? shadowVerticalExtent(shadows) : shadowHorizontalExtent(shadows);
}

template<Shadow ShadowType> auto shadowInlineDirectionExtent(const Shadows<ShadowType>& shadows, WritingMode writingMode) -> std::pair<LayoutUnit, LayoutUnit>
{
    return writingMode.isHorizontal() ? shadowHorizontalExtent(shadows) : shadowVerticalExtent(shadows);
}

template<Shadow ShadowType> void adjustRectForShadow(LayoutRect& rect, const Shadows<ShadowType>& shadows)
{
    auto shadowExtent = shadowOutsetExtent(shadows);

    rect.move(shadowExtent.left(), shadowExtent.top());
    rect.setWidth(rect.width() - shadowExtent.left() + shadowExtent.right());
    rect.setHeight(rect.height() - shadowExtent.top() + shadowExtent.bottom());
}

template<Shadow ShadowType> void adjustRectForShadow(FloatRect& rect, const Shadows<ShadowType>& shadows)
{
    auto shadowExtent = shadowOutsetExtent(shadows);

    rect.move(shadowExtent.left(), shadowExtent.top());
    rect.setWidth(rect.width() - shadowExtent.left() + shadowExtent.right());
    rect.setHeight(rect.height() - shadowExtent.top() + shadowExtent.bottom());
}

} // namespace Style
} // namespace WebCore
