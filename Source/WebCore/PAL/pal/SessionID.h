/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include <optional>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>

namespace PAL {

class SessionID {
public:
    SessionID() = delete;

    enum SessionConstants : uint64_t {
        EphemeralSessionMask    = 0x8000000000000000,
        DefaultSessionID        = 1,
        LegacyPrivateSessionID  = DefaultSessionID | EphemeralSessionMask,
        HashTableEmptyValueID   = 0,
        HashTableDeletedValueID = std::numeric_limits<uint64_t>::max(),
    };

    static SessionID defaultSessionID() { return SessionID(DefaultSessionID); }
    static SessionID legacyPrivateSessionID() { return SessionID(LegacyPrivateSessionID); }

    explicit SessionID(WTF::HashTableDeletedValueType)
        : m_identifier(HashTableDeletedValueID)
    {
    }

    explicit SessionID(WTF::HashTableEmptyValueType)
        : m_identifier(HashTableEmptyValueID)
    {
    }

    explicit SessionID(uint64_t identifier)
        : m_identifier(identifier)
    {
    }

    PAL_EXPORT static SessionID generateEphemeralSessionID();
    PAL_EXPORT static SessionID generatePersistentSessionID();
    PAL_EXPORT static void enableGenerationProtection();

    bool isValid() const { return isValidSessionIDValue(m_identifier); }
    bool isEphemeral() const { return m_identifier & EphemeralSessionMask && m_identifier != HashTableDeletedValueID; }
    bool isHashTableDeletedValue() const { return m_identifier == HashTableDeletedValueID; }

    uint64_t toUInt64() const { return m_identifier; }
    friend bool operator==(SessionID, SessionID) = default;
    bool isAlwaysOnLoggingAllowed() const { return !isEphemeral(); }

    SessionID isolatedCopy() const { return *this; }

    explicit operator bool() const { return m_identifier; }

    static bool isValidSessionIDValue(uint64_t sessionID) { return sessionID != HashTableEmptyValueID && sessionID != HashTableDeletedValueID; }
private:
    uint64_t m_identifier;
};

} // namespace PAL

namespace WTF {

struct SessionIDHash {
    static unsigned hash(const PAL::SessionID& p) { return intHash(p.toUInt64()); }
    static bool equal(const PAL::SessionID& a, const PAL::SessionID& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<PAL::SessionID> : GenericHashTraits<PAL::SessionID> {
    static PAL::SessionID emptyValue() { return PAL::SessionID(HashTableEmptyValue); }
    static void constructDeletedValue(PAL::SessionID& slot) { new (NotNull, &slot) PAL::SessionID(HashTableDeletedValue); }
    static bool isDeletedValue(const PAL::SessionID& slot) { return slot.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<PAL::SessionID> : SessionIDHash { };

} // namespace WTF
