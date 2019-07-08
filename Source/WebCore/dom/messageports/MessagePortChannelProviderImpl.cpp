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
#include "MessagePortChannelProviderImpl.h"

#include "MessagePort.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {

MessagePortChannelProviderImpl::MessagePortChannelProviderImpl()
    : m_registry(*this)
{
}

MessagePortChannelProviderImpl::~MessagePortChannelProviderImpl()
{
    ASSERT_NOT_REACHED();
}

void MessagePortChannelProviderImpl::performActionOnMainThread(Function<void()>&& action)
{
    if (isMainThread())
        action();
    else
        callOnMainThread(WTFMove(action));
}

void MessagePortChannelProviderImpl::createNewMessagePortChannel(const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    performActionOnMainThread([registry = &m_registry, local, remote] {
        registry->didCreateMessagePortChannel(local, remote);
    });
}

void MessagePortChannelProviderImpl::entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    performActionOnMainThread([registry = &m_registry, local, remote] {
        registry->didEntangleLocalToRemote(local, remote, Process::identifier());
    });
}

void MessagePortChannelProviderImpl::messagePortDisentangled(const MessagePortIdentifier& local)
{
    performActionOnMainThread([registry = &m_registry, local] {
        registry->didDisentangleMessagePort(local);
    });
}

void MessagePortChannelProviderImpl::messagePortClosed(const MessagePortIdentifier& local)
{
    performActionOnMainThread([registry = &m_registry, local] {
        registry->didCloseMessagePort(local);
    });
}

void MessagePortChannelProviderImpl::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& remoteTarget)
{
    performActionOnMainThread([registry = &m_registry, message = WTFMove(message), remoteTarget]() mutable {
        if (registry->didPostMessageToRemote(WTFMove(message), remoteTarget))
            MessagePort::notifyMessageAvailable(remoteTarget);
    });
}

void MessagePortChannelProviderImpl::takeAllMessagesForPort(const MessagePortIdentifier& port, Function<void(Vector<MessageWithMessagePorts>&&, Function<void()>&&)>&& outerCallback)
{
    // It is the responsibility of outerCallback to get itself to the appropriate thread (e.g. WebWorker thread)
    auto callback = [outerCallback = WTFMove(outerCallback)](Vector<MessageWithMessagePorts>&& messages, Function<void()>&& messageDeliveryCallback) {
        ASSERT(isMainThread());
        outerCallback(WTFMove(messages), WTFMove(messageDeliveryCallback));
    };

    performActionOnMainThread([registry = &m_registry, port, callback = WTFMove(callback)]() mutable {
        registry->takeAllMessagesForPort(port, WTFMove(callback));
    });
}

void MessagePortChannelProviderImpl::checkRemotePortForActivity(const MessagePortIdentifier& remoteTarget, Function<void(HasActivity)>&& outerCallback)
{
    auto callback = Function<void(HasActivity)> { [outerCallback = WTFMove(outerCallback)](HasActivity hasActivity) mutable {
        ASSERT(isMainThread());
        outerCallback(hasActivity);
    } };

    performActionOnMainThread([registry = &m_registry, remoteTarget, callback = WTFMove(callback)]() mutable {
        registry->checkRemotePortForActivity(remoteTarget, WTFMove(callback));
    });
}

void MessagePortChannelProviderImpl::checkProcessLocalPortForActivity(const MessagePortIdentifier& identifier, ProcessIdentifier, CompletionHandler<void(HasActivity)>&& callback)
{
    ASSERT(isMainThread());

    callback(MessagePort::isExistingMessagePortLocallyReachable(identifier) ? HasActivity::Yes : HasActivity::No);
}


} // namespace WebCore
