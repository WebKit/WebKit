/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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
#include "NetworkProcessSupplement.h"
#include "WebProcessSupplement.h"
#include <WebCore/AuthenticationChallenge.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class MessageSender;
}

namespace WebCore {
class AuthenticationChallenge;
class Credential;
}

namespace WebKit {

class ChildProcess;
class Download;
class DownloadID;
class WebFrame;

enum class AuthenticationChallengeDisposition : uint8_t;
using ChallengeCompletionHandler = CompletionHandler<void(AuthenticationChallengeDisposition, const WebCore::Credential&)>;

class AuthenticationManager : public NetworkProcessSupplement, public IPC::MessageReceiver, public CanMakeWeakPtr<AuthenticationManager> {
    WTF_MAKE_NONCOPYABLE(AuthenticationManager);
public:
    explicit AuthenticationManager(ChildProcess&);

    static const char* supplementName();

    void didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&);
    void didReceiveAuthenticationChallenge(IPC::MessageSender& download, const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&);

    void completeAuthenticationChallenge(uint64_t challengeID, AuthenticationChallengeDisposition, WebCore::Credential&&);

    uint64_t outstandingAuthenticationChallengeCount() const { return m_challenges.size(); }

private:
    struct Challenge {
        uint64_t pageID;
        WebCore::AuthenticationChallenge challenge;
        ChallengeCompletionHandler completionHandler;
    };

#if HAVE(SEC_KEY_PROXY)
    // NetworkProcessSupplement
    void initializeConnection(IPC::Connection*) final;
#endif
    
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    uint64_t addChallengeToChallengeMap(Challenge&&);
    bool shouldCoalesceChallenge(uint64_t pageID, uint64_t challengeID, const WebCore::AuthenticationChallenge&) const;

    Vector<uint64_t> coalesceChallengesMatching(uint64_t challengeID) const;

    ChildProcess& m_process;

    HashMap<uint64_t, Challenge> m_challenges;
};

} // namespace WebKit

#endif // AuthenticationManager_h
