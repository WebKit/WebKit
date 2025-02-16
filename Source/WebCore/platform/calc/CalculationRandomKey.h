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

#include <optional>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace Calculation {

struct RandomKey {
    AtomString identifier;
    double min;
    double max;
    std::optional<double> step;

    RandomKey(AtomString identifier, double min, double max, std::optional<double> step)
        : identifier { WTFMove(identifier) }
        , min { min }
        , max { max }
        , step { step }
    {
        RELEASE_ASSERT(!std::isnan(min));
        RELEASE_ASSERT(!std::isnan(max));
    }

    explicit RandomKey(WTF::HashTableDeletedValueType)
        : identifier { }
        , min { std::numeric_limits<double>::quiet_NaN() }
        , max { 0 }
        , step { std::nullopt }
    {
    }

    explicit RandomKey(WTF::HashTableEmptyValueType)
        : identifier { }
        , min { 0 }
        , max { std::numeric_limits<double>::quiet_NaN() }
        , step { std::nullopt }
    {
    }

    bool isHashTableDeletedValue() const { return std::isnan(min); }
    bool isHashTableEmptyValue() const { return std::isnan(max); }

    bool operator==(const RandomKey&) const = default;
};

} // namespace Calculation
} // namespace WebCore

namespace WTF {

struct CalculationRandomKeyHash {
    static unsigned hash(const WebCore::Calculation::RandomKey& key) { return computeHash(key.identifier, key.min, key.max, key.step); }
    static bool equal(const WebCore::Calculation::RandomKey& a, const WebCore::Calculation::RandomKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<> struct HashTraits<WebCore::Calculation::RandomKey> : GenericHashTraits<WebCore::Calculation::RandomKey> {
    static WebCore::Calculation::RandomKey emptyValue() { return WebCore::Calculation::RandomKey(HashTableEmptyValue); }
    static bool isEmptyValue(const WebCore::Calculation::RandomKey& value) { return value.isHashTableEmptyValue(); }
    static void constructDeletedValue(WebCore::Calculation::RandomKey& slot) { new (NotNull, &slot) WebCore::Calculation::RandomKey(HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::Calculation::RandomKey& slot) { return slot.isHashTableDeletedValue(); }

    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;
};

template<> struct DefaultHash<WebCore::Calculation::RandomKey> : CalculationRandomKeyHash { };

} // namespace WTF
