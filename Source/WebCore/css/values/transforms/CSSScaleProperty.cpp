
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
#include "CSSScaleProperty.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace CSS {

static bool isUnity(const NumberOrPercentageResolvedToNumber& value)
{
    return WTF::switchOn(value.value,
        [](const Number<>& number) {
            if (auto raw = number.raw())
                return raw->value == 1.0;
            return false;
        },
        [](const Percentage<>& percentage) {
            if (auto raw = percentage.raw())
                return raw->value == 100.0;
            return false;
        }
    );
}

void Serialize<ScaleProperty>::operator()(StringBuilder& builder, const ScaleProperty& scale)
{
    // https://drafts.csswg.org/css-transforms-2/#individual-transform-serialization
    //
    // "If a scale is specified, the property must serialize with only one through
    //  three values. As usual, if the third value is 1, the default, then it is
    //  omitted when serializing. If the third value is omitted and the second value
    //  is the same as the first (the default), then the second value is also omitted
    //  when serializing."

    return WTF::switchOn(scale.value,
        [&](None none) {
            serializationForCSS(builder, none);
        },
        [&](const Scale& value) {
            serializationForCSS(builder, value.x);

            if (value.y && *value.y != value.x) {
                builder.append(' ');
                serializationForCSS(builder, *value.y);
            }
        },
        [&](const Scale3D& value) {
            serializationForCSS(builder, value.x);

            if (!isUnity(value.z)) {
                builder.append(' ');
                serializationForCSS(builder, value.y);
                builder.append(' ');
                serializationForCSS(builder, value.z);
            } else if (value.y != value.x) {
                builder.append(' ');
                serializationForCSS(builder, value.y);
            }
        }
    );
}

} // namespace CSS
} // namespace WebCore
