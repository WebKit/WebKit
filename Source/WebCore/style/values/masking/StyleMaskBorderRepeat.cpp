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
#include "StyleMaskBorderRepeat.h"

#include "CSSMaskBorderRepeatValue.h"
#include "StyleKeyword+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

static auto toCSSMaskBorderRepeatValue(const MaskBorderRepeat::Value& value) -> CSS::MaskBorderRepeat::Value
{
    switch (value) {
    case NinePieceImageRule::Stretch: return CSS::Keyword::Stretch { };
    case NinePieceImageRule::Round:   return CSS::Keyword::Round { };
    case NinePieceImageRule::Space:   return CSS::Keyword::Space { };
    case NinePieceImageRule::Repeat:  return CSS::Keyword::Repeat { };
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static auto toStyleMaskBorderRepeatValue(const CSS::MaskBorderRepeat::Value& value) -> MaskBorderRepeat::Value
{
    return WTF::switchOn(value,
        [](CSS::Keyword::Stretch) { return NinePieceImageRule::Stretch; },
        [](CSS::Keyword::Round)   { return NinePieceImageRule::Round; },
        [](CSS::Keyword::Space)   { return NinePieceImageRule::Space; },
        [](CSS::Keyword::Repeat)  { return NinePieceImageRule::Repeat; }
    );
}

auto ToCSS<MaskBorderRepeat>::operator()(const MaskBorderRepeat& value, const RenderStyle&) -> CSS::MaskBorderRepeat
{
    return { {
        toCSSMaskBorderRepeatValue(value.values.width()),
        toCSSMaskBorderRepeatValue(value.values.height()),
    } };
}

auto ToStyle<CSS::MaskBorderRepeat>::operator()(const CSS::MaskBorderRepeat& value, const BuilderState&) -> MaskBorderRepeat
{
    return {
        toStyleMaskBorderRepeatValue(value.values.width()),
        toStyleMaskBorderRepeatValue(value.values.height()),
    };
}

auto CSSValueConversion<MaskBorderRepeat>::operator()(BuilderState& state, const CSSValue& value) -> MaskBorderRepeat
{
    if (RefPtr repeatValue = dynamicDowncast<CSSMaskBorderRepeatValue>(value))
        return toStyle(repeatValue->repeats(), state);

    // Values coming from CSS Typed OM may not have been converted to a CSSMaskBorderRepeatValue.
    return toStyleFromCSSValue<NinePieceImageRule>(state, value);
}

auto CSSValueCreation<MaskBorderRepeat>::operator()(CSSValuePool&, const RenderStyle& style, const MaskBorderRepeat& value) -> Ref<CSSValue>
{
    return CSSMaskBorderRepeatValue::create(toCSS(value, style));
}

} // namespace Style
} // namespace WebCore
