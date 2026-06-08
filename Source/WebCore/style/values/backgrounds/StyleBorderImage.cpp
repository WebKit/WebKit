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
#include "StyleBorderImage.h"

#include "CSSBorderImage.h"
#include "CSSValueList.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StyleKeyword+CSSValueCreation.h"
#include "StyleKeyword+Logging.h"
#include "StyleKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

BorderImage::BorderImage()
    : borderImageSource { Style::ComputedStyle::initialBorderImageSource() }
    , borderImageSlice { Style::ComputedStyle::initialBorderImageSlice() }
    , borderImageWidth { Style::ComputedStyle::initialBorderImageWidth() }
    , borderImageOutset { Style::ComputedStyle::initialBorderImageOutset() }
    , borderImageRepeat { Style::ComputedStyle::initialBorderImageRepeat() }
{
}

BorderImage::BorderImage(BorderImageSource&& source, BorderImageSlice&& slice, BorderImageWidth&& width, BorderImageOutset&& outset, BorderImageRepeat&& repeat)
    : borderImageSource { WTF::move(source) }
    , borderImageSlice { WTF::move(slice) }
    , borderImageWidth { WTF::move(width) }
    , borderImageOutset { WTF::move(outset) }
    , borderImageRepeat { WTF::move(repeat) }
{
}

// MARK: - Conversion

auto ToCSS<BorderImage>::operator()(const BorderImage& value, const RenderStyle& style) -> CSS::BorderImage
{
    return {
        .borderImageSource = toCSS(value.borderImageSource, style),
        .borderImageSlice = toCSS(value.borderImageSlice, style),
        .borderImageWidth = toCSS(value.borderImageWidth, style),
        .borderImageOutset = toCSS(value.borderImageOutset, style),
        .borderImageRepeat = toCSS(value.borderImageRepeat, style),
    };
}

auto ToStyle<CSS::BorderImage>::operator()(const CSS::BorderImage& value, const BuilderState& state) -> BorderImage
{
    BorderImage result { };

    if (value.borderImageSource)
        result.borderImageSource = toStyle(*value.borderImageSource, state);
    if (value.borderImageSlice)
        result.borderImageSlice = toStyle(*value.borderImageSlice, state);
    if (value.borderImageWidth)
        result.borderImageWidth = toStyle(*value.borderImageWidth, state);
    if (value.borderImageOutset)
        result.borderImageOutset = toStyle(*value.borderImageOutset, state);
    if (value.borderImageRepeat)
        result.borderImageRepeat = toStyle(*value.borderImageRepeat, state);

    return result;
}

auto CSSValueCreation<BorderImage>::operator()(CSSValuePool& pool, const RenderStyle& style, const BorderImage& value) -> Ref<CSSValue>
{
    return CSS::createCSSValue(pool, toCSS(value, style));
}

// MARK: - Serialization

void Serialize<BorderImage>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const BorderImage& value)
{
    if (value.borderImageSource.isNone()) {
        serializationForCSS(builder, context, style, value.borderImageSource);
        return;
    }

    // FIXME: Omit values that have their initial value.

    serializationForCSS(builder, context, style, value.borderImageSource);
    builder.append(' ');
    serializationForCSS(builder, context, style, value.borderImageSlice);
    builder.append(" / "_s);
    serializationForCSS(builder, context, style, value.borderImageWidth);
    builder.append(" / "_s);
    serializationForCSS(builder, context, style, value.borderImageOutset);
    builder.append(' ');
    serializationForCSS(builder, context, style, value.borderImageRepeat);
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const BorderImage& value)
{
    return ts << "style-image "_s << value.borderImageSource << " slices "_s << value.borderImageSlice;
}

} // namespace Style
} // namespace WebCore
