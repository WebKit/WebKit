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

#include "config.h"
#include "StyleVisibility.h"

#include "AnimationUtilities.h"
#include "StyleInterpolationContext.h"

namespace WebCore {
namespace Style {

// MARK: - Blending

auto Blending<Visibility>::canBlend(const Visibility& a, const Visibility& b) -> bool
{
    // https://drafts.csswg.org/web-animations-1/#animating-visibility
    // If neither value is visible, then discrete animation is used.
    return a == Style::Visibility::Visible || b == Style::Visibility::Visible;
}

auto Blending<Visibility>::blend(const Visibility& a, const Visibility& b, const Interpolation::Context& context) -> Visibility
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    // Any non-zero result means we consider the object to be visible. Only at 0 do we consider the object to be
    // invisible. The invisible value we use (Visibility::Hidden vs. Style::Visibility::Collapse) depends on the specified from/to values.
    auto aVal = a == Style::Visibility::Visible ? 1.0 : 0.9;
    auto bVal = b == Style::Visibility::Visible ? 1.0 : 0.0;
    if (aVal == bVal)
        return b;

     // The composite operation here is irrelevant.
    auto result = WebCore::blend(aVal, bVal, Interpolation::Context {
            context.property,
            context.progress,
            false,
            CompositeOperation::Replace,
            IterationCompositeOperation::Replace,
            0,
            { },
            { },
            context.client
        }
    );
    return result > 0.0 ? Style::Visibility::Visible : (b != Style::Visibility::Visible ? b : a);
}

} // namespace Style
} // namespace WebCore
