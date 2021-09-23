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
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/UUID.h>

namespace WebKit {
using namespace WebCore;

DownloadProxy::DownloadProxy(DownloadProxyMap& downloadProxyMap, WebsiteDataStore& dataStore, API::DownloadClient& client, const ResourceRequest& resourceRequest, const FrameInfoData& frameInfoData, WebPageProxy* originatingPage)
    : m_downloadProxyMap(downloadProxyMap)
    , m_dataStore(&dataStore)
    , m_client(client)
    , m_downloadID(DownloadID::generate())
    , m_request(resourceRequest)
    , m_originatingPage(makeWeakPtr(originatingPage))
    , m_frameInfo(API::FrameInfo::create(FrameInfoData { frameInfoData }, originatingPage))
    , m_uuid(createCanonicalUUIDString())
{
    if (auto* instrumentation = m_dataStore->downloadInstrumentation())
      instrumentation->downloadCreated(m_uuid, m_request, frameInfoData, originatingPage, this);
}

DownloadProxy::~DownloadProxy()
{
    if (m_didStartCallback)
        m_didStartCallback(nullptr);
}

static RefPtr<API::Data> createData(const IPC::DataReference& data)
{
    if (data.isEmpty())
        return nullptr;
    return API::Data::create(data.data(), data.size());
}

void DownloadProxy::cancel(CompletionHandler<void(API::Data*)>&& completionHandler)
{
    if (m_dataStore) {
        auto* instrumentation = m_dataStore->downloadInstrumentation();
        m_dataStore->networkProcess().sendWithAsyncReply(Messages::NetworkProcess::CancelDownload(m_downloadID), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), instrumentation] (const IPC::DataReference& resumeData) mutable {
            m_legacyResumeData = createData(resumeData);
            completionHandler(m_legacyResumeData.get());
            if (instrumentation)
                instrumentation->downloadFinished(m_uuid, "canceled"_s);
            m_downloadProxyMap.downloadFinished(*this);
        });
    } else
        completionHandler(nullptr);
}

void DownloadProxy::invalidate()
{
    ASSERT(m_dataStore);
    m_dataStore = nullptr;
}

void DownloadProxy::processDidClose()
{
    m_client->processDidCrash(*this);
}

WebPageProxy* DownloadProxy::originatingPage() const
{
    return m_originatingPage.get();
}

#if PLATFORM(COCOA)
void DownloadProxy::publishProgress(const URL& URL)
{
    if (!m_dataStore)
        return;

    SandboxExtension::Handle handle;
    if (auto createdHandle = SandboxExtension::createHandle(URL.fileSystemPath(), SandboxExtension::Type::ReadWrite))
        handle = WTFMove(*createdHandle);
    else
        ASSERT_NOT_REACHED();

    m_dataStore->networkProcess().send(Messages::NetworkProcess::PublishDownloadProgress(m_downloadID, URL, handle), 0);
}
#endif // PLATFORM(COCOA)

void DownloadProxy::didStart(const ResourceRequest& request, const String& suggestedFilename)
{
    m_request = request;
    m_suggestedFilename = suggestedFilename;

    if (m_redirectChain.isEmpty() || m_redirectChain.last() != request.url())
        m_redirectChain.append(request.url());

    if (m_didStartCallback)
        m_didStartCallback(this);
    m_client->legacyDidStart(*this);
}

void DownloadProxy::didReceiveAuthenticationChallenge(AuthenticationChallenge&& authenticationChallenge, AuthenticationChallengeIdentifier challengeID)
{
    if (!m_dataStore)
        return;

    auto authenticationChallengeProxy = AuthenticationChallengeProxy::create(WTFMove(authenticationChallenge), challengeID, *m_dataStore->networkProcess().connection(), nullptr);
    m_client->didReceiveAuthenticationChallenge(*this, authenticationChallengeProxy.get());
}

void DownloadProxy::willSendRequest(ResourceRequest&& proposedRequest, const ResourceResponse& redirectResponse)
{
    m_client->willSendRequest(*this, WTFMove(proposedRequest), redirectResponse, [this, protectedThis = Ref { *this }](ResourceRequest&& newRequest) {
        m_redirectChain.append(newRequest.url());
        if (!protectedThis->m_dataStore)
            return;
        auto& networkProcessProxy = protectedThis->m_dataStore->networkProcess();
        networkProcessProxy.send(Messages::NetworkProcess::ContinueWillSendRequest(protectedThis->m_downloadID, newRequest), 0);
    });
}

void DownloadProxy::didReceiveData(uint64_t bytesWritten, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite)
{
    m_client->didReceiveData(*this, bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);
}

void DownloadProxy::decideDestinationWithSuggestedFilename(const WebCore::ResourceResponse& response, String&& suggestedFilename, CompletionHandler<void(String, SandboxExtension::Handle, AllowOverwrite)>&& completionHandler)
{
    // As per https://html.spec.whatwg.org/#as-a-download (step 2), the filename from the Content-Disposition header
    // should override the suggested filename from the download attribute.
    if (response.isAttachmentWithFilename() || (suggestedFilename.isEmpty() && m_suggestedFilename.isEmpty()))
        suggestedFilename = response.suggestedFilename();
    else if (!m_suggestedFilename.isEmpty())
        suggestedFilename = m_suggestedFilename;
    suggestedFilename = MIMETypeRegistry::appendFileExtensionIfNecessary(suggestedFilename, response.mimeType());

    if (auto* instrumentation = m_dataStore->downloadInstrumentation())
      instrumentation->downloadFilenameSuggested(m_uuid, suggestedFilename);

    if (m_dataStore->allowDownloadForAutomation()) {
        SandboxExtension::Handle sandboxExtensionHandle;
        String destination;
        if (*m_dataStore->allowDownloadForAutomation()) {
            destination = FileSystem::pathByAppendingComponent(m_dataStore->downloadPathForAutomation(), m_uuid);
            if (auto handle = SandboxExtension::createHandle(destination, SandboxExtension::Type::ReadWrite))
                sandboxExtensionHandle = WTFMove(*handle);
        }
        completionHandler(destination, WTFMove(sandboxExtensionHandle), AllowOverwrite::Yes);
        return;
    }

    m_client->decideDestinationWithSuggestedFilename(*this, response, ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (AllowOverwrite allowOverwrite, String destination) mutable {
        SandboxExtension::Handle sandboxExtensionHandle;
        if (!destination.isNull()) {
            if (auto handle = SandboxExtension::createHandle(destination, SandboxExtension::Type::ReadWrite))
                sandboxExtensionHandle = WTFMove(*handle);
        }

        setDestinationFilename(destination);
        completionHandler(destination, WTFMove(sandboxExtensionHandle), allowOverwrite);
    });
}

void DownloadProxy::didCreateDestination(const String& path)
{
    m_client->didCreateDestination(*this, path);
}

void DownloadProxy::didFinish()
{
    m_client->didFinish(*this);
    if (auto* instrumentation = m_dataStore->downloadInstrumentation())
      instrumentation->downloadFinished(m_uuid, String());

    // This can cause the DownloadProxy object to be deleted.
    m_downloadProxyMap.downloadFinished(*this);
}

void DownloadProxy::didFail(const ResourceError& error, const IPC::DataReference& resumeData)
{
    m_legacyResumeData = createData(resumeData);

    m_client->didFail(*this, error, m_legacyResumeData.get());
    if (auto* instrumentation = m_dataStore->downloadInstrumentation())
      instrumentation->downloadFinished(m_uuid, error.localizedDescription());

    // This can cause the DownloadProxy object to be deleted.
    m_downloadProxyMap.downloadFinished(*this);
}

void DownloadProxy::setClient(Ref<API::DownloadClient>&& client)
{
    m_client = WTFMove(client);
}

} // namespace WebKit

