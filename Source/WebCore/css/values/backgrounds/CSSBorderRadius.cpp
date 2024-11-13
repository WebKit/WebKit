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
#include "CSSBorderRadius.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

static bool hasDefaultValueForAxis(const SpaceSeparatedArray<LengthPercentage<Nonnegative>, 4>& values)
{
    return values.value[3] == values.value[1]
        && values.value[2] == values.value[0]
        && values.value[1] == values.value[0]
        && values.value[0] == LengthPercentage<Nonnegative> { LengthPercentageRaw<Nonnegative> { CSSUnitType::CSS_PX, 0 } };
}

bool hasDefaultValue(const BorderRadius& borderRadius)
{
    return hasDefaultValueForAxis(borderRadius.horizontal) && hasDefaultValueForAxis(borderRadius.vertical);
}

static std::pair<SpaceSeparatedVector<LengthPercentage<Nonnegative>, 4>, bool> gatherSerializableRadiiForAxis(const SpaceSeparatedArray<LengthPercentage<Nonnegative>, 4>& values)
{
    bool isDefaultValue = false;

    SpaceSeparatedVector<LengthPercentage<Nonnegative>, 4>::Vector result;
    result.append(values.value[0]);

    if (values.value[3] != values.value[1]) {
        result.append(values.value[1]);
        result.append(values.value[2]);
        result.append(values.value[3]);
    } else if (values.value[2] != values.value[0]) {
        result.append(values.value[1]);
        result.append(values.value[2]);
    } else if (values.value[1] != values.value[0]) {
        result.append(values.value[1]);
    } else {
        isDefaultValue = result[0] == LengthPercentage<Nonnegative> { LengthPercentageRaw<Nonnegative> { CSSUnitType::CSS_PX, 0 } };
    }

    return { { WTFMove(result) }, isDefaultValue };
}

void Serialize<BorderRadius>::operator()(StringBuilder& builder, const BorderRadius& borderRadius)
{
    // <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?

    auto [horizontal, horizontalIsDefault] = gatherSerializableRadiiForAxis(borderRadius.horizontal);
    auto [vertical, verticalIsDefault] = gatherSerializableRadiiForAxis(borderRadius.vertical);

    if (!horizontalIsDefault || !verticalIsDefault) {
        serializationForCSS(builder, horizontal);

        if (horizontal != vertical) {
            builder.append(" / "_s);
            serializationForCSS(builder, vertical);
        }
    }
}

} // namespace CSS
} // namespace WebCore
