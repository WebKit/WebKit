/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleScrollbarColor.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "CSSValuePair.h"
#include "RenderStyle.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ScrollbarColor>::operator()(BuilderState& state, const CSSValue& value) -> ScrollbarColor
{
    if (isValueID(value, CSSValueAuto))
        return CSS::Keyword::Auto { };

    RefPtr pair = requiredDowncast<CSSValuePair>(state, value);
    if (!pair)
        return CSS::Keyword::Auto { };

    Ref thumb = pair->first();
    Ref track = pair->second();

    return ScrollbarColor::Parts {
        .thumb = toStyleFromCSSValue<Color>(state, thumb, ForVisitedLink::No),
        .track = toStyleFromCSSValue<Color>(state, track, ForVisitedLink::No),
    };
}

// MARK: - Blending

auto Blending<ScrollbarColor>::equals(const ScrollbarColor& a, const ScrollbarColor& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    bool aAuto = a.isAuto();
    bool bAuto = b.isAuto();

    if (aAuto || bAuto)
        return aAuto == bAuto;

    return Style::equalsForBlending(a.tryValue()->thumb, b.tryValue()->thumb, aStyle, bStyle)
        && Style::equalsForBlending(a.tryValue()->track, b.tryValue()->track, aStyle, bStyle);
}

auto Blending<ScrollbarColor>::canBlend(const ScrollbarColor& a, const ScrollbarColor& b) -> bool
{
    return !a.isAuto()
        && !b.isAuto()
        && Style::canBlend(a.tryValue()->thumb, b.tryValue()->thumb)
        && Style::canBlend(a.tryValue()->track, b.tryValue()->track);
}

auto Blending<ScrollbarColor>::blend(const ScrollbarColor& a, const ScrollbarColor& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> ScrollbarColor
{
    if (!Style::canBlend(a, b)) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    return ScrollbarColor::Parts {
        .thumb = Style::blend(a.tryValue()->thumb, b.tryValue()->thumb, aStyle, bStyle, context),
        .track = Style::blend(a.tryValue()->track, b.tryValue()->track, aStyle, bStyle, context),
    };
}

} // namespace Style
} // namespace WebCore
