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
#include "NetworkSession.h"
#include "WebProcessSupplement.h"
#include <WebCore/AuthenticationChallenge.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>

namespace WebCore {
class AuthenticationChallenge;
class CertificateInfo;
class Credential;
}

namespace WebKit {

class ChildProcess;
class Download;
class DownloadID;
class PendingDownload;
class WebFrame;

class AuthenticationManager : public WebProcessSupplement, public NetworkProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(AuthenticationManager);
public:
    explicit AuthenticationManager(ChildProcess*);

    static const char* supplementName();

#if USE(NETWORK_SESSION)
    void didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler);
    void didReceiveAuthenticationChallenge(PendingDownload&, const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler);
    void continueCanAuthenticateAgainstProtectionSpace(DownloadID, bool canAuthenticate);
#endif
    // Called for resources in the WebProcess (NetworkProcess disabled)
    void didReceiveAuthenticationChallenge(WebFrame*, const WebCore::AuthenticationChallenge&);
    // Called for resources in the NetworkProcess (NetworkProcess enabled)
    void didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const WebCore::AuthenticationChallenge&);
#if !USE(NETWORK_SESSION)
    void didReceiveAuthenticationChallenge(Download&, const WebCore::AuthenticationChallenge&);
#endif

    void useCredentialForChallenge(uint64_t challengeID, const WebCore::Credential&, const WebCore::CertificateInfo&);
    void continueWithoutCredentialForChallenge(uint64_t challengeID);
    void cancelChallenge(uint64_t challengeID);
    void performDefaultHandling(uint64_t challengeID);
    void rejectProtectionSpaceAndContinue(uint64_t challengeID);

    uint64_t outstandingAuthenticationChallengeCount() const { return m_challenges.size(); }

    static void receivedCredential(const WebCore::AuthenticationChallenge&, const WebCore::Credential&);
    static void receivedRequestToContinueWithoutCredential(const WebCore::AuthenticationChallenge&);
    static void receivedCancellation(const WebCore::AuthenticationChallenge&);
    static void receivedRequestToPerformDefaultHandling(const WebCore::AuthenticationChallenge&);
    static void receivedChallengeRejection(const WebCore::AuthenticationChallenge&);

private:
    struct Challenge {
        uint64_t pageID;
        WebCore::AuthenticationChallenge challenge;
#if USE(NETWORK_SESSION)
        ChallengeCompletionHandler completionHandler;
#endif
    };
    
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    bool tryUseCertificateInfoForChallenge(const WebCore::AuthenticationChallenge&, const WebCore::CertificateInfo&);

    uint64_t addChallengeToChallengeMap(const Challenge&);
    bool shouldCoalesceChallenge(uint64_t pageID, uint64_t challengeID, const WebCore::AuthenticationChallenge&) const;

    void useCredentialForSingleChallenge(uint64_t challengeID, const WebCore::Credential&, const WebCore::CertificateInfo&);
    void continueWithoutCredentialForSingleChallenge(uint64_t challengeID);
    void cancelSingleChallenge(uint64_t challengeID);
    void performDefaultHandlingForSingleChallenge(uint64_t challengeID);
    void rejectProtectionSpaceAndContinueForSingleChallenge(uint64_t challengeID);

    Vector<uint64_t> coalesceChallengesMatching(uint64_t challengeID) const;

    ChildProcess* m_process;

    HashMap<uint64_t, Challenge> m_challenges;
};

} // namespace WebKit

#endif // AuthenticationManager_h
