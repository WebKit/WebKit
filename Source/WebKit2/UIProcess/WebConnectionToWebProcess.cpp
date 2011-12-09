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

namespace WebKit {

PassRefPtr<WebConnectionToWebProcess> WebConnectionToWebProcess::create(WebProcessProxy* process, CoreIPC::Connection::Identifier connectionIdentifier, RunLoop* runLoop)
{
    return adoptRef(new WebConnectionToWebProcess(process, connectionIdentifier, runLoop));
}

WebConnectionToWebProcess::WebConnectionToWebProcess(WebProcessProxy* process, CoreIPC::Connection::Identifier connectionIdentifier, RunLoop* runLoop)
    : m_process(process)
    , m_connection(CoreIPC::Connection::createServerConnection(connectionIdentifier, this, runLoop))
{
#if OS(DARWIN)
    m_connection->setShouldCloseConnectionOnMachExceptions();
#elif PLATFORM(QT)
    m_connection->setShouldCloseConnectionOnProcessTermination(process->processIdentifier());
#endif

    m_connection->open();
}

void WebConnectionToWebProcess::invalidate()
{
    m_connection->invalidate();
    m_connection = nullptr;
    m_process = 0;
}

// WebConnection

void WebConnectionToWebProcess::postMessage(const String& messageName, APIObject* messageBody)
{
    // We need to check if we have an underlying process here since a user of the API can hold
    // onto us and call postMessage even after the process has been invalidated.
    if (!m_process)
        return;

    m_process->deprecatedSend(WebConnectionLegacyMessage::PostMessage, 0, CoreIPC::In(messageName, WebContextUserMessageEncoder(messageBody)));
}

// CoreIPC::Connection::Client

void WebConnectionToWebProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebConnectionLegacy>()) {
        switch (messageID.get<WebConnectionLegacyMessage::Kind>()) {
            case WebConnectionLegacyMessage::PostMessage: {
                String messageName;
                RefPtr<APIObject> messageBody;
                WebContextUserMessageDecoder messageDecoder(messageBody, m_process->context());
                if (!arguments->decode(CoreIPC::Out(messageName, messageDecoder)))
                    return;

                forwardDidReceiveMessageToClient(messageName, messageBody.get());
                return;
            }
        }
        return;
    }

    m_process->didReceiveMessage(connection, messageID, arguments);
}

void WebConnectionToWebProcess::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{
    m_process->didReceiveSyncMessage(connection, messageID, arguments, reply);
}

void WebConnectionToWebProcess::didClose(CoreIPC::Connection* connection)
{
    RefPtr<WebConnectionToWebProcess> protector(this);

    m_process->didClose(connection);

    // Tell the API client that the connection closed.
    m_client.didClose(this);
}

void WebConnectionToWebProcess::didReceiveInvalidMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID)
{
    RefPtr<WebProcessProxy> process = m_process;

    // This will invalidate the CoreIPC::Connection and the WebProcessProxy member
    // variables, so we should be careful not to use them after this call.
    process->didReceiveInvalidMessage(connection, messageID);

    // Since we've invalidated the connection we'll never get a CoreIPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    process->didClose(connection);

    // Tell the API client that the connection closed.
    m_client.didClose(this);
}

void WebConnectionToWebProcess::syncMessageSendTimedOut(CoreIPC::Connection* connection)
{
    m_process->syncMessageSendTimedOut(connection);
}

#if PLATFORM(WIN)
Vector<HWND> WebConnectionToWebProcess::windowsToReceiveSentMessagesWhileWaitingForSyncReply()
{
    return m_process->windowsToReceiveSentMessagesWhileWaitingForSyncReply();
}
#endif

} // namespace WebKit
