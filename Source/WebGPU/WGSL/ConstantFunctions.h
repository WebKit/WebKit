/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "ConstantValue.h"
#include "Types.h"
#include <wtf/Assertions.h>

namespace WGSL {

template<typename Functor, typename... Arguments>
static ConstantValue scalarOrVector(const Functor& functor, Arguments&&... unpackedArguments)
{
    unsigned vectorSize = 0;
    std::initializer_list<ConstantValue> arguments { unpackedArguments... };
    for (auto argument : arguments) {
        if (auto* vector = std::get_if<ConstantVector>(&argument)) {
            vectorSize = vector->elements.size();
            break;
        }
    }
    if (!vectorSize)
        return functor(std::forward<Arguments>(unpackedArguments)...);

    constexpr auto argumentCount = sizeof...(Arguments);
    ConstantVector result(vectorSize);
    std::array<ConstantValue, argumentCount> scalars;
    for (unsigned i = 0; i < vectorSize; ++i) {
        unsigned j = 0;
        for (const auto& argument : arguments) {
            if (auto* vector = std::get_if<ConstantVector>(&argument))
                scalars[j++] = vector->elements[i];
            else
                scalars[j++] = argument;
        }
        result.elements[i] = std::apply(functor, scalars);
    }
    return result;

}

static ConstantValue constantPow(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](auto value, auto exponent) -> ConstantValue {
        return { std::pow(value.toDouble(), exponent.toDouble()) };
    }, arguments[0], arguments[1]);
}

static ConstantValue constantMinus(const Type*, const FixedVector<ConstantValue>& arguments)
{
    const auto& unaryMinus = [&]() -> ConstantValue {
        return scalarOrVector([&](const auto& value) -> ConstantValue {
            if (value.isInt())
                return -value.toInt();
            return -value.toDouble();
        }, arguments[0]);
    };

    const auto& binaryMinus = [&]() -> ConstantValue {
        return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
            if (left.isInt() && right.isInt())
                return left.toInt() - right.toInt();
            return left.toDouble() - right.toDouble();
        }, arguments[0], arguments[1]);
    };

    if (arguments.size() == 1)
        return unaryMinus();
    ASSERT(arguments.size() == 2);
    return binaryMinus();
}

static ConstantValue constantMultiply(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 2);

    auto& lhs = arguments[0];
    auto& rhs = arguments[1];

    // FIXME: handle constant matrices

    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        if (left.isInt() && right.isInt())
            return left.toInt() * right.toInt();
        return left.toDouble() * right.toDouble();
    }, lhs, rhs);
}

static ConstantValue zeroValue(const Type* type)
{
    return WTF::switchOn(*type,
        [&](const Types::Primitive& primitive) -> ConstantValue {
            switch (primitive.kind) {
            case Types::Primitive::AbstractInt:
            case Types::Primitive::I32:
            case Types::Primitive::U32:
                return { static_cast<int64_t>(0) };
            case Types::Primitive::AbstractFloat:
            case Types::Primitive::F32:
                return { static_cast<double>(0) };
            case Types::Primitive::Bool:
                return { static_cast<double>(false) };
            case Types::Primitive::Void:
            case Types::Primitive::Sampler:
            case Types::Primitive::SamplerComparison:
            case Types::Primitive::TextureExternal:
            case Types::Primitive::AccessMode:
            case Types::Primitive::TexelFormat:
            case Types::Primitive::AddressSpace:
                RELEASE_ASSERT_NOT_REACHED();
            }
        },
        [&](const Types::Vector& vector) -> ConstantValue {
            ConstantVector result(vector.size);
            auto value = zeroValue(vector.element);
            for (unsigned i = 0; i < vector.size; ++i)
                result.elements[i] = value;
            return result;
        },
        [&](const Types::Array& array) -> ConstantValue {
            ASSERT(array.size.has_value());
            ConstantArray result(*array.size);
            auto value = zeroValue(array.element);
            for (unsigned i = 0; i < array.size; ++i)
                result.elements[i] = value;
            return result;
        },
        [&](const Types::Struct&) -> ConstantValue {
            // FIXME: this is valid and needs to be implemented, but we don't
            // yet have ConstantStruct
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Matrix&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Reference&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Pointer&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Function&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Texture&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TextureStorage&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TextureDepth&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Atomic&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TypeConstructor&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Bottom&) -> ConstantValue {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

static ConstantValue constantVector(const Type* resultType, const FixedVector<ConstantValue>& arguments, unsigned size)
{
    ConstantVector result(size);
    auto argumentCount = arguments.size();

    if (!argumentCount) {
        ASSERT(std::holds_alternative<Types::Vector>(*resultType));
        return zeroValue(resultType);
    }

    if (argumentCount == 1 && !std::holds_alternative<ConstantVector>(arguments[0])) {
        for (unsigned i = 0; i < size; ++i)
            result.elements[i] = arguments[0];
        return result;
    }

    unsigned i = 0;
    for (const auto& argument : arguments) {
        const auto* vector = std::get_if<ConstantVector>(&argument);
        if (!vector) {
            result.elements[i++] = argument;
            continue;
        }
        for (auto element : vector->elements)
            result.elements[i++] = element;
    }
    ASSERT(i == size);
    return { result };
}

static ConstantValue constantVector2(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantVector(resultType, arguments, 2);
}

static ConstantValue constantVector3(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantVector(resultType, arguments, 3);
}

static ConstantValue constantVector4(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantVector(resultType, arguments, 4);
}

} // namespace WGSL
