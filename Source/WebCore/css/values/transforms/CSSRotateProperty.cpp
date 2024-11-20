
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
#include "CSSRotateProperty.h"

#include "CSSKeywordValue.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace CSS {

void Serialize<RotateProperty>::operator()(StringBuilder& builder, const RotateProperty& rotate)
{
    // https://drafts.csswg.org/css-transforms-2/#individual-transform-serialization
    //
    // "If a rotation about the z axis (that is, in 2D) is specified, the property
    //  must serialize as just an <angle>.
    //
    //  If any other rotation is specified, the property must serialize with an axis
    //  specified. If the axis is parallel with the x or y axes, it must serialize as
    //  the appropriate keyword."

    return WTF::switchOn(rotate.value,
        [&](None none) {
            serializationForCSS(builder, none);
        },
        [&](const Rotate& value) {
            serializationForCSS(builder, value.value);
        },
        [&](const Rotate3D& value) {
            if (value.x.isKnownNotZero() && value.y.isKnownZero() && value.z.isKnownZero()) {
                serializationForCSS(builder, Constant<CSSValueX>());
                builder.append(' ');
                serializationForCSS(builder, value.angle);
                return;
            }

            if (value.x.isKnownZero() && value.y.isKnownNotZero() && value.z.isKnownZero()) {
                serializationForCSS(builder, Constant<CSSValueY>());
                builder.append(' ');
                serializationForCSS(builder, value.angle);
                return;
            }

            if (value.x.isKnownZero() && value.y.isKnownZero() && value.z.isKnownNotZero()) {
                serializationForCSS(builder, value.angle);
                return;
            }

            serializationForCSS(builder, value.x);
            builder.append(' ');
            serializationForCSS(builder, value.y);
            builder.append(' ');
            serializationForCSS(builder, value.z);
            builder.append(' ');
            serializationForCSS(builder, value.angle);
        }
    );
}

} // namespace CSS
} // namespace WebCore
