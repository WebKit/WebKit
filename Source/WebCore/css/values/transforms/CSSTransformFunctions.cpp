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
#include "CSSTransformFunctions.h"

#include "CSSCalcValue.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

void Serialize<Rotate3D>::operator()(StringBuilder& builder, const Rotate3D& value)
{
    // rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )

    serializationForCSS(builder, value.x);
    builder.append(", "_s);
    serializationForCSS(builder, value.y);
    builder.append(", "_s);
    serializationForCSS(builder, value.z);
    builder.append(", "_s);
    serializationForCSS(builder, value.angle);
}

void Serialize<Skew>::operator()(StringBuilder& builder, const Skew& value)
{
    // skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )
    serializationForCSS(builder, value.x);

    if (value.y) {
        builder.append(", "_s);
        serializationForCSS(builder, *value.y);
    }
}

void Serialize<Scale3D>::operator()(StringBuilder& builder, const Scale3D& value)
{
    // scale3d() = scale3d( [ <number> | <percentage> ]#{3} )

    serializationForCSS(builder, value.x);
    builder.append(", "_s);
    serializationForCSS(builder, value.y);
    builder.append(", "_s);
    serializationForCSS(builder, value.z);
}

void Serialize<Scale>::operator()(StringBuilder& builder, const Scale& value)
{
    // scale() = scale( [ <number> | <percentage> ]#{1,2} )
    serializationForCSS(builder, value.x);

    if (value.y) {
        builder.append(", "_s);
        serializationForCSS(builder, *value.y);
    }
}

void Serialize<Translate3D>::operator()(StringBuilder& builder, const Translate3D& value)
{
    // translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )

    serializationForCSS(builder, value.x);
    builder.append(", "_s);
    serializationForCSS(builder, value.y);
    builder.append(", "_s);
    serializationForCSS(builder, value.z);
}

void Serialize<Translate>::operator()(StringBuilder& builder, const Translate& value)
{
    // translate() = translate( <length-percentage> , <length-percentage>? )

    serializationForCSS(builder, value.x);

    if (value.y) {
        builder.append(", "_s);
        serializationForCSS(builder, *value.y);
    }
}

} // namespace CSS
} // namespace WebCore
