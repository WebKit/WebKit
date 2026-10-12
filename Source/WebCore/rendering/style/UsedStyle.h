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

#include <WebCore/StyleComputedStyle+GettersInlines.h>
#include <WebCore/UsedStyleProperties.h>

namespace WebCore {

namespace Layout {
class Box;
}

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
    explicit UsedStyle(const RenderElement& renderer LIFETIME_BOUND)
        : UsedStyleProperties(renderer, renderer.style())
    {
    }

    // For layout-tree (LFC) code. Wraps the renderer the box was built from.
    explicit UsedStyle(const Layout::Box& layoutBox LIFETIME_BOUND);

    // Used values that need bespoke logic, defined out-of-line in UsedStyle.cpp.
    // float/clear map logical and writing-mode-relative values to physical
    // left/right using the containing block's writing mode.
    UsedFloat floating() const;
    UsedClear clear() const;

    inline UsedVisibility visibility() const;

private:
    friend class RenderElement;

    // The style must belong to the renderer, as returned by RenderElement::firstLineStyle().
    UsedStyle(const RenderElement& renderer LIFETIME_BOUND, const Style::ComputedStyle& pseudoElementStyle LIFETIME_BOUND)
        : UsedStyleProperties(renderer, pseudoElementStyle)
    {
    }
};

inline UsedVisibility UsedStyle::visibility() const
{
    if (m_renderer->isForceHiddenByLineClamp() || computedStyle().isForceHidden()) [[unlikely]]
        return UsedVisibility::Hidden;
    return computedStyle().visibility() == Visibility::Visible ? UsedVisibility::Visible : UsedVisibility::Hidden;
}

} // namespace WebCore
