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

#include "config.h"
#include "StyleGridTrackBreadth.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<GridTrackBreadth>::operator()(BuilderState& state, const CSSPrimitiveValue& primitiveValue) -> GridTrackBreadth
{
    if (primitiveValue.isFlex())
        return toStyleFromCSSValue<Flex<CSS::Nonnegative>>(state, primitiveValue);
    return toStyleFromCSSValue<GridTrackBreadthLength>(state, primitiveValue);
}

// MARK: - Blending

auto Blending<GridTrackBreadth>::blend(const GridTrackBreadth& from, const GridTrackBreadth& to, const BlendingContext& context) -> GridTrackBreadth
{
    if (from.isFlex() != to.isFlex())
        return context.progress < 0.5 ? from : to;

    if (from.isFlex())
        return Style::blend(from.flex(), to.flex(), context);
    return Style::blend(from.length(), to.length(), context);
}

} // namespace Style
} // namespace WebCore
