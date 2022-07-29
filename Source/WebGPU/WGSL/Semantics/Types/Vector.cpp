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
#include "Vector.h"

namespace WGSL::Semantics::Types {

static std::optional<Error> checkValidComponentType(const Type& type, SourceSpan span)
{
    if (!type.isScalar() && !type.isAbstractNumeric()) {
        auto err = makeString(
            "vector component type '",
            type.string(),
            "' is neither a scalar nor abstract numeric type");

        return Error { WTFMove(err), span };
    }

    return { };
}

static std::optional<Error> checkValidSize(uint8_t size, SourceSpan span)
{
    if (size != 2 && size != 3 && size != 4)
        return Error { "vector size must either be 2, 3, or 4"_s, span };

    return { };
}

Vector::Vector(SourceSpan span, UniqueRef<Type> componentType, uint8_t size)
    : Type(Type::Kind::Vector, span)
    , m_componentType(WTFMove(componentType))
    , m_size(size)
{
}

Expected<UniqueRef<Type>, Error> Vector::tryCreate(SourceSpan span, UniqueRef<Type> componentType, uint8_t size)
{
    if (auto err = checkValidComponentType(componentType, span))
        return makeUnexpected(*err);

    if (auto err = checkValidSize(size, span))
        return makeUnexpected(*err);

    // return { makeUniqueRef<Vector>(span, WTFMove(componentType), size) };
    return { UniqueRef(*new Vector(span, WTFMove(componentType), size)) };
}

bool Vector::operator==(const Type& other) const
{
    if (!other.isVector())
        return false;

    const auto& vectorOther = downcast<Vector>(other);
    return (m_componentType.get() == vectorOther.m_componentType.get())
        && (m_size == vectorOther.m_size);
}

String Vector::string() const
{
    return makeString("vec", toString(m_size), "<", m_componentType->string(), ">");
}

} // namespace WGSL::Semantics::Types
