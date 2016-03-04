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

#ifndef PendingDownload_h
#define PendingDownload_h

#if USE(NETWORK_SESSION)

#include "MessageSender.h"
#include "NetworkLoadClient.h"

namespace WebCore {
class ResourceResponse;
}

namespace WebKit {

class DownloadID;
class NetworkLoad;
class NetworkLoadParameters;
    
class PendingDownload : public NetworkLoadClient, public IPC::MessageSender {
public:
    PendingDownload(const NetworkLoadParameters&, DownloadID);

    void continueWillSendRequest(const WebCore::ResourceRequest&);
    void continueCanAuthenticateAgainstProtectionSpace(bool canAuthenticate);

private:    
    // NetworkLoadClient.
    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override { }
    void canAuthenticateAgainstProtectionSpaceAsync(const WebCore::ProtectionSpace&) override;
    bool isSynchronous() const override { return false; }
    void willSendRedirectedRequest(const WebCore::ResourceRequest&, const WebCore::ResourceRequest& redirectRequest, const WebCore::ResourceResponse& redirectResponse) override;
    ShouldContinueDidReceiveResponse didReceiveResponse(const WebCore::ResourceResponse&) override { return ShouldContinueDidReceiveResponse::No; };
    void didReceiveBuffer(RefPtr<WebCore::SharedBuffer>&&, int reportedEncodedDataLength) override { };
    void didFinishLoading(double finishTime) override { };
    void didFailLoading(const WebCore::ResourceError&) override;
    void didBecomeDownload() override;
    
    // MessageSender.
    IPC::Connection* messageSenderConnection() override;
    uint64_t messageSenderDestinationID() override;

private:
    std::unique_ptr<NetworkLoad> m_networkLoad;
};

}

#endif

#endif
