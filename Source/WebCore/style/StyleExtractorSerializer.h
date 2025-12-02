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

#pragma once

#include "CSSDynamicRangeLimitMix.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSValueTypes.h"
#include "ColorSerialization.h"
#include "StyleExtractorConverter.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace Style {

class ExtractorSerializer {
public:
    // MARK: Strong value conversions

    template<typename T, typename... Rest> static void serializeStyleType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const T&, Rest&&...);

    // MARK: Primitive serializations

    template<typename ConvertibleType>
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ConvertibleType&);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, double);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, float);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, unsigned);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, int);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, unsigned short);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, short);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScopedName&);

    template<typename T> static void serializeNumber(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T> static void serializeNumberAsPixels(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);

    // MARK: Transform serializations

    static void serializeTransformationMatrix(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);
    static void serializeTransformationMatrix(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);
};

// MARK: - Strong value serializations

template<typename T, typename... Rest> void ExtractorSerializer::serializeStyleType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const T& value, Rest&&... rest)
{
    serializationForCSS(builder, context, state.style, value, std::forward<Rest>(rest)...);
}

// MARK: - Primitive serializations

template<typename ConvertibleType>
void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ConvertibleType& value)
{
    serializationForCSS(builder, context, state.style, value);
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, double value)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, float value)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, unsigned value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, unsigned> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, int value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, int> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, unsigned short value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, unsigned short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, short value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScopedName& scopedName)
{
    if (scopedName.isIdentifier)
        serializationForCSS(builder, context, state.style, CustomIdentifier { scopedName.name });
    else
        serializationForCSS(builder, context, state.style, scopedName.name);
}

template<typename T> void ExtractorSerializer::serializeNumber(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    serialize(state, builder, context, number);
}

template<typename T> void ExtractorSerializer::serializeNumberAsPixels(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, adjustFloatForAbsoluteZoom(number, state.style) });
}

// MARK: - Transform serializations

inline void ExtractorSerializer::serializeTransformationMatrix(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TransformationMatrix& transform)
{
    serializeTransformationMatrix(state.style, builder, context, transform);
}

inline void ExtractorSerializer::serializeTransformationMatrix(const RenderStyle& style, StringBuilder& builder, const CSS::SerializationContext& context, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        std::array values { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        builder.append(nameLiteral(CSSValueMatrix), '(', interleave(values, [&](auto& builder, auto& value) {
            CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
        }, ", "_s), ')');
        return;
    }

    std::array values {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    builder.append(nameLiteral(CSSValueMatrix3d), '(', interleave(values, [&](auto& builder, auto& value) {
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
    }, ", "_s), ')');
}

} // namespace Style
} // namespace WebCore
