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

#include "CallbackID.h"
#include <wtf/HashTraits.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class OptionalCallbackID {
public:
    explicit OptionalCallbackID() { }

    ALWAYS_INLINE explicit OptionalCallbackID(const CallbackID& otherID)
        : m_id(otherID.m_id)
    {
    }

    ALWAYS_INLINE OptionalCallbackID(const OptionalCallbackID& otherID)
        : m_id(otherID.m_id)
    {
        ASSERT(!HashTraits<uint64_t>::isDeletedValue(m_id));
    }

    ALWAYS_INLINE OptionalCallbackID& operator=(const OptionalCallbackID& otherID)
    {
        m_id = otherID.m_id;
        return *this;
    }

    uint64_t toInteger() { return m_id; }
    CallbackID callbackID()
    {
        RELEASE_ASSERT(CallbackID::isValidCallbackID(m_id));
        return CallbackID(m_id);
    }

    operator bool() { return m_id; }

    ALWAYS_INLINE bool isValid() const { return isValidCallbackID(m_id); }
    static ALWAYS_INLINE bool isValidCallbackID(uint64_t rawId)
    {
        return !HashTraits<uint64_t>::isDeletedValue(rawId);
    }

    template<class Encoder> void encode(Encoder& encoder) const
    {
        RELEASE_ASSERT(isValid());
        encoder << m_id;
    }

    template<class Decoder> static bool decode(Decoder& decoder, OptionalCallbackID& callbackID)
    {
        auto result = decoder.decode(callbackID.m_id);
        RELEASE_ASSERT(callbackID.isValid());
        return result;
    }

private:
    uint64_t m_id { HashTraits<uint64_t>::emptyValue() };
};

}
