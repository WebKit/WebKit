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

#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
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

void WebMessagePortChannelProvider::createNewMessagePortChannel(const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::CreateNewMessagePortChannel(port1, port2), 0);
}

void WebMessagePortChannelProvider::entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::EntangleLocalPortInThisProcessToRemote(local, remote), 0);
}

void WebMessagePortChannelProvider::messagePortDisentangled(const MessagePortIdentifier& port)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::MessagePortDisentangled(port), 0);
}

void WebMessagePortChannelProvider::messagePortClosed(const MessagePortIdentifier& port)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::MessagePortClosed(port), 0);
}

void WebMessagePortChannelProvider::takeAllMessagesForPort(const MessagePortIdentifier& port, Function<void(Vector<MessageWithMessagePorts>&&, Function<void()>&&)>&& completionHandler)
{
    static std::atomic<uint64_t> currentHandlerIdentifier;
    uint64_t identifier = ++currentHandlerIdentifier;

    {
        Locker<Lock> locker(m_takeAllMessagesCallbackLock);
        auto result = m_takeAllMessagesCallbacks.ensure(identifier, [completionHandler = WTFMove(completionHandler)]() mutable {
            return WTFMove(completionHandler);
        });
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::TakeAllMessagesForPort(port, identifier), 0);
}

void WebMessagePortChannelProvider::didTakeAllMessagesForPort(Vector<MessageWithMessagePorts>&& messages, uint64_t messageCallbackIdentifier, uint64_t messageBatchIdentifier)
{
    ASSERT(isMainThread());

    Locker<Lock> locker(m_takeAllMessagesCallbackLock);
    auto callback = m_takeAllMessagesCallbacks.take(messageCallbackIdentifier);
    locker.unlockEarly();

    ASSERT(callback);
    callback(WTFMove(messages), [messageBatchIdentifier] {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::DidDeliverMessagePortMessages(messageBatchIdentifier), 0);
    });
}

void WebMessagePortChannelProvider::didCheckRemotePortForActivity(uint64_t callbackIdentifier, bool hasActivity)
{
    ASSERT(isMainThread());

    Locker<Lock> locker(m_remoteActivityCallbackLock);
    auto callback = m_remoteActivityCallbacks.take(callbackIdentifier);
    locker.unlockEarly();

    ASSERT(callback);
    callback(hasActivity ? HasActivity::Yes : HasActivity::No);
}

void WebMessagePortChannelProvider::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& remoteTarget)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::PostMessageToRemote(WTFMove(message), remoteTarget), 0);
}

void WebMessagePortChannelProvider::checkProcessLocalPortForActivity(const MessagePortIdentifier& identifier, uint64_t callbackIdentifier)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::DidCheckProcessLocalPortForActivity { callbackIdentifier, MessagePort::isExistingMessagePortLocallyReachable(identifier) }, 0);
}

void WebMessagePortChannelProvider::checkProcessLocalPortForActivity(const MessagePortIdentifier&, ProcessIdentifier, CompletionHandler<void(HasActivity)>&&)
{
    // To be called only in the UI process provider, not the Web process provider.
    ASSERT_NOT_REACHED();
}

void WebMessagePortChannelProvider::checkRemotePortForActivity(const MessagePortIdentifier& remoteTarget, Function<void(HasActivity)>&& completionHandler)
{
    static std::atomic<uint64_t> currentHandlerIdentifier;
    uint64_t identifier = ++currentHandlerIdentifier;

    {
        Locker<Lock> locker(m_remoteActivityCallbackLock);
        ASSERT(!m_remoteActivityCallbacks.contains(identifier));
        m_remoteActivityCallbacks.set(identifier, WTFMove(completionHandler));
    }

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::CheckRemotePortForActivity(remoteTarget, identifier), 0);
}

} // namespace WebKit
