/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveNumericTypes.h"
#include "CalculationValue.h"
#include "FloatConversion.h"
#include "StyleNone.h"
#include "StyleValueTypes.h"
#include <variant>
#include <wtf/Forward.h>

namespace WebCore {
namespace Style {

// Concept for use in generic contexts to filter on Numeric Style types.
template<typename T> concept StyleNumeric = requires {
    typename T::CSS;
    typename T::Raw;
};

// Concept for use in generic contexts to filter on base, single type
// primitives, excluding "percentage-dimension" types.
template<typename T> concept StyleNumericPrimitive = StyleNumeric<T> && requires {
    T::unit;
};

// Concept for use in generic contexts to filter on "percentage-dimension"
// types such as `<length-percentage>`.
template<typename T> concept StylePercentageDimension = StyleNumeric<T> && requires {
    T::isPercentageDimension;
};

// MARK: Number Primitive

template<CSS::Range R = CSS::All> struct Number {
    static constexpr auto range = R;
    static constexpr auto unit = CSSUnitType::CSS_NUMBER;
    using CSS = WebCore::CSS::Number<R>;
    using Raw = WebCore::CSS::NumberRaw<R>;

    double value { 0 };

    constexpr bool operator==(const Number<R>&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

// MARK: Percentage Primitive

template<CSS::Range R = CSS::All> struct Percentage {
    static constexpr auto range = R;
    static constexpr auto unit = CSSUnitType::CSS_PERCENTAGE;
    using CSS = WebCore::CSS::Percentage<R>;
    using Raw = WebCore::CSS::PercentageRaw<R>;

    double value { 0 };

    constexpr bool operator==(const Percentage<R>&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

// MARK: Dimension Primitives

template<CSS::Range R = CSS::All> struct Angle {
    static constexpr auto range = R;
    static constexpr auto unit = CSSUnitType::CSS_DEG;
    using CSS = WebCore::CSS::Angle<R>;
    using Raw = WebCore::CSS::AngleRaw<R>;

    double value { 0 };

    constexpr bool operator==(const Angle<R>&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

template<CSS::Range R = CSS::All> struct Length {
    static constexpr auto range = R;
    static constexpr auto unit = CSSUnitType::CSS_PX;
    using CSS = WebCore::CSS::Length<R>;
    using Raw = WebCore::CSS::LengthRaw<R>;

    float value { 0 };
    bool quirk { false };

    constexpr bool hasQuirk() const { return quirk; }
    constexpr bool operator==(const Length<R>&) const = default;
    constexpr bool operator==(float other) const { return value == other; };
};

template<CSS::Range R = CSS::All> struct Time {
    static constexpr auto range = R;
    static constexpr auto unit = CSSUnitType::CSS_S;
    using CSS = WebCore::CSS::Time<R>;
    using Raw = WebCore::CSS::TimeRaw<R>;

    double value { 0 };

    constexpr bool operator==(const Time<R>&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

template<CSS::Range R = CSS::All> struct Frequency {
    static constexpr auto range = R;
    static constexpr auto unit = CSSUnitType::CSS_HZ;
    using CSS = WebCore::CSS::Frequency<R>;
    using Raw = WebCore::CSS::FrequencyRaw<R>;

    double value { 0 };

    constexpr bool operator==(const Frequency<R>&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

template<CSS::Range R = CSS::Nonnegative> struct Resolution {
    static_assert(R.min >= 0, "<resolution> values must always have a minimum range of at least 0.");
    static constexpr auto range = R;
    static constexpr auto unit = CSSUnitType::CSS_DPPX;
    using CSS = WebCore::CSS::Resolution<R>;
    using Raw = WebCore::CSS::ResolutionRaw<R>;

    double value { 0 };

    constexpr bool operator==(const Resolution<R>&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};


template<CSS::Range R = CSS::All> struct Flex {
    static constexpr auto range = R;
    static constexpr auto unit = CSSUnitType::CSS_FR;
    using CSS = WebCore::CSS::Flex<R>;
    using Raw = WebCore::CSS::FlexRaw<R>;

    double value { 0 };

    constexpr bool operator==(const Flex<R>&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

// MARK: Dimension + Percentage Primitives

// Compact representation of std::variant<Angle, Percentage, Ref<CalculationValue>> that takes
// up only 64 bits. Utilizes the knowledge that pointers are at most 48 bits, allowing for the
// storage of the type tag in the remaining space.
// FIXME: Abstract into a generic CompactPointerVariant type.
template<CSS::Range R = CSS::All> class AnglePercentageValue {
public:
    enum class Tag : uint8_t { Angle, Percentage, CalculationValue };

    constexpr AnglePercentageValue(Angle<R> angle)
        : m_data { encodedPayload(angle) | encodedTag(Tag::Angle) }
    {
    }

    constexpr AnglePercentageValue(Percentage<R> percentage)
        : m_data { encodedPayload(percentage) | encodedTag(Tag::Percentage) }
    {
    }

    AnglePercentageValue(Ref<CalculationValue>&& calculationValue)
        : m_data { encodedPayload(WTFMove(calculationValue)) | encodedTag(Tag::CalculationValue) }
    {
    }

    AnglePercentageValue(Calculation::Child&& root)
        : AnglePercentageValue(makeCalculationValue(WTFMove(root)))
    {
    }

    AnglePercentageValue(const AnglePercentageValue<R>& other)
        : m_data { other.m_data }
    {
        if (tag() == Tag::CalculationValue)
            refCalculationValue(); // Balanced by deref() in destructor.
    }

    AnglePercentageValue(AnglePercentageValue<R>&& other)
    {
        *this = WTFMove(other);
    }

    AnglePercentageValue<R>& operator=(const AnglePercentageValue<R>& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            derefCalculationValue();

        m_data = other.m_data;

        if (tag() == Tag::CalculationValue)
            refCalculationValue();

        return *this;
    }

    AnglePercentageValue<R>& operator=(AnglePercentageValue<R>&& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            derefCalculationValue();

        m_data = other.m_data;
        other.m_data = movedFromValue();

        return *this;
    }

    ~AnglePercentageValue()
    {
        if (tag() == Tag::CalculationValue)
            derefCalculationValue(); // Balanced by leakRef() in encodedCalculationValue.
    }

    constexpr Tag tag() const
    {
        return decodedTag(m_data);
    }

    constexpr bool isAngle() const
    {
        return tag() == Tag::Angle;
    }

    constexpr bool isPercentage() const
    {
        return tag() == Tag::Percentage;
    }

    constexpr bool isCalculationValue() const
    {
        return tag() == Tag::CalculationValue;
    }

    constexpr Angle<R> asAngle() const
    {
        ASSERT(tag() == Tag::Angle);
        return decodedAngle(m_data);
    }

    constexpr Percentage<R> asPercentage() const
    {
        ASSERT(tag() == Tag::Percentage);
        return decodedPercentage(m_data);
    }

    Ref<CalculationValue> asCalculationValue() const
    {
        ASSERT(tag() == Tag::CalculationValue);
        return decodedCalculationValue(m_data);
    }

    template<typename F> decltype(auto) visit(F&& functor) const
    {
        switch (tag()) {
        case Tag::Angle:
            return std::invoke(std::forward<F>(functor), asAngle());
        case Tag::Percentage:
            return std::invoke(std::forward<F>(functor), asPercentage());
        case Tag::CalculationValue:
            return std::invoke(std::forward<F>(functor), asCalculationValue());
        }
        WTF_UNREACHABLE();
    }

    template<typename... F> decltype(auto) switchOn(F&&... functors) const
    {
        return visit(WTF::makeVisitor(std::forward<F>(functors)...));
    }

    constexpr bool isZero() const
    {
        switch (tag()) {
        case Tag::Angle:
            return asAngle().value == 0;
        case Tag::Percentage:
            return asPercentage().value == 0;
        case Tag::CalculationValue:
            return false;
        }
        WTF_UNREACHABLE();
    }

    constexpr bool operator==(const AnglePercentageValue<R>&) const = default;

private:
#if CPU(ADDRESS64)
    static constexpr unsigned maxNumberOfBitsInPointer = 48;
#else
    static constexpr unsigned maxNumberOfBitsInPointer = 32;
#endif
    static constexpr uint64_t calculationValueSize = maxNumberOfBitsInPointer;
    static constexpr uint64_t calculationValueMask = (1ULL << calculationValueSize) - 1;
    static constexpr uint64_t angleSize = sizeof(float) * 8;
    static constexpr uint64_t angleMask = (1ULL << angleSize) - 1;
    static constexpr uint64_t percentageSize = sizeof(float) * 8;
    static constexpr uint64_t percentageMask = (1ULL << percentageSize) - 1;
    static constexpr uint64_t tagSize = sizeof(Tag) * 8;
    static constexpr uint64_t tagShift = std::max({ calculationValueSize, angleSize, percentageSize });
    static_assert(tagShift + tagSize <= 64);

    static constexpr uint64_t movedFromValue()
    {
        return encodedPayload(Angle<R> { 0 }) | encodedTag(Tag::Angle);
    }

    static uint64_t encodedPayload(Ref<CalculationValue>&& calculationValue)
    {
        return encodedCalculationValue(WTFMove(calculationValue));
    }

    static constexpr uint64_t encodedPayload(Angle<R> angle)
    {
        return encodedAngle(angle);
    }

    static constexpr uint64_t encodedPayload(Percentage<R> percentage)
    {
        return encodedPercentage(percentage);
    }

    static constexpr uint64_t encodedTag(Tag tag)
    {
        return static_cast<uint64_t>(tag) << tagShift;
    }

    static constexpr Tag decodedTag(uint64_t value)
    {
        return static_cast<Tag>(static_cast<uint8_t>(value >> tagShift));
    }

    void refCalculationValue()
    {
        ASSERT(tag() == Tag::CalculationValue);
#if CPU(ADDRESS64)
        std::bit_cast<CalculationValue*>(m_data & calculationValueMask)->ref();
#else
        std::bit_cast<CalculationValue*>(static_cast<uint32_t>(m_data & calculationValueMask))->ref();
#endif
    }

    void derefCalculationValue()
    {
        ASSERT(tag() == Tag::CalculationValue);
#if CPU(ADDRESS64)
        std::bit_cast<CalculationValue*>(m_data & calculationValueMask)->deref();
#else
        std::bit_cast<CalculationValue*>(static_cast<uint32_t>(m_data & calculationValueMask))->deref();
#endif
    }

    // Payload specific coding.

    static uint64_t encodedCalculationValue(Ref<CalculationValue>&& calculationValue)
    {
#if CPU(ADDRESS64)
        return std::bit_cast<uint64_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#else
        return std::bit_cast<uint32_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#endif
    }

    static Ref<CalculationValue> decodedCalculationValue(uint64_t value)
    {
#if CPU(ADDRESS64)
        Ref<CalculationValue> calculation = *std::bit_cast<CalculationValue*>(value & calculationValueMask);
#else
        Ref<CalculationValue> calculation = *std::bit_cast<CalculationValue*>(static_cast<uint32_t>(value & calculationValueMask));
#endif
        return calculation;
    }

    static constexpr uint64_t encodedAngle(Angle<R> angle)
    {
        return static_cast<uint64_t>(std::bit_cast<uint32_t>(narrowPrecisionToFloat(angle.value)));
    }

    static constexpr Angle<R> decodedAngle(uint64_t value)
    {
        return { std::bit_cast<float>(static_cast<uint32_t>(value & angleMask)) };
    }

    static constexpr uint64_t encodedPercentage(Percentage<R> percentage)
    {
        return static_cast<uint64_t>(std::bit_cast<uint32_t>(narrowPrecisionToFloat(percentage.value)));
    }

    static constexpr Percentage<R> decodedPercentage(uint64_t value)
    {
        return { std::bit_cast<float>(static_cast<uint32_t>(value & percentageMask)) };
    }

    static Ref<CalculationValue> makeCalculationValue(Calculation::Child&& root)
    {
        return CalculationValue::create(
            Calculation::Tree {
                .root = WTFMove(root),
                .category = Calculation::Category::AnglePercentage,
                .range = { R.min, R.max }
            }
        );
    }

    uint64_t m_data { 0 };
};

template<CSS::Range R = CSS::All> struct AnglePercentage {
    static constexpr bool isPercentageDimension = true;
    static constexpr auto range = R;
    using CSS = WebCore::CSS::AnglePercentage<R>;
    using Raw = WebCore::CSS::AnglePercentageRaw<R>;
    using Dimension = Angle<R>;

    AnglePercentageValue<R> value;

    AnglePercentage(Angle<R> length)
        : value { WTFMove(length) }
    {
    }

    AnglePercentage(Percentage<R> percentage)
        : value { WTFMove(percentage) }
    {
    }

    AnglePercentage(Ref<CalculationValue> calculationValue)
        : value { WTFMove(calculationValue) }
    {
    }

    AnglePercentage(Calculation::Child calculationValue)
        : value { WTFMove(calculationValue) }
    {
    }

    bool isPercentage() const { return value.isPercentage(); }
    bool isAngle() const { return value.isAngle(); }
    bool isCalculationValue() const { return value.isCalculationValue(); }

    constexpr bool operator==(const AnglePercentage<R>&) const = default;
};

// Compact representation of std::variant<Length, Percentage, Ref<CalculationValue>> that takes
// up only 64 bits. Utilizes the knowledge that pointers are at most 48 bits, allowing for the
// storage of the type tag in the remaining space.
// FIXME: Abstract into a generic CompactPointerVariant type.
template<CSS::Range R = CSS::All> class LengthPercentageValue {
public:
    enum class Tag : uint8_t { Length, Percentage, CalculationValue };

    constexpr LengthPercentageValue(Length<R> length)
        : m_data { encodedPayload(length) | encodedTag(Tag::Length) }
    {
    }

    constexpr LengthPercentageValue(Percentage<R> percentage)
        : m_data { encodedPayload(percentage) | encodedTag(Tag::Percentage) }
    {
    }

    LengthPercentageValue(Ref<CalculationValue>&& calculationValue)
        : m_data { encodedPayload(WTFMove(calculationValue)) | encodedTag(Tag::CalculationValue) }
    {
    }

    LengthPercentageValue(Calculation::Child&& root)
        : LengthPercentageValue(makeCalculationValue(WTFMove(root)))
    {
    }

    LengthPercentageValue(const LengthPercentageValue<R>& other)
        : m_data { other.m_data }
    {
        if (tag() == Tag::CalculationValue)
            refCalculationValue(); // Balanced by deref() in destructor.
    }

    LengthPercentageValue(LengthPercentageValue<R>&& other)
    {
        *this = WTFMove(other);
    }

    LengthPercentageValue<R>& operator=(const LengthPercentageValue<R>& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            derefCalculationValue();

        m_data = other.m_data;

        if (tag() == Tag::CalculationValue)
            refCalculationValue();

        return *this;
    }

    LengthPercentageValue<R>& operator=(LengthPercentageValue<R>&& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            derefCalculationValue();

        m_data = other.m_data;
        other.m_data = movedFromValue();

        return *this;
    }

    ~LengthPercentageValue()
    {
        if (tag() == Tag::CalculationValue)
            derefCalculationValue(); // Balanced by leakRef() in encodedCalculationValue.
    }

    constexpr Tag tag() const
    {
        return decodedTag(m_data);
    }

    constexpr bool isLength() const
    {
        return tag() == Tag::Length;
    }

    constexpr bool isPercentage() const
    {
        return tag() == Tag::Percentage;
    }

    constexpr bool isCalculationValue() const
    {
        return tag() == Tag::CalculationValue;
    }

    constexpr Length<R> asLength() const
    {
        ASSERT(tag() == Tag::Length);
        return decodedLength(m_data);
    }

    constexpr Percentage<R> asPercentage() const
    {
        ASSERT(tag() == Tag::Percentage);
        return decodedPercentage(m_data);
    }

    Ref<CalculationValue> asCalculationValue() const
    {
        ASSERT(tag() == Tag::CalculationValue);
        return decodedCalculationValue(m_data);
    }

    template<typename F> decltype(auto) visit(F&& functor) const
    {
        switch (tag()) {
        case Tag::Length:
            return std::invoke(std::forward<F>(functor), asLength());
        case Tag::Percentage:
            return std::invoke(std::forward<F>(functor), asPercentage());
        case Tag::CalculationValue:
            return std::invoke(std::forward<F>(functor), asCalculationValue());
        }
        WTF_UNREACHABLE();
    }

    template<typename... F> decltype(auto) switchOn(F&&... functors) const
    {
        return visit(WTF::makeVisitor(std::forward<F>(functors)...));
    }

    constexpr bool isZero() const
    {
        switch (tag()) {
        case Tag::Length:
            return asLength().value == 0;
        case Tag::Percentage:
            return asPercentage().value == 0;
        case Tag::CalculationValue:
            return false;
        }
        WTF_UNREACHABLE();
    }

    bool operator==(const LengthPercentageValue<R>&) const = default;

private:
#if CPU(ADDRESS64)
    static constexpr unsigned maxNumberOfBitsInPointer = 48;
#else
    static constexpr unsigned maxNumberOfBitsInPointer = 32;
#endif
    static constexpr uint64_t calculationValueSize = maxNumberOfBitsInPointer;
    static constexpr uint64_t calculationValueMask = (1ULL << calculationValueSize) - 1;
    static constexpr uint64_t lengthValueSize = sizeof(float) * 8;
    static constexpr uint64_t lengthValueMask = (1ULL << lengthValueSize) - 1;
    static constexpr uint64_t lengthQuirkSize = sizeof(bool) * 8;
    static constexpr uint64_t lengthQuirkShift = lengthValueSize;
    static constexpr uint64_t lengthSize = lengthValueSize + lengthQuirkSize;
    static constexpr uint64_t percentageSize = sizeof(float) * 8;
    static constexpr uint64_t percentageMask = (1ULL << percentageSize) - 1;
    static constexpr uint64_t tagSize = sizeof(Tag) * 8;
    static constexpr uint64_t tagShift = std::max({ calculationValueSize, lengthSize, percentageSize });
    static_assert(tagShift + tagSize <= 64);

    static constexpr uint64_t movedFromValue()
    {
        return encodedPayload(Length<R> { 0 }) | encodedTag(Tag::Length);
    }

    static uint64_t encodedPayload(Ref<CalculationValue>&& calculationValue)
    {
        return encodedCalculationValue(WTFMove(calculationValue));
    }

    static constexpr uint64_t encodedPayload(Length<R> length)
    {
        return encodedLength(length);
    }

    static constexpr uint64_t encodedPayload(Percentage<R> percentage)
    {
        return encodedPercentage(percentage);
    }

    static constexpr uint64_t encodedTag(Tag tag)
    {
        return static_cast<uint64_t>(tag) << tagShift;
    }

    static constexpr Tag decodedTag(uint64_t value)
    {
        return static_cast<Tag>(static_cast<uint8_t>(value >> tagShift));
    }

    void refCalculationValue()
    {
        ASSERT(tag() == Tag::CalculationValue);
#if CPU(ADDRESS64)
        std::bit_cast<CalculationValue*>(m_data & calculationValueMask)->ref();
#else
        std::bit_cast<CalculationValue*>(static_cast<uint32_t>(m_data & calculationValueMask))->ref();
#endif
    }

    void derefCalculationValue()
    {
        ASSERT(tag() == Tag::CalculationValue);
#if CPU(ADDRESS64)
        std::bit_cast<CalculationValue*>(m_data & calculationValueMask)->deref();
#else
        std::bit_cast<CalculationValue*>(static_cast<uint32_t>(m_data & calculationValueMask))->deref();
#endif
    }

    // Payload specific coding.

    static uint64_t encodedCalculationValue(Ref<CalculationValue>&& calculationValue)
    {
#if CPU(ADDRESS64)
        return std::bit_cast<uint64_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#else
        return std::bit_cast<uint32_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#endif
    }

    static Ref<CalculationValue> decodedCalculationValue(uint64_t value)
    {
#if CPU(ADDRESS64)
        Ref<CalculationValue> calculation = *std::bit_cast<CalculationValue*>(value & calculationValueMask);
#else
        Ref<CalculationValue> calculation = *std::bit_cast<CalculationValue*>(static_cast<uint32_t>(value & calculationValueMask));
#endif
        return calculation;
    }

    static constexpr uint64_t encodedLength(Length<R> length)
    {
        return static_cast<uint64_t>(std::bit_cast<uint32_t>(length.value)) | (static_cast<uint64_t>(length.quirk) << lengthQuirkShift);
    }

    static constexpr Length<R> decodedLength(uint64_t value)
    {
        return { std::bit_cast<float>(static_cast<uint32_t>(value & lengthValueMask)), static_cast<bool>(value >> lengthQuirkShift) };
    }

    static constexpr uint64_t encodedPercentage(Percentage<R> percentage)
    {
        return static_cast<uint64_t>(std::bit_cast<uint32_t>(narrowPrecisionToFloat(percentage.value)));
    }

    static constexpr Percentage<R> decodedPercentage(uint64_t value)
    {
        return { std::bit_cast<float>(static_cast<uint32_t>(value & percentageMask)) };
    }

    static Ref<CalculationValue> makeCalculationValue(Calculation::Child&& root)
    {
        return CalculationValue::create(
            Calculation::Tree {
                .root = WTFMove(root),
                .category = Calculation::Category::LengthPercentage,
                .range = { R.min, R.max }
            }
        );
    }

    uint64_t m_data { 0 };
};

template<CSS::Range R = CSS::All> struct LengthPercentage {
    static constexpr bool isPercentageDimension = true;
    static constexpr auto range = R;
    using CSS = WebCore::CSS::LengthPercentage<R>;
    using Raw = WebCore::CSS::LengthPercentageRaw<R>;
    using Dimension = Length<R>;

    LengthPercentageValue<R> value;

    LengthPercentage(Length<R> length)
        : value { WTFMove(length) }
    {
    }

    LengthPercentage(Percentage<R> percentage)
        : value { WTFMove(percentage) }
    {
    }

    LengthPercentage(Ref<CalculationValue> calculationValue)
        : value { WTFMove(calculationValue) }
    {
    }

    LengthPercentage(Calculation::Child calculationValue)
        : value { WTFMove(calculationValue) }
    {
    }

    // CalculatedValue is intentionally not part of IPCData.
    using IPCData = std::variant<Length<R>, Percentage<R>>;
    LengthPercentage(IPCData&& data)
        : value { WTF::switchOn(data, [&](auto&& data) -> LengthPercentageValue<R> { return { data }; }) }
    {
    }

    IPCData ipcData() const
    {
        switch (value.tag()) {
        case LengthPercentageValue<R>::Tag::Length:
            return value.asLength();
        case LengthPercentageValue<R>::Tag::Percentage:
            return value.asPercentage();
        case LengthPercentageValue<R>::Tag::CalculationValue:
            ASSERT_NOT_REACHED();
            return Length<R> { 0 };
        }
        WTF_UNREACHABLE();
    }

    bool hasQuirk() const { return value.isLength() && value.asLength().hasQuirk(); }

    bool isPercentage() const { return value.isPercentage(); }
    bool isLength() const { return value.isLength(); }
    bool isCalculationValue() const { return value.isCalculationValue(); }

    constexpr bool operator==(const LengthPercentage<R>&) const = default;
};

// MARK: - Conversion to `Calculation::Child`.

inline Calculation::Child copyCalculation(Ref<CalculationValue> value)
{
    return value->copyRoot();
}

template<auto R> Calculation::Child copyCalculation(const Number<R>& value)
{
    return Calculation::number(value.value);
}

template<auto R> Calculation::Child copyCalculation(const Percentage<R>& value)
{
    return Calculation::percentage(value.value);
}

template<StyleNumericPrimitive Dimension> Calculation::Child copyCalculation(const Dimension& value)
{
    return Calculation::dimension(value.value);
}

template<StylePercentageDimension PercentageDimension> Calculation::Child copyCalculation(const PercentageDimension& value)
{
    return value.value.switchOn([](const auto& value) { return copyCalculation(value); });
}

// MARK: Additional Common Type and Groupings

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
using NumberOrPercentage = std::variant<Number<>, Percentage<>>;

struct NumberOrPercentageResolvedToNumber {
    double value { 0 };

    constexpr NumberOrPercentageResolvedToNumber(double value)
        : value { value }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Number<> number)
        : value { number.value }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Percentage<> percentage)
        : value { percentage.value / 100.0 }
    {
    }

    constexpr bool operator==(const NumberOrPercentageResolvedToNumber&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

// Standard Numbers
using NumberAll = Number<CSS::All>;
using NumberNonnegative = Number<CSS::Nonnegative>;

// Standard Angles
using AngleAll = Length<CSS::All>;

// Standard Lengths
using LengthAll = Length<CSS::All>;
using LengthNonnegative = Length<CSS::Nonnegative>;

// Standard LengthPercentages
using LengthPercentageAll = LengthPercentage<CSS::All>;
using LengthPercentageNonnegative = LengthPercentage<CSS::Nonnegative>;

// Standard Percentages
using Percentage0To100 = LengthPercentage<CSS::Range{0,100}>;

// Standard Points
using LengthPercentagePointAll = Point<LengthPercentageAll>;
using LengthPercentagePointNonnegative = Point<LengthPercentageNonnegative>;

// Standard Sizes
using LengthPercentageSizeAll = Size<LengthPercentageAll>;
using LengthPercentageSizeNonnegative = Size<LengthPercentageNonnegative>;

} // namespace Style

namespace CSS {
namespace TypeTransform {

// MARK: - Type transforms

namespace Type {

// MARK: CSS type -> Style type mapping (Style type -> CSS type directly available via typename StyleType::CSS)

template<typename> struct ToStyleMapping;
template<auto R> struct ToStyleMapping<Number<R>> { using Style = Style::Number<R>; };
template<auto R> struct ToStyleMapping<Percentage<R>> { using Style = Style::Percentage<R>; };
template<auto R> struct ToStyleMapping<Angle<R>> { using Style = Style::Angle<R>; };
template<auto R> struct ToStyleMapping<Length<R>> { using Style = Style::Length<R>; };
template<auto R> struct ToStyleMapping<Time<R>> { using Style = Style::Time<R>; };
template<auto R> struct ToStyleMapping<Frequency<R>> { using Style = Style::Frequency<R>; };
template<auto R> struct ToStyleMapping<Resolution<R>> { using Style = Style::Resolution<R>; };
template<auto R> struct ToStyleMapping<Flex<R>> { using Style = Style::Flex<R>; };
template<auto R> struct ToStyleMapping<AnglePercentage<R>> { using Style = Style::AnglePercentage<R>; };
template<auto R> struct ToStyleMapping<LengthPercentage<R>> { using Style = Style::LengthPercentage<R>; };
template<> struct ToStyleMapping<None> { using Style = Style::None; };

// MARK: Transform CSS type -> Style type.

// Transform `css1`  -> `style1`
template<typename T> struct CSSToStyleLazy {
    using type = typename ToStyleMapping<T>::Style;
};
template<typename T> using CSSToStyle = typename CSSToStyleLazy<T>::type;

// MARK: Transform Raw type -> Style type.

// Transform `raw1`  -> `style1`
template<typename T> struct RawToStyleLazy {
    using type = typename ToStyleMapping<typename RawToCSSMapping<T>::CSS>::Style;
};
template<typename T> using RawToStyle = typename RawToStyleLazy<T>::type;

} // namespace Type

namespace List {

// MARK: - List transforms

// MARK: Transform CSS type list -> Style type list.

// Transform `brigand::list<css1, css2, ...>`  -> `brigand::list<style1, style2, ...>`
template<typename TypeList> struct CSSToStyleLazy {
    using type = brigand::transform<TypeList, Type::CSSToStyleLazy<brigand::_1>>;
};
template<typename TypeList> using CSSToStyle = typename CSSToStyleLazy<TypeList>::type;

// MARK: Transform Raw type list -> Style type list.

// Transform `brigand::list<raw1, raw2, ...>`  -> `brigand::list<style1, style2, ...>`
template<typename TypeList> struct RawToStyleLazy {
    using type = brigand::transform<TypeList, Type::RawToStyleLazy<brigand::_1>>;
};
template<typename TypeList> using RawToStyle = typename RawToStyleLazy<TypeList>::type;

} // namespace List
} // namespace TypeTransform

} // namespace CSS
} // namespace WebCore
