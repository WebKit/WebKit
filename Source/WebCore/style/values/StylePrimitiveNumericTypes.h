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
#include "StyleValueTypes.h"
#include <variant>
#include <wtf/Forward.h>

namespace WebCore {
namespace Style {

// Concept for use in generic contexts to filter on Numeric Style types.
template<typename T> concept StyleNumeric = requires(T) {
    typename T::CSS;
    typename T::Raw;
};

// MARK: Number Primitive

struct Number {
    static constexpr auto unit = CSSUnitType::CSS_NUMBER;
    using CSS = WebCore::CSS::Number;
    using Raw = WebCore::CSS::NumberRaw;

    float value { 0 };

    constexpr bool operator==(const Number&) const = default;
};
constexpr Number canonicalizeNoConversionDataRequired(const CSS::NumberRaw& raw) { return { narrowPrecisionToFloat(raw.value) }; }
constexpr Number canonicalize(const CSS::NumberRaw& raw, const CSSToLengthConversionData&) { return canonicalizeNoConversionDataRequired(raw); }

// MARK: Percentage Primitive

struct Percentage {
    static constexpr auto unit = CSSUnitType::CSS_PERCENTAGE;
    using CSS = WebCore::CSS::Percentage;
    using Raw = WebCore::CSS::PercentageRaw;

    float value { 0 };

    constexpr bool operator==(const Percentage&) const = default;
};
constexpr Percentage canonicalizeNoConversionDataRequired(const CSS::PercentageRaw& raw) { return { narrowPrecisionToFloat(raw.value) }; }
constexpr Percentage canonicalize(const CSS::PercentageRaw& raw, const CSSToLengthConversionData&) { return canonicalizeNoConversionDataRequired(raw); }
float evaluate(const Percentage&, float percentResolutionLength);

// MARK: Dimension Primitives

struct Angle {
    static constexpr auto unit = CSSUnitType::CSS_DEG;
    using CSS = WebCore::CSS::Angle;
    using Raw = WebCore::CSS::AngleRaw;

    float value { 0 };

    constexpr bool operator==(const Angle&) const = default;
};
Angle canonicalizeNoConversionDataRequired(const CSS::AngleRaw&);
Angle canonicalize(const CSS::AngleRaw&, const CSSToLengthConversionData&);

struct Length {
    static constexpr auto unit = CSSUnitType::CSS_PX;
    using CSS = WebCore::CSS::Length;
    using Raw = WebCore::CSS::LengthRaw;

    float value { 0 };
    bool quirk { false };

    constexpr bool hasQuirk() const { return quirk; }
    constexpr bool operator==(const Length&) const = default;
};
Length canonicalizeNoConversionDataRequired(const CSS::LengthRaw&);
Length canonicalize(const CSS::LengthRaw&, const CSSToLengthConversionData&);

struct Time {
    static constexpr auto unit = CSSUnitType::CSS_S;
    using CSS = WebCore::CSS::Time;
    using Raw = WebCore::CSS::TimeRaw;

    float value { 0 };

    constexpr bool operator==(const Time&) const = default;
};
Time canonicalizeNoConversionDataRequired(const CSS::TimeRaw&);
Time canonicalize(const CSS::TimeRaw&, const CSSToLengthConversionData&);

struct Frequency {
    static constexpr auto unit = CSSUnitType::CSS_HZ;
    using CSS = WebCore::CSS::Frequency;
    using Raw = WebCore::CSS::FrequencyRaw;

    float value { 0 };

    constexpr bool operator==(const Frequency&) const = default;
};
Frequency canonicalizeNoConversionDataRequired(const CSS::FrequencyRaw&);
Frequency canonicalize(const CSS::FrequencyRaw&, const CSSToLengthConversionData&);

struct Resolution {
    static constexpr auto unit = CSSUnitType::CSS_DPPX;
    using CSS = WebCore::CSS::Resolution;
    using Raw = WebCore::CSS::ResolutionRaw;

    float value { 0 };

    constexpr bool operator==(const Resolution&) const = default;
};
Resolution canonicalizeNoConversionDataRequired(const CSS::ResolutionRaw&);
Resolution canonicalize(const CSS::ResolutionRaw&, const CSSToLengthConversionData&);

struct Flex {
    static constexpr auto unit = CSSUnitType::CSS_FR;
    using CSS = WebCore::CSS::Flex;
    using Raw = WebCore::CSS::FlexRaw;

    float value { 0 };

    constexpr bool operator==(const Flex&) const = default;
};
constexpr Flex canonicalizeNoConversionDataRequired(const CSS::FlexRaw& raw) { return { narrowPrecisionToFloat(raw.value) }; }
constexpr Flex canonicalize(const CSS::FlexRaw& raw, const CSSToLengthConversionData&) { return canonicalizeNoConversionDataRequired(raw); }

// MARK: Dimension + Percentage Primitives

// Compact representation of std::variant<Angle, Percentage, Ref<CalculationValue>> that takes
// up only 64 bits. Utilizes the knowledge that pointers are at most 48 bits, allowing for the
// storage of the type tag in the remaining space.
// FIXME: Abstract into a generic CompactPointerVariant type.
class AnglePercentageValue {
public:
    enum class Tag : uint8_t { Angle, Percentage, CalculationValue };

    constexpr AnglePercentageValue(Angle angle)
        : m_data { encodedPayload(angle) | encodedTag(Tag::Angle) }
    {
    }

    constexpr AnglePercentageValue(Percentage percentage)
        : m_data { encodedPayload(percentage) | encodedTag(Tag::Percentage) }
    {
    }

    AnglePercentageValue(Ref<CalculationValue>&& calculationValue)
        : m_data { encodedPayload(WTFMove(calculationValue)) | encodedTag(Tag::CalculationValue) }
    {
    }

    AnglePercentageValue(const AnglePercentageValue& other)
        : m_data { other.m_data }
    {
        if (tag() == Tag::CalculationValue)
            asCalculationValue().ref(); // Balanced by deref() in destructor.
    }

    AnglePercentageValue(AnglePercentageValue&& other)
    {
        *this = WTFMove(other);
    }

    AnglePercentageValue& operator=(const AnglePercentageValue& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            asCalculationValue().deref();

        m_data = other.m_data;

        if (tag() == Tag::CalculationValue)
            asCalculationValue().ref();

        return *this;
    }

    AnglePercentageValue& operator=(AnglePercentageValue&& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            asCalculationValue().deref();

        m_data = other.m_data;
        other.m_data = movedFromValue();

        return *this;
    }


    ~AnglePercentageValue()
    {
        if (tag() == Tag::CalculationValue)
            asCalculationValue().deref(); // Balanced by leakRef() in encodedCalculationValue.
    }

    constexpr Tag tag() const
    {
        return decodedTag(m_data);
    }

    constexpr Angle asAngle() const
    {
        ASSERT(tag() == Tag::Angle);
        return decodedAngle(m_data);
    }

    constexpr Percentage asPercentage() const
    {
        ASSERT(tag() == Tag::Percentage);
        return decodedPercentage(m_data);
    }

    constexpr const CalculationValue& asCalculationValue() const
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

    constexpr bool operator==(const AnglePercentageValue&) const = default;

private:
#if CPU(ADDRESS64)
    static constexpr unsigned maxNumberOfBitsInPointer = 48;
#else
    static constexpr unsigned maxNumberOfBitsInPointer = 32;
#endif
    static constexpr uint64_t calculationValueSize = maxNumberOfBitsInPointer;
    static constexpr uint64_t calculationValueMask = (1ULL << calculationValueSize) - 1;
    static constexpr uint64_t angleSize = sizeof(Angle) * 8;
    static constexpr uint64_t angleMask = (1ULL << angleSize) - 1;
    static constexpr uint64_t percentageSize = sizeof(Percentage) * 8;
    static constexpr uint64_t percentageMask = (1ULL << percentageSize) - 1;
    static constexpr uint64_t tagSize = sizeof(Tag) * 8;
    static constexpr uint64_t tagShift = std::max({ calculationValueSize, angleSize, percentageSize });
    static_assert(tagShift + tagSize <= 64);

    static constexpr uint64_t movedFromValue()
    {
        return encodedPayload(Angle { 0 }) | encodedTag(Tag::Angle);
    }

    static uint64_t encodedPayload(Ref<CalculationValue>&& calculationValue)
    {
        return encodedCalculationValue(WTFMove(calculationValue));
    }

    static constexpr uint64_t encodedPayload(Angle angle)
    {
        return encodedAngle(angle);
    }

    static constexpr uint64_t encodedPayload(Percentage percentage)
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

    // Payload specific coding.

    static uint64_t encodedCalculationValue(Ref<CalculationValue>&& calculationValue)
    {
#if CPU(ADDRESS64)
        return bitwise_cast<uint64_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#else
        return bitwise_cast<uint32_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#endif
    }

    static constexpr CalculationValue& decodedCalculationValue(uint64_t value)
    {
#if CPU(ADDRESS64)
        return *bitwise_cast<CalculationValue*>(value & calculationValueMask);
#else
        return *bitwise_cast<CalculationValue*>(static_cast<uint32_t>(value & calculationValueMask));
#endif
    }

    static constexpr uint64_t encodedAngle(Angle angle)
    {
        return static_cast<uint64_t>(bitwise_cast<uint32_t>(angle.value));
    }

    static constexpr Angle decodedAngle(uint64_t value)
    {
        return { bitwise_cast<float>(static_cast<uint32_t>(value & angleMask)) };
    }

    static constexpr uint64_t encodedPercentage(Percentage percentage)
    {
        return static_cast<uint64_t>(bitwise_cast<uint32_t>(percentage.value));
    }

    static constexpr Percentage decodedPercentage(uint64_t value)
    {
        return { bitwise_cast<float>(static_cast<uint32_t>(value & percentageMask)) };
    }

    uint64_t m_data { 0 };
};

struct AnglePercentage {
    using CSS = WebCore::CSS::AnglePercentage;
    using Raw = WebCore::CSS::AnglePercentageRaw;

    AnglePercentageValue value;

    constexpr bool operator==(const AnglePercentage&) const = default;
};
AnglePercentage canonicalizeNoConversionDataRequired(const CSS::AnglePercentageRaw&);
AnglePercentage canonicalize(const CSS::AnglePercentageRaw&, const CSSToLengthConversionData&);
float evaluate(const AnglePercentage&, float percentResolutionLength);

// Compact representation of std::variant<Length, Percentage, Ref<CalculationValue>> that takes
// up only 64 bits. Utilizes the knowledge that pointers are at most 48 bits, allowing for the
// storage of the type tag in the remaining space.
// FIXME: Abstract into a generic CompactPointerVariant type.
class LengthPercentageValue {
public:
    enum class Tag : uint8_t { Length, Percentage, CalculationValue };

    constexpr LengthPercentageValue(Length length)
        : m_data { encodedPayload(length) | encodedTag(Tag::Length) }
    {
    }

    constexpr LengthPercentageValue(Percentage percentage)
        : m_data { encodedPayload(percentage) | encodedTag(Tag::Percentage) }
    {
    }

    LengthPercentageValue(Ref<CalculationValue>&& calculationValue)
        : m_data { encodedPayload(WTFMove(calculationValue)) | encodedTag(Tag::CalculationValue) }
    {
    }

    LengthPercentageValue(const LengthPercentageValue& other)
        : m_data { other.m_data }
    {
        if (tag() == Tag::CalculationValue)
            asCalculationValue().ref(); // Balanced by deref() in destructor.
    }

    LengthPercentageValue(LengthPercentageValue&& other)
    {
        *this = WTFMove(other);
    }

    LengthPercentageValue& operator=(const LengthPercentageValue& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            asCalculationValue().deref();

        m_data = other.m_data;

        if (tag() == Tag::CalculationValue)
            asCalculationValue().ref();

        return *this;
    }

    LengthPercentageValue& operator=(LengthPercentageValue&& other)
    {
        if (*this == other)
            return *this;

        if (tag() == Tag::CalculationValue)
            asCalculationValue().deref();

        m_data = other.m_data;
        other.m_data = movedFromValue();

        return *this;
    }

    ~LengthPercentageValue()
    {
        if (tag() == Tag::CalculationValue)
            asCalculationValue().deref(); // Balanced by leakRef() in encodedCalculationValue.
    }

    constexpr Tag tag() const
    {
        return decodedTag(m_data);
    }

    constexpr Length asLength() const
    {
        ASSERT(tag() == Tag::Length);
        return decodedLength(m_data);
    }

    constexpr Percentage asPercentage() const
    {
        ASSERT(tag() == Tag::Percentage);
        return decodedPercentage(m_data);
    }

    const CalculationValue& asCalculationValue() const
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

    bool operator==(const LengthPercentageValue&) const = default;

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
    static constexpr uint64_t percentageSize = sizeof(Percentage) * 8;
    static constexpr uint64_t percentageMask = (1ULL << percentageSize) - 1;
    static constexpr uint64_t tagSize = sizeof(Tag) * 8;
    static constexpr uint64_t tagShift = std::max({ calculationValueSize, lengthSize, percentageSize });
    static_assert(tagShift + tagSize <= 64);

    static constexpr uint64_t movedFromValue()
    {
        return encodedPayload(Length { 0 }) | encodedTag(Tag::Length);
    }

    static uint64_t encodedPayload(Ref<CalculationValue>&& calculationValue)
    {
        return encodedCalculationValue(WTFMove(calculationValue));
    }

    static constexpr uint64_t encodedPayload(Length length)
    {
        return encodedLength(length);
    }

    static constexpr uint64_t encodedPayload(Percentage percentage)
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

    // Payload specific coding.

    static uint64_t encodedCalculationValue(Ref<CalculationValue>&& calculationValue)
    {
#if CPU(ADDRESS64)
        return bitwise_cast<uint64_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#else
        return bitwise_cast<uint32_t>(&calculationValue.leakRef()); // Balanced by deref() in destructor.
#endif
    }

    static constexpr CalculationValue& decodedCalculationValue(uint64_t value)
    {
#if CPU(ADDRESS64)
        return *bitwise_cast<CalculationValue*>(value & calculationValueMask);
#else
        return *bitwise_cast<CalculationValue*>(static_cast<uint32_t>(value & calculationValueMask));
#endif
    }

    static constexpr uint64_t encodedLength(Length length)
    {
        return static_cast<uint64_t>(bitwise_cast<uint32_t>(length.value)) | (static_cast<uint64_t>(length.quirk) << lengthQuirkShift);
    }

    static constexpr Length decodedLength(uint64_t value)
    {
        return { bitwise_cast<float>(static_cast<uint32_t>(value & lengthValueMask)), static_cast<bool>(value >> lengthQuirkShift) };
    }

    static constexpr uint64_t encodedPercentage(Percentage percentage)
    {
        return static_cast<uint64_t>(bitwise_cast<uint32_t>(percentage.value));
    }

    static constexpr Percentage decodedPercentage(uint64_t value)
    {
        return { bitwise_cast<float>(static_cast<uint32_t>(value & percentageMask)) };
    }

    uint64_t m_data { 0 };
};

struct LengthPercentage {
    using CSS = WebCore::CSS::LengthPercentage;
    using Raw = WebCore::CSS::LengthPercentageRaw;

    LengthPercentageValue value;

    bool hasQuirk() const { return value.tag() == LengthPercentageValue::Tag::Length && value.asLength().hasQuirk(); }

    constexpr bool operator==(const LengthPercentage&) const = default;
};
LengthPercentage canonicalizeNoConversionDataRequired(const CSS::LengthPercentageRaw&);
LengthPercentage canonicalize(const CSS::LengthPercentageRaw&, const CSSToLengthConversionData&);
float evaluate(const LengthPercentage&, float percentResolutionLength);

// MARK: Additional Common Groupings

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
using PercentageOrNumber = std::variant<Percentage, Number>;

// MARK: Additional Numeric Adjacent Types

struct None {
    using CSS = WebCore::CSS::None;
    using Raw = WebCore::CSS::NoneRaw;

    constexpr bool operator==(const None&) const = default;
};

} // namespace Style

namespace CSS {
namespace TypeTransform {

// MARK: - Type transforms

namespace Type {

// MARK: CSS type -> Style type mapping (Style type -> CSS type directly available via typename StyleType::CSS)

template<typename> struct CSSToStyleMapping;
template<> struct CSSToStyleMapping<Number> { using Style = Style::Number; };
template<> struct CSSToStyleMapping<Percentage> { using Style = Style::Percentage; };
template<> struct CSSToStyleMapping<Angle> { using Style = Style::Angle; };
template<> struct CSSToStyleMapping<Length> { using Style = Style::Length; };
template<> struct CSSToStyleMapping<Time> { using Style = Style::Time; };
template<> struct CSSToStyleMapping<Frequency> { using Style = Style::Frequency; };
template<> struct CSSToStyleMapping<Resolution> { using Style = Style::Resolution; };
template<> struct CSSToStyleMapping<Flex> { using Style = Style::Flex; };
template<> struct CSSToStyleMapping<AnglePercentage> { using Style = Style::AnglePercentage; };
template<> struct CSSToStyleMapping<LengthPercentage> { using Style = Style::LengthPercentage; };
template<> struct CSSToStyleMapping<None> { using Style = Style::None; };

// MARK: Transform CSS type -> Style type.

// Transform `css1`  -> `style1`
template<typename T> struct CSSToStyleLazy {
    using type = typename CSSToStyleMapping<T>::Style;
};
template<typename T> using CSSToStyle = typename CSSToStyleLazy<T>::type;

// MARK: Transform Raw type -> Style type.

// Transform `raw1`  -> `style1`
template<typename T> struct RawToStyleLazy {
    using type = typename CSSToStyleMapping<typename RawToCSSMapping<T>::CSS>::Style;
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
