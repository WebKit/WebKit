/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RemotePageWebAuthenticatorCoordinatorProxy.h"

#if ENABLE(WEB_AUTHN)

#include "WebAuthenticatorCoordinatorProxy.h"
#include "WebAuthenticatorCoordinatorProxyMessages.h"
#include "WebProcessProxy.h"

namespace WebKit {

Ref<RemotePageWebAuthenticatorCoordinatorProxy> RemotePageWebAuthenticatorCoordinatorProxy::create(WebCore::PageIdentifier identifier, WebAuthenticatorCoordinatorProxy* coordinator, WebProcessProxy& process)
{
    return adoptRef(*new RemotePageWebAuthenticatorCoordinatorProxy(identifier, coordinator, process));
}

RemotePageWebAuthenticatorCoordinatorProxy::RemotePageWebAuthenticatorCoordinatorProxy(WebCore::PageIdentifier identifier, WebAuthenticatorCoordinatorProxy* coordinator, WebProcessProxy& process)
    : m_identifier(identifier)
    , m_coordinator(coordinator)
    , m_process(process)
{
    process.addMessageReceiver(Messages::WebAuthenticatorCoordinatorProxy::messageReceiverName(), m_identifier, *this);
}

RemotePageWebAuthenticatorCoordinatorProxy::~RemotePageWebAuthenticatorCoordinatorProxy()
{
    m_process->removeMessageReceiver(Messages::WebAuthenticatorCoordinatorProxy::messageReceiverName(), m_identifier);
}

void RemotePageWebAuthenticatorCoordinatorProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (RefPtr coordinator = m_coordinator.get())
        coordinator->didReceiveMessage(connection, decoder);
}

}

#endif
