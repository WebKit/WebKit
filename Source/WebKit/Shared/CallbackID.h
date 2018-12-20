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
#include <wtf/RunLoop.h>

namespace WTF {

struct CallbackIDHash;

}

namespace WebKit {

class CallbackID {
public:
    ALWAYS_INLINE explicit CallbackID()
    { }

    ALWAYS_INLINE CallbackID(const CallbackID& otherID)
        : m_id(otherID.m_id)
    {
        ASSERT(HashTraits<uint64_t>::emptyValue() != m_id && !HashTraits<uint64_t>::isDeletedValue(m_id));
    }

    bool operator==(const CallbackID& other) const { return m_id == other.m_id; }

    uint64_t toInteger() const { return m_id; }
    ALWAYS_INLINE bool isValid() const { return isValidCallbackID(m_id); }
    static ALWAYS_INLINE bool isValidCallbackID(uint64_t rawId)
    {
        return HashTraits<uint64_t>::emptyValue() != rawId && !HashTraits<uint64_t>::isDeletedValue(rawId);
    }

    static ALWAYS_INLINE CallbackID fromInteger(uint64_t rawId)
    {
        RELEASE_ASSERT(isValidCallbackID(rawId));
        return CallbackID(rawId);
    }

    static CallbackID generateID()
    {
        ASSERT(RunLoop::isMain());
        static uint64_t uniqueCallbackID = 1;
        return CallbackID(uniqueCallbackID++);
    }

    template<class Encoder> void encode(Encoder& encoder) const
    {
        RELEASE_ASSERT(isValid());
        encoder << m_id;
    }

    template<class Decoder> static Optional<CallbackID> decode(Decoder& decoder)
    {
        Optional<uint64_t> identifier;
        decoder >> identifier;
        if (!identifier)
            return WTF::nullopt;
        RELEASE_ASSERT(isValidCallbackID(*identifier));
        return fromInteger(*identifier);
    }

private:
    ALWAYS_INLINE explicit CallbackID(uint64_t newID)
        : m_id(newID)
    {
        ASSERT(newID != HashTraits<uint64_t>::emptyValue());
    }

    friend class CallbackMap;
    template <typename CallbackType> friend class SpecificCallbackMap;
    friend class OptionalCallbackID;
    friend struct WTF::CallbackIDHash;
    friend HashTraits<WebKit::CallbackID>;

    uint64_t m_id { HashTraits<uint64_t>::emptyValue() };
};

}

namespace WTF {

struct CallbackIDHash {
    static unsigned hash(const WebKit::CallbackID& callbackID) { return intHash(callbackID.m_id); }
    static bool equal(const WebKit::CallbackID& a, const WebKit::CallbackID& b) { return a.m_id == b.m_id; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};
template<> struct HashTraits<WebKit::CallbackID> : GenericHashTraits<WebKit::CallbackID> {
    static WebKit::CallbackID emptyValue() { return WebKit::CallbackID(); }
    static void constructDeletedValue(WebKit::CallbackID& slot) { slot = WebKit::CallbackID(std::numeric_limits<uint64_t>::max()); }
    static bool isDeletedValue(const WebKit::CallbackID& slot) { return slot.m_id == std::numeric_limits<uint64_t>::max(); }
};
template<> struct DefaultHash<WebKit::CallbackID> {
    typedef CallbackIDHash Hash;
};

}
