/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebConnectionToWebProcess.h"

#include "WebConnectionMessageKinds.h"
#include "WebContextUserMessageCoders.h"
#include "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebConnectionToWebProcess> WebConnectionToWebProcess::create(WebProcessProxy* process, CoreIPC::Connection::Identifier connectionIdentifier, RunLoop* runLoop)
{
    return adoptRef(new WebConnectionToWebProcess(process, connectionIdentifier, runLoop));
}

WebConnectionToWebProcess::WebConnectionToWebProcess(WebProcessProxy* process, CoreIPC::Connection::Identifier connectionIdentifier, RunLoop* runLoop)
    : WebConnection(CoreIPC::Connection::createServerConnection(connectionIdentifier, this, runLoop))
    , m_process(process)
{
#if OS(DARWIN)
    m_connection->setShouldCloseConnectionOnMachExceptions();
#elif PLATFORM(QT) && !OS(WINDOWS)
    m_connection->setShouldCloseConnectionOnProcessTermination(process->processIdentifier());
#endif
}

// WebConnection

void WebConnectionToWebProcess::encodeMessageBody(CoreIPC::ArgumentEncoder& encoder, APIObject* messageBody)
{
    encoder << WebContextUserMessageEncoder(messageBody);
}

bool WebConnectionToWebProcess::decodeMessageBody(CoreIPC::ArgumentDecoder& decoder, RefPtr<APIObject>& messageBody)
{
    WebContextUserMessageDecoder messageBodyDecoder(messageBody, m_process);

    if (!decoder.decode(messageBodyDecoder))
        return false;

    return true;
}

// CoreIPC::Connection::Client

void WebConnectionToWebProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    if (messageID.is<CoreIPC::MessageClassWebConnection>()) {
        didReceiveWebConnectionMessage(connection, messageID, decoder);
        return;
    }

    m_process->didReceiveMessage(connection, messageID, decoder);
}

void WebConnectionToWebProcess::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder, OwnPtr<CoreIPC::MessageEncoder>& replyEncoder)
{
    m_process->didReceiveSyncMessage(connection, messageID, decoder, replyEncoder);
}

void WebConnectionToWebProcess::didClose(CoreIPC::Connection* connection)
{
    RefPtr<WebConnectionToWebProcess> protector(this);

    m_process->didClose(connection);

    // Tell the API client that the connection closed.
    m_client.didClose(this);
}

void WebConnectionToWebProcess::didReceiveInvalidMessage(CoreIPC::Connection* connection, CoreIPC::StringReference messageReceiverName, CoreIPC::StringReference messageName)
{
    RefPtr<WebConnectionToWebProcess> protector = this;
    RefPtr<WebProcessProxy> process = m_process;

    // This will invalidate the CoreIPC::Connection and the WebProcessProxy member
    // variables, so we should be careful not to use them after this call.
    process->didReceiveInvalidMessage(connection, messageReceiverName, messageName);

    // Since we've invalidated the connection we'll never get a CoreIPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    process->didClose(connection);

    // Tell the API client that the connection closed.
    m_client.didClose(this);
}

#if PLATFORM(WIN)
Vector<HWND> WebConnectionToWebProcess::windowsToReceiveSentMessagesWhileWaitingForSyncReply()
{
    return m_process->windowsToReceiveSentMessagesWhileWaitingForSyncReply();
}
#endif

} // namespace WebKit
