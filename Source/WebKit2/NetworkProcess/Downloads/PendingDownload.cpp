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

#include "NetworkLoad.h"
#include "NetworkProcess.h"

using namespace WebCore;

namespace WebKit {

PendingDownload::PendingDownload(const NetworkLoadParameters& parameters, DownloadID downloadID)
    : m_networkLoad(std::make_unique<NetworkLoad>(*this, parameters))
{
    m_networkLoad->setPendingDownloadID(downloadID);
    m_networkLoad->setPendingDownload(*this);
}

void PendingDownload::willSendRedirectedRequest(const WebCore::ResourceRequest&, const WebCore::ResourceRequest& redirectRequest, const WebCore::ResourceResponse& redirectResponse)
{
    // FIXME: We should ask the UI process directly if we actually want to continue this request.
    continueWillSendRequest(redirectRequest);
};
    
void PendingDownload::continueWillSendRequest(const WebCore::ResourceRequest& newRequest)
{
    m_networkLoad->continueWillSendRequest(newRequest);
}

void PendingDownload::canAuthenticateAgainstProtectionSpaceAsync(const WebCore::ProtectionSpace&)
{
    // FIXME: This should ask the UI process.
    continueCanAuthenticateAgainstProtectionSpace(true);
}
    
void PendingDownload::continueCanAuthenticateAgainstProtectionSpace(bool canAuthenticate)
{
    m_networkLoad->continueCanAuthenticateAgainstProtectionSpace(canAuthenticate);
}
    
void PendingDownload::didConvertToDownload()
{
    m_networkLoad = nullptr;
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
