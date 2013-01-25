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

using namespace WebCore;

namespace WebKit {

static uint64_t generateAuthenticationChallengeID()
{
    static uint64_t uniqueAuthenticationChallengeID = 1;
    return uniqueAuthenticationChallengeID++;
}

const AtomicString& AuthenticationManager::supplementName()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("AuthenticationManager", AtomicString::ConstructFromLiteral));
    return name;
}

AuthenticationManager::AuthenticationManager(ChildProcess* process)
    : m_process(process)
{
    m_process->addMessageReceiver(Messages::AuthenticationManager::messageReceiverName(), this);
}

void AuthenticationManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    didReceiveAuthenticationManagerMessage(connection, messageID, decoder);
}

void AuthenticationManager::didReceiveAuthenticationChallenge(WebFrame* frame, const AuthenticationChallenge& authenticationChallenge)
{
    ASSERT(frame);
    ASSERT(frame->page());

    uint64_t challengeID = generateAuthenticationChallengeID();
    m_challenges.set(challengeID, authenticationChallenge);    
    
    m_process->send(Messages::WebPageProxy::DidReceiveAuthenticationChallenge(frame->frameID(), authenticationChallenge, challengeID), frame->page()->pageID());
}

void AuthenticationManager::didReceiveAuthenticationChallenge(Download* download, const AuthenticationChallenge& authenticationChallenge)
{
    uint64_t challengeID = generateAuthenticationChallengeID();
    m_challenges.set(challengeID, authenticationChallenge);

    download->send(Messages::DownloadProxy::DidReceiveAuthenticationChallenge(authenticationChallenge, challengeID));
}

// Currently, only Mac knows how to respond to authentication challenges with certificate info.
#if !USE(SECURITY_FRAMEWORK)
bool AuthenticationManager::tryUsePlatformCertificateInfoForChallenge(const WebCore::AuthenticationChallenge&, const PlatformCertificateInfo&)
{
    return false;
}
#endif

void AuthenticationManager::useCredentialForChallenge(uint64_t challengeID, const Credential& credential, const PlatformCertificateInfo& certificateInfo)
{
    AuthenticationChallenge challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.isNull());
    
    if (tryUsePlatformCertificateInfoForChallenge(challenge, certificateInfo))
        return;
    
    AuthenticationClient* coreClient = challenge.authenticationClient();
    if (!coreClient) {
        // This authentication challenge comes from a download.
        Download::receivedCredential(challenge, credential);
        return;
    }

    coreClient->receivedCredential(challenge, credential);
}

void AuthenticationManager::continueWithoutCredentialForChallenge(uint64_t challengeID)
{
    AuthenticationChallenge challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.isNull());
    AuthenticationClient* coreClient = challenge.authenticationClient();
    if (!coreClient) {
        // This authentication challenge comes from a download.
        Download::receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    coreClient->receivedRequestToContinueWithoutCredential(challenge);
}

void AuthenticationManager::cancelChallenge(uint64_t challengeID)
{
    AuthenticationChallenge challenge = m_challenges.take(challengeID);
    ASSERT(!challenge.isNull());
    AuthenticationClient* coreClient = challenge.authenticationClient();
    if (!coreClient) {
        // This authentication challenge comes from a download.
        Download::receivedCancellation(challenge);
        return;
    }

    coreClient->receivedCancellation(challenge);
}

} // namespace WebKit
