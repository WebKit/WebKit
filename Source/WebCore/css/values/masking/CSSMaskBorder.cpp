/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSMaskBorder.h"

#include "CSSPrimitiveNumericTypes+CSSValueCreation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace CSS {

using MaskBorderExtractedType = SpaceSeparatedTuple<
   std::optional<MaskBorderSource>,
   std::optional<SlashSeparatedTuple<
       std::optional<MaskBorderSlice>,
       std::optional<MaskBorderWidth>,
       std::optional<MaskBorderOutset>
   >>,
   std::optional<MaskBorderRepeat>
>;

static MaskBorderExtractedType makeExtractedType(const MaskBorder& value)
{
    return MaskBorderExtractedType {
        value.maskBorderSource,
        (value.maskBorderSlice || value.maskBorderWidth || value.maskBorderOutset)
             ? std::optional { SlashSeparatedTuple {
                  value.maskBorderSlice,
                  value.maskBorderWidth,
                  value.maskBorderOutset,
               } }
             : std::nullopt,
        value.maskBorderRepeat,
    };
}

// MARK: - Conversion

auto CSSValueCreation<MaskBorder>::operator()(CSSValuePool& pool, const MaskBorder& value) -> Ref<CSSValue>
{
    return createCSSValue(pool, makeExtractedType(value));
}

// MARK: - Serialization

void Serialize<MaskBorder>::operator()(StringBuilder& builder, const SerializationContext& context, const MaskBorder& value)
{
    serializationForCSS(builder, context, makeExtractedType(value));
}

} // namespace CSS
} // namespace WebCore
