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
#include "StyleZIndex.h"

#include "AnimationUtilities.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ZIndex>::operator()(BuilderState& state, const CSSValue& value) -> ZIndex
{
    if (value.valueID() == CSSValueAuto)
        return CSS::Keyword::Auto { };
    return toStyleFromCSSValue<ZIndex::Value>(state, value);
}

// MARK: - Blending

auto Blending<ZIndex>::canBlend(const ZIndex& a, const ZIndex& b) -> bool
{
    return !a.isAuto() && !b.isAuto();
}

auto Blending<ZIndex>::blend(const ZIndex& a, const ZIndex& b, const BlendingContext& context) -> ZIndex
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(!a.isAuto());
    ASSERT(!b.isAuto());
    return { WebCore::Style::blend(*a.tryValue(), *b.tryValue(), context) };
}

} // namespace Style
} // namespace WebCore
