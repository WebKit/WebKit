/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebMessagePortChannelProvider.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include <WebCore/MessagePort.h>
#include <WebCore/MessagePortIdentifier.h>
#include <WebCore/MessageWithMessagePorts.h>

namespace WebKit {
using namespace WebCore;

WebMessagePortChannelProvider& WebMessagePortChannelProvider::singleton()
{
    static WebMessagePortChannelProvider* provider = new WebMessagePortChannelProvider;
    return *provider;
}

WebMessagePortChannelProvider::WebMessagePortChannelProvider()
{
}

WebMessagePortChannelProvider::~WebMessagePortChannelProvider()
{
    ASSERT_NOT_REACHED();
}

static inline IPC::Connection& networkProcessConnection()
{
    return WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

void WebMessagePortChannelProvider::createNewMessagePortChannel(const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
{
    networkProcessConnection().send(Messages::NetworkConnectionToWebProcess::CreateNewMessagePortChannel { port1, port2 }, 0);
}

void WebMessagePortChannelProvider::entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    networkProcessConnection().send(Messages::NetworkConnectionToWebProcess::EntangleLocalPortInThisProcessToRemote { local, remote }, 0);
}

void WebMessagePortChannelProvider::messagePortDisentangled(const MessagePortIdentifier& port)
{
    networkProcessConnection().send(Messages::NetworkConnectionToWebProcess::MessagePortDisentangled { port }, 0);
}

void WebMessagePortChannelProvider::messagePortClosed(const MessagePortIdentifier& port)
{
    networkProcessConnection().send(Messages::NetworkConnectionToWebProcess::MessagePortClosed { port }, 0);
}

void WebMessagePortChannelProvider::takeAllMessagesForPort(const MessagePortIdentifier& port, CompletionHandler<void(Vector<MessageWithMessagePorts>&&, Function<void()>&&)>&& completionHandler)
{
    networkProcessConnection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::TakeAllMessagesForPort { port }, [completionHandler = WTFMove(completionHandler)](auto&& messages, uint64_t messageBatchIdentifier) mutable {
        completionHandler(WTFMove(messages), [messageBatchIdentifier] {
            networkProcessConnection().send(Messages::NetworkConnectionToWebProcess::DidDeliverMessagePortMessages { messageBatchIdentifier }, 0);
        });
    }, 0);
}

void WebMessagePortChannelProvider::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& remoteTarget)
{
    networkProcessConnection().send(Messages::NetworkConnectionToWebProcess::PostMessageToRemote { message, remoteTarget }, 0);
}

void WebMessagePortChannelProvider::checkRemotePortForActivity(const MessagePortIdentifier& remoteTarget, CompletionHandler<void(HasActivity)>&& completionHandler)
{
    networkProcessConnection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::CheckRemotePortForActivity { remoteTarget }, [completionHandler = WTFMove(completionHandler)](bool hasActivity) mutable {
        completionHandler(hasActivity ? HasActivity::Yes : HasActivity::No);
    }, 0);
}

} // namespace WebKit
