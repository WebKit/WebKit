/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "TextFlags.h"
#include <algorithm>
#include <tuple>
#include <wtf/Hasher.h>
#include <wtf/Optional.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

// Unclamped, unchecked, signed fixed-point number representing a value used for font variations.
// Sixteen bits in total, one sign bit, two fractional bits, smallest positive value is 0.25,
// maximum value is 8191.75, and minimum value is -8192.
class FontSelectionValue {
public:
    using BackingType = int16_t;

    FontSelectionValue() = default;

    // Explicit because it won't work correctly for values outside the representable range.
    explicit constexpr FontSelectionValue(int);

    // Explicit because it won't work correctly for values outside the representable range and because precision can be lost.
    explicit constexpr FontSelectionValue(float);

    // Precision can be lost, but value will be clamped to the representable range.
    static constexpr FontSelectionValue clampFloat(float);

    // Since floats have 23 mantissa bits, every value can be represented losslessly.
    constexpr operator float() const;

    static constexpr FontSelectionValue maximumValue();
    static constexpr FontSelectionValue minimumValue();

    friend constexpr FontSelectionValue operator+(FontSelectionValue, FontSelectionValue);
    friend constexpr FontSelectionValue operator-(FontSelectionValue, FontSelectionValue);
    friend constexpr FontSelectionValue operator*(FontSelectionValue, FontSelectionValue);
    friend constexpr FontSelectionValue operator/(FontSelectionValue, FontSelectionValue);
    friend constexpr FontSelectionValue operator-(FontSelectionValue);

    constexpr BackingType rawValue() const { return m_backing; }

private:
    enum class RawTag { RawTag };
    constexpr FontSelectionValue(int, RawTag);

    static constexpr int fractionalEntropy = 4;
    BackingType m_backing { 0 };
};

constexpr FontSelectionValue::FontSelectionValue(int x)
    : m_backing(x * fractionalEntropy)
{
    // FIXME: Should we assert the passed in value was in range?
}

constexpr FontSelectionValue::FontSelectionValue(float x)
    : m_backing(x * fractionalEntropy)
{
    // FIXME: Should we assert the passed in value was in range?
}

constexpr FontSelectionValue::operator float() const
{
    return m_backing / static_cast<float>(fractionalEntropy);
}

constexpr FontSelectionValue FontSelectionValue::maximumValue()
{
    return { std::numeric_limits<BackingType>::max(), RawTag::RawTag };
}

constexpr FontSelectionValue FontSelectionValue::minimumValue()
{
    return { std::numeric_limits<BackingType>::min(), RawTag::RawTag };
}

constexpr FontSelectionValue FontSelectionValue::clampFloat(float value)
{
    return FontSelectionValue { std::max<float>(minimumValue(), std::min<float>(value, maximumValue())) };
}

constexpr FontSelectionValue::FontSelectionValue(int rawValue, RawTag)
    : m_backing(rawValue)
{
}

constexpr FontSelectionValue operator+(FontSelectionValue a, FontSelectionValue b)
{
    return { a.m_backing + b.m_backing, FontSelectionValue::RawTag::RawTag };
}

constexpr FontSelectionValue operator-(FontSelectionValue a, FontSelectionValue b)
{
    return { a.m_backing - b.m_backing, FontSelectionValue::RawTag::RawTag };
}

constexpr FontSelectionValue operator*(FontSelectionValue a, FontSelectionValue b)
{
    return { a.m_backing * b.m_backing / FontSelectionValue::fractionalEntropy, FontSelectionValue::RawTag::RawTag };
}

constexpr FontSelectionValue operator/(FontSelectionValue a, FontSelectionValue b)
{
    return { a.m_backing * FontSelectionValue::fractionalEntropy / b.m_backing, FontSelectionValue::RawTag::RawTag };
}

constexpr FontSelectionValue operator-(FontSelectionValue value)
{
    return { -value.m_backing, FontSelectionValue::RawTag::RawTag };
}

constexpr bool operator==(FontSelectionValue a, FontSelectionValue b)
{
    return a.rawValue() == b.rawValue();
}

constexpr bool operator!=(FontSelectionValue a, FontSelectionValue b)
{
    return a.rawValue() != b.rawValue();
}

constexpr bool operator<(FontSelectionValue a, FontSelectionValue b)
{
    return a.rawValue() < b.rawValue();
}

constexpr bool operator<=(FontSelectionValue a, FontSelectionValue b)
{
    return a.rawValue() <= b.rawValue();
}

constexpr bool operator>(FontSelectionValue a, FontSelectionValue b)
{
    return a.rawValue() > b.rawValue();
}

constexpr bool operator>=(FontSelectionValue a, FontSelectionValue b)
{
    return a.rawValue() >= b.rawValue();
}

constexpr FontSelectionValue italicThreshold()
{
    return FontSelectionValue { 20 };
}

constexpr bool isItalic(Optional<FontSelectionValue> fontWeight)
{
    return fontWeight && fontWeight.value() >= italicThreshold();
}

constexpr FontSelectionValue normalItalicValue()
{
    return FontSelectionValue { 0 };
}

constexpr FontSelectionValue italicValue()
{
    return FontSelectionValue { 20 };
}

constexpr FontSelectionValue boldThreshold()
{
    return FontSelectionValue { 600 };
}

constexpr FontSelectionValue boldWeightValue()
{
    return FontSelectionValue { 700 };
}

constexpr FontSelectionValue normalWeightValue()
{
    return FontSelectionValue { 400 };
}

constexpr FontSelectionValue lightWeightValue()
{
    return FontSelectionValue { 200 };
}

constexpr bool isFontWeightBold(FontSelectionValue fontWeight)
{
    return fontWeight >= boldThreshold();
}

constexpr FontSelectionValue lowerWeightSearchThreshold()
{
    return FontSelectionValue { 400 };
}

constexpr FontSelectionValue upperWeightSearchThreshold()
{
    return FontSelectionValue { 500 };
}

constexpr FontSelectionValue ultraCondensedStretchValue()
{
    return FontSelectionValue { 50 };
}

constexpr FontSelectionValue extraCondensedStretchValue()
{
    return FontSelectionValue { 62.5f };
}

constexpr FontSelectionValue condensedStretchValue()
{
    return FontSelectionValue { 75 };
}

constexpr FontSelectionValue semiCondensedStretchValue()
{
    return FontSelectionValue { 87.5f };
}

constexpr FontSelectionValue normalStretchValue()
{
    return FontSelectionValue { 100 };
}

constexpr FontSelectionValue semiExpandedStretchValue()
{
    return FontSelectionValue { 112.5f };
}

constexpr FontSelectionValue expandedStretchValue()
{
    return FontSelectionValue { 125 };
}

constexpr FontSelectionValue extraExpandedStretchValue()
{
    return FontSelectionValue { 150 };
}

constexpr FontSelectionValue ultraExpandedStretchValue()
{
    return FontSelectionValue { 200 };
}

// [Inclusive, Inclusive]
struct FontSelectionRange {
    using Value = FontSelectionValue;

    constexpr FontSelectionRange(Value minimum, Value maximum)
        : minimum(minimum)
        , maximum(maximum)
    {
    }

    explicit constexpr FontSelectionRange(Value value)
        : minimum(value)
        , maximum(value)
    {
    }

    constexpr bool operator==(const FontSelectionRange& other) const
    {
        return WTF::tie(minimum, maximum) == WTF::tie(other.minimum, other.maximum);
    }

    constexpr bool isValid() const
    {
        return minimum <= maximum;
    }

    void expand(const FontSelectionRange& other)
    {
        ASSERT(other.isValid());
        if (!isValid())
            *this = other;
        else {
            minimum = std::min(minimum, other.minimum);
            maximum = std::max(maximum, other.maximum);
        }
        ASSERT(isValid());
    }

    constexpr bool includes(Value target) const
    {
        return target >= minimum && target <= maximum;
    }

    // FIXME: This name is not so great. Move this into the add function below
    // once we move FontPlatformDataCacheKeyHash from IntegerHasher to Hasher,
    // and then it doesn't need to have a name.
    constexpr uint32_t uniqueValue() const
    {
        return minimum.rawValue() << 16 | maximum.rawValue();
    }

    Value minimum { 1 };
    Value maximum { 0 };
};

inline void add(Hasher& hasher, const FontSelectionRange& range)
{
    add(hasher, range.uniqueValue());
}

struct FontSelectionRequest {
    using Value = FontSelectionValue;

    Value weight;
    Value width;
    // FIXME: We are using an optional here to be able to distinguish between an explicit
    // or implicit slope (for "italic" and "oblique") and the "normal" value which has no
    // slope. The "italic" and "oblique" values can be distinguished by looking at the
    // "fontStyleAxis" on the FontDescription. We should come up with a tri-state member
    // so that it's a lot clearer whether we're dealing with a "normal", "italic" or explicit
    // "oblique" font style. See webkit.org/b/187774.
    Optional<Value> slope;

    std::tuple<Value, Value, Optional<Value>> tied() const
    {
        return WTF::tie(weight, width, slope);
    }
};

inline TextStream& operator<<(TextStream& ts, const FontSelectionValue& fontSelectionValue)
{
    ts << TextStream::FormatNumberRespectingIntegers(fontSelectionValue.rawValue());
    return ts;
}

inline TextStream& operator<<(TextStream& ts, const Optional<FontSelectionValue>& optionalFontSelectionValue)
{
    ts << optionalFontSelectionValue.valueOr(normalItalicValue());
    return ts;
}

inline bool operator==(const FontSelectionRequest& a, const FontSelectionRequest& b)
{
    return a.tied() == b.tied();
}

inline bool operator!=(const FontSelectionRequest& a, const FontSelectionRequest& b)
{
    return !(a == b);
}

inline void add(Hasher& hasher, const FontSelectionRequest& request)
{
    add(hasher, request.tied());
}

struct FontSelectionCapabilities {
    using Range = FontSelectionRange;

    FontSelectionCapabilities& operator=(const FontSelectionCapabilities&) = default;

    constexpr std::tuple<Range, Range, Range> tied() const
    {
        return WTF::tie(weight, width, slope);
    }

    void expand(const FontSelectionCapabilities& capabilities)
    {
        weight.expand(capabilities.weight);
        width.expand(capabilities.width);
        slope.expand(capabilities.slope);
    }

    Range weight { normalWeightValue() };
    Range width { normalStretchValue() };
    Range slope { normalItalicValue() };
};

constexpr bool operator==(const FontSelectionCapabilities& a, const FontSelectionCapabilities& b)
{
    return a.tied() == b.tied();
}

constexpr bool operator!=(const FontSelectionCapabilities& a, const FontSelectionCapabilities& b)
{
    return !(a == b);
}

struct FontSelectionSpecifiedCapabilities {
    using Capabilities = FontSelectionCapabilities;
    using Range = FontSelectionRange;
    using OptionalRange = Optional<Range>;

    constexpr Capabilities computeFontSelectionCapabilities() const
    {
        return { computeWeight(), computeWidth(), computeSlope() };
    }

    constexpr std::tuple<OptionalRange&, OptionalRange&, OptionalRange&> tied()
    {
        return WTF::tie(weight, width, slope);
    }

    constexpr std::tuple<const OptionalRange&, const OptionalRange&, const OptionalRange&> tied() const
    {
        return WTF::tie(weight, width, slope);
    }

    FontSelectionSpecifiedCapabilities& operator=(const Capabilities& other)
    {
        tied() = other.tied();
        return *this;
    }

    constexpr Range computeWeight() const
    {
        return weight.valueOr(Range { normalWeightValue() });
    }

    constexpr Range computeWidth() const
    {
        return width.valueOr(Range { normalStretchValue() });
    }

    constexpr Range computeSlope() const
    {
        return slope.valueOr(Range { normalItalicValue() });
    }

    OptionalRange weight;
    OptionalRange width;
    OptionalRange slope;
};

constexpr bool operator==(const FontSelectionSpecifiedCapabilities& a, const FontSelectionSpecifiedCapabilities& b)
{
    return a.tied() == b.tied();
}

constexpr bool operator!=(const FontSelectionSpecifiedCapabilities& a, const FontSelectionSpecifiedCapabilities& b)
{
    return !(a == b);
}

class FontSelectionAlgorithm {
public:
    using Capabilities = FontSelectionCapabilities;

    FontSelectionAlgorithm() = delete;
    FontSelectionAlgorithm(FontSelectionRequest, const Vector<Capabilities>&, Optional<Capabilities> capabilitiesBounds = WTF::nullopt);

    struct DistanceResult {
        FontSelectionValue distance;
        FontSelectionValue value;
    };
    DistanceResult stretchDistance(Capabilities) const;
    DistanceResult styleDistance(Capabilities) const;
    DistanceResult weightDistance(Capabilities) const;

    size_t indexOfBestCapabilities();

private:
    using DistanceFunction = DistanceResult (FontSelectionAlgorithm::*)(Capabilities) const;
    using CapabilitiesRange = FontSelectionRange Capabilities::*;
    FontSelectionValue bestValue(const bool eliminated[], DistanceFunction) const;
    void filterCapability(bool eliminated[], DistanceFunction, CapabilitiesRange);

    FontSelectionRequest m_request;
    Capabilities m_capabilitiesBounds;
    const Vector<Capabilities>& m_capabilities;
};

}
