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
#include "CSSMeshGradient.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

static void appendControlPoints(StringBuilder& builder, const SerializationContext& context, const auto& controlPoints)
{
    if (controlPoints.isEmpty())
        return;

    builder.append(' ', nameLiteralForSerialization(CSSValueID::With), ' ');
    serializationForCSS(builder, context, controlPoints);
}

void Serialize<MeshGradientRelativeControlPoint>::operator()(StringBuilder& builder, const SerializationContext& context, const MeshGradientRelativeControlPoint& value)
{
    serializationForCSS(builder, context, value.offset);

    if (value.anchor) {
        builder.append(' ', nameLiteralForSerialization(CSSValueID::From), ' ');
        serializationForCSS(builder, context, *value.anchor);
    }
}

void Serialize<MeshGradientAbsolutePosition>::operator()(StringBuilder& builder, const SerializationContext& context, const MeshGradientAbsolutePosition& value)
{
    serializationForCSS(builder, context, value.position);
    appendControlPoints(builder, context, value.controlPoints);
}

void Serialize<MeshGradientRelativePosition>::operator()(StringBuilder& builder, const SerializationContext& context, const MeshGradientRelativePosition& value)
{
    builder.append(nameLiteralForSerialization(CSSValueID::By), ' ');
    serializationForCSS(builder, context, value.offset);
    appendControlPoints(builder, context, value.controlPoints);
}

void Serialize<MeshGradientPoint>::operator()(StringBuilder& builder, const SerializationContext& context, const MeshGradientPoint& value)
{
    serializationForCSS(builder, context, value.color);

    if (value.location) {
        builder.append(' ');
        serializationForCSS(builder, context, *value.location);
    }
}

void Serialize<MeshGradient>::operator()(StringBuilder& builder, const SerializationContext& context, const MeshGradient& gradient)
{
    // The color interpolation method is serialized whenever there is a background color, as a lone leading color is parsed as a <mesh-point>.
    if (gradient.backgroundColor || !gradient.colorInterpolationMethod.isDefault()) {
        serializationForCSS(builder, context, gradient.colorInterpolationMethod);

        if (gradient.backgroundColor) {
            builder.append(' ');
            serializationForCSS(builder, context, *gradient.backgroundColor);
        }

        builder.append(", "_s);
    }

    serializationForCSS(builder, context, gradient.rows);
}

} // namespace CSS
} // namespace WebCore
