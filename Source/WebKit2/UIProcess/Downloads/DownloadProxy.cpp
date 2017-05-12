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
#include "AuthenticationChallengeProxy.h"
#include "DataReference.h"
#include "DownloadProxyMap.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProtectionSpace.h"
#include <WebCore/FileSystem.h>
#include <WebCore/MIMETypeRegistry.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateDownloadID()
{
    static uint64_t uniqueDownloadID = 0;
    return ++uniqueDownloadID;
}
    
Ref<DownloadProxy> DownloadProxy::create(DownloadProxyMap& downloadProxyMap, WebProcessPool& processPool, const ResourceRequest& resourceRequest)
{
    return adoptRef(*new DownloadProxy(downloadProxyMap, processPool, resourceRequest));
}

DownloadProxy::DownloadProxy(DownloadProxyMap& downloadProxyMap, WebProcessPool& processPool, const ResourceRequest& resourceRequest)
    : m_downloadProxyMap(downloadProxyMap)
    , m_processPool(&processPool)
    , m_downloadID(generateDownloadID())
    , m_request(resourceRequest)
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
}

void DownloadProxy::processDidClose()
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().processDidCrash(m_processPool.get(), this);
}

void DownloadProxy::didStart(const ResourceRequest& request, const String& suggestedFilename)
{
    m_request = request;
    m_suggestedFilename = suggestedFilename;

    if (!m_processPool)
        return;

    m_processPool->downloadClient().didStart(m_processPool.get(), this);
}

void DownloadProxy::didReceiveAuthenticationChallenge(const AuthenticationChallenge& authenticationChallenge, uint64_t challengeID)
{
    if (!m_processPool)
        return;

    RefPtr<AuthenticationChallengeProxy> authenticationChallengeProxy = AuthenticationChallengeProxy::create(authenticationChallenge, challengeID, m_processPool->networkingProcessConnection());

    m_processPool->downloadClient().didReceiveAuthenticationChallenge(m_processPool.get(), this, authenticationChallengeProxy.get());
}

#if USE(NETWORK_SESSION)
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void DownloadProxy::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace)
{
    if (!m_processPool)
        return;
    
    auto* networkProcessProxy = m_processPool->networkProcess();
    if (!networkProcessProxy)
        return;

    bool result = m_processPool->downloadClient().canAuthenticateAgainstProtectionSpace(getPtr(WebProtectionSpace::create(protectionSpace)));
    
    networkProcessProxy->send(Messages::NetworkProcess::ContinueCanAuthenticateAgainstProtectionSpaceDownload(m_downloadID, result), 0);
}
#endif

void DownloadProxy::willSendRequest(const ResourceRequest& proposedRequest, const ResourceResponse& redirectResponse)
{
    if (!m_processPool)
        return;

    RefPtr<DownloadProxy> protectedThis(this);
    m_processPool->downloadClient().willSendRequest(proposedRequest, redirectResponse, [protectedThis](const ResourceRequest& newRequest) {
        if (!protectedThis->m_processPool)
            return;
        
        auto* networkProcessProxy = protectedThis->m_processPool->networkProcess();
        if (!networkProcessProxy)
            return;
        
        networkProcessProxy->send(Messages::NetworkProcess::ContinueWillSendRequest(protectedThis->m_downloadID, newRequest), 0);
    });
}
#endif

void DownloadProxy::didReceiveResponse(const ResourceResponse& response)
{
    if (!m_processPool)
        return;

#if !USE(NETWORK_SESSION)
    // As per https://html.spec.whatwg.org/#as-a-download (step 2), the filename from the Content-Disposition header
    // should override the suggested filename from the download attribute.
    if (!m_suggestedFilename.isNull() && response.isAttachmentWithFilename())
        m_suggestedFilename = String();
#endif

    m_processPool->downloadClient().didReceiveResponse(m_processPool.get(), this, response);
}

void DownloadProxy::didReceiveData(uint64_t length)
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().didReceiveData(m_processPool.get(), this, length);
}

void DownloadProxy::shouldDecodeSourceDataOfMIMEType(const String& mimeType, bool& result)
{
    result = false;

    if (!m_processPool)
        return;

    result = m_processPool->downloadClient().shouldDecodeSourceDataOfMIMEType(m_processPool.get(), this, mimeType);
}

void DownloadProxy::decideDestinationWithSuggestedFilenameAsync(DownloadID downloadID, const String& suggestedFilename)
{
    bool allowOverwrite = false;
    
    if (!m_processPool)
        return;
    
    String destination = m_processPool->downloadClient().decideDestinationWithSuggestedFilename(m_processPool.get(), this, suggestedFilename, allowOverwrite);
    
    SandboxExtension::Handle sandboxExtensionHandle;
    if (!destination.isNull())
        SandboxExtension::createHandle(destination, SandboxExtension::ReadWrite, sandboxExtensionHandle);

    if (NetworkProcessProxy* networkProcess = m_processPool->networkProcess())
        networkProcess->send(Messages::NetworkProcess::ContinueDecidePendingDownloadDestination(downloadID, destination, sandboxExtensionHandle, allowOverwrite), 0);
}

#if !USE(NETWORK_SESSION)

void DownloadProxy::decideDestinationWithSuggestedFilename(const String& filename, const String& mimeType, String& destination, bool& allowOverwrite, SandboxExtension::Handle& sandboxExtensionHandle)
{
    allowOverwrite = false;

    if (!m_processPool)
        return;

    String suggestedFilename = MIMETypeRegistry::appendFileExtensionIfNecessary(m_suggestedFilename.isEmpty() ? filename : m_suggestedFilename, mimeType);
    destination = m_processPool->downloadClient().decideDestinationWithSuggestedFilename(m_processPool.get(), this, suggestedFilename, allowOverwrite);

    if (!destination.isNull())
        SandboxExtension::createHandle(destination, SandboxExtension::ReadWrite, sandboxExtensionHandle);
}

#endif

void DownloadProxy::didCreateDestination(const String& path)
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().didCreateDestination(m_processPool.get(), this, path);
}

void DownloadProxy::didFinish()
{
    if (!m_processPool)
        return;

    m_processPool->downloadClient().didFinish(m_processPool.get(), this);

    // This can cause the DownloadProxy object to be deleted.
    m_downloadProxyMap.downloadFinished(this);
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

    m_processPool->downloadClient().didFail(m_processPool.get(), this, error);

    // This can cause the DownloadProxy object to be deleted.
    m_downloadProxyMap.downloadFinished(this);
}

void DownloadProxy::didCancel(const IPC::DataReference& resumeData)
{
    m_resumeData = createData(resumeData);

    m_processPool->downloadClient().didCancel(m_processPool.get(), this);

    // This can cause the DownloadProxy object to be deleted.
    m_downloadProxyMap.downloadFinished(this);
}

} // namespace WebKit

