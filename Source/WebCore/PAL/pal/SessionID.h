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

#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>

namespace PAL {

class SessionID {
public:
    SessionID()
        : SessionID(emptySessionID())
    {
    }

    enum SessionConstants : uint64_t {
        EphemeralSessionMask    = 0x8000000000000000,
        DefaultSessionID        = 1,
        LegacyPrivateSessionID  = DefaultSessionID | EphemeralSessionMask,
        HashTableEmptyValueID   = 0,
        HashTableDeletedValueID = std::numeric_limits<uint64_t>::max(),
    };

    static SessionID emptySessionID() { return SessionID(HashTableEmptyValueID); }
    static SessionID hashTableDeletedValue() { return SessionID(HashTableDeletedValueID); }
    static SessionID defaultSessionID() { return SessionID(DefaultSessionID); }
    static SessionID legacyPrivateSessionID() { return SessionID(LegacyPrivateSessionID); }

    bool isValid() const { return m_sessionID != HashTableEmptyValueID && m_sessionID != HashTableDeletedValueID; }
    bool isEphemeral() const { return m_sessionID & EphemeralSessionMask && m_sessionID != HashTableDeletedValueID; }

    PAL_EXPORT static SessionID generateEphemeralSessionID();
    PAL_EXPORT static SessionID generatePersistentSessionID();
    PAL_EXPORT static void enableGenerationProtection();

    uint64_t sessionID() const { return m_sessionID; }
    bool operator==(SessionID sessionID) const { return m_sessionID == sessionID.m_sessionID; }
    bool operator!=(SessionID sessionID) const { return m_sessionID != sessionID.m_sessionID; }
    bool isAlwaysOnLoggingAllowed() const { return !isEphemeral(); }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, SessionID&);
    template<class Decoder> static Optional<SessionID> decode(Decoder&);

    SessionID isolatedCopy() const;

private:
    explicit SessionID(uint64_t sessionID)
        : m_sessionID(sessionID)
    {
    }

    uint64_t m_sessionID;
};

template<class Encoder>
void SessionID::encode(Encoder& encoder) const
{
    // FIXME: Eliminate places that encode invalid SessionIDs, then ASSERT here that the sessionID is valid.
    encoder << m_sessionID;
}

template<class Decoder>
bool SessionID::decode(Decoder& decoder, SessionID& sessionID)
{
    Optional<SessionID> decodedSessionID;
    decoder >> decodedSessionID;
    if (!decodedSessionID)
        return false;

    sessionID = decodedSessionID.value();
    return true;
}

template<class Decoder>
Optional<SessionID> SessionID::decode(Decoder& decoder)
{
    Optional<uint64_t> sessionID;
    decoder >> sessionID;
    if (!sessionID)
        return WTF::nullopt;

    // FIXME: Eliminate places that encode invalid SessionIDs, then fail to decode an invalid sessionID.
    return SessionID { *sessionID };
}

} // namespace PAL

namespace WTF {

struct SessionIDHash {
    static unsigned hash(const PAL::SessionID& p) { return intHash(p.sessionID()); }
    static bool equal(const PAL::SessionID& a, const PAL::SessionID& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};
template<> struct HashTraits<PAL::SessionID> : GenericHashTraits<PAL::SessionID> {
    static PAL::SessionID emptyValue() { return PAL::SessionID::emptySessionID(); }

    static void constructDeletedValue(PAL::SessionID& slot) { slot = PAL::SessionID::hashTableDeletedValue(); }
    static bool isDeletedValue(const PAL::SessionID& slot) { return slot == PAL::SessionID::hashTableDeletedValue(); }
};
template<> struct DefaultHash<PAL::SessionID> {
    typedef SessionIDHash Hash;
};

} // namespace WTF
