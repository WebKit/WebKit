/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteObjectRegistry.h"

#import "MessageSender.h"
#import "RemoteObjectInvocation.h"
#import "RemoteObjectRegistryMessages.h"
#import "UserData.h"
#import "_WKRemoteObjectRegistryInternal.h"

namespace WebKit {

RemoteObjectRegistry::RemoteObjectRegistry(_WKRemoteObjectRegistry *remoteObjectRegistry)
    : m_remoteObjectRegistry(remoteObjectRegistry)
{
}

RemoteObjectRegistry::~RemoteObjectRegistry()
{
}

void RemoteObjectRegistry::sendInvocation(const RemoteObjectInvocation& invocation)
{

    if (auto* replyInfo = invocation.replyInfo()) {
        ASSERT(!m_pendingReplies.contains(replyInfo->replyID));
        m_pendingReplies.add(replyInfo->replyID, takeBackgroundActivityToken());
    }

    messageSender().send(Messages::RemoteObjectRegistry::InvokeMethod(invocation));
}

void RemoteObjectRegistry::sendReplyBlock(uint64_t replyID, const UserData& blockInvocation)
{
    messageSender().send(Messages::RemoteObjectRegistry::CallReplyBlock(replyID, blockInvocation));
}

void RemoteObjectRegistry::sendUnusedReply(uint64_t replyID)
{
    messageSender().send(Messages::RemoteObjectRegistry::ReleaseUnusedReplyBlock(replyID));
}

void RemoteObjectRegistry::invokeMethod(const RemoteObjectInvocation& invocation)
{
    [m_remoteObjectRegistry _invokeMethod:invocation];
}

void RemoteObjectRegistry::callReplyBlock(uint64_t replyID, const UserData& blockInvocation)
{
    bool wasRemoved = m_pendingReplies.remove(replyID);
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    [m_remoteObjectRegistry _callReplyWithID:replyID blockInvocation:blockInvocation];
}

void RemoteObjectRegistry::releaseUnusedReplyBlock(uint64_t replyID)
{
    bool wasRemoved = m_pendingReplies.remove(replyID);
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    [m_remoteObjectRegistry _releaseReplyWithID:replyID];
}

} // namespace WebKit
