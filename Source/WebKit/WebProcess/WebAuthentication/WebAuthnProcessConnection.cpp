/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "WebAuthnProcessConnection.h"

#if ENABLE(WEB_AUTHN)

#include "WebAuthnConnectionToWebProcessMessages.h"
#include "WebPageMessages.h"
#include "WebProcess.h"

namespace WebKit {
using namespace WebCore;

WebAuthnProcessConnection::WebAuthnProcessConnection(IPC::Connection::Identifier connectionIdentifier)
    : m_connection(IPC::Connection::createClientConnection(connectionIdentifier, *this))
{
    m_connection->open();
}

WebAuthnProcessConnection::~WebAuthnProcessConnection()
{
    m_connection->invalidate();
}

void WebAuthnProcessConnection::didClose(IPC::Connection&)
{
    WebProcess::singleton().webAuthnProcessConnectionClosed(this);
}

void WebAuthnProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName)
{
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
