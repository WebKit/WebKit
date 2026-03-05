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
#include "StyleMaskBorder.h"

#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSValueList.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

MaskBorder::MaskBorder()
    : maskBorderSource { Style::ComputedStyle::initialMaskBorderSource() }
    , maskBorderSlice { Style::ComputedStyle::initialMaskBorderSlice() }
    , maskBorderWidth { Style::ComputedStyle::initialMaskBorderWidth() }
    , maskBorderOutset { Style::ComputedStyle::initialMaskBorderOutset() }
    , maskBorderRepeat { Style::ComputedStyle::initialMaskBorderRepeat() }
{
}

MaskBorder::MaskBorder(MaskBorderSource&& source, MaskBorderSlice&& slice, MaskBorderWidth&& width, MaskBorderOutset&& outset, MaskBorderRepeat&& repeat)
    : maskBorderSource { WTF::move(source) }
    , maskBorderSlice { WTF::move(slice) }
    , maskBorderWidth { WTF::move(width) }
    , maskBorderOutset { WTF::move(outset) }
    , maskBorderRepeat { WTF::move(repeat) }
{
}

// MARK: - Conversion

auto CSSValueConversion<MaskBorder>::operator()(BuilderState& state, const CSSValue& value, MaskBorderSliceOverride maskBorderSliceOverride) -> MaskBorder
{
    MaskBorder result { };

    RefPtr borderImage = requiredDowncast<CSSValueList>(state, value);
    if (!borderImage)
        return result;

    for (Ref current : *borderImage) {
        if (current->isImage())
            result.maskBorderSource = toStyleFromCSSValue<MaskBorderSource>(state, current);
        else if (RefPtr slice = dynamicDowncast<CSSBorderImageSliceValue>(current))
            result.maskBorderSlice = toStyleFromCSSValue<MaskBorderSlice>(state, *slice);
        else if (RefPtr slashList = dynamicDowncast<CSSValueList>(current)) {
            if (RefPtr slice = dynamicDowncast<CSSBorderImageSliceValue>(slashList->item(0)))
                result.maskBorderSlice = toStyleFromCSSValue<MaskBorderSlice>(state, *slice);
            if (RefPtr width = dynamicDowncast<CSSBorderImageWidthValue>(slashList->item(1)))
                result.maskBorderWidth = toStyleFromCSSValue<MaskBorderWidth>(state, *width);
            if (RefPtr outset = slashList->item(2))
                result.maskBorderOutset = toStyleFromCSSValue<MaskBorderOutset>(state, *outset);
        } else if (current->isPair())
            result.maskBorderRepeat = toStyleFromCSSValue<MaskBorderRepeat>(state, current);
    }

    if (maskBorderSliceOverride == MaskBorderSliceOverride::AlwaysFill)
        result.maskBorderSlice.fill = CSS::Keyword::Fill { };

    return result;
}

auto CSSValueCreation<MaskBorder>::operator()(CSSValuePool& pool, const RenderStyle& style, const MaskBorder& value) -> Ref<CSSValue>
{
    return createBorderImageValue({
        .source = createCSSValue(pool, style, value.maskBorderSource),
        .slice  = createCSSValue(pool, style, value.maskBorderSlice),
        .width  = createCSSValue(pool, style, value.maskBorderWidth),
        .outset = createCSSValue(pool, style, value.maskBorderOutset),
        .repeat = createCSSValue(pool, style, value.maskBorderRepeat),
    });
}

// MARK: - Serialization

void Serialize<MaskBorder>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const MaskBorder& value)
{
    if (value.maskBorderSource.isNone()) {
        serializationForCSS(builder, context, style, value.maskBorderSource);
        return;
    }

    // FIXME: Omit values that have their initial value.

    serializationForCSS(builder, context, style, value.maskBorderSource);
    builder.append(' ');
    serializationForCSS(builder, context, style, value.maskBorderSlice);
    builder.append(" / "_s);
    serializationForCSS(builder, context, style, value.maskBorderWidth);
    builder.append(" / "_s);
    serializationForCSS(builder, context, style, value.maskBorderOutset);
    builder.append(' ');
    serializationForCSS(builder, context, style, value.maskBorderRepeat);
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const MaskBorder& value)
{
    return ts << "style-image "_s << value.maskBorderSource << " slices "_s << value.maskBorderSlice;
}

} // namespace Style
} // namespace WebCore
