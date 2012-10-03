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
#include "WebConnectionToUIProcess.h"

#include "InjectedBundleUserMessageCoders.h"
#include "WebConnectionMessageKinds.h"
#include "WebProcess.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebConnectionToUIProcess> WebConnectionToUIProcess::create(WebProcess* process, CoreIPC::Connection::Identifier connectionIdentifier, RunLoop* runLoop)
{
    return adoptRef(new WebConnectionToUIProcess(process, connectionIdentifier, runLoop));
}

WebConnectionToUIProcess::WebConnectionToUIProcess(WebProcess* process, CoreIPC::Connection::Identifier connectionIdentifier, RunLoop* runLoop)
    : m_process(process)
    , m_connection(CoreIPC::Connection::createClientConnection(connectionIdentifier, this, runLoop))
{
    m_connection->setDidCloseOnConnectionWorkQueueCallback(ChildProcess::didCloseOnConnectionWorkQueue);
    m_connection->setShouldExitOnSyncMessageSendFailure(true);
}

void WebConnectionToUIProcess::invalidate()
{
    m_connection->invalidate();
    m_connection = nullptr;
    m_process = 0;
}

// WebConnection

void WebConnectionToUIProcess::postMessage(const String& messageName, APIObject* messageBody)
{
    if (!m_process)
        return;

    m_connection->deprecatedSend(WebConnectionLegacyMessage::PostMessage, 0, CoreIPC::In(messageName, InjectedBundleUserMessageEncoder(messageBody)));
}

// CoreIPC::Connection::Client

void WebConnectionToUIProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebConnectionLegacy>()) {
        switch (messageID.get<WebConnectionLegacyMessage::Kind>()) {
            case WebConnectionLegacyMessage::PostMessage: {
                String messageName;            
                RefPtr<APIObject> messageBody;
                InjectedBundleUserMessageDecoder messageDecoder(messageBody);
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

void WebConnectionToUIProcess::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{
    m_process->didReceiveSyncMessage(connection, messageID, arguments, reply);
}

void WebConnectionToUIProcess::didClose(CoreIPC::Connection* connection)
{
    m_process->didClose(connection);
}

void WebConnectionToUIProcess::didReceiveInvalidMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID)
{
    m_process->didReceiveInvalidMessage(connection, messageID);
}

#if PLATFORM(WIN)
Vector<HWND> WebConnectionToUIProcess::windowsToReceiveSentMessagesWhileWaitingForSyncReply()
{
    return m_process->windowsToReceiveSentMessagesWhileWaitingForSyncReply();
}
#endif

} // namespace WebKit
