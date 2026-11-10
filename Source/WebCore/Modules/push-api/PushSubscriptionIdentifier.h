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

#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/Markable.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct PushSubscriptionIdentifierType;
using PushSubscriptionIdentifier = ObjectIdentifier<PushSubscriptionIdentifierType>;

struct PushSubscriptionSetIdentifier {
    RefPtr<StringImpl> bundleIdentifier;
    RefPtr<StringImpl> pushPartition;
    Markable<WTF::UUID> dataStoreIdentifier;

    bool operator==(const PushSubscriptionSetIdentifier&) const;
    void add(Hasher&, const PushSubscriptionSetIdentifier&);
    bool isHashTableDeletedValue() const;
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    PushSubscriptionSetIdentifier isolatedCopy() const &;
    PushSubscriptionSetIdentifier isolatedCopy() &&;

    WEBCORE_EXPORT RefPtr<StringImpl> debugDescription() const;
};

WEBCORE_EXPORT RefPtr<StringImpl> makePushTopic(const PushSubscriptionSetIdentifier&, StringImpl* scope);

inline bool PushSubscriptionSetIdentifier::operator==(const PushSubscriptionSetIdentifier& other) const
{
    // Treat null and empty strings as empty strings for the purposes of hashing and comparison. The
    // reason for this is that null and empty strings are stored as empty strings in PushDatabase
    // (the columns are marked NOT NULL). We want to be able to compare instances that use null
    // strings with instances deserialized from the database that use empty strings.
    auto makeNotNull = [](StringImpl* s) -> StringImpl* {
        return s ? s : emptyString().get();
    };
    return equal(makeNotNull(bundleIdentifier), makeNotNull(other.bundleIdentifier)) && equal(makeNotNull(pushPartition), makeNotNull(other.pushPartition)) && dataStoreIdentifier == other.dataStoreIdentifier;
}

inline void add(Hasher& hasher, const PushSubscriptionSetIdentifier& sub)
{
    // Treat null and empty strings as empty strings for the purposes of hashing and comparison. See
    // the comment in operator== for more explanation.
    auto makeNotNull = [](StringImpl* s) -> StringImpl* {
        return s ? s : emptyString().get();
    };
    if (sub.dataStoreIdentifier)
        return add(hasher, makeNotNull(sub.bundleIdentifier), makeNotNull(sub.pushPartition), sub.dataStoreIdentifier.value());
    return add(hasher, makeNotNull(sub.bundleIdentifier), makeNotNull(sub.pushPartition));
}

inline bool PushSubscriptionSetIdentifier::isHashTableDeletedValue() const
{
    return dataStoreIdentifier && dataStoreIdentifier->isHashTableDeletedValue();
}

inline PushSubscriptionSetIdentifier PushSubscriptionSetIdentifier::isolatedCopy() const &
{
    return { WTF::isolatedCopy(bundleIdentifier), WTF::isolatedCopy(pushPartition), dataStoreIdentifier };
}

inline PushSubscriptionSetIdentifier PushSubscriptionSetIdentifier::isolatedCopy() &&
{
    return { WTF::isolatedCopy(WTF::move(bundleIdentifier)), WTF::isolatedCopy(WTF::move(pushPartition)), dataStoreIdentifier };
}

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::PushSubscriptionSetIdentifier> : GenericHashTraits<WebCore::PushSubscriptionSetIdentifier> {
    static const bool emptyValueIsZero = false;
    static WebCore::PushSubscriptionSetIdentifier emptyValue() { return { emptyString(), emptyString(), std::nullopt }; }

    static void constructDeletedValue(WebCore::PushSubscriptionSetIdentifier& slot) { new (NotNull, &slot) WebCore::PushSubscriptionSetIdentifier { emptyString(), emptyString(), WTF::UUID { HashTableDeletedValue } }; }
    static bool isDeletedValue(const WebCore::PushSubscriptionSetIdentifier& value) { return value.isHashTableDeletedValue(); }
};

} // namespace WTF
