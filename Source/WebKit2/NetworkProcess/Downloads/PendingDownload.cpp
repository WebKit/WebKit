/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "PendingDownload.h"

#if USE(NETWORK_SESSION)

#include "DataReference.h"
#include "DownloadProxyMessages.h"
#include "NetworkLoad.h"
#include "NetworkProcess.h"
#include "WebCoreArgumentCoders.h"

using namespace WebCore;

namespace WebKit {

PendingDownload::PendingDownload(NetworkLoadParameters&& parameters, DownloadID downloadID, NetworkSession& networkSession)
    : m_networkLoad(std::make_unique<NetworkLoad>(*this, WTFMove(parameters), networkSession))
{
    m_networkLoad->setPendingDownloadID(downloadID);
    m_networkLoad->setPendingDownload(*this);
}

void PendingDownload::willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse)
{
    send(Messages::DownloadProxy::WillSendRequest(WTFMove(redirectRequest), WTFMove(redirectResponse)));
};
    
void PendingDownload::continueWillSendRequest(WebCore::ResourceRequest&& newRequest)
{
    m_networkLoad->continueWillSendRequest(WTFMove(newRequest));
}

void PendingDownload::canAuthenticateAgainstProtectionSpaceAsync(const WebCore::ProtectionSpace& protectionSpace)
{
    send(Messages::DownloadProxy::CanAuthenticateAgainstProtectionSpace(protectionSpace));
}

void PendingDownload::continueCanAuthenticateAgainstProtectionSpace(bool canAuthenticate)
{
    m_networkLoad->continueCanAuthenticateAgainstProtectionSpace(canAuthenticate);
}

void PendingDownload::didBecomeDownload()
{
    m_networkLoad = nullptr;
}
    
void PendingDownload::didFailLoading(const WebCore::ResourceError& error)
{
    send(Messages::DownloadProxy::DidFail(error, { }));
}
    
IPC::Connection* PendingDownload::messageSenderConnection()
{
    return NetworkProcess::singleton().parentProcessConnection();
}

uint64_t PendingDownload::messageSenderDestinationID()
{
    return m_networkLoad->pendingDownloadID().downloadID();
}
    
}

#endif
