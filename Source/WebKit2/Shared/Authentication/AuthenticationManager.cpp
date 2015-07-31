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
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/AuthenticationClient.h>

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessProxyMessages.h"
#endif

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

AuthenticationManager::AuthenticationManager(ChildProcess* process)
    : m_process(process)
{
    m_process->addMessageReceiver(Messages::AuthenticationManager::messageReceiverName(), *this);
}

uint64_t AuthenticationManager::addChallengeToChallengeMap(const WebCore::AuthenticationChallenge& authenticationChallenge)
{
    ASSERT(RunLoop::isMain());

    uint64_t challengeID = generateAuthenticationChallengeID();
    m_challenges.set(challengeID, authenticationChallenge);
    return challengeID;
}

bool AuthenticationManager::shouldCoalesceChallenge(uint64_t challengeID, const AuthenticationChallenge& challenge) const
{
    if (!canCoalesceChallenge(challenge))
        return false;

    auto end =  m_challenges.end();
    for (auto it = m_challenges.begin(); it != end; ++it) {
        if (it->key != challengeID && ProtectionSpace::compare(challenge.protectionSpace(), it->value.protectionSpace()))
            return true;
    }
    return false;
}

Vector<uint64_t> AuthenticationManager::coalesceChallengesMatching(uint64_t challengeID) const
{
    AuthenticationChallenge challenge = m_challenges.get(challengeID);
    ASSERT(!challenge.isNull());

    Vector<uint64_t> challengesToCoalesce;
    challengesToCoalesce.append(challengeID);

    if (!canCoalesceChallenge(challenge))
        return challengesToCoalesce;

    auto end = m_challenges.end();
    for (auto it = m_challenges.begin(); it != end; ++it) {
        if (it->key != challengeID && ProtectionSpace::compare(challenge.protectionSpace(), it->value.protectionSpace()))
            challengesToCoalesce.append(it->key);
    }

    return challengesToCoalesce;
}

void AuthenticationManager::didReceiveAuthenticationChallenge(WebFrame* frame, const AuthenticationChallenge& authenticationChallenge)
{
    ASSERT(frame);
    ASSERT(frame->page());

    uint64_t challengeID = addChallengeToChallengeMap(authenticationChallenge);

    // Coalesce challenges in the same protection space.
    if (shouldCoalesceChallenge(challengeID, authenticationChallenge))
        return;
    
    m_process->send(Messages::WebPageProxy::DidReceiveAuthenticationChallenge(frame->frameID(), authenticationChallenge, challengeID), frame->page()->pageID());
}

#if ENABLE(NETWORK_PROCESS)
void AuthenticationManager::didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const AuthenticationChallenge& authenticationChallenge)
{
    ASSERT(pageID);
    ASSERT(frameID);

    uint64_t challengeID = addChallengeToChallengeMap(authenticationChallenge);
    if (shouldCoalesceChallenge(challengeID, authenticationChallenge))
        return;
    
    m_process->send(Messages::NetworkProcessProxy::DidReceiveAuthenticationChallenge(pageID, frameID, authenticationChallenge, addChallengeToChallengeMap(authenticationChallenge)));
}
#endif

void AuthenticationManager::didReceiveAuthenticationChallenge(Download* download, const AuthenticationChallenge& authenticationChallenge)
{
    uint64_t challengeID = addChallengeToChallengeMap(authenticationChallenge);
    if (shouldCoalesceChallenge(challengeID, authenticationChallenge))
        return;

    download->send(Messages::DownloadProxy::DidReceiveAuthenticationChallenge(authenticationChallenge, addChallengeToChallengeMap(authenticationChallenge)));
}

// Currently, only Mac knows how to respond to authentication challenges with certificate info.
#if !HAVE(SEC_IDENTITY)
bool AuthenticationManager::tryUseCertificateInfoForChallenge(const WebCore::AuthenticationChallenge&, const CertificateInfo&)
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
    AuthenticationChallenge challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.isNull());
    
    if (tryUseCertificateInfoForChallenge(challenge, certificateInfo))
        return;
    
    AuthenticationClient* coreClient = challenge.authenticationClient();
    if (!coreClient) {
        // FIXME: The authentication client is null for downloads, but it can also be null for canceled loads.
        // We should not call Download::receivedCredential in the latter case.
        Download::receivedCredential(challenge, credential);
        return;
    }

    coreClient->receivedCredential(challenge, credential);
}

void AuthenticationManager::continueWithoutCredentialForChallenge(uint64_t challengeID)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        continueWithoutCredentialForSingleChallenge(coalescedChallengeID);
}

void AuthenticationManager::continueWithoutCredentialForSingleChallenge(uint64_t challengeID)
{
    AuthenticationChallenge challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.isNull());
    AuthenticationClient* coreClient = challenge.authenticationClient();
    if (!coreClient) {
        // FIXME: The authentication client is null for downloads, but it can also be null for canceled loads.
        // We should not call Download::receivedCredential in the latter case.
        Download::receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    coreClient->receivedRequestToContinueWithoutCredential(challenge);
}

void AuthenticationManager::cancelChallenge(uint64_t challengeID)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        cancelSingleChallenge(coalescedChallengeID);
}

void AuthenticationManager::cancelSingleChallenge(uint64_t challengeID)
{
    AuthenticationChallenge challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.isNull());
    AuthenticationClient* coreClient = challenge.authenticationClient();
    if (!coreClient) {
        // FIXME: The authentication client is null for downloads, but it can also be null for canceled loads.
        // We should not call Download::receivedCredential in the latter case.
        Download::receivedCancellation(challenge);
        return;
    }

    coreClient->receivedCancellation(challenge);
}

void AuthenticationManager::performDefaultHandling(uint64_t challengeID)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        performDefaultHandlingForSingleChallenge(coalescedChallengeID);
}

void AuthenticationManager::performDefaultHandlingForSingleChallenge(uint64_t challengeID)
{
    AuthenticationChallenge challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.isNull());
    AuthenticationClient* coreClient = challenge.authenticationClient();
    if (!coreClient) {
        // FIXME: The authentication client is null for downloads, but it can also be null for canceled loads.
        // We should not call Download::receivedCredential in the latter case.
        Download::receivedRequestToPerformDefaultHandling(challenge);
        return;
    }

    coreClient->receivedRequestToPerformDefaultHandling(challenge);
}

void AuthenticationManager::rejectProtectionSpaceAndContinue(uint64_t challengeID)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID))
        rejectProtectionSpaceAndContinueForSingleChallenge(coalescedChallengeID);
}

void AuthenticationManager::rejectProtectionSpaceAndContinueForSingleChallenge(uint64_t challengeID)
{
    AuthenticationChallenge challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.isNull());
    AuthenticationClient* coreClient = challenge.authenticationClient();
    if (!coreClient) {
        // FIXME: The authentication client is null for downloads, but it can also be null for canceled loads.
        // We should not call Download::receivedCredential in the latter case.
        Download::receivedChallengeRejection(challenge);
        return;
    }

    coreClient->receivedChallengeRejection(challenge);
}

} // namespace WebKit
