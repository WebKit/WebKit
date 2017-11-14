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

#include "config.h"
#include "AuthenticationManager.h"

#include "AuthenticationManagerMessages.h"
#include "ChildProcess.h"
#include "Download.h"
#include "DownloadProxyMessages.h"
#include "NetworkProcessProxyMessages.h"
#include "PendingDownload.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/AuthenticationClient.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateAuthenticationChallengeID()
{
    ASSERT(RunLoop::isMain());

    static int64_t uniqueAuthenticationChallengeID;
    return ++uniqueAuthenticationChallengeID;
}

static bool canCoalesceChallenge(const WebCore::AuthenticationChallenge& challenge)
{
    // Do not coalesce server trust evaluation requests because ProtectionSpace comparison does not evaluate server trust (e.g. certificate).
    return challenge.protectionSpace().authenticationScheme() != ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested;
}

const char* AuthenticationManager::supplementName()
{
    return "AuthenticationManager";
}

AuthenticationManager::AuthenticationManager(ChildProcess& process)
    : m_process(process)
{
    m_process.addMessageReceiver(Messages::AuthenticationManager::messageReceiverName(), *this);
}

uint64_t AuthenticationManager::addChallengeToChallengeMap(Challenge&& challenge)
{
    ASSERT(RunLoop::isMain());

    uint64_t challengeID = generateAuthenticationChallengeID();
    m_challenges.set(challengeID, WTFMove(challenge));
    return challengeID;
}

bool AuthenticationManager::shouldCoalesceChallenge(uint64_t pageID, uint64_t challengeID, const AuthenticationChallenge& challenge) const
{
    if (!canCoalesceChallenge(challenge))
        return false;

    for (auto& item : m_challenges) {
        if (item.key != challengeID && item.value.pageID == pageID && ProtectionSpace::compare(challenge.protectionSpace(), item.value.challenge.protectionSpace()))
            return true;
    }
    return false;
}

Vector<uint64_t> AuthenticationManager::coalesceChallengesMatching(uint64_t challengeID) const
{
    auto iterator = m_challenges.find(challengeID);
    ASSERT(iterator != m_challenges.end());

    auto& challenge = iterator->value;

    Vector<uint64_t> challengesToCoalesce;
    challengesToCoalesce.append(challengeID);

    if (!canCoalesceChallenge(challenge.challenge))
        return challengesToCoalesce;

    for (auto& item : m_challenges) {
        if (item.key != challengeID && item.value.pageID == challenge.pageID && ProtectionSpace::compare(challenge.challenge.protectionSpace(), item.value.challenge.protectionSpace()))
            challengesToCoalesce.append(item.key);
    }

    return challengesToCoalesce;
}

void AuthenticationManager::didReceiveAuthenticationChallenge(WebFrame* frame, const AuthenticationChallenge& authenticationChallenge)
{
    ASSERT(frame);
    ASSERT(frame->page());

    auto pageID = frame->page()->pageID();
    uint64_t challengeID = addChallengeToChallengeMap({pageID, authenticationChallenge
#if USE(NETWORK_SESSION)
        , { }
#endif
    });

    // Coalesce challenges in the same protection space and in the same page.
    if (shouldCoalesceChallenge(pageID, challengeID, authenticationChallenge))
        return;
    
    m_process.send(Messages::WebPageProxy::DidReceiveAuthenticationChallenge(frame->frameID(), authenticationChallenge, challengeID), frame->page()->pageID());
}

#if USE(NETWORK_SESSION)
void AuthenticationManager::didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const AuthenticationChallenge& authenticationChallenge, ChallengeCompletionHandler&& completionHandler)
{
    ASSERT(pageID);
    ASSERT(frameID);

    uint64_t challengeID = addChallengeToChallengeMap({ pageID, authenticationChallenge, WTFMove(completionHandler) });

    // Coalesce challenges in the same protection space and in the same page.
    if (shouldCoalesceChallenge(pageID, challengeID, authenticationChallenge))
        return;
    
    m_process.send(Messages::NetworkProcessProxy::DidReceiveAuthenticationChallenge(pageID, frameID, authenticationChallenge, challengeID));
}

void AuthenticationManager::didReceiveAuthenticationChallenge(IPC::MessageSender& download, const WebCore::AuthenticationChallenge& authenticationChallenge, ChallengeCompletionHandler&& completionHandler)
{
    uint64_t dummyPageID = 0;
    uint64_t challengeID = addChallengeToChallengeMap({ dummyPageID, authenticationChallenge, WTFMove(completionHandler) });
    
    // Coalesce challenges in the same protection space and in the same page.
    if (shouldCoalesceChallenge(dummyPageID, challengeID, authenticationChallenge))
        return;
    
    download.send(Messages::DownloadProxy::DidReceiveAuthenticationChallenge(authenticationChallenge, challengeID));
}
#endif

#if !USE(NETWORK_SESSION)
void AuthenticationManager::didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const AuthenticationChallenge& authenticationChallenge)
{
    ASSERT(pageID);
    ASSERT(frameID);

    uint64_t challengeID = addChallengeToChallengeMap({pageID, authenticationChallenge});

    // Coalesce challenges in the same protection space and in the same page.
    if (shouldCoalesceChallenge(pageID, challengeID, authenticationChallenge))
        return;
    
    m_process.send(Messages::NetworkProcessProxy::DidReceiveAuthenticationChallenge(pageID, frameID, authenticationChallenge, challengeID));
}
#endif

#if !USE(NETWORK_SESSION)
void AuthenticationManager::didReceiveAuthenticationChallenge(Download& download, const AuthenticationChallenge& authenticationChallenge)
{
    uint64_t dummyPageID = 0;
    uint64_t challengeID = addChallengeToChallengeMap({dummyPageID, authenticationChallenge});

    // Coalesce challenges in the same protection space and in the same page.
    if (shouldCoalesceChallenge(dummyPageID, challengeID, authenticationChallenge))
        return;

    download.send(Messages::DownloadProxy::DidReceiveAuthenticationChallenge(authenticationChallenge, challengeID));
}
#endif

// Currently, only Mac knows how to respond to authentication challenges with certificate info.
#if !HAVE(SEC_IDENTITY)
bool AuthenticationManager::tryUseCertificateInfoForChallenge(const WebCore::AuthenticationChallenge&, const CertificateInfo&, ChallengeCompletionHandler&)
{
    return false;
}
#endif

void AuthenticationManager::useCredentialForChallenge(uint64_t challengeID, const Credential& credential, const CertificateInfo& certificateInfo)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        useCredentialForSingleChallenge(coalescedChallengeID, credential, certificateInfo);
}

void AuthenticationManager::useCredentialForSingleChallenge(uint64_t challengeID, const Credential& credential, const CertificateInfo& certificateInfo)
{
    auto challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.challenge.isNull());

#if USE(NETWORK_SESSION)
    auto completionHandler = WTFMove(challenge.completionHandler);
#else
    ChallengeCompletionHandler completionHandler = nullptr;
#endif
    
    if (tryUseCertificateInfoForChallenge(challenge.challenge, certificateInfo, completionHandler))
        return;

    AuthenticationClient* coreClient = challenge.challenge.authenticationClient();
#if USE(NETWORK_SESSION)
    // If there is a completion handler, then there is no AuthenticationClient.
    // FIXME: Remove the use of AuthenticationClient in WebKit2 once NETWORK_SESSION is used for all loads.
    if (completionHandler) {
        ASSERT(!coreClient);
        completionHandler(AuthenticationChallengeDisposition::UseCredential, credential);
        return;
    }
    ASSERT(coreClient);
#endif

    if (coreClient)
        coreClient->receivedCredential(challenge.challenge, credential);
#if !USE(NETWORK_SESSION)
    else
        receivedCredential(challenge.challenge, credential);
#endif
}

void AuthenticationManager::continueWithoutCredentialForChallenge(uint64_t challengeID)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        continueWithoutCredentialForSingleChallenge(coalescedChallengeID);
}

void AuthenticationManager::continueWithoutCredentialForSingleChallenge(uint64_t challengeID)
{
    auto challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.challenge.isNull());

    AuthenticationClient* coreClient = challenge.challenge.authenticationClient();
#if USE(NETWORK_SESSION)
    if (challenge.completionHandler) {
        ASSERT(!coreClient);
        challenge.completionHandler(AuthenticationChallengeDisposition::UseCredential, Credential());
        return;
    }
    ASSERT(coreClient);
#endif

    if (coreClient)
        coreClient->receivedRequestToContinueWithoutCredential(challenge.challenge);
#if !USE(NETWORK_SESSION)
    else
        receivedRequestToContinueWithoutCredential(challenge.challenge);
#endif
}

void AuthenticationManager::cancelChallenge(uint64_t challengeID)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        cancelSingleChallenge(coalescedChallengeID);
}

void AuthenticationManager::cancelSingleChallenge(uint64_t challengeID)
{
    auto challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.challenge.isNull());

    AuthenticationClient* coreClient = challenge.challenge.authenticationClient();
#if USE(NETWORK_SESSION)
    if (challenge.completionHandler) {
        ASSERT(!coreClient);
        challenge.completionHandler(AuthenticationChallengeDisposition::Cancel, Credential());
        return;
    }
    ASSERT(coreClient);
#endif

    if (coreClient)
        coreClient->receivedCancellation(challenge.challenge);
#if !USE(NETWORK_SESSION)
    else
        receivedCancellation(challenge.challenge);
#endif
}

void AuthenticationManager::performDefaultHandling(uint64_t challengeID)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        performDefaultHandlingForSingleChallenge(coalescedChallengeID);
}

void AuthenticationManager::performDefaultHandlingForSingleChallenge(uint64_t challengeID)
{
    auto challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.challenge.isNull());

    AuthenticationClient* coreClient = challenge.challenge.authenticationClient();
#if USE(NETWORK_SESSION)
    if (challenge.completionHandler) {
        ASSERT(!coreClient);
        challenge.completionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, Credential());
        return;
    }
    ASSERT(coreClient);
#endif

    if (coreClient)
        coreClient->receivedRequestToPerformDefaultHandling(challenge.challenge);
#if !USE(NETWORK_SESSION)
    else
        receivedRequestToPerformDefaultHandling(challenge.challenge);
#endif
}

void AuthenticationManager::rejectProtectionSpaceAndContinue(uint64_t challengeID)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        rejectProtectionSpaceAndContinueForSingleChallenge(coalescedChallengeID);
}

void AuthenticationManager::rejectProtectionSpaceAndContinueForSingleChallenge(uint64_t challengeID)
{
    auto challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.challenge.isNull());

    AuthenticationClient* coreClient = challenge.challenge.authenticationClient();
#if USE(NETWORK_SESSION)
    if (challenge.completionHandler) {
        ASSERT(!coreClient);
        challenge.completionHandler(AuthenticationChallengeDisposition::RejectProtectionSpace, Credential());
        return;
    }
    ASSERT(coreClient);
#endif

    if (coreClient)
        coreClient->receivedChallengeRejection(challenge.challenge);
#if !USE(NETWORK_SESSION)
    else
        receivedChallengeRejection(challenge.challenge);
#endif
}

} // namespace WebKit
