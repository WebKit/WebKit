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
#include "UIMessagePortChannelProvider.h"

#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include <wtf/CompletionHandler.h>

namespace WebKit {
using namespace WebCore;

UIMessagePortChannelProvider& UIMessagePortChannelProvider::singleton()
{
    static UIMessagePortChannelProvider* provider = new UIMessagePortChannelProvider;
    return *provider;
}

UIMessagePortChannelProvider::UIMessagePortChannelProvider()
    : m_registry(*this)
{
}

UIMessagePortChannelProvider::~UIMessagePortChannelProvider()
{
    ASSERT_NOT_REACHED();
}

void UIMessagePortChannelProvider::createNewMessagePortChannel(const MessagePortIdentifier&, const MessagePortIdentifier&)
{
    // Should never be called in the UI process provider.
    ASSERT_NOT_REACHED();
}

void UIMessagePortChannelProvider::entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier&, const MessagePortIdentifier&)
{
    // Should never be called in the UI process provider.
    ASSERT_NOT_REACHED();
}

void UIMessagePortChannelProvider::messagePortDisentangled(const MessagePortIdentifier&)
{
    // Should never be called in the UI process provider.
    ASSERT_NOT_REACHED();
}

void UIMessagePortChannelProvider::messagePortClosed(const MessagePortIdentifier&)
{
    // Should never be called in the UI process provider.
    ASSERT_NOT_REACHED();
}

void UIMessagePortChannelProvider::takeAllMessagesForPort(const MessagePortIdentifier&, Function<void(Vector<MessageWithMessagePorts>&&, Function<void()>&&)>&&)
{
    // Should never be called in the UI process provider.
    ASSERT_NOT_REACHED();
}

void UIMessagePortChannelProvider::postMessageToRemote(MessageWithMessagePorts&&, const MessagePortIdentifier&)
{
    // Should never be called in the UI process provider.
    ASSERT_NOT_REACHED();
}

void UIMessagePortChannelProvider::checkRemotePortForActivity(const MessagePortIdentifier&, Function<void(HasActivity)>&&)
{
    // Should never be called in the UI process provider.
    ASSERT_NOT_REACHED();
}

void UIMessagePortChannelProvider::checkProcessLocalPortForActivity(const MessagePortIdentifier& port, ProcessIdentifier processIdentifier, CompletionHandler<void(HasActivity)>&& completionHandler)
{
    auto* process = WebProcessProxy::processForIdentifier(processIdentifier);
    if (!process) {
        completionHandler(HasActivity::No);
        return;
    }

    process->checkProcessLocalPortForActivity(port, WTFMove(completionHandler));
}

} // namespace WebKit
