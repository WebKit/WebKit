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

#include <WebCore/UsedStyleProperties.h>

namespace WebCore {

namespace Layout {
class Box;
}

// A lightweight, non-owning view of a renderer's used values, as defined by
// CSS Cascade & Inheritance: the computed value with any remaining layout-time
// calculations completed. It wraps a renderer and its computed style
// (Style::ComputedStyle, available via computedStyle()).
//
// Getters mirror the Style::ComputedStyle getter names but return used values.
// Properties opt in via CSSProperties.json: `used-style` generates a plain
// forwarding getter, `used-style-getter-evaluate-zoomed` generates getters that
// resolve a length-percentage against a reference with zoom (see
// UsedStyleProperties). Used values needing bespoke logic (float/clear) are
// hand-written here.
class UsedStyle final : public UsedStyleProperties {
public:
    UsedStyle(const Style::ComputedStyle& computedStyle LIFETIME_BOUND, const RenderElement& renderer LIFETIME_BOUND)
        : UsedStyleProperties(computedStyle, &renderer)
    {
    }

    // For layout-tree (LFC) code, which is render-tree-independent: wraps the box's style with
    // no renderer. Renderer-derived references are therefore unavailable, so only the explicit
    // LayoutUnit getters (where the caller supplies the reference) may be used. Out-of-line in
    // UsedStyle.cpp to keep the Layout::Box dependency out of this header.
    explicit UsedStyle(const Layout::Box&);

    // Used values that need bespoke logic, defined out-of-line in UsedStyle.cpp.
    // float/clear map logical and writing-mode-relative values to physical
    // left/right using the containing block's writing mode.
    UsedFloat floating() const;
    UsedClear clear() const;
};

} // namespace WebCore
