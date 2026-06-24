/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSBorderImageWidth.h"

#include "CSSBorderImageWidthValue.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSUnevaluatedCalc.h"

namespace WebCore {
namespace CSS {

bool BorderImageWidthValue::isLength() const
{
    auto* lengthPercentage = std::get_if<LengthPercentage>(&m_value);
    if (!lengthPercentage)
        return false;

    return WTF::switchOn(*lengthPercentage,
        [](const LengthPercentage::Calc& calc) {
            return calc.primitiveType() == CSSUnitType::CSS_PX;
        },
        [](const LengthPercentage::Raw& raw) {
            return raw.unit != CSS::PercentageUnit::Percentage;
        }
    );
}

// MARK: - Conversion

auto CSSValueCreation<BorderImageWidth>::operator()(CSSValuePool&, const BorderImageWidth& value) -> Ref<CSSValue>
{
    return CSSBorderImageWidthValue::create(BorderImageWidth { value });
}

// MARK: - Serialization

void Serialize<BorderImageWidth>::operator()(StringBuilder& builder, const SerializationContext& context, const BorderImageWidth& value)
{
    // The border-image-width longhand can't set legacyWebkitBorderImage to true, so serialize as empty string.
    // This can only be created by the -webkit-border-image shorthand, which will not serialize as empty string in this case.
    // This is an unconventional relationship between a longhand and a shorthand, which we may want to revise.

    if (value.legacyWebkitBorderImage)
        return;
    CSS::serializationForCSS(builder, context, value.values);
}

} // namespace CSS
} // namespace WebCore
