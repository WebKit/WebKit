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
#include "StyleBackgroundSize.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "CSSValuePair.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<BackgroundSize>::operator()(BuilderState& state, const CSSValue& value) -> BackgroundSize
{
    if (RefPtr pair = dynamicDowncast<CSSValuePair>(value)) {
        return BackgroundSize::LengthSize {
            toStyleFromCSSValue<BackgroundSizeLength>(state, pair->first()),
            toStyleFromCSSValue<BackgroundSizeLength>(state, pair->second()),
        };
    }

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Auto { };

    if (primitiveValue->isValueID()) {
        switch (primitiveValue->valueID()) {
        case CSSValueCover:
            return CSS::Keyword::Cover { };
        case CSSValueContain:
            return CSS::Keyword::Contain { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    return toStyleFromCSSValue<BackgroundSizeLength>(state, *primitiveValue);
}

auto CSSValueCreation<BackgroundSize>::operator()(CSSValuePool& pool, const RenderStyle& style, const BackgroundSize& value) -> Ref<CSSValue>
{
    return WTF::switchOn(value,
        [&](const CSS::Keyword::Cover& keyword) -> Ref<CSSValue> {
            return createCSSValue(pool, style, keyword);
        },
        [&](const CSS::Keyword::Contain& keyword) -> Ref<CSSValue> {
            return createCSSValue(pool, style, keyword);
        },
        [&](const BackgroundSize::LengthSize& lengthSize) -> Ref<CSSValue> {
            if (lengthSize.width().isAuto() && lengthSize.height().isAuto())
                return createCSSValue(pool, style, CSS::Keyword::Auto { });
            else
                return createCSSValue(pool, style, lengthSize);
        }
    );
}

// MARK: - Serialization

void Serialize<BackgroundSize>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const BackgroundSize& value)
{
    WTF::switchOn(value,
        [&](const CSS::Keyword::Cover& keyword) {
            serializationForCSS(builder, context, style, keyword);
        },
        [&](const CSS::Keyword::Contain& keyword) {
            serializationForCSS(builder, context, style, keyword);
        },
        [&](const BackgroundSize::LengthSize& lengthSize) {
            // FIXME: This should probably serialize just the first value if the second is `auto` but this currently causes a WPT test to fail.
            if (lengthSize.width().isAuto() && lengthSize.height().isAuto())
                serializationForCSS(builder, context, style, CSS::Keyword::Auto { });
            else
                serializationForCSS(builder, context, style, lengthSize);
        }
    );
}

// MARK: - Blending

auto Blending<BackgroundSize>::canBlend(const BackgroundSize& a, const BackgroundSize& b) -> bool
{
    if (!a.hasSameType(b))
        return false;

    if (!a.isLengthSize())
        return true;

    return Style::canBlend(a.tryLengthSize()->width(),  b.tryLengthSize()->width())
        && Style::canBlend(a.tryLengthSize()->height(), b.tryLengthSize()->height());
}

auto Blending<BackgroundSize>::blend(const BackgroundSize& a, const BackgroundSize& b, const BlendingContext& context) -> BackgroundSize
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(a.hasSameType(b));

    if (!a.isLengthSize())
        return context.progress ? b : a;

    return BackgroundSize::LengthSize {
        Style::blend(a.tryLengthSize()->width(),  b.tryLengthSize()->width(), context),
        Style::blend(a.tryLengthSize()->height(), b.tryLengthSize()->height(), context),
    };
}

} // namespace Style
} // namespace WebCore
