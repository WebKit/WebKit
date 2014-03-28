/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef SessionID_h
#define SessionID_h

namespace WebCore {

class SessionID {
public:
    SessionID()
        : SessionID(emptySessionID()) { }
    explicit SessionID(uint64_t sessionID)
        : m_sessionID(sessionID) { }
    bool isValid() const { return *this != emptySessionID(); }
    bool isEphemeral() const { return *this != defaultSessionID(); }
    uint64_t sessionID() const { return m_sessionID; }
    bool operator==(SessionID sessionID) const { return m_sessionID == sessionID.m_sessionID; }
    bool operator!=(SessionID sessionID) const { return m_sessionID != sessionID.m_sessionID; }

    static SessionID emptySessionID() { return SessionID(0); }
    static SessionID defaultSessionID() { return SessionID(1); }
    static SessionID legacyPrivateSessionID() { return SessionID(2); }
    static SessionID bypassCacheSessionID() { return SessionID(3); }
private:
    uint64_t m_sessionID;
};

} // namespace WebCore

#endif // SessionID_h
