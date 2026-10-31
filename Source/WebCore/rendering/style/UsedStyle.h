/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/RenderObjectStyle.h>
#include <WebCore/StyleComputedStyle+GettersInlines.h>
#include <WebCore/StylePrimitiveNumericTypes+Evaluation.h>
#include <WebCore/UsedStyleProperties.h>

namespace WebCore {

// A lightweight, non-owning view of a renderer's used values, as defined by CSS Cascade &
// Inheritance: the computed value with any remaining layout-time calculations completed. It wraps
// a renderer and either its style or the style of one of its non-box pseudo-elements, such as
// ::first-line, available via computedStyle().
//
// Getters mirror the Style::ComputedStyle getter names but return used values. Properties opt in
// via the `used-style` codegen property in CSSProperties.json (see UsedStyleProperties). Used
// values needing bespoke logic (float/clear) are hand-written here.
class UsedStyle final : public UsedStyleProperties {
public:
    explicit UsedStyle(const RenderObject& renderer LIFETIME_BOUND)
        : UsedStyleProperties(renderer, renderer.style())
    {
    }

    // Used values that need bespoke logic, defined out-of-line in UsedStyle.cpp.
    // float/clear map logical and writing-mode-relative values to physical
    // left/right using the containing block's writing mode.
    UsedFloat floating() const;
    UsedClear clear() const;

    inline UsedVisibility visibility() const;

    // The box's own border widths, zoomed and snapped to device pixels. Formatting-context
    // adjustments, like collapsed table borders, are applied by RenderBoxModelObject::borderTop()
    // and friends.
    template<typename T = LayoutUnit> RectEdges<T> borderWidths() const;
    LayoutUnit borderTopWidth() const { return computedStyle().hasZeroEffectiveBorderWidths() ? 0_lu : evaluateZoomed<LayoutUnit>(Style::effectiveBorderWidths(computedStyle()).top()); }
    LayoutUnit borderRightWidth() const { return computedStyle().hasZeroEffectiveBorderWidths() ? 0_lu : evaluateZoomed<LayoutUnit>(Style::effectiveBorderWidths(computedStyle()).right()); }
    LayoutUnit borderBottomWidth() const { return computedStyle().hasZeroEffectiveBorderWidths() ? 0_lu : evaluateZoomed<LayoutUnit>(Style::effectiveBorderWidths(computedStyle()).bottom()); }
    LayoutUnit borderLeftWidth() const { return computedStyle().hasZeroEffectiveBorderWidths() ? 0_lu : evaluateZoomed<LayoutUnit>(Style::effectiveBorderWidths(computedStyle()).left()); }
    LayoutUnit borderWidthStart() const { return computedStyle().hasZeroEffectiveBorderWidths() ? 0_lu : evaluateZoomed<LayoutUnit>(Style::effectiveBorderWidths(computedStyle()).start(computedStyle().writingMode())); }
    LayoutUnit borderWidthEnd() const { return computedStyle().hasZeroEffectiveBorderWidths() ? 0_lu : evaluateZoomed<LayoutUnit>(Style::effectiveBorderWidths(computedStyle()).end(computedStyle().writingMode())); }
    LayoutUnit borderWidthBefore() const { return computedStyle().hasZeroEffectiveBorderWidths() ? 0_lu : evaluateZoomed<LayoutUnit>(Style::effectiveBorderWidths(computedStyle()).before(computedStyle().writingMode())); }
    LayoutUnit borderWidthAfter() const { return computedStyle().hasZeroEffectiveBorderWidths() ? 0_lu : evaluateZoomed<LayoutUnit>(Style::effectiveBorderWidths(computedStyle()).after(computedStyle().writingMode())); }

    // Zoomed and snapped to device pixels, like the border widths.
    template<typename T = LayoutUnit> T outlineWidth() const { return evaluateZoomed<T>(Style::effectiveOutlineWidth(computedStyle())); }
    template<typename T = LayoutUnit> T outlineOffset() const { return evaluateZoomed<T>(Style::effectiveOutlineOffset(computedStyle())); }
    // How far the outline reaches outside the border box.
    float outlineSize() const { return Style::effectiveOutlineSize(computedStyle(), computedStyle().usedZoomForLength(), deviceScaleFactor()); }
    template<typename T = LayoutUnit> T columnRuleWidth() const { return evaluateZoomed<T>(Style::effectiveColumnRuleWidth(computedStyle())); }

private:
    friend class RenderObject;

    float deviceScaleFactor() const { return computedStyle().deviceScaleFactor(); }
    template<typename T, typename Value> T evaluateZoomed(const Value& value) const { return Style::evaluate<T>(value, computedStyle().usedZoomForLength(), deviceScaleFactor()); }

    UsedStyle(const RenderObject& renderer LIFETIME_BOUND, const Style::ComputedStyle& pseudoElementStyle LIFETIME_BOUND)
        : UsedStyleProperties(renderer, pseudoElementStyle)
    {
    }
};

template<typename T> RectEdges<T> UsedStyle::borderWidths() const
{
    if (computedStyle().hasZeroEffectiveBorderWidths())
        return { };
    return RectEdges<T>::map(Style::effectiveBorderWidths(computedStyle()), [&](const auto& width) {
        return evaluateZoomed<T>(width);
    });
}

inline UsedVisibility UsedStyle::visibility() const
{
    // Text is hidden with its parent.
    const RenderObject& renderer = m_renderer;
    auto& element = renderer.isRenderElement() ? uncheckedDowncast<RenderElement>(renderer) : *renderer.parent();
    if (element.isForceHiddenByLineClamp() || computedStyle().isForceHidden()) [[unlikely]]
        return UsedVisibility::Hidden;
    return computedStyle().visibility() == Visibility::Visible ? UsedVisibility::Visible : UsedVisibility::Hidden;
}

} // namespace WebCore
