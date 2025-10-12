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
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

DataRef<MaskBorder::Data>& MaskBorder::defaultData()
{
    static NeverDestroyed<DataRef<Data>> data { Data::create() };
    return data.get();
}

MaskBorder::MaskBorder()
    : m_data(defaultData())
{
}

MaskBorder::MaskBorder(MaskBorderSource&& source, MaskBorderSlice&& slice, MaskBorderWidth&& width, MaskBorderOutset&& outset, MaskBorderRepeat&& repeat)
    : m_data(Data::create(WTFMove(source), WTFMove(slice), WTFMove(width), WTFMove(outset), WTFMove(repeat)))
{
}

MaskBorder::Data::Data() = default;
MaskBorder::Data::Data(MaskBorderSource&& source, MaskBorderSlice&& slice, MaskBorderWidth&& width, MaskBorderOutset&& outset, MaskBorderRepeat&& repeat)
    : source { WTFMove(source) }
    , slice { WTFMove(slice) }
    , width { WTFMove(width) }
    , outset { WTFMove(outset) }
    , repeat { WTFMove(repeat) }
{
}

MaskBorder::Data::Data(const Data& other)
    : RefCounted<Data>()
    , source { other.source }
    , slice { other.slice }
    , width { other.width }
    , outset { other.outset }
    , repeat { other.repeat }
{
}

Ref<MaskBorder::Data> MaskBorder::Data::create()
{
    return adoptRef(*new Data);
}

Ref<MaskBorder::Data> MaskBorder::Data::create(MaskBorderSource&& source, MaskBorderSlice&& slice, MaskBorderWidth&& width, MaskBorderOutset&& outset, MaskBorderRepeat&& repeat)
{
    return adoptRef(*new Data(WTFMove(source), WTFMove(slice), WTFMove(width), WTFMove(outset), WTFMove(repeat)));
}

Ref<MaskBorder::Data> MaskBorder::Data::copy() const
{
    return adoptRef(*new Data(*this));
}

bool MaskBorder::Data::operator==(const Data& other) const
{
    return source == other.source
        && slice == other.slice
        && width == other.width
        && outset == other.outset
        && repeat == other.repeat;
}

// MARK: - Conversion

auto CSSValueConversion<MaskBorder>::operator()(BuilderState& state, const CSSValue& value) -> MaskBorder
{
    MaskBorder result { };

    RefPtr borderImage = dynamicDowncast<CSSValueList>(value);
    if (!borderImage)
        return result;

    for (Ref current : *borderImage) {
        if (current->isImage())
            result.setSource(toStyleFromCSSValue<MaskBorderSource>(state, current));
        else if (RefPtr imageSlice = dynamicDowncast<CSSBorderImageSliceValue>(current))
            result.setSlice(toStyleFromCSSValue<MaskBorderSlice>(state, *imageSlice));
        else if (RefPtr slashList = dynamicDowncast<CSSValueList>(current)) {
            if (RefPtr imageSlice = dynamicDowncast<CSSBorderImageSliceValue>(slashList->item(0)))
                result.setSlice(toStyleFromCSSValue<MaskBorderSlice>(state, *imageSlice));
            if (RefPtr borderImageWidth = dynamicDowncast<CSSBorderImageWidthValue>(slashList->item(1)))
                result.setWidth(toStyleFromCSSValue<MaskBorderWidth>(state, *borderImageWidth));
            if (RefPtr outsetValue = slashList->item(2))
                result.setOutset(toStyleFromCSSValue<MaskBorderOutset>(state, *outsetValue));
        } else if (current->isPair())
            result.setRepeat(toStyleFromCSSValue<MaskBorderRepeat>(state, current));
    }

    return result;
}

auto CSSValueCreation<MaskBorder>::operator()(CSSValuePool& pool, const RenderStyle& style, const MaskBorder& value) -> Ref<CSSValue>
{
    return createBorderImageValue({
        .source = createCSSValue(pool, style, value.source()),
        .slice  = createCSSValue(pool, style, value.slice()),
        .width  = createCSSValue(pool, style, value.width()),
        .outset = createCSSValue(pool, style, value.outset()),
        .repeat = createCSSValue(pool, style, value.repeat()),
    });
}

// MARK: - Serialization

void Serialize<MaskBorder>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const MaskBorder& value)
{
    if (value.source().isNone()) {
        serializationForCSS(builder, context, style, value.source());
        return;
    }

    // FIXME: Omit values that have their initial value.

    serializationForCSS(builder, context, style, value.source());
    builder.append(' ');
    serializationForCSS(builder, context, style, value.slice());
    builder.append(" / "_s);
    serializationForCSS(builder, context, style, value.width());
    builder.append(" / "_s);
    serializationForCSS(builder, context, style, value.outset());
    builder.append(' ');
    serializationForCSS(builder, context, style, value.repeat());
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const MaskBorder& image)
{
    return ts << "style-image "_s << image.source() << " slices "_s << image.slice();
}

} // namespace Style
} // namespace WebCore
