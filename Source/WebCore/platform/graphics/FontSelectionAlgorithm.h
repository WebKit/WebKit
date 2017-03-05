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
#include <wtf/GetPtr.h>
#include <wtf/Hasher.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace WebCore {

// Unclamped, unchecked, signed fixed-point number representing a value used for font variations.
// Sixteen bits in total, one sign bit, two fractional bits, means the smallest positive representable value is 0.25,
// the maximum representable value is 8191.75, and the minimum representable value is -8192.
class FontSelectionValue {
public:
    FontSelectionValue() = default;

    // Explicit because it is lossy.
    explicit FontSelectionValue(int x)
        : m_backing(x * fractionalEntropy)
    {
    }

    // Explicit because it is lossy.
    explicit FontSelectionValue(float x)
        : m_backing(x * fractionalEntropy)
    {
    }

    operator float() const
    {
        // floats have 23 fractional bits, but only 14 fractional bits are necessary, so every value can be represented losslessly.
        return m_backing / static_cast<float>(fractionalEntropy);
    }

    FontSelectionValue operator+(const FontSelectionValue other) const;
    FontSelectionValue operator-(const FontSelectionValue other) const;
    FontSelectionValue operator*(const FontSelectionValue other) const;
    FontSelectionValue operator/(const FontSelectionValue other) const;
    FontSelectionValue operator-() const;
    bool operator==(const FontSelectionValue other) const;
    bool operator!=(const FontSelectionValue other) const;
    bool operator<(const FontSelectionValue other) const;
    bool operator<=(const FontSelectionValue other) const;
    bool operator>(const FontSelectionValue other) const;
    bool operator>=(const FontSelectionValue other) const;

    int16_t rawValue() const
    {
        return m_backing;
    }

    static FontSelectionValue maximumValue()
    {
        static NeverDestroyed<FontSelectionValue> result = FontSelectionValue(std::numeric_limits<int16_t>::max(), RawTag::RawTag);
        return result.get();
    }

    static FontSelectionValue minimumValue()
    {
        static NeverDestroyed<FontSelectionValue> result = FontSelectionValue(std::numeric_limits<int16_t>::min(), RawTag::RawTag);
        return result.get();
    }

private:
    enum class RawTag { RawTag };

    FontSelectionValue(int16_t rawValue, RawTag)
        : m_backing(rawValue)
    {
    }

    static constexpr int fractionalEntropy = 4;
    int16_t m_backing { 0 };
};

inline FontSelectionValue FontSelectionValue::operator+(const FontSelectionValue other) const
{
    return FontSelectionValue(m_backing + other.m_backing, RawTag::RawTag);
}

inline FontSelectionValue FontSelectionValue::operator-(const FontSelectionValue other) const
{
    return FontSelectionValue(m_backing - other.m_backing, RawTag::RawTag);
}

inline FontSelectionValue FontSelectionValue::operator*(const FontSelectionValue other) const
{
    return FontSelectionValue(static_cast<int32_t>(m_backing) * other.m_backing / fractionalEntropy, RawTag::RawTag);
}

inline FontSelectionValue FontSelectionValue::operator/(const FontSelectionValue other) const
{
    return FontSelectionValue(static_cast<int32_t>(m_backing) / other.m_backing * fractionalEntropy, RawTag::RawTag);
}

inline FontSelectionValue FontSelectionValue::operator-() const
{
    return FontSelectionValue(-m_backing, RawTag::RawTag);
}

inline bool FontSelectionValue::operator==(const FontSelectionValue other) const
{
    return m_backing == other.m_backing;
}

inline bool FontSelectionValue::operator!=(const FontSelectionValue other) const
{
    return !operator==(other);
}

inline bool FontSelectionValue::operator<(const FontSelectionValue other) const
{
    return m_backing < other.m_backing;
}

inline bool FontSelectionValue::operator<=(const FontSelectionValue other) const
{
    return m_backing <= other.m_backing;
}

inline bool FontSelectionValue::operator>(const FontSelectionValue other) const
{
    return m_backing > other.m_backing;
}

inline bool FontSelectionValue::operator>=(const FontSelectionValue other) const
{
    return m_backing >= other.m_backing;
}

static inline FontSelectionValue italicThreshold()
{
    static NeverDestroyed<FontSelectionValue> result = FontSelectionValue(20);
    return result.get();
}

static inline FontSelectionValue boldThreshold()
{
    static NeverDestroyed<FontSelectionValue> result = FontSelectionValue(600);
    return result.get();
}

static inline FontSelectionValue weightSearchThreshold()
{
    static NeverDestroyed<FontSelectionValue> result = FontSelectionValue(500);
    return result.get();
}

// [Inclusive, Inclusive]
struct FontSelectionRange {
    FontSelectionRange(FontSelectionValue minimum, FontSelectionValue maximum)
        : minimum(minimum)
        , maximum(maximum)
    {
    }

    bool operator==(const FontSelectionRange& other) const
    {
        return minimum == other.minimum
            && maximum == other.maximum;
    }

    bool isValid() const
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

    bool includes(FontSelectionValue target) const
    {
        return target >= minimum && target <= maximum;
    }

    FontSelectionValue minimum { FontSelectionValue(1) };
    FontSelectionValue maximum { FontSelectionValue(0) };
};

struct FontSelectionRequest {
    bool operator==(const FontSelectionRequest& other) const
    {
        return weight == other.weight
            && width == other.width
            && slope == other.slope;
    }

    bool operator!=(const FontSelectionRequest& other) const
    {
        return !operator==(other);
    }

    FontSelectionValue weight;
    FontSelectionValue width;
    FontSelectionValue slope;
};

// Only used for HashMaps. We don't want to put the bool into FontSelectionRequest
// because FontSelectionRequest needs to be as small as possible because it's inside
// every FontDescription.
struct FontSelectionRequestKey {
    FontSelectionRequestKey() = default;

    FontSelectionRequestKey(FontSelectionRequest request)
        : request(request)
    {
    }

    explicit FontSelectionRequestKey(WTF::HashTableDeletedValueType)
        : isDeletedValue(true)
    {
    }

    bool isHashTableDeletedValue() const
    {
        return isDeletedValue;
    }

    bool operator==(const FontSelectionRequestKey& other) const
    {
        return request == other.request
            && isDeletedValue == other.isDeletedValue;
    }

    FontSelectionRequest request;
    bool isDeletedValue { false };
};

struct FontSelectionRequestKeyHash {
    static unsigned hash(const FontSelectionRequestKey& key)
    {
        IntegerHasher hasher;
        hasher.add(key.request.weight.rawValue());
        hasher.add(key.request.width.rawValue());
        hasher.add(key.request.slope.rawValue());
        hasher.add(key.isDeletedValue);
        return hasher.hash();
    }

    static bool equal(const FontSelectionRequestKey& a, const FontSelectionRequestKey& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct FontSelectionCapabilities {
    void expand(const FontSelectionCapabilities& capabilities)
    {
        weight.expand(capabilities.weight);
        width.expand(capabilities.width);
        slope.expand(capabilities.slope);
    }

    FontSelectionRange weight { FontSelectionValue(400), FontSelectionValue(400) };
    FontSelectionRange width { FontSelectionValue(100), FontSelectionValue(100) };
    FontSelectionRange slope { FontSelectionValue(), FontSelectionValue() };
};

class FontSelectionAlgorithm {
public:
    FontSelectionAlgorithm() = delete;

    FontSelectionAlgorithm(FontSelectionRequest request, const Vector<FontSelectionCapabilities>& capabilities, std::optional<FontSelectionCapabilities> capabilitiesBounds = std::nullopt)
        : m_request(request)
        , m_capabilities(capabilities)
        , m_filter(new bool[m_capabilities.size()])
    {
        ASSERT(!m_capabilities.isEmpty());
        if (capabilitiesBounds)
            m_capabilitiesBounds = capabilitiesBounds.value();
        else {
            for (auto capabilities : m_capabilities)
                m_capabilitiesBounds.expand(capabilities);
        }
        for (size_t i = 0; i < m_capabilities.size(); ++i)
            m_filter[i] = true;
    }

    struct DistanceResult {
        FontSelectionValue distance;
        FontSelectionValue value;
    };

    DistanceResult stretchDistance(FontSelectionCapabilities) const;
    DistanceResult styleDistance(FontSelectionCapabilities) const;
    DistanceResult weightDistance(FontSelectionCapabilities) const;

    size_t indexOfBestCapabilities();

private:
    template <typename T>
    using IterateActiveCapabilitiesWithReturnCallback = std::function<std::optional<T>(FontSelectionCapabilities, size_t)>;

    template <typename T>
    inline std::optional<T> iterateActiveCapabilitiesWithReturn(IterateActiveCapabilitiesWithReturnCallback<T> callback)
    {
        for (size_t i = 0; i < m_capabilities.size(); ++i) {
            if (!m_filter[i])
                continue;
            if (auto result = callback(m_capabilities[i], i))
                return result;
        }
        return std::nullopt;
    }

    template <typename T>
    inline void iterateActiveCapabilities(T callback)
    {
        iterateActiveCapabilitiesWithReturn<int>([&](FontSelectionCapabilities capabilities, size_t i) -> std::optional<int> {
            callback(capabilities, i);
            return std::nullopt;
        });
    }

    void filterCapability(DistanceResult(FontSelectionAlgorithm::*computeDistance)(FontSelectionCapabilities) const, FontSelectionRange FontSelectionCapabilities::*inclusionRange);

    FontSelectionRequest m_request;
    FontSelectionCapabilities m_capabilitiesBounds;
    const Vector<FontSelectionCapabilities>& m_capabilities;
    std::unique_ptr<bool[]> m_filter;
};

FontSelectionRequest fontSelectionRequestForTraitsMask(FontTraitsMask, FontSelectionValue stretch);
FontSelectionCapabilities fontSelectionCapabilitiesForTraitsMask(FontTraitsMask, FontSelectionValue stretch);
FontSelectionCapabilities fontSelectionCapabilitiesForTraitsMask(FontTraitsMask, FontSelectionRange stretch);

}
