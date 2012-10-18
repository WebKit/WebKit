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
    : WebConnection(CoreIPC::Connection::createClientConnection(connectionIdentifier, this, runLoop))
    , m_process(process)
{
    m_connection->setDidCloseOnConnectionWorkQueueCallback(ChildProcess::didCloseOnConnectionWorkQueue);
    m_connection->setShouldExitOnSyncMessageSendFailure(true);
}

// WebConnection

void WebConnectionToUIProcess::encodeMessageBody(CoreIPC::ArgumentEncoder& encoder, APIObject* messageBody)
{
    encoder.encode(InjectedBundleUserMessageEncoder(messageBody));
}

bool WebConnectionToUIProcess::decodeMessageBody(CoreIPC::ArgumentDecoder& decoder, RefPtr<APIObject>& messageBody)
{
    if (!decoder.decode(InjectedBundleUserMessageDecoder(messageBody)))
        return false;

    return true;
}

// CoreIPC::Connection::Client
void WebConnectionToUIProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    if (messageID.is<CoreIPC::MessageClassWebConnection>()) {
        didReceiveWebConnectionMessage(connection, messageID, decoder);
        return;
    }

    m_process->didReceiveMessage(connection, messageID, decoder);
}

void WebConnectionToUIProcess::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder, OwnPtr<CoreIPC::MessageEncoder>& replyEncoder)
{
    m_process->didReceiveSyncMessage(connection, messageID, decoder, replyEncoder);
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
