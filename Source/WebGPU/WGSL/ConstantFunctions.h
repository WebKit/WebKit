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

// Helpers

template<Constraint constraint, typename Functor>
static ConstantValue constantUnaryOperation(const FixedVector<ConstantValue>& arguments, const Functor& fn)
{
    ASSERT(arguments.size() == 1);
    return scalarOrVector([&](auto& arg) -> ConstantValue {
        if constexpr (constraint & Constraints::Bool) {
            if (arg.isBool())
                return fn(arg.toBool());
        }
        if constexpr (constraint & Constraints::Integer) {
            if (arg.isInt())
                return fn(arg.toInt());
        }
        if constexpr (constraint & Constraints::Float) {
            if (arg.isNumber())
                return fn(arg.toDouble());
        }
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0]);
}

template<Constraint constraint, typename Functor>
static ConstantValue constantBinaryOperation(const FixedVector<ConstantValue>& arguments, const Functor& fn)
{
    ASSERT(arguments.size() == 2);
    return scalarOrVector([&](auto& left, auto& right) -> ConstantValue {
        if constexpr (constraint & Constraints::Bool) {
            if (left.isBool() && right.isBool())
                return fn(left.toBool(), right.toBool());
        }
        if constexpr (constraint & Constraints::Integer) {
            if (left.isInt() && right.isInt())
                return fn(left.toInt(), right.toInt());
        }
        if constexpr (constraint & Constraints::Float) {
            if (left.isNumber() && right.isNumber())
                return fn(left.toDouble(), right.toDouble());
        }
        RELEASE_ASSERT_NOT_REACHED();
    }, arguments[0], arguments[1]);
}

template<Constraint constraint, typename Functor>
static ConstantValue constantTernaryOperation(const FixedVector<ConstantValue>& arguments, const Functor& fn)
{
    ASSERT(arguments.size() == 3);
    return scalarOrVector([&](auto& first, auto& second, auto& third) -> ConstantValue {
        if constexpr (constraint & Constraints::Bool) {
            if (first.isBool() && second.isBool() && third.isBool())
                return fn(first.toBool(), second.toBool(), third.toBool());
        }
        if constexpr (constraint & Constraints::Integer) {
            if (first.isInt() && second.isInt() && third.isInt())
                return fn(first.toInt(), second.toInt(), third.toInt());
        }
        if constexpr (constraint & Constraints::Float) {
            if (first.isNumber() && second.isNumber() && third.isNumber())
                return fn(first.toDouble(), second.toDouble(), third.toDouble());
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
    if (arg.isBool())
        return static_cast<DestinationType>(arg.toBool());
    if (arg.isInt())
        return static_cast<DestinationType>(arg.toInt());
    ASSERT(arg.isNumber());
    return static_cast<DestinationType>(arg.toDouble());
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

    // FIXME: we don't support matrices yet
    UNUSED_PARAM(columns);
    UNUSED_PARAM(rows);
    RELEASE_ASSERT_NOT_REACHED();
}

#define UNARY_OPERATION(name, constraint, fn) \
    static ConstantValue constant##name(const Type* resultType, const FixedVector<ConstantValue>& arguments) \
    { \
        UNUSED_PARAM(resultType); \
        return constantUnaryOperation<Constraints::constraint>(arguments, fn); \
    }

#define BINARY_OPERATION(name, constraint, fn) \
    static ConstantValue constant##name(const Type* resultType, const FixedVector<ConstantValue>& arguments) \
    { \
        UNUSED_PARAM(resultType); \
        return constantBinaryOperation<Constraints::constraint>(arguments, fn); \
    }

#define TERNARY_OPERATION(name, constraint, fn) \
    static ConstantValue constant##name(const Type* resultType, const FixedVector<ConstantValue>& arguments) \
    { \
        UNUSED_PARAM(resultType); \
        return constantTernaryOperation<Constraints::constraint>(arguments, fn); \
    }

#define CONSTANT_CONSTRUCTOR(name, type) \
    static ConstantValue constant##name(const Type* resultType, const FixedVector<ConstantValue>& arguments) \
    { \
        return constantConstructor<type>(resultType, arguments); \
    }

#define MATRIX_CONSTRUCTOR(columns, rows) \
    static ConstantValue constantMat ## columns ## x ## rows(const Type* resultType, const FixedVector<ConstantValue>& arguments) \
    { \
        return constantMatrix(resultType, arguments, columns, rows); \
    }

#define CONSTANT_TRIGONOMETRIC(name, fn) UNARY_OPERATION(name, Float, WRAP_STD(fn))

#define WRAP_STD(fn) \
    [&]<typename... Args>(Args&&... args) { return std::fn(std::forward<Args>(args)...); }

// Arithmetic operators
static ConstantValue constantAdd(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 2);

    // FIXME: handle constant matrices

    return constantBinaryOperation<Constraints::Number>(arguments, [&](auto left, auto right) {
        return left + right;
    });
}

static ConstantValue constantMinus(const Type*, const FixedVector<ConstantValue>& arguments)
{
    if (arguments.size() == 1) {
        return constantUnaryOperation<Constraints::Number>(arguments, [&](auto arg) {
            return -arg;
        });
    }
    return constantBinaryOperation<Constraints::Number>(arguments, [&](auto left, auto right) {
        return left - right;
    });
}


static ConstantValue constantMultiply(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 2);

    // FIXME: handle constant matrices

    return constantBinaryOperation<Constraints::Number>(arguments, [&](auto left, auto right) {
        return left * right;
    });
}

BINARY_OPERATION(Divide, Number, [&](auto left, auto right) {
    return left / right;
});

BINARY_OPERATION(Modulo, Number, [&](auto left, auto right) {
    if constexpr (std::is_same_v<decltype(left), double>)
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

UNARY_OPERATION(BitwiseNot, Integer, [&](auto arg) { return ~arg; })
BINARY_OPERATION(BitwiseXor, Integer, [&](auto left, auto right) { return left ^ right; })
BINARY_OPERATION(BitwiseShiftLeft, Integer, [&](auto left, auto right) { return left << right; })
BINARY_OPERATION(BitwiseShiftRight, Integer, [&](auto left, auto right) { return left >> right; })


// Constructors

CONSTANT_CONSTRUCTOR(Bool, bool)
CONSTANT_CONSTRUCTOR(I32, int32_t)
CONSTANT_CONSTRUCTOR(U32, uint32_t)
CONSTANT_CONSTRUCTOR(F32, float)

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

static ConstantValue constantAll(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    if (arg.isBool())
        return arg;

    ASSERT(arg.isVector());
    for (auto element : arg.toVector().elements) {
        if (!element.toBool())
            return false;
    }
    return true;
}

static ConstantValue constantAny(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    if (arg.isBool())
        return arg;

    ASSERT(arg.isVector());
    for (auto element : arg.toVector().elements) {
        if (element.toBool())
            return true;
    }
    return false;
}

static ConstantValue constantSelect(const Type*, const FixedVector<ConstantValue>& arguments)
{
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
    return result;
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

UNARY_OPERATION(Abs, Number, WRAP_STD(abs))
BINARY_OPERATION(Atan2, Float, WRAP_STD(atan2))
UNARY_OPERATION(Ceil, Float, WRAP_STD(ceil))
TERNARY_OPERATION(Clamp, Number, [&](auto e, auto low, auto high) { return std::min(std::max(e, low), high); })

UNARY_OPERATION(CountLeadingZeros, Integer, [&](uint32_t arg) { return std::countl_zero(arg); })
UNARY_OPERATION(CountOneBits, Integer, [&](uint32_t arg) { return std::popcount(arg); })
UNARY_OPERATION(CountTrailingZeros, Integer, [&](uint32_t arg) { return std::countr_zero(arg); })

static ConstantValue constantCross(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 2);
    auto& lhs = arguments[0].toVector();
    auto& rhs = arguments[1].toVector();

    auto u0 = lhs.elements[0].toDouble();
    auto u1 = lhs.elements[1].toDouble();
    auto u2 = lhs.elements[2].toDouble();
    auto v0 = rhs.elements[0].toDouble();
    auto v1 = rhs.elements[1].toDouble();
    auto v2 = rhs.elements[2].toDouble();

    ConstantVector result(3);
    result.elements[0] = u1 * v2 - u2 * v1;
    result.elements[1] = u2 * v0 - u0 * v2;
    result.elements[2] = u0 * v1 - u1 * v0;
    return result;
}

UNARY_OPERATION(Degrees, Float, [&](float arg) { return arg * (180 / std::numbers::pi); })

static ConstantValue constantDeterminant(const Type*, const FixedVector<ConstantValue>&)
{
    // FIXME: we don't support matrices yet
    RELEASE_ASSERT_NOT_REACHED();
}

static ConstantValue constantLength(const Type*, const FixedVector<ConstantValue>&);
static ConstantValue constantDistance(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    return constantLength(resultType, { constantMinus(resultType, arguments) });
}

static ConstantValue constantDot(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    auto product = constantMultiply(resultType, arguments).toVector();
    if (satisfies(resultType, Constraints::Integer)) {
        int64_t result = 0;
        for (auto& element : product.elements)
            result += element.toInt();
        return result;
    }

    double result = 0;
    for (auto& element : product.elements)
        result += element.toDouble();
    return result;
}

static ConstantValue constantLength(const Type*, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    if (!arg.isVector())
        return arg;
    double result = 0;
    for (auto& element : arg.toVector().elements)
        result += std::pow(element.toDouble(), 2);
    return result;
}

UNARY_OPERATION(Exp, Float, WRAP_STD(exp));
UNARY_OPERATION(Exp2, Float, WRAP_STD(exp2));

TERNARY_OPERATION(ExtractBits, Integer, [&](auto e, unsigned offset, unsigned count) {
    if (auto* vector = std::get_if<Types::Vector>(resultType))
        resultType = vector->element;
    auto& primitive = std::get<Types::Primitive>(*resultType);
    bool isSigned = primitive.kind != Types::Primitive::U32;

    constexpr unsigned w = 32;
    unsigned o = std::min(offset, w);
    unsigned c = std::min(count, w - o);
    if (!c)
        return 0;
    if (c == w)
        return static_cast<int>(e);
    unsigned srcMask = ((1 << c) - 1) << o;
    int result = (e & srcMask) >> o;
    if (isSigned) {
        if (result & (1 << (c - 1))) {
            unsigned dstMask = srcMask >> o;
            result |= (~0 & ~dstMask);
        }
    }
    return result;
})

static ConstantValue constantFaceForward(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 3);
    const auto& e1 = arguments[0];
    const auto& e2 = arguments[1];
    const auto& e3 = arguments[2];

    auto dot = constantDot(resultType, { e2, e3 });
    if (constantLt(resultType, { dot, 0.0 }).toBool())
        return e1;
    return constantMinus(resultType, { e1 });
}

UNARY_OPERATION(FirstLeadingBit, Integer, [&](auto e) {
    if (auto* vector = std::get_if<Types::Vector>(resultType))
        resultType = vector->element;
    auto& primitive = std::get<Types::Primitive>(*resultType);
    bool isSigned = primitive.kind != Types::Primitive::U32;

    unsigned ue = e;
    if (isSigned) {
        if (!e || e == -1)
            return -1;
        unsigned count = e < 0 ? std::countl_one(ue) : std::countl_zero(ue);
        return static_cast<int>(31 - count);
    }

    if (!e)
        return -1;
    return static_cast<int>(31 - std::countl_zero(ue));
})

UNARY_OPERATION(FirstTrailingBit, Integer, [&](auto e) {
    if (!e)
        return -1;
    unsigned ue = e;
    return static_cast<int>(std::countr_zero(ue));
})

UNARY_OPERATION(Floor, Float, WRAP_STD(floor));

static ConstantValue constantFma(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 3);
    return constantAdd(resultType, { constantMultiply(resultType, { arguments[0], arguments[1] }), arguments[2] });
}

static ConstantValue constantFract(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    return constantMinus(resultType, { arg, constantFloor(resultType, { arg }) });
}

static ConstantValue constantFrexp(const Type*, const FixedVector<ConstantValue>&)
{
    // FIXME: this needs the special return types __frexp_result_*
    RELEASE_ASSERT_NOT_REACHED();
}

static ConstantValue constantInsertBits(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](auto e, auto newbits, auto offset, auto count) -> ConstantValue {
        constexpr unsigned w = 32;
        unsigned o = std::min(static_cast<unsigned>(offset.toInt()), w);
        unsigned c = std::min(static_cast<unsigned>(count.toInt()), w - o);
        if (!c)
            return e;
        if (c == w)
            return newbits;
        unsigned from = newbits.toInt() << o;
        unsigned mask = ((1 << c) - 1) << o;
        int result = e.toInt();
        result &= ~mask;
        result |= (from & mask);
        return result;
    }, arguments[0], arguments[1], arguments[2], arguments[3]);
}

static ConstantValue constantSqrt(const Type*, const FixedVector<ConstantValue>&);
static ConstantValue constantInverseSqrt(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    return constantDivide(resultType, { 1, constantSqrt(resultType, { arg }) });
}

static ConstantValue constantLdexp(const Type*, const FixedVector<ConstantValue>& arguments)
{
    return scalarOrVector([&](const auto& e1, auto& e2) -> ConstantValue {
        return e1.toDouble() * std::pow(2, e2.toInt());
    }, arguments[0], arguments[1]);
}

UNARY_OPERATION(Log, Float, WRAP_STD(log))
UNARY_OPERATION(Log2, Float, WRAP_STD(log2))
BINARY_OPERATION(Max, Number, WRAP_STD(max))
BINARY_OPERATION(Min, Number, WRAP_STD(min))
TERNARY_OPERATION(Mix, Number, [&](auto e1, auto e2, auto e3) { return  e1 * (1 - e3) + e2 * e3; })

static ConstantValue constantModf(const Type*, const FixedVector<ConstantValue>&)
{
    // FIXME: this needs the special return types __modf_result_*
    RELEASE_ASSERT_NOT_REACHED();
}

static ConstantValue constantNormalize(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 1);
    const auto& arg = arguments[0];
    return constantDivide(resultType, { arg, constantLength(nullptr, arguments) });
}

BINARY_OPERATION(Pow, Float, WRAP_STD(pow))

static ConstantValue constantQuantizeToF16(const Type*, const FixedVector<ConstantValue>&)
{
    // FIXME: add support for f16
    RELEASE_ASSERT_NOT_REACHED();
}

UNARY_OPERATION(Radians, Float, [&](float arg) { return arg * std::numbers::pi / 180; })

static ConstantValue constantReflect(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 2);
    const auto& e1 = arguments[0];
    const auto& e2 = arguments[1];
    auto dot = constantDot(resultType, { e2, e1 });
    auto prod = constantMultiply(resultType, { dot, e2 });
    auto doubleResult = constantMultiply(resultType, { 2, e2 });
    return constantMinus(resultType, { e1, doubleResult });
}

static ConstantValue constantRefract(const Type* resultType, const FixedVector<ConstantValue>& arguments)
{
    ASSERT(arguments.size() == 3);
    const auto& e1 = arguments[0];
    const auto& e2 = arguments[1];
    const auto& e3 = arguments[2];

    auto dot = constantDot(resultType, { e2, e1 });
    auto pow = constantPow(resultType, { dot, 2.0 });
    auto sub = constantMinus(resultType, { 1.0, pow });
    auto pow2 = constantPow(resultType, { e3, 2.0 });
    auto mul = constantMultiply(resultType, { pow2, sub });
    auto k = constantMinus(resultType, { 1.0, mul });

    if (constantLt(resultType, { k, 0.0 }).toBool())
        return zeroValue(resultType);

    {
        auto mul = constantMultiply(resultType, { e3, dot });
        auto sqrt = constantSqrt(resultType, { k });
        auto sum = constantAdd(resultType, { mul, sqrt });
        auto mul2 = constantMultiply(resultType, { e3, e1 });
        auto mul3 = constantMultiply(resultType, { sum, e2 });
        return constantMinus(resultType, { mul2, mul3 });
    }
}

UNARY_OPERATION(ReverseBits, Integer, [&](auto e) {
    unsigned v = e;
    int result = 0;
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
    return std::min(std::max(e, 0.0), 1.0);
})

UNARY_OPERATION(Sign, Number, [&](auto e) {
    int result;
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
    auto t = std::min(std::max(e, 0.0), 1.0);
    return t * t * (3.0 - 2.0 * t);
})

UNARY_OPERATION(Sqrt, Float, WRAP_STD(sqrt))
BINARY_OPERATION(Step, Float, [&](auto edge, auto x) { return edge <= x ? 1.0 : 0.0; })

static ConstantValue constantTranspose(const Type*, const FixedVector<ConstantValue>&)
{
    // FIXME: we don't support matrices yet
    RELEASE_ASSERT_NOT_REACHED();
}

UNARY_OPERATION(Trunc, Float, WRAP_STD(trunc))

} // namespace WGSL
