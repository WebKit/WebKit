/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "SubframePageProxy.h"

#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessProxy.h"

namespace WebKit {

SubframePageProxy::SubframePageProxy(WebFrameProxy& frame, WebPageProxy& page, WebProcessProxy& process)
    : m_webPageID(page.webPageID())
    , m_process(process)
    , m_frame(frame)
    , m_page(page)
{
    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID, *this);
}

SubframePageProxy::~SubframePageProxy()
{
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID);
}

IPC::Connection* SubframePageProxy::messageSenderConnection() const
{
    return m_process->connection();
}

uint64_t SubframePageProxy::messageSenderDestinationID() const
{
    return m_webPageID.toUInt64();
}

void SubframePageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (m_page)
        m_page->didReceiveMessage(connection, decoder);
}

bool SubframePageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (m_page)
        return m_page->didReceiveSyncMessage(connection, decoder, encoder);
    return false;
}

}
