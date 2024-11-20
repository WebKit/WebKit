
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
#include "CSSTranslateProperty.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace CSS {

static bool is0px(const CSS::LengthPercentage<>& lengthPercentage)
{
    if (auto raw = lengthPercentage.raw())
        return raw->type == CSSUnitType::CSS_PX && raw->value == 0.0;
    return false;
}

static bool is0px(const CSS::Length<>& length)
{
    if (auto raw = length.raw())
        return raw->type == CSSUnitType::CSS_PX && raw->value == 0.0;
    return false;
}

void Serialize<TranslateProperty>::operator()(StringBuilder& builder, const TranslateProperty& translate)
{
    // https://drafts.csswg.org/css-transforms-2/#individual-transform-serialization
    //
    // "If a translation is specified, the property must serialize with one through
    //  three values. (As usual, if the second and third values are 0px, the default,
    //  or if only the third value is 0px, then those 0px values must be omitted
    //  when serializing)."

    return WTF::switchOn(translate.value,
        [&](None none) {
            serializationForCSS(builder, none);
        },
        [&](const Translate& value) {
            serializationForCSS(builder, value.x);
            if (value.y && !is0px(*value.y)) {
                builder.append(' ');
                serializationForCSS(builder, *value.y);
            }
        },
        [&](const Translate3D& value) {
            serializationForCSS(builder, value.x);

            if (!is0px(value.z)) {
                builder.append(' ');
                serializationForCSS(builder, value.y);
                builder.append(' ');
                serializationForCSS(builder, value.z);
            } else if (!is0px(value.y)) {
                builder.append(' ');
                serializationForCSS(builder, value.y);
            }
        }
    );
}

} // namespace CSS
} // namespace WebCore
