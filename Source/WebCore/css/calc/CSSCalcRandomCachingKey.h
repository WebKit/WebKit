/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSCalcTree.h>
#include <optional>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace CSSCalc {

struct RandomCachingKey {
    using Identifier = Variant<
        Random::SharingOptions::Auto,
        AtomString,
        WTF::HashTableDeletedValueType,
        WTF::HashTableEmptyValueType
    >;
    Identifier identifier;

    RandomCachingKey(Variant<Random::SharingOptions::Auto, AtomString>&& identifier)
        : identifier { WTF::switchOn(WTFMove(identifier), [](auto&& alternative) { return Identifier { WTFMove(alternative) }; }) }
    {
    }

    RandomCachingKey(const Variant<Random::SharingOptions::Auto, AtomString>& identifier)
        : identifier { WTF::switchOn(identifier, [](const auto& alternative) { return Identifier { alternative }; }) }
    {
    }

    explicit RandomCachingKey(WTF::HashTableDeletedValueType)
        : identifier { WTF::HashTableDeletedValue }
    {
    }

    explicit RandomCachingKey(WTF::HashTableEmptyValueType)
        : identifier { WTF::HashTableEmptyValue }
    {
    }

    bool isHashTableDeletedValue() const { return std::holds_alternative<WTF::HashTableDeletedValueType>(identifier); }
    bool isHashTableEmptyValue() const { return std::holds_alternative<WTF::HashTableEmptyValueType>(identifier); }

    bool operator==(const RandomCachingKey&) const = default;
};

} // namespace CSSCalc
} // namespace WebCore

namespace WTF {

struct CSSCalcRandomCachingKeyHash {
    static unsigned hash(const WebCore::CSSCalc::RandomCachingKey& key)
    {
        Hasher hasher;
        add(hasher, key.identifier.index());
        WTF::switchOn(key.identifier,
            [&](const WebCore::CSSCalc::Random::SharingOptions::Auto& autoValue) {
                add(hasher, autoValue.property);
                add(hasher, autoValue.index);
            },
            [&](const AtomString& string) {
                add(hasher, string);
            },
            [](const WTF::HashTableDeletedValueType&) {
                RELEASE_ASSERT_NOT_REACHED();
            },
            [](const WTF::HashTableEmptyValueType&) {
                RELEASE_ASSERT_NOT_REACHED();
            }
        );
        return hasher.hash();
    }
    static bool equal(const WebCore::CSSCalc::RandomCachingKey& a, const WebCore::CSSCalc::RandomCachingKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebCore::CSSCalc::RandomCachingKey> : GenericHashTraits<WebCore::CSSCalc::RandomCachingKey> {
    static WebCore::CSSCalc::RandomCachingKey emptyValue() { return WebCore::CSSCalc::RandomCachingKey(HashTableEmptyValue); }
    static bool isEmptyValue(const WebCore::CSSCalc::RandomCachingKey& value) { return value.isHashTableEmptyValue(); }
    static void constructDeletedValue(WebCore::CSSCalc::RandomCachingKey& slot) { new (NotNull, &slot) WebCore::CSSCalc::RandomCachingKey(HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::CSSCalc::RandomCachingKey& slot) { return slot.isHashTableDeletedValue(); }

    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;
};

template<> struct DefaultHash<WebCore::CSSCalc::RandomCachingKey> : CSSCalcRandomCachingKeyHash { };

} // namespace WTF
