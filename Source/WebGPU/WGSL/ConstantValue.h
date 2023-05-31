
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

#include <wtf/FixedVector.h>

namespace WGSL {

struct Type;

// A constant value might be:
// - a scalar
// - a vector
// - a matrix
// - a fixed-size array type
// - a structure
struct ConstantValue;

struct ConstantArray {
    ConstantArray(size_t size)
        : elements(size)
    {
    }

    ConstantArray(FixedVector<ConstantValue>&& elements)
        : elements(WTFMove(elements))
    {
    }

    FixedVector<ConstantValue> elements;
};

struct ConstantVector {
    ConstantVector(size_t size)
        : elements(size)
    {
    }

    ConstantVector(FixedVector<ConstantValue>&& elements)
        : elements(WTFMove(elements))
    {
    }

    FixedVector<ConstantValue> elements;
};

using BaseValue = std::variant<double, int64_t, bool, ConstantArray, ConstantVector>;
struct ConstantValue : BaseValue {
    ConstantValue() = default;

    template<typename T>
    ConstantValue(const Type* type, T&& value)
        : BaseValue(std::forward<T>(value))
        , type(type)
    {
    }

    static void constructDeletedValue(ConstantValue& slot)
    {
        slot.type = bitwise_cast<Type*>(static_cast<intptr_t>(-1));
    }
    static bool isDeletedValue(const ConstantValue& value)
    {
        return value.type == bitwise_cast<Type*>(static_cast<intptr_t>(-1));
    }

    void dump(PrintStream&) const;

    bool isNumber() const
    {
        return std::holds_alternative<int64_t>(*this) || std::holds_alternative<double>(*this);
    }

    double toDouble() const
    {
        ASSERT(isNumber());
        if (std::holds_alternative<double>(*this))
            return std::get<double>(*this);
        return static_cast<double>(std::get<int64_t>(*this));
    }

    const Type* type;
};

} // namespace WGSL
