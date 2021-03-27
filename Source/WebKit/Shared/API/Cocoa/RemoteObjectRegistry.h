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

#pragma once

#include "MessageReceiver.h"
#include "ProcessThrottler.h"
#include "WebPageProxyIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/WeakObjCPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS _WKRemoteObjectRegistry;

namespace IPC {
class MessageSender;
}

namespace WebKit {

class RemoteObjectInvocation;
class UserData;

class RemoteObjectRegistry : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~RemoteObjectRegistry();

    virtual void sendInvocation(const RemoteObjectInvocation&);
    void sendReplyBlock(uint64_t replyID, const UserData& blockInvocation);
    void sendUnusedReply(uint64_t replyID);

protected:
    explicit RemoteObjectRegistry(_WKRemoteObjectRegistry *);
    
private:
    virtual std::unique_ptr<ProcessThrottler::BackgroundActivity> backgroundActivity(ASCIILiteral) { return nullptr; }
    virtual IPC::MessageSender& messageSender() = 0;
    virtual uint64_t messageDestinationID() = 0;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Message handlers
    void invokeMethod(const RemoteObjectInvocation&);
    void callReplyBlock(uint64_t replyID, const UserData& blockInvocation);
    void releaseUnusedReplyBlock(uint64_t replyID);

    WeakObjCPtr<_WKRemoteObjectRegistry> m_remoteObjectRegistry;
    HashMap<uint64_t, std::unique_ptr<ProcessThrottler::BackgroundActivity>> m_pendingReplies;
};

} // namespace WebKit
