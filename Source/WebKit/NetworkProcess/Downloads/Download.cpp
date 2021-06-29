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
#include "Download.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "Connection.h"
#include "DownloadManager.h"
#include "DownloadMonitor.h"
#include "DownloadProxyMessages.h"
#include "Logging.h"
#include "NetworkDataTask.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "SandboxExtension.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/NotImplemented.h>

#if PLATFORM(COCOA)
#include "NetworkDataTaskCocoa.h"
#endif

#define DOWNLOAD_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - Download::" fmt, this, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

Download::Download(DownloadManager& downloadManager, DownloadID downloadID, NetworkDataTask& download, NetworkSession& session, const String& suggestedName)
    : m_downloadManager(downloadManager)
    , m_downloadID(downloadID)
    , m_client(downloadManager.client())
    , m_download(&download)
    , m_sessionID(session.sessionID())
    , m_suggestedName(suggestedName)
    , m_testSpeedMultiplier(session.testSpeedMultiplier())
{
    ASSERT(m_downloadID);

    m_downloadManager.didCreateDownload();
}

#if PLATFORM(COCOA)
Download::Download(DownloadManager& downloadManager, DownloadID downloadID, NSURLSessionDownloadTask* download, NetworkSession& session, const String& suggestedName)
    : m_downloadManager(downloadManager)
    , m_downloadID(downloadID)
    , m_client(downloadManager.client())
    , m_downloadTask(download)
    , m_sessionID(session.sessionID())
    , m_suggestedName(suggestedName)
    , m_testSpeedMultiplier(session.testSpeedMultiplier())
{
    ASSERT(m_downloadID);

    m_downloadManager.didCreateDownload();
}
#endif

Download::~Download()
{
    platformDestroyDownload();
    m_downloadManager.didDestroyDownload();
}

void Download::cancel(CompletionHandler<void(const IPC::DataReference&)>&& completionHandler, IgnoreDidFailCallback ignoreDidFailCallback)
{
    RELEASE_ASSERT(isMainRunLoop());

    // URLSession:task:didCompleteWithError: is still called after cancelByProducingResumeData's completionHandler.
    // If this cancel request came from the API, we do not want to send DownloadProxy::DidFail because the
    // completionHandler will inform the API that the cancellation succeeded.
    m_ignoreDidFailCallback = ignoreDidFailCallback;

    auto completionHandlerWrapper = [this, weakThis = makeWeakPtr(*this), completionHandler = WTFMove(completionHandler)] (const IPC::DataReference& resumeData) mutable {
        completionHandler(resumeData);
        if (!weakThis || m_ignoreDidFailCallback == IgnoreDidFailCallback::No)
            return;
        DOWNLOAD_RELEASE_LOG("didCancel: (id = %" PRIu64 ")", downloadID().toUInt64());
        if (auto extension = std::exchange(m_sandboxExtension, nullptr))
            extension->revoke();
        m_downloadManager.downloadFinished(*this);
    };

    if (m_download) {
        m_download->cancel();
        completionHandlerWrapper({ });
        return;
    }
    platformCancelNetworkLoad(WTFMove(completionHandlerWrapper));
}

void Download::didReceiveChallenge(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler&& completionHandler)
{
    if (challenge.protectionSpace().isPasswordBased() && !challenge.proposedCredential().isEmpty() && !challenge.previousFailureCount()) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, challenge.proposedCredential());
        return;
    }

    m_client->downloadsAuthenticationManager().didReceiveAuthenticationChallenge(*this, challenge, WTFMove(completionHandler));
}

void Download::didCreateDestination(const String& path)
{
    send(Messages::DownloadProxy::DidCreateDestination(path));
}

void Download::didReceiveData(uint64_t bytesWritten, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite)
{
    if (!m_hasReceivedData) {
        DOWNLOAD_RELEASE_LOG("didReceiveData: Started receiving data (id = %" PRIu64 ")", downloadID().toUInt64());
        m_hasReceivedData = true;
    }
    
    m_monitor.downloadReceivedBytes(bytesWritten);

    send(Messages::DownloadProxy::DidReceiveData(bytesWritten, totalBytesWritten, totalBytesExpectedToWrite));
}

void Download::didFinish()
{
    DOWNLOAD_RELEASE_LOG("didFinish: (id = %" PRIu64 ")", downloadID().toUInt64());

    send(Messages::DownloadProxy::DidFinish());

    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }

    m_downloadManager.downloadFinished(*this);
}

void Download::didFail(const ResourceError& error, const IPC::DataReference& resumeData)
{
    if (m_ignoreDidFailCallback == IgnoreDidFailCallback::Yes)
        return;

    DOWNLOAD_RELEASE_LOG("didFail: (id = %" PRIu64 ", isTimeout = %d, isCancellation = %d, errCode = %d)",
        downloadID().toUInt64(), error.isTimeout(), error.isCancellation(), error.errorCode());

    send(Messages::DownloadProxy::DidFail(error, resumeData));

    if (m_sandboxExtension) {
        m_sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }
    m_downloadManager.downloadFinished(*this);
}

IPC::Connection* Download::messageSenderConnection() const
{
    return m_downloadManager.downloadProxyConnection();
}

uint64_t Download::messageSenderDestinationID() const
{
    return m_downloadID.toUInt64();
}

#if !PLATFORM(COCOA)
void Download::platformCancelNetworkLoad(CompletionHandler<void(const IPC::DataReference&)>&& completionHandler)
{
    completionHandler({ });
}

void Download::platformDestroyDownload()
{
}
#endif

} // namespace WebKit

#undef DOWNLOAD_RELEASE_LOG
