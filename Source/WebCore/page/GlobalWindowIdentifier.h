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

#include "ProcessIdentifier.h"
#include <wtf/HashTraits.h>
#include <wtf/ObjectIdentifier.h>

namespace WebCore {

enum WindowIdentifierType { };
using WindowIdentifier = ObjectIdentifier<WindowIdentifierType>;

// Window identifier that is unique across all WebContent processes.
struct GlobalWindowIdentifier {
    ProcessIdentifier processIdentifier;
    WindowIdentifier windowIdentifier;

    unsigned hash() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GlobalWindowIdentifier> decode(Decoder&);
};

inline bool operator==(const GlobalWindowIdentifier& a, const GlobalWindowIdentifier& b)
{
    return a.processIdentifier == b.processIdentifier &&  a.windowIdentifier == b.windowIdentifier;
}

inline unsigned GlobalWindowIdentifier::hash() const
{
    uint64_t identifiers[2];
    identifiers[0] = processIdentifier.toUInt64();
    identifiers[1] = windowIdentifier.toUInt64();

    return StringHasher::hashMemory(identifiers, sizeof(identifiers));
}

template<class Encoder>
void GlobalWindowIdentifier::encode(Encoder& encoder) const
{
    encoder << processIdentifier << windowIdentifier;
}

template<class Decoder>
std::optional<GlobalWindowIdentifier> GlobalWindowIdentifier::decode(Decoder& decoder)
{
    std::optional<ProcessIdentifier> processIdentifier;
    decoder >> processIdentifier;
    if (!processIdentifier)
        return std::nullopt;

    std::optional<WindowIdentifier> windowIdentifier;
    decoder >> windowIdentifier;
    if (!windowIdentifier)
        return std::nullopt;

    return { { WTFMove(*processIdentifier), WTFMove(*windowIdentifier) } };
}

} // namespace WebCore

namespace WTF {

struct GlobalWindowIdentifierHash {
    static unsigned hash(const WebCore::GlobalWindowIdentifier& key) { return key.hash(); }
    static bool equal(const WebCore::GlobalWindowIdentifier& a, const WebCore::GlobalWindowIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebCore::GlobalWindowIdentifier> : GenericHashTraits<WebCore::GlobalWindowIdentifier> {
    static WebCore::GlobalWindowIdentifier emptyValue() { return { }; }

    static void constructDeletedValue(WebCore::GlobalWindowIdentifier& slot) { new (NotNull, &slot.windowIdentifier) WebCore::WindowIdentifier(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::GlobalWindowIdentifier& slot) { return slot.windowIdentifier.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebCore::GlobalWindowIdentifier> : GlobalWindowIdentifierHash { };

} // namespace WTF
