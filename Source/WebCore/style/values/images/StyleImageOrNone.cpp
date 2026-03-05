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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleImageOrNone.h"

#include "AnimationUtilities.h"
#include "StyleBuilderState.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ImageOrNone>::operator()(BuilderState& state, const CSSValue& value) -> ImageOrNone
{
    if (value.valueID() == CSSValueNone)
        return CSS::Keyword::None { };

    RefPtr image = state.createStyleImage(value);
    if (!image)
        return CSS::Keyword::None { };

    return ImageWrapper { image.releaseNonNull() };
}

// MARK: - Blending

auto Blending<ImageOrNone>::canBlend(const ImageOrNone& a, const ImageOrNone& b) -> bool
{
    return !a.isNone() && !b.isNone();
}

auto Blending<ImageOrNone>::blend(const ImageOrNone& a, const ImageOrNone& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> ImageOrNone
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    ASSERT(canBlend(a, b));
    return Style::blend(*a.tryImage(), *b.tryImage(), aStyle, bStyle, context);
}

} // namespace Style
} // namespace WebCore
