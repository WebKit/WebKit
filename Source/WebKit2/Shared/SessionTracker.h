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

namespace WebCore {
class NetworkStorageSession;
}

#include <WebCore/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class NetworkSession;

class SessionTracker {
    WTF_MAKE_NONCOPYABLE(SessionTracker);
public:
    static const String& getIdentifierBase();
    static void setIdentifierBase(const String&);

    static WebCore::NetworkStorageSession* storageSession(WebCore::SessionID);
    
    // FIXME: Remove uses of this as it is just a member access into NetworkStorageSession.
    static WebCore::SessionID sessionID(const WebCore::NetworkStorageSession&);

#if USE(NETWORK_SESSION)
    static void setSession(WebCore::SessionID, Ref<NetworkSession>&&);
#else
    static void setSession(WebCore::SessionID, std::unique_ptr<WebCore::NetworkStorageSession>);
#endif
    static void destroySession(WebCore::SessionID);
    
#if USE(NETWORK_SESSION)
    // FIXME: A NetworkSession and a NetworkStorageSession should be the same object once NETWORK_SESSION is used by default.
    static NetworkSession* networkSession(WebCore::SessionID);
#endif

    // FIXME: This does not include the default network storage sesion in it's iteration.
    static void forEachNetworkStorageSession(std::function<void(const WebCore::NetworkStorageSession&)>);
};

} // namespace WebKit

#endif // SessionTracker_h
