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
#include "StyleCursor.h"

#include "CSSCursorImageValue.h"
#include "CSSValueList.h"
#include "StyleBuilderChecking.h"
#include "StyleCursorImage.h"
#include "StyleInvalidImage.h"
#include "StyleKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<Cursor>::operator()(BuilderState& state, const CSSValue& value) -> Cursor
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value))
        return toStyleFromCSSValue<CursorType>(state, *keywordValue);

    auto list = requiredListDowncast<CSSValueList, CSSValue, 2>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    auto images = CursorImageList::createWithSizeFromGenerator(list->size() - 1, [&](auto index) {
        Ref item = list->item(index);
        RefPtr image = requiredDowncast<CSSCursorImageValue>(state, item);
        if (!image)
            return CursorImageAndHotSpot { InvalidImage::create(), std::nullopt };

        auto styleImage = image->createStyleImage(state);
        if (!styleImage) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CursorImageAndHotSpot { InvalidImage::create(), std::nullopt };
        }

        auto hotSpot = styleImage->hotSpot();
        return CursorImageAndHotSpot { styleImage.releaseNonNull(), WTF::move(hotSpot) };
    });

    return { WTF::move(images), toStyleFromCSSValue<CursorType>(state, list->item(list->size() - 1)) };
}

Ref<CSSValue> CSSValueCreation<CursorImageAndHotSpot>::operator()(CSSValuePool&, const RenderStyle& style, const CursorImageAndHotSpot& value)
{
    Ref image = value.image;
    return image->computedStyleValue(style);
}

// MARK: - Serialization

void Serialize<CursorImageAndHotSpot>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const CursorImageAndHotSpot& value)
{
    Ref image = value.image;
    Ref computedValue = image->computedStyleValue(style);
    builder.append(computedValue->cssText(context));
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const CursorImageAndHotSpot& value)
{
    return ts << "cursor image with hotspot " << value.hotSpot;
}

} // namespace Style
} // namespace WebCore
