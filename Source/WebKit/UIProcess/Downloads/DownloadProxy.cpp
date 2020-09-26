/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
#include "DownloadProxy.h"

#include "APIData.h"
#include "APIDownloadClient.h"
#include "APIFrameInfo.h"
#include "AuthenticationChallengeProxy.h"
#include "DataReference.h"
#include "DownloadProxyMap.h"
#include "FrameInfoData.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProtectionSpace.h"
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/ResourceResponseBase.h>
#include <wtf/FileSystem.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

static uint64_t generateDownloadID()
{
    static uint64_t uniqueDownloadID = 0;
    return ++uniqueDownloadID;
}

DownloadProxy::DownloadProxy(DownloadProxyMap& downloadProxyMap, WebsiteDataStore& dataStore, WebProcessPool& processPool, const ResourceRequest& resourceRequest, const FrameInfoData& frameInfoData, WebPageProxy* originatingPage)
    : m_downloadProxyMap(downloadProxyMap)
    , m_dataStore(&dataStore)
    , m_processPool(&processPool)
    , m_downloadID(generateDownloadID())
    , m_request(resourceRequest)
    , m_originatingPage(makeWeakPtr(originatingPage))
    , m_frameInfo(API::FrameInfo::create(FrameInfoData { frameInfoData }, originatingPage))
{
}

DownloadProxy::~DownloadProxy()
{
    ASSERT(!m_processPool);
}

void DownloadProxy::cancel()
{
    if (!m_processPool)
        return;

    if (NetworkProcessProxy* networkProcess = m_processPool->networkProcess())
        networkProcess->send(Messages::NetworkProcess::CancelDownload(m_downloadID), 0);
}

void DownloadProxy::invalidate()
{
    ASSERT(m_processPool);
    m_processPool = nullptr;
    ASSERT(m_dataStore);
    m_dataStore = nullptr;
}

void DownloadProxy::processDidClose()
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().processDidCrash(*this);
}

WebPageProxy* DownloadProxy::originatingPage() const
{
    return m_originatingPage.get();
}

#if PLATFORM(COCOA)
void DownloadProxy::publishProgress(const URL& URL)
{
    if (!m_processPool)
        return;

    if (auto* networkProcess = m_processPool->networkProcess()) {
        SandboxExtension::Handle handle;
        bool createdSandboxExtension = SandboxExtension::createHandle(URL.fileSystemPath(), SandboxExtension::Type::ReadWrite, handle);
        ASSERT_UNUSED(createdSandboxExtension, createdSandboxExtension);
        networkProcess->send(Messages::NetworkProcess::PublishDownloadProgress(m_downloadID, URL, handle), 0);
    }
}
#endif // PLATFORM(COCOA)

void DownloadProxy::didStart(const ResourceRequest& request, const String& suggestedFilename)
{
    m_request = request;
    m_suggestedFilename = suggestedFilename;

    if (m_redirectChain.isEmpty() || m_redirectChain.last() != request.url())
        m_redirectChain.append(request.url());

    if (!m_processPool)
        return;

    m_processPool->downloadClient().didStart(*this);
}

void DownloadProxy::didReceiveAuthenticationChallenge(AuthenticationChallenge&& authenticationChallenge, uint64_t challengeID)
{
    if (!m_processPool)
        return;

    auto authenticationChallengeProxy = AuthenticationChallengeProxy::create(WTFMove(authenticationChallenge), challengeID, makeRef(*m_processPool->networkingProcessConnection()), nullptr);

    m_processPool->downloadClient().didReceiveAuthenticationChallenge(*this, authenticationChallengeProxy.get());
}

void DownloadProxy::willSendRequest(ResourceRequest&& proposedRequest, const ResourceResponse& redirectResponse)
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().willSendRequest(*this, WTFMove(proposedRequest), redirectResponse, [this, protectedThis = makeRef(*this)](ResourceRequest&& newRequest) {
        m_redirectChain.append(newRequest.url());

        if (!protectedThis->m_processPool)
            return;

        auto* networkProcessProxy = protectedThis->m_processPool->networkProcess();
        if (!networkProcessProxy)
            return;

        networkProcessProxy->send(Messages::NetworkProcess::ContinueWillSendRequest(protectedThis->m_downloadID, newRequest), 0);
    });
}

void DownloadProxy::didReceiveResponse(const ResourceResponse& response)
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().didReceiveResponse(*this, response);
}

void DownloadProxy::didReceiveData(uint64_t bytesWritten, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite)
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().didReceiveData(*this, bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);
}

void DownloadProxy::decideDestinationWithSuggestedFilenameAsync(DownloadID downloadID, const String& suggestedFilename)
{
    if (!m_processPool)
        return;
    
    m_processPool->downloadClient().decideDestinationWithSuggestedFilename(*this, ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename), [this, protectedThis = makeRef(*this), downloadID = downloadID] (AllowOverwrite allowOverwrite, String destination) {
        SandboxExtension::Handle sandboxExtensionHandle;
        if (!destination.isNull())
            SandboxExtension::createHandle(destination, SandboxExtension::Type::ReadWrite, sandboxExtensionHandle);

        if (!m_processPool)
            return;

        if (auto* networkProcess = m_processPool->networkProcess())
            networkProcess->send(Messages::NetworkProcess::ContinueDecidePendingDownloadDestination(downloadID, destination, sandboxExtensionHandle, allowOverwrite == AllowOverwrite::Yes), 0);
    });
}

void DownloadProxy::didCreateDestination(const String& path)
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().didCreateDestination(*this, path);
}

void DownloadProxy::didFinish()
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().didFinish(*this);

    // This can cause the DownloadProxy object to be deleted.
    m_downloadProxyMap.downloadFinished(*this);
}

static RefPtr<API::Data> createData(const IPC::DataReference& data)
{
    if (data.isEmpty())
        return 0;

    return API::Data::create(data.data(), data.size());
}

void DownloadProxy::didFail(const ResourceError& error, const IPC::DataReference& resumeData)
{
    if (!m_processPool)
        return;

    m_resumeData = createData(resumeData);

    m_processPool->downloadClient().didFail(*this, error);

    // This can cause the DownloadProxy object to be deleted.
    m_downloadProxyMap.downloadFinished(*this);
}

void DownloadProxy::didCancel(const IPC::DataReference& resumeData)
{
    m_resumeData = createData(resumeData);

    m_processPool->downloadClient().didCancel(*this);

    // This can cause the DownloadProxy object to be deleted.
    m_downloadProxyMap.downloadFinished(*this);
}

} // namespace WebKit

