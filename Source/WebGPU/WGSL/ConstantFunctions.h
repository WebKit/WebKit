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
#include "Constraints.h"
#include "Types.h"
#include <wtf/Assertions.h>

namespace WGSL {

// Zero values

static ConstantValue zeroValue(const Type* type)
{
    return WTF::switchOn(*type,
        [&](const Types::Primitive& primitive) -> ConstantValue {
            switch (primitive.kind) {
            case Types::Primitive::AbstractInt:
            case Types::Primitive::I32:
            case Types::Primitive::U32:
                return 0;
            case Types::Primitive::AbstractFloat:
            case Types::Primitive::F32:
                return 0.0;
            case Types::Primitive::Bool:
                return false;
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

static ConstantValue constantAdd(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 2);

    auto& lhs = arguments[0];
    auto& rhs = arguments[1];

    // FIXME: handle constant matrices
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        if (left.isInt() && right.isInt())
            return left.toInt() + right.toInt();
        return left.toDouble() + right.toDouble();
    }, lhs, rhs);
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

static ConstantValue constantDivide(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 2);

    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        if (left.isInt() && right.isInt())
            return left.toInt() / right.toInt();
        return left.toDouble() / right.toDouble();
    }, arguments[0], arguments[1]);
}

template<Constraint constraint, typename Functor>
static ConstantValue constantBinaryOperation(const Type*, const FixedVector<ConstantValue>& arguments, const Functor& fn)
{
    return scalarOrVector([&](auto& left, auto& right) -> ConstantValue {
        if ((constraint & Constraints::Bool) && left.isBool() && right.isBool())
            return fn(left.toBool(), right.toBool());
        if ((constraint & Constraints::Integer) && left.isInt() && right.isInt())
            return fn(left.toInt(), right.toInt());
        if ((constraint & Constraints::Float) && left.isNumber() && right.isNumber())
            return fn(left.toDouble(), right.toDouble());
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1]);
}

static ConstantValue constantEqual(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantBinaryOperation<Constraints::Scalar>(resultType, arguments, [&](auto left, auto right) {
        return left == right;
    });
}

static ConstantValue constantNotEqual(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantBinaryOperation<Constraints::Scalar>(resultType, arguments, [&](auto left, auto right) {
        return left != right;
    });
}

static ConstantValue constantLt(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantBinaryOperation<Constraints::Number>(resultType, arguments, [&](auto left, auto right) {
        return left < right;
    });
}

static ConstantValue constantLtEq(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantBinaryOperation<Constraints::Number>(resultType, arguments, [&](auto left, auto right) {
        return left <= right;
    });
}

static ConstantValue constantGt(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantBinaryOperation<Constraints::Number>(resultType, arguments, [&](auto left, auto right) {
        return left > right;
    });
}

static ConstantValue constantGtEq(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantBinaryOperation<Constraints::Number>(resultType, arguments, [&](auto left, auto right) {
        return left >= right;
    });
}

static ConstantValue constantNot(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& arg) -> ConstantValue {
        return !arg.toBool();
    }, arguments[0]);
}

static ConstantValue constantOr(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        return left.toBool() || right.toBool();
    }, arguments[0], arguments[1]);
}

static ConstantValue constantAnd(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        return left.toBool() && right.toBool();
    }, arguments[0], arguments[1]);
}

static ConstantValue constantBitwiseOr(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        if (left.isBool() && right.isBool())
            return static_cast<bool>(static_cast<int>(left.toBool()) | static_cast<int>(right.toBool()));
        return left.toInt() | right.toInt();
    }, arguments[0], arguments[1]);
}

static ConstantValue constantBitwiseAnd(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        if (left.isBool() && right.isBool())
            return static_cast<bool>(static_cast<int>(left.toBool()) & static_cast<int>(right.toBool()));
        return left.toInt() & right.toInt();
    }, arguments[0], arguments[1]);
}

static ConstantValue constantBitwiseNot(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& arg) -> ConstantValue {
        return ~arg.toInt();
    }, arguments[0]);
}

static ConstantValue constantBitwiseXor(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        return left.toInt() ^ right.toInt();
    }, arguments[0], arguments[1]);
}

static ConstantValue constantBitwiseShiftLeft(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        return left.toInt() << right.toInt();
    }, arguments[0], arguments[1]);
}

static ConstantValue constantBitwiseShiftRight(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        return left.toInt() >> right.toInt();
    }, arguments[0], arguments[1]);
}

template<typename DestinationType>
static ConstantValue constantConstructor(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    if (arguments.isEmpty())
        return zeroValue(resultType);

    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    if (arg.isBool())
        return static_cast<DestinationType>(arg.toBool());
    if (arg.isInt())
        return static_cast<DestinationType>(arg.toInt());
    ASSERT(arg.isNumber());
    return static_cast<DestinationType>(arg.toDouble());
}

static ConstantValue constantBool(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantConstructor<bool>(resultType, arguments);
}

static ConstantValue constantI32(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantConstructor<int32_t>(resultType, arguments);
}

static ConstantValue constantU32(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantConstructor<uint32_t>(resultType, arguments);
}

static ConstantValue constantF32(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantConstructor<float>(resultType, arguments);
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

static ConstantValue constantVec2(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantVector(resultType, arguments, 2);
}

static ConstantValue constantVec3(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantVector(resultType, arguments, 3);
}

static ConstantValue constantVec4(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantVector(resultType, arguments, 4);
}

static ConstantValue constantMatrix(const Type* resultType, const FixedVector<ConstantValue>& arguments, unsigned columns, unsigned rows)
{
    if (arguments.isEmpty())
        return zeroValue(resultType);

    // FIXME: we don't support matrices yet
    UNUSED_PARAM(columns);
    UNUSED_PARAM(rows);
    RELEASE_ASSERT_NOT_REACHED();
}

#define MATRIX_CONSTRUCTOR(columns, rows) \
    static ConstantValue constantMat ## columns ## x ## rows(const Type* resultType, const FixedVector<ConstantValue>& arguments) \
    { \
        return constantMatrix(resultType, arguments, columns, rows); \
    }

MATRIX_CONSTRUCTOR(2, 2);
MATRIX_CONSTRUCTOR(2, 3);
MATRIX_CONSTRUCTOR(2, 4);
MATRIX_CONSTRUCTOR(3, 2);
MATRIX_CONSTRUCTOR(3, 3);
MATRIX_CONSTRUCTOR(3, 4);
MATRIX_CONSTRUCTOR(4, 2);
MATRIX_CONSTRUCTOR(4, 3);
MATRIX_CONSTRUCTOR(4, 4);

#undef MATRIX_CONSTRUCTOR

} // namespace WGSL
