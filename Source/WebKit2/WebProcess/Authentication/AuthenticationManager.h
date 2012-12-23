/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef AuthenticationManager_h
#define AuthenticationManager_h

#include "MessageReceiver.h"
#include <wtf/HashMap.h>

namespace CoreIPC {
class ArgumentDecoder;
class Connection;
class MessageID;
class MessageReceiverMap;
}

namespace WebCore {
    class AuthenticationChallenge;
    class Credential;
}

namespace WebKit {

class ChildProcess;
class Download;
class PlatformCertificateInfo;
class WebFrame;

class AuthenticationManager : private CoreIPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(AuthenticationManager);

public:
    explicit AuthenticationManager(ChildProcess*);

    void setConnection(CoreIPC::Connection*);

    void didReceiveAuthenticationChallenge(WebFrame*, const WebCore::AuthenticationChallenge&);
    void didReceiveAuthenticationChallenge(Download*, const WebCore::AuthenticationChallenge&);

    void useCredentialForChallenge(uint64_t challengeID, const WebCore::Credential&, const PlatformCertificateInfo&);
    void continueWithoutCredentialForChallenge(uint64_t challengeID);
    void cancelChallenge(uint64_t challengeID);

private:
    // CoreIPC::MessageReceiver
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&) OVERRIDE;
    void didReceiveAuthenticationManagerMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);

    bool tryUsePlatformCertificateInfoForChallenge(const WebCore::AuthenticationChallenge&, const PlatformCertificateInfo&);

    RefPtr<CoreIPC::Connection> m_connection;

    typedef HashMap<uint64_t, WebCore::AuthenticationChallenge> AuthenticationChallengeMap;
    AuthenticationChallengeMap m_challenges;
};

} // namespace WebKit

#endif // AuthenticationManager_h
