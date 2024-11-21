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

    // NOTE: Unlike the other primitive numeric types, Length<> uses a `float`, not a `double` for its value type.
    float value { 0 };

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

template<typename T> struct PrimitivePercentageDimensionCalculationCategory;

template<auto R> struct PrimitivePercentageDimensionCalculationCategory<Angle<R>> {
    static constexpr auto category = Calculation::Category::AnglePercentage;
};

template<auto R> struct PrimitivePercentageDimensionCalculationCategory<Length<R>> {
    static constexpr auto category = Calculation::Category::LengthPercentage;
};

// Compact representation of std::variant<Percentage<R>, Dimension<R>, Ref<CalculationValue>> that
// takes up only 64 bits. Utilizes the knowledge that pointers are at most 48 bits, allowing for the
// storage of the type tag in the remaining space.
//
// FIXME: Abstract into a generic CompactPointerVariant type.

template<typename D> class PrimitivePercentageDimension {
public:
    static constexpr auto R = D::range;

    enum class Tag : uint8_t { Percentage, Dimension, CalculationValue };

    constexpr PrimitivePercentageDimension(Percentage<R> percentage)
        : m_data { encodedPayload(percentage) | encodedTag(Tag::Percentage) }
    {
    }

    constexpr PrimitivePercentageDimension(D dimension)
        : m_data { encodedPayload(dimension) | encodedTag(Tag::Dimension) }
    {
    }


    PrimitivePercentageDimension(Ref<CalculationValue>&& calculationValue)
        : m_data { encodedPayload(WTFMove(calculationValue)) | encodedTag(Tag::CalculationValue) }
    {
    }

    PrimitivePercentageDimension(Calculation::Child&& root)
        : PrimitivePercentageDimension(makeCalculationValue(WTFMove(root)))
    {
    }

    PrimitivePercentageDimension(const PrimitivePercentageDimension<D>& other)
        : m_data { other.m_data }
    {
        if (tag() == Tag::CalculationValue)
            refCalculationValue(); // Balanced by deref() in destructor.
    }

    PrimitivePercentageDimension(PrimitivePercentageDimension<D>&& other)
    {
        *this = WTFMove(other);
    }

    PrimitivePercentageDimension<D>& operator=(const PrimitivePercentageDimension<D>& other)
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

    PrimitivePercentageDimension<D>& operator=(PrimitivePercentageDimension<D>&& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            derefCalculationValue();

        m_data = other.m_data;
        other.m_data = movedFromValue();

        return *this;
    }

    ~PrimitivePercentageDimension()
    {
        if (tag() == Tag::CalculationValue)
            derefCalculationValue(); // Balanced by leakRef() in encodedCalculationValue.
    }

    constexpr Tag tag() const
    {
        return decodedTag(m_data);
    }

    constexpr bool isPercentage() const
    {
        return tag() == Tag::Percentage;
    }

    constexpr bool isDimension() const
    {
        return tag() == Tag::Dimension;
    }

    constexpr bool isCalculationValue() const
    {
        return tag() == Tag::CalculationValue;
    }

    constexpr Percentage<R> asPercentage() const
    {
        ASSERT(tag() == Tag::Percentage);
        return decodedPercentage(m_data);
    }

    constexpr D asDimension() const
    {
        ASSERT(tag() == Tag::Dimension);
        return decodedDimension(m_data);
    }

    Ref<CalculationValue> asCalculationValue() const
    {
        ASSERT(tag() == Tag::CalculationValue);
        return decodedCalculationValue(m_data);
    }

    template<typename F> decltype(auto) visit(F&& functor) const
    {
        switch (tag()) {
        case Tag::Percentage:
            return std::invoke(std::forward<F>(functor), asPercentage());
        case Tag::Dimension:
            return std::invoke(std::forward<F>(functor), asDimension());
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
        case Tag::Percentage:
            return asPercentage().value == 0;
        case Tag::Dimension:
            return asDimension().value == 0;
        case Tag::CalculationValue:
            return false;
        }
        WTF_UNREACHABLE();
    }

    bool operator==(const PrimitivePercentageDimension<D>& other) const
    {
        if (tag() != other.tag())
            return false;

        switch (tag()) {
        case Tag::Percentage:
            return asPercentage() == other.asPercentage();
        case Tag::Dimension:
            return asDimension() == other.asDimension();
        case Tag::CalculationValue:
            return asCalculationValue().get() == other.asCalculationValue().get();
        }
        WTF_UNREACHABLE();
    }

private:
#if CPU(ADDRESS64)
    static constexpr unsigned maxNumberOfBitsInPointer = 48;
#else
    static constexpr unsigned maxNumberOfBitsInPointer = 32;
#endif
    static constexpr uint64_t calculationValueSize = maxNumberOfBitsInPointer;
    static constexpr uint64_t calculationValueMask = (1ULL << calculationValueSize) - 1;
    static constexpr uint64_t percentageSize = sizeof(float) * 8;
    static constexpr uint64_t percentageMask = (1ULL << percentageSize) - 1;
    static constexpr uint64_t dimensionSize = sizeof(float) * 8;
    static constexpr uint64_t dimensionMask = (1ULL << dimensionSize) - 1;
    static constexpr uint64_t tagSize = sizeof(Tag) * 8;
    static constexpr uint64_t tagShift = std::max({ percentageSize, dimensionSize, calculationValueSize });
    static_assert(tagShift + tagSize <= 64);

    static constexpr uint64_t movedFromValue()
    {
        return encodedPayload(D { 0 }) | encodedTag(Tag::Dimension);
    }

    static constexpr uint64_t encodedPayload(Percentage<R> percentage)
    {
        return static_cast<uint64_t>(std::bit_cast<uint32_t>(clampTo<float>(percentage.value)));
    }

    static constexpr uint64_t encodedPayload(D dimension)
    {
        return static_cast<uint64_t>(std::bit_cast<uint32_t>(clampTo<float>(dimension.value)));
    }

    static uint64_t encodedPayload(Ref<CalculationValue>&& calculationValue)
    {
#if CPU(ADDRESS64)
        return std::bit_cast<uint64_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#else
        return std::bit_cast<uint32_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#endif
    }

    static constexpr uint64_t encodedTag(Tag tag)
    {
        return static_cast<uint64_t>(tag) << tagShift;
    }

    static constexpr Tag decodedTag(uint64_t value)
    {
        return static_cast<Tag>(static_cast<uint8_t>(value >> tagShift));
    }

    // Payload specific decoding.

    static constexpr Percentage<R> decodedPercentage(uint64_t value)
    {
        return { std::bit_cast<float>(static_cast<uint32_t>(value & percentageMask)) };
    }

    static constexpr D decodedDimension(uint64_t value)
    {
        return { std::bit_cast<float>(static_cast<uint32_t>(value & dimensionMask)) };
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

    // CalculationValue specific handling.

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

    static Ref<CalculationValue> makeCalculationValue(Calculation::Child&& root)
    {
        return CalculationValue::create(
            Calculation::Tree {
                .root = WTFMove(root),
                .category = PrimitivePercentageDimensionCalculationCategory<D>::category,
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

    PrimitivePercentageDimension<Angle<R>> value;

    AnglePercentage(Angle<R> angle)
        : value { WTFMove(angle) }
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
    bool isAngle() const { return value.isDimension(); }
    bool isCalculationValue() const { return value.isCalculationValue(); }

    auto asPercentage() const -> Percentage<R> { return value.asPercentage(); }
    auto asAngle() const -> Angle<R> { return value.asDimension(); }
    auto asCalculationValue() const -> Ref<CalculationValue> { return value.asCalculationValue(); }

    bool isZero() const { return value.isZero(); }

    bool operator==(const AnglePercentage<R>&) const = default;
};

template<CSS::Range R = CSS::All> struct LengthPercentage {
    static constexpr bool isPercentageDimension = true;
    static constexpr auto range = R;
    using CSS = WebCore::CSS::LengthPercentage<R>;
    using Raw = WebCore::CSS::LengthPercentageRaw<R>;
    using Dimension = Length<R>;

    PrimitivePercentageDimension<Length<R>> value;

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
        : value { WTF::switchOn(data, [&](auto&& data) -> PrimitivePercentageDimension<Length<R>> { return { data }; }) }
    {
    }

    IPCData ipcData() const
    {
        switch (value.tag()) {
        case PrimitivePercentageDimension<Length<R>>::Tag::Percentage:
            return asPercentage();
        case PrimitivePercentageDimension<Length<R>>::Tag::Dimension:
            return asLength();
        case PrimitivePercentageDimension<Length<R>>::Tag::CalculationValue:
            ASSERT_NOT_REACHED();
            return Length<R> { 0 };
        }
        WTF_UNREACHABLE();
    }

    bool isPercentage() const { return value.isPercentage(); }
    bool isLength() const { return value.isDimension(); }
    bool isCalculationValue() const { return value.isCalculationValue(); }

    auto asPercentage() const -> Percentage<R> { return value.asPercentage(); }
    auto asLength() const -> Length<R> { return value.asDimension(); }
    auto asCalculationValue() const -> Ref<CalculationValue> { return value.asCalculationValue(); }

    bool isZero() const { return value.isZero(); }

    bool operator==(const LengthPercentage<R>&) const = default;
};

// MARK: Additional Common Type and Groupings

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
template<CSS::Range R = CSS::All> using NumberOrPercentage = std::variant<Number<R>, Percentage<R>>;

template<CSS::Range R = CSS::All> struct NumberOrPercentageResolvedToNumber {
    double value { 0 };

    constexpr NumberOrPercentageResolvedToNumber(double value)
        : value { value }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Number<R> number)
        : value { number.value }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Percentage<R> percentage)
        : value { percentage.value / 100.0 }
    {
    }

    constexpr bool operator==(const NumberOrPercentageResolvedToNumber<R>&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

// MARK: - Standard type aliases (used by some scripts that require non-template type names)

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
} // namespace WebCore
