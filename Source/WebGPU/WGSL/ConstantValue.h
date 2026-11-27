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

#include <cmath>
#include <optional>
#include <type_traits>
#include <wtf/CheckedArithmetic.h>
#include <wtf/FixedVector.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/StringPrintStream.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Variant.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

struct SourceSpan;
struct Type;

#if HAVE(FP16_HALF_SUPPORT)
using half = __fp16;
#else
// Wrap a struct around the supported fp16 type.
struct half {
#if PLATFORM(COCOA)
    using f16 = __fp16;
#else
    // _Float16 is the 16bit float type in C++23, and is often available
    // in compilers prior to C++23.
    using f16 = _Float16;
#endif
    half()
    {
    }

    // Constructor from an arithmetic type. Use a template here because the
    // explicit list of types may differ among platforms.
    template <typename A,
        std::enable_if_t<std::is_arithmetic_v<std::decay_t<A>>, bool> = true>
    half(const A& val)
        : value(static_cast<f16>(val)) { }

    // Constructor from a ConstantResult.
    template <typename C,
        std::enable_if_t<std::is_class_v<std::decay_t<C>>, bool> = true>
    half(const C& val)
        : value(val.value().getHalf().value) { }

    operator float() const
    {
        return static_cast<float>(value);
    }

    f16 value { 0 };
};
#endif

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
        : elements(WTF::move(elements))
    {
    }

    size_t upperBound() const { return elements.size(); }
    ConstantValue operator[](unsigned) const;

    FixedVector<ConstantValue> elements;
};

struct ConstantVector {
    ConstantVector(size_t size)
        : elements(size)
    {
    }

    ConstantVector(FixedVector<ConstantValue>&& elements)
        : elements(WTF::move(elements))
    {
    }

    size_t upperBound() const { return elements.size(); }
    ConstantValue operator[](unsigned) const;

    FixedVector<ConstantValue> elements;
};

struct ConstantMatrix {
    ConstantMatrix(uint32_t columns, uint32_t rows)
        : columns(columns)
        , rows(rows)
        , elements(columns * rows)
    {
    }

    ConstantMatrix(uint32_t columns, uint32_t rows, const FixedVector<ConstantValue>& elements)
        : columns(columns)
        , rows(rows)
        , elements(elements)
    {
        RELEASE_ASSERT(elements.size() == columns * rows);
    }

    size_t upperBound() const { return columns; }
    ConstantVector operator[](unsigned) const;

    uint32_t columns;
    uint32_t rows;
    FixedVector<ConstantValue> elements;
};

struct ConstantStruct {
    HashMap<String, ConstantValue> fields;
};

using BaseValue = Variant<float, half, double, int32_t, uint32_t, int64_t, bool, ConstantArray, ConstantVector, ConstantMatrix, ConstantStruct>;
struct ConstantValue : BaseValue {
    ConstantValue() = default;

    using BaseValue::BaseValue;

    void dump(PrintStream&) const;
    size_t extraMemoryFootprint() const;

    bool isBool() const { return std::holds_alternative<bool>(*this); }
    bool isVector() const { return std::holds_alternative<ConstantVector>(*this); }
    bool isMatrix() const { return std::holds_alternative<ConstantMatrix>(*this); }
    bool isArray() const { return std::holds_alternative<ConstantArray>(*this); }

    bool toBool() const { return std::get<bool>(*this); }

    int64_t integerValue() const
    {
        if (auto* i32 = std::get_if<int32_t>(this))
            return *i32;
        if (auto* u32 = std::get_if<uint32_t>(this))
            return *u32;
        if (auto* abstractInt = std::get_if<int64_t>(this))
            return *abstractInt;
        RELEASE_ASSERT_NOT_REACHED();
    }
    half getHalf() const
    {
        if (auto* f32 = std::get_if<float>(this))
            return *f32;
        if (auto* f64 = std::get_if<double>(this))
            return *f64;
        RELEASE_ASSERT_NOT_REACHED();
    }

    const ConstantVector& toVector() const
    {
        return std::get<ConstantVector>(*this);
    }

    size_t upperBound() const
    {
        if (auto* array = std::get_if<ConstantArray>(this))
            return array->upperBound();
        if (auto* vector = std::get_if<ConstantVector>(this))
            return vector->upperBound();
        if (auto* matrix = std::get_if<ConstantMatrix>(this))
            return matrix->upperBound();
        RELEASE_ASSERT_NOT_REACHED();
    }

    ConstantValue operator[](unsigned index) const
    {
        if (auto* array = std::get_if<ConstantArray>(this))
            return (*array)[index];
        if (auto* vector = std::get_if<ConstantVector>(this))
            return (*vector)[index];
        if (auto* matrix = std::get_if<ConstantMatrix>(this))
            return (*matrix)[index];
        RELEASE_ASSERT_NOT_REACHED();
    }
};

class OptionalConstantValue {
public:
    OptionalConstantValue() = default;
    OptionalConstantValue(std::nullopt_t) { }
    OptionalConstantValue(const ConstantValue& value)
        : m_box(Box::create(value))
    {
    }
    OptionalConstantValue(ConstantValue&& value)
        : m_box(Box::create(WTF::move(value)))
    {
    }
    OptionalConstantValue(const std::optional<ConstantValue>& value)
    {
        if (value)
            m_box = Box::create(*value);
    }
    OptionalConstantValue(std::optional<ConstantValue>&& value)
    {
        if (value)
            m_box = Box::create(WTF::move(*value));
    }
    OptionalConstantValue(const OptionalConstantValue&) = default;
    OptionalConstantValue(OptionalConstantValue&&) = default;
    OptionalConstantValue& operator=(const OptionalConstantValue&) = default;
    OptionalConstantValue& operator=(OptionalConstantValue&&) = default;

    OptionalConstantValue& operator=(std::nullopt_t)
    {
        m_box = nullptr;
        return *this;
    }

    OptionalConstantValue& operator=(const ConstantValue& value)
    {
        if (m_box && m_box->hasOneRef()) {
            m_box->value = value;
            m_box->convertedTypeAddress = 0;
        } else
            m_box = Box::create(value);
        return *this;
    }

    OptionalConstantValue& operator=(ConstantValue&& value)
    {
        if (m_box && m_box->hasOneRef()) {
            m_box->value = WTF::move(value);
            m_box->convertedTypeAddress = 0;
        } else
            m_box = Box::create(WTF::move(value));
        return *this;
    }

    explicit operator bool() const { return !!m_box; }
    bool has_value() const { return !!m_box; } // NOLINT

    const ConstantValue& operator*() const LIFETIME_BOUND { RELEASE_ASSERT(m_box); return m_box->value; }
    const ConstantValue* operator->() const LIFETIME_BOUND { RELEASE_ASSERT(m_box); return &m_box->value; }
    const ConstantValue& value() const LIFETIME_BOUND { RELEASE_ASSERT(m_box); return m_box->value; }

    ConstantValue& mutableValue() LIFETIME_BOUND
    {
        RELEASE_ASSERT(m_box);
        if (!m_box->hasOneRef())
            m_box = Box::create(m_box->value);
        else
            m_box->convertedTypeAddress = 0;
        return m_box->value;
    }

    bool isConvertedTo(const Type* type) const { return m_box && type && m_box->convertedTypeAddress == reinterpret_cast<uintptr_t>(type); }
    void setConvertedType(const Type* type)
    {
        if (m_box)
            m_box->convertedTypeAddress = reinterpret_cast<uintptr_t>(type);
    }

    const void* storageIdentity() const { return m_box.get(); }
    size_t storageMemoryFootprint() const { return m_box ? sizeof(Box) + m_box->value.extraMemoryFootprint() : 0; }

    operator std::optional<ConstantValue>() const
    {
        if (!m_box)
            return std::nullopt;
        return m_box->value;
    }

    void dump(PrintStream& out) const
    {
        if (m_box)
            m_box->value.dump(out);
        else
            out.print("<nullopt>");
    }

private:
    struct Box : ThreadSafeRefCounted<Box> {
        static Ref<Box> create(const ConstantValue& value) { return adoptRef(*new Box(value)); }
        static Ref<Box> create(ConstantValue&& value) { return adoptRef(*new Box(WTF::move(value))); }
        explicit Box(const ConstantValue& value)
            : value(value)
        {
        }
        explicit Box(ConstantValue&& value)
            : value(WTF::move(value))
        {
        }
        ConstantValue value;
        uintptr_t convertedTypeAddress { 0 };
    };

    RefPtr<Box> m_box;
};
static_assert(sizeof(OptionalConstantValue) == sizeof(void*));

template<typename To, typename From>
std::optional<To> convertInteger(From value)
{
    auto result = Checked<To, RecordOverflow>(value);
    if (result.hasOverflowed()) [[unlikely]]
        return std::nullopt;
    return { result.value() };
}

template<typename To, typename From>
std::optional<To> convertFloat(From value)
{
    static_assert(std::is_floating_point<To>::value || std::is_same<To, half>::value, "Result type is expected to be a floating point type: double, float, or half");

    To max;
    To lowest;
    if constexpr (std::is_floating_point<To>::value) {
        max = std::numeric_limits<To>::max();
        lowest = std::numeric_limits<To>::lowest();
    } else {
        max = 0x1.ffcp15;
        lowest = -max;
    }

    if (value > max)
        return std::nullopt;
    if (value < lowest)
        return std::nullopt;
    if (std::isnan(value))
        return std::nullopt;

    return { value };
}

bool convertValueImpl(const SourceSpan&, const Type*, ConstantValue&);

} // namespace WGSL

namespace WTF {

template<> class StringTypeAdapter<WGSL::ConstantValue> {
public:
    StringTypeAdapter(const WGSL::ConstantValue& value)
    {
        StringPrintStream valueString;
        value.dump(valueString);
        m_string = valueString.toString();
    }

    unsigned length() const { return m_string.length(); }
    bool is8Bit() const { return m_string.is8Bit(); }
    template<typename CharacterType>
    void writeTo(std::span<CharacterType> destination) const
    {
        StringView { m_string }.getCharacters(destination);
        WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
    }

private:
    String m_string;
};

} // namespace WTF
