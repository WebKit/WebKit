/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "Process.h"
#include <wtf/DebugUtilities.h>
#include <wtf/Hasher.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct BackForwardItemIdentifier {
    ProcessIdentifier processIdentifier;
    enum ItemIdentifierType { };
    ObjectIdentifier<ItemIdentifierType> itemIdentifier;

    unsigned hash() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<BackForwardItemIdentifier> decode(Decoder&);

#if !LOG_DISABLED
    const char* logString() const;
#endif
};

#if !LOG_DISABLED
inline const char* BackForwardItemIdentifier::logString() const
{
    return debugString(String::number(processIdentifier.toUInt64()), "-", String::number(itemIdentifier.toUInt64()));
}
#endif

inline bool operator==(const BackForwardItemIdentifier& a, const BackForwardItemIdentifier& b)
{
    return a.processIdentifier == b.processIdentifier &&  a.itemIdentifier == b.itemIdentifier;
}

template<class Encoder>
void BackForwardItemIdentifier::encode(Encoder& encoder) const
{
    encoder << processIdentifier << itemIdentifier;
}

template<class Decoder>
Optional<BackForwardItemIdentifier> BackForwardItemIdentifier::decode(Decoder& decoder)
{
    Optional<ProcessIdentifier> processIdentifier;
    decoder >> processIdentifier;
    if (!processIdentifier)
        return WTF::nullopt;

    Optional<ObjectIdentifier<ItemIdentifierType>> itemIdentifier;
    decoder >> itemIdentifier;
    if (!itemIdentifier)
        return WTF::nullopt;

    return { { WTFMove(*processIdentifier), WTFMove(*itemIdentifier) } };
}

inline unsigned BackForwardItemIdentifier::hash() const
{
    return computeHash(processIdentifier.toUInt64(), itemIdentifier.toUInt64());
}

} // namespace WebCore

namespace WTF {

struct BackForwardItemIdentifierHash {
    static unsigned hash(const WebCore::BackForwardItemIdentifier& key) { return key.hash(); }
    static bool equal(const WebCore::BackForwardItemIdentifier& a, const WebCore::BackForwardItemIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebCore::BackForwardItemIdentifier> : GenericHashTraits<WebCore::BackForwardItemIdentifier> {
    static WebCore::BackForwardItemIdentifier emptyValue() { return { }; }

    static void constructDeletedValue(WebCore::BackForwardItemIdentifier& slot) { slot.processIdentifier = ObjectIdentifier<WebCore::ProcessIdentifierType>(HashTableDeletedValue); }

    static bool isDeletedValue(const WebCore::BackForwardItemIdentifier& slot) { return slot.processIdentifier.toUInt64() == std::numeric_limits<uint64_t>::max(); }
};

template<> struct DefaultHash<WebCore::BackForwardItemIdentifier> {
    typedef BackForwardItemIdentifierHash Hash;
};

} // namespace WTF
