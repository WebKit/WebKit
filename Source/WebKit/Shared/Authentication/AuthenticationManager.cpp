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
#include "Download.h"
#include "DownloadProxyMessages.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "PendingDownload.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/AuthenticationChallenge.h>

namespace WebKit {
using namespace WebCore;

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

AuthenticationManager::AuthenticationManager(NetworkProcess& process)
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

bool AuthenticationManager::shouldCoalesceChallenge(PageIdentifier pageID, uint64_t challengeID, const AuthenticationChallenge& challenge) const
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

void AuthenticationManager::didReceiveAuthenticationChallenge(PAL::SessionID sessionID, PageIdentifier pageID, FrameIdentifier frameID, const AuthenticationChallenge& authenticationChallenge, ChallengeCompletionHandler&& completionHandler)
{
    ASSERT(pageID);
    ASSERT(frameID);

    uint64_t challengeID = addChallengeToChallengeMap({ pageID, authenticationChallenge, WTFMove(completionHandler) });

    // Coalesce challenges in the same protection space and in the same page.
    if (shouldCoalesceChallenge(pageID, challengeID, authenticationChallenge))
        return;
    
    m_process.send(Messages::NetworkProcessProxy::DidReceiveAuthenticationChallenge(sessionID, pageID, frameID, authenticationChallenge, challengeID));
}

void AuthenticationManager::didReceiveAuthenticationChallenge(IPC::MessageSender& download, const WebCore::AuthenticationChallenge& authenticationChallenge, ChallengeCompletionHandler&& completionHandler)
{
    PageIdentifier dummyPageID;
    uint64_t challengeID = addChallengeToChallengeMap({ dummyPageID, authenticationChallenge, WTFMove(completionHandler) });
    
    // Coalesce challenges in the same protection space and in the same page.
    if (shouldCoalesceChallenge(dummyPageID, challengeID, authenticationChallenge))
        return;
    
    download.send(Messages::DownloadProxy::DidReceiveAuthenticationChallenge(authenticationChallenge, challengeID));
}

void AuthenticationManager::completeAuthenticationChallenge(uint64_t challengeID, AuthenticationChallengeDisposition disposition, WebCore::Credential&& credential)
{
    ASSERT(RunLoop::isMain());

    for (auto& coalescedChallengeID : coalesceChallengesMatching(challengeID)) {
        auto challenge = m_challenges.take(coalescedChallengeID);
        ASSERT(!challenge.challenge.isNull());
        challenge.completionHandler(disposition, credential);
    }
}

} // namespace WebKit
