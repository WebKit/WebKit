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

#ifndef SessionTracker_h
#define SessionTracker_h

#include <WebCore/NetworkStorageSession.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/CString.h>

namespace WebKit {

class SessionTracker {
    WTF_MAKE_NONCOPYABLE(SessionTracker);
public:
    static const uint64_t defaultSessionID = 1;
    static const uint64_t legacyPrivateSessionID = 2;
    static bool isEphemeralID(uint64_t sessionID) { return sessionID != SessionTracker::defaultSessionID; }
    static const HashMap<uint64_t, std::unique_ptr<WebCore::NetworkStorageSession>>& getSessionMap();
    static const String& getIdentifierBase();
    static std::unique_ptr<WebCore::NetworkStorageSession>& session(uint64_t sessionID);
    static void destroySession(uint64_t sessionID);
    static void setIdentifierBase(const String&);
};

} // namespace WebKit

#endif // SessionTracker_h
