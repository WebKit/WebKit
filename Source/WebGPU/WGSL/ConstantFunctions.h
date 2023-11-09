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
#include <bit>
#include <numbers>
#include <wtf/Assertions.h>
#include <wtf/DataLog.h>

namespace WGSL {

// Zero values

using ConstantResult = Expected<ConstantValue, String>;
using ConstantFunction = ConstantResult(*)(const Type*, const FixedVector<ConstantValue>&);

#define CONSTANT_FUNCTION(name) \
    static Expected<ConstantValue, String>(constant ## name)(const Type* resultType, const FixedVector<ConstantValue>& arguments)

#define CALL_(__tmp, __variable, __fnName, ...) \
    auto __tmp = constant##__fnName(__VA_ARGS__); \
    if (!__tmp) \
        return makeUnexpected(__tmp.error()); \
    auto __variable = WTFMove(*__tmp)

#define CALL(__variable, __fnName, ...) \
    CALL_(WTF_LAZY_JOIN(tmp, __COUNTER__), __variable, __fnName, __VA_ARGS__)

#define CALL_MOVE_(__tmp, __target, __fnName, ...) \
    do { \
        auto __tmp = constant##__fnName(__VA_ARGS__); \
        if (!__tmp) \
            return makeUnexpected(__tmp.error()); \
        __target = WTFMove(*__tmp); \
    } while (0)

#define CALL_MOVE(__target, __fnName, ...) \
    CALL_MOVE_(tmp ## __COUNTER__, __target, __fnName, __VA_ARGS__)


static ConstantValue zeroValue(const Type* type)
{
    return WTF::switchOn(*type,
        [&](const Types::Primitive& primitive) -> ConstantValue {
            switch (primitive.kind) {
            case Types::Primitive::AbstractInt:
                return static_cast<int64_t>(0);
            case Types::Primitive::I32:
                return 0;
            case Types::Primitive::U32:
                return 0u;
            case Types::Primitive::AbstractFloat:
                return 0.0;
            case Types::Primitive::F32:
                return 0.0f;
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
        [&](const Types::PrimitiveStruct&) -> ConstantValue {
            // FIXME: this is valid and needs to be implemented, but we don't
            // yet have ConstantStruct
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Matrix& matrix) -> ConstantValue {
            ConstantMatrix result(matrix.columns, matrix.rows);
            auto value = zeroValue(matrix.element);
            for (unsigned i = 0; i < result.elements.size(); ++i)
                result.elements[i] = value;
            return result;
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

// Helpers

template<Constraint constraint, typename Functor>
static ConstantResult constantUnaryOperation(const FixedVector<ConstantValue>& arguments, const Functor& fn)
{
    ASSERT(arguments.size() == 1);
    return scalarOrVector([&](auto& arg) -> ConstantResult {
        if constexpr (constraint & Constraints::Bool) {
            if (auto* boolean = std::get_if<bool>(&arg))
                return { { fn(*boolean) } };
        }
        if constexpr (constraint & Constraints::I32) {
            if (auto* i32 = std::get_if<int32_t>(&arg))
                return { { fn(*i32) } };
        }
        if constexpr (constraint & Constraints::U32) {
            if (auto* u32 = std::get_if<uint32_t>(&arg))
                return { { fn(*u32) } };
        }
        if constexpr (constraint & Constraints::AbstractInt) {
            if (auto* abstractInt = std::get_if<int64_t>(&arg))
                return { { fn(*abstractInt) } };
        }
        // FIXME: implement f16
        if constexpr (constraint & Constraints::F32) {
            if (auto* f32 = std::get_if<float>(&arg))
                return { { fn(*f32) } };
        }
        if constexpr (constraint & Constraints::AbstractFloat) {
            if (auto* abstractFloat = std::get_if<double>(&arg))
                return { { fn(*abstractFloat) } };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0]);
}

template<Constraint constraint, typename Functor>
static ConstantResult constantBinaryOperation(const FixedVector<ConstantValue>& arguments, const Functor& fn)
{
    ASSERT(arguments.size() == 2);
    return scalarOrVector([&](auto& left, auto& right) -> ConstantResult {
        if constexpr (constraint & Constraints::Bool) {
            if (auto* leftBool = std::get_if<bool>(&left))
                return { { fn(*leftBool, std::get<bool>(right)) } };
        }
        if constexpr (constraint & Constraints::I32) {
            if (auto* leftI32 = std::get_if<int32_t>(&left))
                return { { fn(*leftI32, std::get<int32_t>(right)) } };
        }
        if constexpr (constraint & Constraints::U32) {
            if (auto* leftU32 = std::get_if<uint32_t>(&left))
                return { { fn(*leftU32, std::get<uint32_t>(right)) } };
        }
        if constexpr (constraint & Constraints::AbstractInt) {
            if (auto* leftAbstractInt = std::get_if<int64_t>(&left))
                return { { fn(*leftAbstractInt, std::get<int64_t>(right)) } };
        }
        // FIXME: implement f16
        if constexpr (constraint & Constraints::F32) {
            if (auto* leftF32 = std::get_if<float>(&left))
                return { { fn(*leftF32, std::get<float>(right)) } };
        }
        if constexpr (constraint & Constraints::AbstractFloat) {
            if (auto* leftAbstractFloat = std::get_if<double>(&left))
                return { { fn(*leftAbstractFloat, std::get<double>(right)) } };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1]);
}

template<Constraint constraint, typename Functor>
static ConstantResult constantTernaryOperation(const FixedVector<ConstantValue>& arguments, const Functor& fn)
{
    ASSERT(arguments.size() == 3);
    return scalarOrVector([&](auto& first, auto& second, auto& third) -> ConstantResult {
        if constexpr (constraint & Constraints::Bool) {
            if (auto* firstBool = std::get_if<bool>(&first))
                return { { fn(*firstBool, std::get<bool>(second), std::get<bool>(third)) } };
        }
        if constexpr (constraint & Constraints::I32) {
            if (auto* firstI32 = std::get_if<int32_t>(&first))
                return { { fn(*firstI32, std::get<int32_t>(second), std::get<int32_t>(third)) } };
        }
        if constexpr (constraint & Constraints::U32) {
            if (auto* firstU32 = std::get_if<uint32_t>(&first))
                return { { fn(*firstU32, std::get<uint32_t>(second), std::get<uint32_t>(third)) } };
        }
        if constexpr (constraint & Constraints::AbstractInt) {
            if (auto* firstAbstractInt = std::get_if<int64_t>(&first))
                return { { fn(*firstAbstractInt, std::get<int64_t>(second), std::get<int64_t>(third)) } };
        }
        // FIXME: implement f16
        if constexpr (constraint & Constraints::F32) {
            if (auto* firstF32 = std::get_if<float>(&first))
                return { { fn(*firstF32, std::get<float>(second), std::get<float>(third)) } };
        }
        if constexpr (constraint & Constraints::AbstractFloat) {
            if (auto* firstAbstractFloat = std::get_if<double>(&first))
                return { { fn(*firstAbstractFloat, std::get<double>(second), std::get<double>(third)) } };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1], arguments[2]);
}

template<typename DestinationType>
static ConstantValue constantConstructor(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    if (arguments.isEmpty())
        return zeroValue(resultType);

    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];

    if (auto* boolean = std::get_if<bool>(&arg))
        return static_cast<DestinationType>(*boolean);
    if (auto* i32 = std::get_if<int32_t>(&arg))
        return static_cast<DestinationType>(*i32);
    if (auto* u32 = std::get_if<uint32_t>(&arg))
        return static_cast<DestinationType>(*u32);
    if (auto* abstractInt = std::get_if<int64_t>(&arg))
        return static_cast<DestinationType>(*abstractInt);
    // FIXME: implement f16
    if (auto* f32 = std::get_if<float>(&arg))
        return static_cast<DestinationType>(*f32);
    if (auto* abstractFloat = std::get_if<double>(&arg))
        return static_cast<DestinationType>(*abstractFloat);
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Functor, typename... Arguments>
static ConstantResult scalarOrVector(const Functor& functor, Arguments&&... unpackedArguments)
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
        ConstantResult tmp = std::apply(functor, scalars);
        if (!tmp)
            return makeUnexpected(tmp.error());
        result.elements[i] = WTFMove(*tmp);
    }
    return { { result } };
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

static ConstantValue constantMatrix(const Type* resultType, const FixedVector<ConstantValue>& arguments, unsigned columns, unsigned rows)
{
    if (arguments.isEmpty())
        return zeroValue(resultType);


    if (arguments.size() == 1) {
        auto& arg = arguments[0];
        ASSERT(arg.isMatrix());
        // FIXME: we might need to convert the type of the result when we support f16
        return arg;
    }

    if (arguments.size() == columns * rows)
        return ConstantMatrix { columns, rows, arguments };

    RELEASE_ASSERT(arguments.size() == columns);
    ConstantMatrix result(columns, rows);
    unsigned i = 0;
    for (auto& arg : arguments) {
        ASSERT(arg.isVector());
        auto& vector = arg.toVector();
        ASSERT(vector.elements.size() == rows);
        for (auto& element : vector.elements)
            result.elements[i++] = element;
    }
    ASSERT(i == columns * rows);
    return result;
}

#define UNARY_OPERATION(name, constraint, fn) \
    CONSTANT_FUNCTION(name) \
    { \
        UNUSED_PARAM(resultType); \
        return constantUnaryOperation<Constraints::constraint>(arguments, fn); \
    }

#define BINARY_OPERATION(name, constraint, fn) \
    CONSTANT_FUNCTION(name) \
    { \
        UNUSED_PARAM(resultType); \
        return constantBinaryOperation<Constraints::constraint>(arguments, fn); \
    }

#define TERNARY_OPERATION(name, constraint, fn) \
    CONSTANT_FUNCTION(name) \
    { \
        UNUSED_PARAM(resultType); \
        return constantTernaryOperation<Constraints::constraint>(arguments, fn); \
    }

#define CONSTANT_CONSTRUCTOR(name, type) \
    CONSTANT_FUNCTION(name) \
    { \
        return { constantConstructor<type>(resultType, arguments) }; \
    }

#define MATRIX_CONSTRUCTOR(columns, rows) \
    CONSTANT_FUNCTION(Mat ## columns ## x ## rows) \
    { \
        return constantMatrix(resultType, arguments, columns, rows); \
    }

#define CONSTANT_TRIGONOMETRIC(name, fn) UNARY_OPERATION(name, Float, WRAP_STD(fn))

#define WRAP_STD(fn) \
    [&]<typename... Args>(Args&&... args) { return std::fn(std::forward<Args>(args)...); }

// Arithmetic operators

CONSTANT_FUNCTION(Add)
{
    ASSERT(arguments.size() == 2);

    if (auto* left = std::get_if<ConstantMatrix>(&arguments[0])) {
        auto& right = std::get<ConstantMatrix>(arguments[1]);
        auto* elementType = std::get<Types::Matrix>(*resultType).element;
        ASSERT(left->columns == right.columns);
        ASSERT(left->rows == right.rows);
        ConstantMatrix result(left->columns, left->rows);
        for (unsigned i = 0; i < result.elements.size(); ++i)
            CALL_MOVE(result.elements[i], Add, elementType, { left->elements[i], right.elements[i] });
        return { { result } };
    }

    return constantBinaryOperation<Constraints::Number>(arguments, [&](auto left, auto right) {
        return left + right;
    });
}

CONSTANT_FUNCTION(Minus)
{
    UNUSED_PARAM(resultType);
    if (arguments.size() == 1) {
        return constantUnaryOperation<Constraints::Number>(arguments, [&](auto arg) {
            return -arg;
        });
    }

    if (auto* left = std::get_if<ConstantMatrix>(&arguments[0])) {
        auto& right = std::get<ConstantMatrix>(arguments[1]);
        auto* elementType = std::get<Types::Matrix>(*resultType).element;
        ASSERT(left->columns == right.columns);
        ASSERT(left->rows == right.rows);
        ConstantMatrix result(left->columns, left->rows);
        for (unsigned i = 0; i < result.elements.size(); ++i)
            CALL_MOVE(result.elements[i], Minus, elementType, { left->elements[i], right.elements[i] });
        return { { result } };
    }

    return constantBinaryOperation<Constraints::Number>(arguments, [&](auto left, auto right) {
        return left - right;
    });
}


CONSTANT_FUNCTION(Dot);

CONSTANT_FUNCTION(Multiply)
{
    ASSERT(arguments.size() == 2);

    auto* leftMatrix = std::get_if<ConstantMatrix>(&arguments[0]);
    auto* rightMatrix = std::get_if<ConstantMatrix>(&arguments[1]);
    if (leftMatrix && rightMatrix) {
        ASSERT(leftMatrix->columns == rightMatrix->rows);
        auto* elementType = std::get<Types::Matrix>(*resultType).element;
        ConstantMatrix result(rightMatrix->columns, leftMatrix->rows);
        for (unsigned i = 0; i < rightMatrix->columns; ++i) {
            for (unsigned j = 0; j < leftMatrix->rows; ++j) {
                ConstantValue value = zeroValue(elementType);
                for (unsigned k = 0; k < leftMatrix->columns; ++k) {
                    CALL(tmp, Multiply, elementType, { leftMatrix->elements[k * leftMatrix->rows + j], rightMatrix->elements[i * rightMatrix->rows + k] });
                    CALL_MOVE(value, Add, elementType, { value, tmp });
                }
                result.elements[i * result.rows + j] = value;
            }
        }
        return { { result } };
    }
    if (leftMatrix || rightMatrix) {
        if (auto* rightVector = std::get_if<ConstantVector>(&arguments[1])) {
            auto* elementType = std::get<Types::Vector>(*resultType).element;
            auto columns = leftMatrix->columns;
            auto rows = leftMatrix->rows;
            ConstantVector result(rows);
            ConstantVector leftVector(columns);
            for (unsigned i = 0; i < rows; ++i) {
                for (unsigned j = 0; j < columns; ++j)
                    leftVector.elements[j] = leftMatrix->elements[j * rows + i];
                CALL_MOVE(result.elements[i], Dot, elementType, { leftVector, *rightVector });
            }
            return { { result } };
        }

        if (auto* leftVector = std::get_if<ConstantVector>(&arguments[0])) {
            auto* elementType = std::get<Types::Vector>(*resultType).element;
            auto columns = rightMatrix->columns;
            auto rows = rightMatrix->rows;
            ConstantVector result(columns);
            ConstantVector rightVector(rows);
            for (unsigned i = 0; i < columns; ++i) {
                for (unsigned j = 0; j < rows; ++j)
                    rightVector.elements[j] = rightMatrix->elements[i * rows + j];
                CALL_MOVE(result.elements[i], Dot, elementType, { *leftVector, rightVector });
            }
            return { { result } };
        }

        const ConstantMatrix* matrix;
        ConstantValue scalar;
        if (leftMatrix) {
            matrix = leftMatrix;
            scalar = arguments[1];
        } else {
            matrix = rightMatrix;
            scalar = arguments[0];
        }

        auto* elementType = std::get<Types::Matrix>(*resultType).element;
        ConstantMatrix result(matrix->columns, matrix->rows);
        for (unsigned i = 0; i < result.elements.size(); ++i)
            CALL_MOVE(result.elements[i], Multiply, elementType, { matrix->elements[i], scalar });
        return { { result } };
    }

    return constantBinaryOperation<Constraints::Number>(arguments, [&](auto left, auto right) {
        return left * right;
    });
}

BINARY_OPERATION(Divide, Number, [&]<typename T>(T left, T right) -> ConstantResult {
    if constexpr (std::is_integral_v<T>) {
        if (!right)
            return makeUnexpected("invalid division by zero"_s);
        if constexpr (std::is_signed_v<T>) {
            if (left == std::numeric_limits<T>::lowest() && right == -1)
                return makeUnexpected("invalid division overflow"_s);
        }
    }
    return { { left / right } };
});

BINARY_OPERATION(Modulo, Number, [&](auto left, auto right) {
    if constexpr (std::is_floating_point_v<decltype(left)>)
        return fmod(left, right);
    else
        return left % right;
});

// Comparison Operations

BINARY_OPERATION(Equal, Scalar, [&](auto left, auto right) { return left == right; })
BINARY_OPERATION(NotEqual, Scalar, [&](auto left, auto right) { return left != right; })
BINARY_OPERATION(Lt, Scalar, [&](auto left, auto right) { return left < right; })
BINARY_OPERATION(LtEq, Scalar, [&](auto left, auto right) { return left <= right; })
BINARY_OPERATION(Gt, Scalar, [&](auto left, auto right) { return left > right; })
BINARY_OPERATION(GtEq, Scalar, [&](auto left, auto right) { return left >= right; })

// Logical Expressions

UNARY_OPERATION(Not, Bool, [&](bool arg) { return !arg; })
BINARY_OPERATION(Or, Bool, [&](bool left, bool right) { return left || right; })
BINARY_OPERATION(And, Bool, [&](bool left, bool right) { return left && right; })


// Bit Expressions

CONSTANT_FUNCTION(BitwiseOr)
{
    UNUSED_PARAM(resultType);
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        if (auto* leftBool = std::get_if<bool>(&left))
            return static_cast<bool>(static_cast<int>(*leftBool) | static_cast<int>(std::get<bool>(right)));
        if (auto* leftI32 = std::get_if<int32_t>(&left))
            return *leftI32 | std::get<int32_t>(right);
        if (auto* leftU32 = std::get_if<uint32_t>(&left))
            return *leftU32 | std::get<uint32_t>(right);
        if (auto* leftAbstractInt = std::get_if<int64_t>(&left))
            return *leftAbstractInt | std::get<int64_t>(right);
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1]);
}

CONSTANT_FUNCTION(BitwiseAnd)
{
    UNUSED_PARAM(resultType);
    return scalarOrVector([&](const auto& left, auto& right) -> ConstantValue {
        if (auto* leftBool = std::get_if<bool>(&left))
            return static_cast<bool>(static_cast<int>(*leftBool) & static_cast<int>(std::get<bool>(right)));
        if (auto* leftI32 = std::get_if<int32_t>(&left))
            return *leftI32 & std::get<int32_t>(right);
        if (auto* leftU32 = std::get_if<uint32_t>(&left))
            return *leftU32 & std::get<uint32_t>(right);
        if (auto* leftAbstractInt = std::get_if<int64_t>(&left))
            return *leftAbstractInt & std::get<int64_t>(right);
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1]);
}

UNARY_OPERATION(BitwiseNot, Integer, [&](auto arg) { return ~arg; })
BINARY_OPERATION(BitwiseXor, Integer, [&](auto left, auto right) { return left ^ right; })

CONSTANT_FUNCTION(BitwiseShiftLeft)
{
    // We can't use a BINARY_OPERATION here since the arguments might not all have the same type
    // i.e. we accept (u32, u32) as well as (i32, u32)
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 2);
    const auto& shift = [&]<typename T>(T left, uint32_t right) {
        return left << right;
    };

    return scalarOrVector([&](const auto& left, const auto& rightValue) -> ConstantValue {
        auto right = std::get<uint32_t>(rightValue);
        if (auto* i32 = std::get_if<int32_t>(&left))
            return shift(*i32, right);
        if (auto* u32 = std::get_if<uint32_t>(&left))
            return shift(*u32, right);
        if (auto* abstractInt = std::get_if<int64_t>(&left))
            return shift(*abstractInt, right);
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1]);
}

CONSTANT_FUNCTION(BitwiseShiftRight)
{
    // We can't use a BINARY_OPERATION here since the arguments might not all have the same type
    // i.e. we accept (u32, u32) as well as (i32, u32)
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 2);
    const auto& shift = [&]<typename T>(T left, uint32_t right) {
        return left >> right;
    };

    return scalarOrVector([&](const auto& left, const auto& rightValue) -> ConstantValue {
        auto right = std::get<uint32_t>(rightValue);
        if (auto* i32 = std::get_if<int32_t>(&left))
            return shift(*i32, right);
        if (auto* u32 = std::get_if<uint32_t>(&left))
            return shift(*u32, right);
        if (auto* abstractInt = std::get_if<int64_t>(&left))
            return shift(*abstractInt, right);
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1]);
}


// Constructors

CONSTANT_CONSTRUCTOR(Bool, bool)
CONSTANT_CONSTRUCTOR(I32, int32_t)
CONSTANT_CONSTRUCTOR(U32, uint32_t)
CONSTANT_CONSTRUCTOR(F32, float)

CONSTANT_FUNCTION(Vec2)
{
    return constantVector(resultType, arguments, 2);
}

CONSTANT_FUNCTION(Vec3)
{
    return constantVector(resultType, arguments, 3);
}

CONSTANT_FUNCTION(Vec4)
{
    return constantVector(resultType, arguments, 4);
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

// Logical Built-in Functions

CONSTANT_FUNCTION(All)
{
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    if (arg.isBool())
        return arg;

    ASSERT(arg.isVector());
    for (auto element : arg.toVector().elements) {
        if (!element.toBool())
            return { { false } };
    }
    return { { true } };
}

CONSTANT_FUNCTION(Any)
{
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    if (arg.isBool())
        return arg;

    ASSERT(arg.isVector());
    for (auto element : arg.toVector().elements) {
        if (element.toBool())
            return { { true } };
    }
    return { { false } };
}

CONSTANT_FUNCTION(Select)
{
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 3);
    const auto& falseValue = arguments[0];
    const auto& trueValue = arguments[1];
    const auto& condition = arguments[2];
    if (condition.isBool())
        return condition.toBool() ? trueValue : falseValue;

    ASSERT(falseValue.isVector());
    ASSERT(trueValue.isVector());
    ASSERT(condition.isVector());

    const auto& falseVector = arguments[0].toVector();
    const auto& trueVector = arguments[1].toVector();
    const auto& conditionVector = arguments[2].toVector();

    auto size = conditionVector.elements.size();
    ASSERT(size == falseVector.elements.size());
    ASSERT(size == trueVector.elements.size());

    ConstantVector result(size);
    for (unsigned i = 0; i < size; ++i) {
        result.elements[i] = conditionVector.elements[i].toBool()
            ? trueVector.elements[i]
            : falseVector.elements[i];
    }
    return { { result } };
}

// Numeric Built-in Functions

CONSTANT_TRIGONOMETRIC(Acos, acos);
CONSTANT_TRIGONOMETRIC(Asin, asin);
CONSTANT_TRIGONOMETRIC(Atan, atan);
CONSTANT_TRIGONOMETRIC(Cos,  cos);
CONSTANT_TRIGONOMETRIC(Sin,  sin);
CONSTANT_TRIGONOMETRIC(Tan,  tan);
CONSTANT_TRIGONOMETRIC(Acosh, acosh);
CONSTANT_TRIGONOMETRIC(Asinh, asinh);
CONSTANT_TRIGONOMETRIC(Atanh, atanh);
CONSTANT_TRIGONOMETRIC(Cosh, cosh);
CONSTANT_TRIGONOMETRIC(Sinh, sinh);
CONSTANT_TRIGONOMETRIC(Tanh, tanh);

UNARY_OPERATION(Abs, Number, [&](auto n) {
    if constexpr (std::is_same_v<decltype(n), uint32_t>)
        return static_cast<uint32_t>(std::abs(static_cast<int32_t>(n)));
    else
        return std::abs(n);
});

BINARY_OPERATION(Atan2, Float, WRAP_STD(atan2))
UNARY_OPERATION(Ceil, Float, WRAP_STD(ceil))
TERNARY_OPERATION(Clamp, Number, [&](auto e, auto low, auto high) { return std::min(std::max(e, low), high); })

UNARY_OPERATION(CountLeadingZeros, ConcreteInteger, [&]<typename T>(T arg) -> T {
    return std::countl_zero(static_cast<unsigned>(arg));
})

UNARY_OPERATION(CountOneBits, ConcreteInteger, [&]<typename T>(T arg) -> T {
    return std::popcount(static_cast<unsigned>(arg));
})

UNARY_OPERATION(CountTrailingZeros, ConcreteInteger, [&]<typename T>(T arg) -> T {
    return std::countr_zero(static_cast<unsigned>(arg));
})

CONSTANT_FUNCTION(Cross)
{
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 2);
    auto& lhs = arguments[0].toVector();
    auto& rhs = arguments[1].toVector();

    const auto& cross = [&]<typename T>() -> ConstantResult {
        auto u0 = std::get<T>(lhs.elements[0]);
        auto u1 = std::get<T>(lhs.elements[1]);
        auto u2 = std::get<T>(lhs.elements[2]);
        auto v0 = std::get<T>(rhs.elements[0]);
        auto v1 = std::get<T>(rhs.elements[1]);
        auto v2 = std::get<T>(rhs.elements[2]);

        ConstantVector result(3);
        result.elements[0] = u1 * v2 - u2 * v1;
        result.elements[1] = u2 * v0 - u0 * v2;
        result.elements[2] = u0 * v1 - u1 * v0;
        return { { result } };
    };

    if (std::holds_alternative<float>(lhs.elements[0]))
        return cross.operator()<float>();
    if (std::holds_alternative<double>(lhs.elements[0]))
        return cross.operator()<double>();
    // FIXME: implement f16
    RELEASE_ASSERT_NOT_REACHED();
}

UNARY_OPERATION(Degrees, Float, [&]<typename T>(T arg) -> T { return arg * (180 / std::numbers::pi); })

CONSTANT_FUNCTION(Determinant)
{
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 1);
    auto& matrix = std::get<ConstantMatrix>(arguments[0]);
    auto columns = matrix.columns;
    auto solve2 = [&](
        auto a, auto b,
        auto c, auto d
    ) {
        return a * d - b * c;
    };

    auto solve3 = [&](
        auto a, auto b, auto c,
        auto d, auto e, auto f,
        auto g, auto h, auto i
    ) {
        return a * e * i + b * f * g + c * d * h - c * e * g - b * d * i - a * f * h;
    };

    auto solve4 = [&](
        auto a, auto b, auto c, auto d,
        auto e, auto f, auto g, auto h,
        auto i, auto j, auto k, auto l,
        auto m, auto n, auto o, auto p
    ) {
        return a * solve3(f, g, h, j, k, l, n, o, p) - b * solve3(e, g, h, i, k, l, m, o, p) + c * solve3(e, f, h, i, j, l, m, n, p) - d * solve3(e, f, g, i, j, k, m, n, o);
    };

    const auto& determinant = [&]<typename T>() {
        switch (columns) {
        case 2:
            return solve2(
                std::get<T>(matrix.elements[0]), std::get<T>(matrix.elements[2]),
                std::get<T>(matrix.elements[1]), std::get<T>(matrix.elements[3])
            );

        case 3:
            return solve3(
                std::get<T>(matrix.elements[0]), std::get<T>(matrix.elements[3]), std::get<T>(matrix.elements[6]),
                std::get<T>(matrix.elements[1]), std::get<T>(matrix.elements[4]), std::get<T>(matrix.elements[7]),
                std::get<T>(matrix.elements[2]), std::get<T>(matrix.elements[5]), std::get<T>(matrix.elements[8])
            );
        case 4:
            return solve4(
                std::get<T>(matrix.elements[0]), std::get<T>(matrix.elements[4]), std::get<T>(matrix.elements[8]),  std::get<T>(matrix.elements[12]),
                std::get<T>(matrix.elements[1]), std::get<T>(matrix.elements[5]), std::get<T>(matrix.elements[9]),  std::get<T>(matrix.elements[13]),
                std::get<T>(matrix.elements[2]), std::get<T>(matrix.elements[6]), std::get<T>(matrix.elements[10]), std::get<T>(matrix.elements[14]),
                std::get<T>(matrix.elements[3]), std::get<T>(matrix.elements[7]), std::get<T>(matrix.elements[11]), std::get<T>(matrix.elements[15])
            );
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    if (std::holds_alternative<float>(matrix.elements[0]))
        return { { determinant.operator()<float>() } };
    if (std::holds_alternative<double>(matrix.elements[0]))
        return { { determinant.operator()<double>() } };
    // FIXME: implement f16
    RELEASE_ASSERT_NOT_REACHED();
}

CONSTANT_FUNCTION(Length);

CONSTANT_FUNCTION(Distance)
{
    // we can't produce the type for the intermediate computation here. Pass
    // nullptr instead so we crash if we ever start depending on it.
    CALL(minus, Minus, nullptr, arguments);
    return constantLength(resultType, { minus });
}

CONSTANT_FUNCTION(Dot)
{
    CALL(product, Multiply, resultType, arguments);
    ConstantValue result = zeroValue(resultType);
    for (auto& element : product.toVector().elements)
        CALL_MOVE(result, Add, resultType,  { result, element });
    return { { result } };
}

CONSTANT_FUNCTION(Length)
{
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    if (!arg.isVector())
        return constantAbs(resultType, arguments);

    ConstantValue result = zeroValue(resultType);
    for (auto& element : arg.toVector().elements) {
        CALL(tmp, Multiply, resultType, { element, element });
        CALL_MOVE(result, Add, resultType, { result, tmp });
    }
    return { { result } };
}

UNARY_OPERATION(Exp, Float, WRAP_STD(exp));
UNARY_OPERATION(Exp2, Float, WRAP_STD(exp2));

CONSTANT_FUNCTION(ExtractBits)
{
    // We can't use a TERNARY_OPERATION here since the arguments might not all have the same type
    // i.e. we accept (u32, u32, u32) as well as (i32, u32, u32)
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 3);
    auto offset = std::get<uint32_t>(arguments[1]);
    auto count = std::get<uint32_t>(arguments[2]);

    const auto& extractBits = [&]<typename T>(T e) {
        constexpr unsigned w = 32;
        unsigned o = std::min(offset, w);
        unsigned c = std::min(count, w - o);
        if (!c)
            return static_cast<T>(0);
        if (c == w)
            return e;
        unsigned srcMask = ((1 << c) - 1) << o;
        T result = (e & srcMask) >> o;
        if constexpr (std::is_signed_v<T>) {
            if (result & (1 << (c - 1))) {
                unsigned dstMask = srcMask >> o;
                result |= (~0 & ~dstMask);
            }
        }
        return result;
    };

    return scalarOrVector([&](const auto& e) -> ConstantValue {
        if (auto* i32 = std::get_if<int32_t>(&e))
            return extractBits(*i32);
        if (auto* u32 = std::get_if<uint32_t>(&e))
            return extractBits(*u32);
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0]);
}

CONSTANT_FUNCTION(FaceForward)
{
    ASSERT(arguments.size() == 3);
    const auto& e1 = arguments[0];
    const auto& e2 = arguments[1];
    const auto& e3 = arguments[2];

    auto* elementType = std::get<Types::Vector>(*resultType).element;
    CALL(dot, Dot, elementType, { e2, e3 });
    CALL(lt, Lt, elementType, { dot, zeroValue(elementType) });
    if (lt.toBool())
        return e1;
    return constantMinus(resultType, { e1 });
}

UNARY_OPERATION(FirstLeadingBit, ConcreteInteger, [&](auto e) {
    if constexpr (std::is_same_v<decltype(e), int32_t>) {
        unsigned ue = e;
        if (!e || e == -1)
            return -1;
        unsigned count = e < 0 ? std::countl_one(ue) : std::countl_zero(ue);
        return static_cast<int32_t>(31 - count);
    } else {
        if (!e)
            return static_cast<uint32_t>(-1);
        return static_cast<uint32_t>(31 - std::countl_zero(e));
    }
})

UNARY_OPERATION(FirstTrailingBit, ConcreteInteger, [&](auto e) {
    if (!e)
        return static_cast<decltype(e)>(-1);
    unsigned ue = e;
    return static_cast<decltype(e)>(std::countr_zero(ue));
})

UNARY_OPERATION(Floor, Float, WRAP_STD(floor));

CONSTANT_FUNCTION(Fma)
{
    ASSERT(arguments.size() == 3);
    CALL(product, Multiply, resultType, { arguments[0], arguments[1] });
    return constantAdd(resultType, { product, arguments[2] });
}

CONSTANT_FUNCTION(Fract)
{
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    CALL(floor, Floor, resultType, { arg });
    return constantMinus(resultType, { arg, floor });
}

CONSTANT_FUNCTION(Frexp)
{
    // FIXME: this needs the special return types __frexp_result_*
    UNUSED_PARAM(resultType);
    UNUSED_PARAM(arguments);
    RELEASE_ASSERT_NOT_REACHED();
}

CONSTANT_FUNCTION(InsertBits)
{
    UNUSED_PARAM(resultType);
    return scalarOrVector([&](auto eValue, auto newbitsValue, auto offset, auto count) -> ConstantValue {
        constexpr unsigned w = 32;
        unsigned o = std::min(std::get<uint32_t>(offset), w);
        unsigned c = std::min(std::get<uint32_t>(count), w - o);
        if (!c)
            return eValue;
        if (c == w)
            return newbitsValue;

        const auto& fn = [&](auto e, auto newbits) {
            auto from = newbits << o;
            auto mask = ((1 << c) - 1) << o;
            auto result = e;
            result &= ~mask;
            result |= (from & mask);
            return result;
        };

        if (auto* e = std::get_if<int32_t>(&eValue))
            return fn(*e, std::get<int32_t>(newbitsValue));
        return fn(std::get<uint32_t>(eValue), std::get<uint32_t>(newbitsValue));
    }, arguments[0], arguments[1], arguments[2], arguments[3]);
}

UNARY_OPERATION(InverseSqrt, Float, [&]<typename T>(T arg) -> T {
    return 1.0 / std::sqrt(arg);
})

CONSTANT_FUNCTION(Ldexp)
{
    UNUSED_PARAM(resultType);
    return scalarOrVector([&](const auto& e1, auto& e2) -> ConstantValue {
        if (auto* abstractE2 = std::get_if<int64_t>(&e2))
            return std::get<double>(e1) * std::pow(2.0, static_cast<double>(*abstractE2));
        if (auto* f32E1 = std::get_if<float>(&e1))
            return *f32E1 * std::pow(2.f, static_cast<float>(std::get<int32_t>(e2)));
        // FIXME: implement f16
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1]);
}

UNARY_OPERATION(Log, Float, WRAP_STD(log))
UNARY_OPERATION(Log2, Float, WRAP_STD(log2))
BINARY_OPERATION(Max, Number, WRAP_STD(max))
BINARY_OPERATION(Min, Number, WRAP_STD(min))
TERNARY_OPERATION(Mix, Number, [&](auto e1, auto e2, auto e3) { return  e1 * (1 - e3) + e2 * e3; })

CONSTANT_FUNCTION(Modf)
{
    // FIXME: this needs the special return types __modf_result_*
    UNUSED_PARAM(resultType);
    UNUSED_PARAM(arguments);
    RELEASE_ASSERT_NOT_REACHED();
}

CONSTANT_FUNCTION(Normalize)
{
    ASSERT(arguments.size() == 1);
    auto* elementType = std::get<Types::Vector>(*resultType).element;
    const auto& arg = arguments[0];
    CALL(length, Length, elementType, arguments);
    return constantDivide(resultType, { arg, length });
}

BINARY_OPERATION(Pow, Float, WRAP_STD(pow))

CONSTANT_FUNCTION(QuantizeToF16)
{
    // FIXME: add support for f16
    UNUSED_PARAM(resultType);
    UNUSED_PARAM(arguments);
    RELEASE_ASSERT_NOT_REACHED();
}

UNARY_OPERATION(Radians, Float, [&]<typename T>(T arg) -> T { return arg * std::numbers::pi / 180; })

CONSTANT_FUNCTION(Reflect)
{
    ASSERT(arguments.size() == 2);
    const auto& e1 = arguments[0];
    const auto& e2 = arguments[1];
    auto* elementType = std::get<Types::Vector>(*resultType).element;
    auto& primitive = std::get<Types::Primitive>(*elementType);

    const auto& reflect = [&]<typename T>() -> ConstantResult {
        CALL(dot, Dot, elementType, { e2, e1 });
        CALL(prod, Multiply, resultType, { dot, e2 });
        CALL(doubleResult, Multiply, resultType, { static_cast<T>(2), e2 });
        return constantMinus(resultType, { e1, doubleResult });
    };

    if (primitive.kind == Types::Primitive::F32)
        return reflect.operator()<float>();
    if (primitive.kind == Types::Primitive::AbstractFloat)
        return reflect.operator()<double>();
    // FIXME: implement f16
    RELEASE_ASSERT_NOT_REACHED();
}

CONSTANT_FUNCTION(Sqrt);

CONSTANT_FUNCTION(Refract)
{
    ASSERT(arguments.size() == 3);
    const auto& e1 = arguments[0];
    const auto& e2 = arguments[1];
    const auto& e3 = arguments[2];

    const auto& refract = [&]<typename T>(T e3) -> ConstantResult {
        auto* elementType = std::get<Types::Vector>(*resultType).element;
        CALL(dot, Dot, elementType, { e2, e1 });
        CALL(pow, Pow, elementType, { dot, static_cast<T>(2.0) });
        CALL(sub, Minus, elementType, { static_cast<T>(1.0), pow });
        CALL(pow2, Pow, elementType, { e3, static_cast<T>(2.0) });
        CALL(mul, Multiply, elementType, { pow2, sub });
        CALL(k, Minus, elementType, { static_cast<T>(1.0), mul });
        CALL(lt, Lt, elementType, { k, static_cast<T>(0.0) });

        if (lt.toBool())
            return zeroValue(resultType);

        {
            CALL(mul, Multiply, elementType, { e3, dot });
            CALL(sqrt, Sqrt, elementType, { k });
            CALL(sum, Add, elementType, { mul, sqrt });
            CALL(mul2, Multiply, resultType, { e3, e1 });
            CALL(mul3, Multiply, resultType, { sum, e2 });
            return constantMinus(resultType, { mul2, mul3 });
        }
    };

    if (auto* f32 = std::get_if<float>(&e3))
        return refract(*f32);
    if (auto* abstractFloat = std::get_if<double>(&e3))
        return refract(*abstractFloat);
    // FIXME: implement f16
    RELEASE_ASSERT_NOT_REACHED();
}

UNARY_OPERATION(ReverseBits, Integer, [&]<typename T>(T e) -> T {
    unsigned v = e;
    T result = 0;
    for (unsigned k = 0; k < 32; ++k)
        result |= (v & (31 - k)) << k;
    return result;
})

UNARY_OPERATION(Round, Float, [&](auto v) {
    auto rounded = std::round(v);
    if (rounded - v == 0.5 && fmod(rounded, 2))
        return rounded - 1;
    return rounded;
})

UNARY_OPERATION(Saturate, Float, [&](auto e) {
    return std::min(std::max(e, static_cast<decltype(e)>(0)), static_cast<decltype(e)>(1));
})

UNARY_OPERATION(Sign, Number, [&]<typename T>(T e) -> T {
    T result;
    if (e > 0)
        result = 1;
    else if (e < 0)
        result = -1;
    else
        result = 0;
    return result;
})

TERNARY_OPERATION(Smoothstep, Float, [&](auto low, auto high, auto x) {
    auto e = (x - low) / (high - low);
    auto t = std::min(std::max(e, static_cast<decltype(e)>(0)), static_cast<decltype(e)>(1));
    return t * t * (3.0 - 2.0 * t);
})

UNARY_OPERATION(Sqrt, Float, WRAP_STD(sqrt))
BINARY_OPERATION(Step, Float, [&](auto edge, auto x) { return edge <= x ? 1.0 : 0.0; })

CONSTANT_FUNCTION(Transpose)
{
    UNUSED_PARAM(resultType);
    ASSERT(arguments.size() == 1);
    auto& matrix = std::get<ConstantMatrix>(arguments[0]);
    auto columns = matrix.columns;
    auto rows = matrix.rows;
    ConstantMatrix result(rows, columns);
    for (unsigned j = 0; j < rows; ++j) {
        for (unsigned i = 0; i < columns; ++i)
            result.elements[j * columns + i] = matrix.elements[i * rows + j];
    }
    return { { result } };
}

UNARY_OPERATION(Trunc, Float, WRAP_STD(trunc))

// Type checker helpers

static bool containsZero(ConstantValue value, const Type* valueType)
{
    auto wrapped = [&]() -> Expected<bool, String> {
        auto zero = zeroValue(valueType);
        CALL(equal, Equal, nullptr, { value, zero });
        CALL(any, Any, nullptr, { equal });
        return any.toBool();
    }();

    if (!wrapped)
        return false;
    return *wrapped;
}

#undef CALL_
#undef CALL
#undef CALL_MOVE_
#undef CALL_MOVE
#undef CONSTANT_FUNCTION

} // namespace WGSL
