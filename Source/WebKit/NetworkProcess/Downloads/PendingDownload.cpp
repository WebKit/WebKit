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

#include "DataReference.h"
#include "Download.h"
#include "DownloadProxyMessages.h"
#include "MessageSenderInlines.h"
#include "NetworkLoad.h"
#include "NetworkProcess.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {
using namespace WebCore;

PendingDownload::PendingDownload(IPC::Connection* parentProcessConnection, NetworkLoadParameters&& parameters, DownloadID downloadID, NetworkSession& networkSession, const String& suggestedName)
    : m_networkLoad(makeUnique<NetworkLoad>(*this, WTFMove(parameters), networkSession))
    , m_parentProcessConnection(parentProcessConnection)
{
    m_networkLoad->start();
    m_isAllowedToAskUserForCredentials = parameters.clientCredentialPolicy == ClientCredentialPolicy::MayAskClientForCredentials;

    m_networkLoad->setPendingDownloadID(downloadID);
    m_networkLoad->setPendingDownload(*this);
    m_networkLoad->setSuggestedFilename(suggestedName);

    send(Messages::DownloadProxy::DidStart(m_networkLoad->currentRequest(), suggestedName));
}

PendingDownload::PendingDownload(IPC::Connection* parentProcessConnection, std::unique_ptr<NetworkLoad>&& networkLoad, ResponseCompletionHandler&& completionHandler, DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
    : m_networkLoad(WTFMove(networkLoad))
    , m_parentProcessConnection(parentProcessConnection)
{
    m_isAllowedToAskUserForCredentials = m_networkLoad->isAllowedToAskUserForCredentials();

    m_networkLoad->setPendingDownloadID(downloadID);
    send(Messages::DownloadProxy::DidStart(request, String()));

    m_networkLoad->convertTaskToDownload(*this, request, response, WTFMove(completionHandler));
}

void PendingDownload::willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::DownloadProxy::WillSendRequest(WTFMove(redirectRequest), WTFMove(redirectResponse)), WTFMove(completionHandler));
};

void PendingDownload::cancel(CompletionHandler<void(const IPC::DataReference&)>&& completionHandler)
{
    ASSERT(m_networkLoad);
    m_networkLoad->cancel();
    completionHandler({ });
}

#if PLATFORM(COCOA)
void PendingDownload::publishProgress(const URL& url, SandboxExtension::Handle&& sandboxExtension)
{
    ASSERT(!m_progressURL.isValid());
    m_progressURL = url;
    m_progressSandboxExtension = WTFMove(sandboxExtension);
}

void PendingDownload::didBecomeDownload(const std::unique_ptr<Download>& download)
{
    if (m_progressURL.isValid())
        download->publishProgress(m_progressURL, WTFMove(m_progressSandboxExtension));
}
#endif // PLATFORM(COCOA)

void PendingDownload::didFailLoading(const WebCore::ResourceError& error)
{
    send(Messages::DownloadProxy::DidFail(error, { }));
}
    
IPC::Connection* PendingDownload::messageSenderConnection() const
{
    return m_parentProcessConnection.get();
}

void PendingDownload::didReceiveResponse(WebCore::ResourceResponse&& response, PrivateRelayed, ResponseCompletionHandler&& completionHandler)
{
    completionHandler(WebCore::PolicyAction::Download);
}

uint64_t PendingDownload::messageSenderDestinationID() const
{
    return m_networkLoad->pendingDownloadID().toUInt64();
}
    
}
