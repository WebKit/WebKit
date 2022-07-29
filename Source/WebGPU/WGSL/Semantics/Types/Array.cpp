/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Array.h"

namespace WGSL::Semantics::Types {

static std::optional<Error> checkValidElementType(const Type& type, SourceSpan span)
{
    // An array element type MUST be one of:

    // a scalar type
    if (type.isScalar())
        return { };

    // a vector type with concrete components
    if (type.isVector()) {
        const Vector& vectorType = downcast<Vector>(type);
        const Type& componentType = vectorType.componentType();

        if (!componentType.isConcrete()) {
            auto errorMsg = makeString(
                "element type '",
                type.string(),
                "' is a vector with component of type '",
                componentType.string(),
                "', which is not concrete"
            );

            return Error { WTFMove(errorMsg), componentType.span() };
        }

        return { };
    }

    // a matrix type with concrete components
    if (type.isMatrix()) {
        const Matrix& matrixType = downcast<Matrix>(type);
        const Type& componentType = matrixType.componentType();

        if (!componentType.isConcrete()) {
            auto errorMsg = makeString(
                "element type '",
                type.string(),
                "' is a matrix with component of type '",
                componentType.string(),
                "', which is not concrete"
            );

            return Error { WTFMove(errorMsg), componentType.span() };
        }

        return { };
    }

    // an atomic type
    if (type.isAtomic())
        return { };

    // an array having a creation-fixed footprint.
    if (type.isArray()) {
        if (!type.hasCreationFixedFootprint()) {
            auto errorMsg = makeString(
                "element type '",
                type.string(),
                "' is an array with non-creation-fixed footprint"
            );

            return Error { WTFMove(errorMsg), type.span() };
        }

        return { };
    }

    if (type.isStruct()) {
        if (!type.hasCreationFixedFootprint()) {
            auto errorMsg = makeString(
                "element type '",
                type.string(),
                "' is a struct with non-creation-fixed footprint"
            );

            return Error { WTFMove(errorMsg), type.span() };
        }

        return { };
    }

    auto errorMsg = makeString(
        "element type '",
        type.string(),
        "' is not valid as a array element type"
    );

    return Error { WTFMove(errorMsg), span };
}

Array::Array(SourceSpan span, UniqueRef<Type> elementType, size_t count)
    : Type(Type::Kind::Array, span)
    , m_elementType(WTFMove(elementType))
    , m_count(count)
{
}

Expected<UniqueRef<Type>, Error> Array::tryCreate(SourceSpan span, UniqueRef<Type> elementType, size_t count)
{
    if (auto err = checkValidElementType(elementType, span))
        return makeUnexpected(*err);

    if (!count)
        return makeUnexpected(Error { "array size must be non-zero"_s, span });

    return { UniqueRef(*new Array(span, WTFMove(elementType), count)) };
}

bool Array::operator==(const Type &other) const
{
    if (!other.isArray())
        return false;

    const auto& arrayOther = downcast<Array>(other);
    return (m_elementType.get() == arrayOther.m_elementType.get())
        && (m_count == arrayOther.m_count);
}

String Array::string() const
{
    return makeString("array<", m_elementType->string(), ",", toString(m_count), ">");
}


} // namespace WGSL::Semantics::Types
