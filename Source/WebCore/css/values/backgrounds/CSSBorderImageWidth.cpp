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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSBorderImageWidth.h"

#include "CSSCalcTree.h"
#include "CSSCalcValue.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace CSS {

void Serialize<BorderImageWidth>::operator()(StringBuilder& builder, const SerializationContext& context, const BorderImageWidth& value)
{
    // The border-image-width longhand can't set `overridesBorderWidths` to true, so serialize as empty string.
    // This can only be created by the -webkit-border-image shorthand, which will not serialize as empty string in this case.
    // This is an unconventional relationship between a longhand and a shorthand, which we may want to revise.
    if (value.overridesBorderWidths)
        return;
    serializationForCSS(builder, context, value.widths);
}

bool hasLength(const CSS::BorderImageWidth::Widths& widths)
{
    auto isLength = [](const auto& width) {
        return WTF::switchOn(width,
            [](const CSS::LengthPercentage<CSS::Nonnegative>& lengthPercentage) {
                return WTF::switchOn(lengthPercentage,
                    [](const CSS::LengthPercentage<CSS::Nonnegative>::Raw& raw) {
                        return raw.unit != CSS::PercentageUnit::Percentage;
                    },
                    [](const CSS::LengthPercentage<CSS::Nonnegative>::Calc& calc) {
                        return !calc.protectedCalc()->tree().type.percentHint;
                    }
                );
            },
            [](const auto&) {
                return false;
            }
        );
    };

    return isLength(widths.top())
        || isLength(widths.right())
        || isLength(widths.bottom())
        || isLength(widths.left());
}

} // namespace CSS
} // namespace WebCore
