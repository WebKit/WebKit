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
#include "MessageSenderInlines.h"
#include "NetworkDataTask.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "SandboxExtension.h"
#include <WebCore/NotImplemented.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include "NetworkDataTaskCocoa.h"
#endif

#define DOWNLOAD_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - Download::" fmt, this, ##__VA_ARGS__)
#define DOWNLOAD_RELEASE_LOG_WITH_THIS(thisPtr, fmt, ...) RELEASE_LOG(Network, "%p - Download::" fmt, thisPtr, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(Download);

Ref<Download> Download::create(DownloadManager& downloadManager, DownloadID downloadID, NetworkDataTask& download, NetworkSession& session, const String& suggestedName)
{
    return adoptRef(*new Download(downloadManager, downloadID, download, session, suggestedName));
}

Download::Download(DownloadManager& downloadManager, DownloadID downloadID, NetworkDataTask& download, NetworkSession& session, const String& suggestedName)
    : m_downloadManager(downloadManager)
    , m_downloadID(downloadID)
    , m_client(downloadManager.client())
    , m_download(&download)
    , m_sessionID(session.sessionID())
    , m_testSpeedMultiplier(session.testSpeedMultiplier())
{
    downloadManager.didCreateDownload();
}

#if PLATFORM(COCOA)
Ref<Download> Download::create(DownloadManager& downloadManager, DownloadID downloadID, NSURLSessionDownloadTask* download, NetworkSession& session, const String& suggestedName)
{
    return adoptRef(*new Download(downloadManager, downloadID, download, session, suggestedName));
}

Download::Download(DownloadManager& downloadManager, DownloadID downloadID, NSURLSessionDownloadTask* download, NetworkSession& session, const String& suggestedName)
    : m_downloadManager(downloadManager)
    , m_downloadID(downloadID)
    , m_client(downloadManager.client())
    , m_downloadTask(download)
    , m_sessionID(session.sessionID())
    , m_testSpeedMultiplier(session.testSpeedMultiplier())
{
    downloadManager.didCreateDownload();
}
#endif

Download::~Download()
{
    platformDestroyDownload();
    if (CheckedPtr downloadManager = m_downloadManager)
        downloadManager->didDestroyDownload();
}

void Download::cancel(CompletionHandler<void(std::span<const uint8_t>)>&& completionHandler, IgnoreDidFailCallback ignoreDidFailCallback)
{
    RELEASE_ASSERT(isMainRunLoop());

    // URLSession:task:didCompleteWithError: is still called after cancelByProducingResumeData's completionHandler.
    // If this cancel request came from the API, we do not want to send DownloadProxy::DidFail because the
    // completionHandler will inform the API that the cancellation succeeded.
    m_ignoreDidFailCallback = ignoreDidFailCallback;

    auto completionHandlerWrapper = [weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)] (std::span<const uint8_t> resumeData) mutable {
        completionHandler(resumeData);
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || protectedThis->m_ignoreDidFailCallback == IgnoreDidFailCallback::No)
            return;
        DOWNLOAD_RELEASE_LOG_WITH_THIS(protectedThis.get(), "didCancel: (id = %" PRIu64 ")", protectedThis->downloadID().toUInt64());
        if (auto extension = std::exchange(protectedThis->m_sandboxExtension, nullptr))
            extension->revoke();
        if (CheckedPtr downloadManager = protectedThis->m_downloadManager)
            downloadManager->downloadFinished(*protectedThis);
    };

    if (m_download) {
        m_download->cancel();
        completionHandlerWrapper({ });
        return;
    }
    platformCancelNetworkLoad(WTF::move(completionHandlerWrapper));
}

void Download::didReceiveChallenge(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler&& completionHandler)
{
    if (challenge.protectionSpace().isPasswordBased() && !challenge.proposedCredential().isEmpty() && !challenge.previousFailureCount()) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, challenge.proposedCredential());
        return;
    }

    protect(m_client->downloadsAuthenticationManager())->didReceiveAuthenticationChallenge(*this, challenge, WTF::move(completionHandler));
}

void Download::didCreateDestination(const String& path)
{
    send(Messages::DownloadProxy::DidCreateDestination(path));
}

void Download::didReceiveData(uint64_t bytesWritten, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite)
{
    if (!m_hasReceivedData) {
        DOWNLOAD_RELEASE_LOG("didReceiveData: Started receiving data (id = %" PRIu64 ", expected length = %" PRIu64 ")", downloadID().toUInt64(), totalBytesExpectedToWrite);
        m_hasReceivedData = true;
    }
    
    protectedMonitor()->downloadReceivedBytes(bytesWritten);

#if HAVE(MODERN_DOWNLOADPROGRESS)
    updateProgress(totalBytesWritten, totalBytesExpectedToWrite);
#endif

    send(Messages::DownloadProxy::DidReceiveData(bytesWritten, totalBytesWritten, totalBytesExpectedToWrite));
}

void Download::didFinish()
{
    DOWNLOAD_RELEASE_LOG("didFinish: (id = %" PRIu64 ")", downloadID().toUInt64());

    platformDidFinish([weakThis = WeakPtr { *this }] {
        RELEASE_ASSERT(isMainRunLoop());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->send(Messages::DownloadProxy::DidFinish());

        if (RefPtr extension = std::exchange(protectedThis->m_sandboxExtension, nullptr))
            extension->revoke();

        if (CheckedPtr downloadManager = protectedThis->m_downloadManager)
            downloadManager->downloadFinished(*protectedThis);
    });
}

void Download::didFail(const ResourceError& error, std::span<const uint8_t> resumeData)
{
    if (m_ignoreDidFailCallback == IgnoreDidFailCallback::Yes)
        return;

    DOWNLOAD_RELEASE_LOG("didFail: (id = %" PRIu64 ", isTimeout = %d, isCancellation = %d, errCode = %d)",
        downloadID().toUInt64(), error.isTimeout(), error.isCancellation(), error.errorCode());

#if HAVE(MODERN_DOWNLOADPROGRESS)
    auto resumeDataWithPlaceholder = updateResumeDataWithPlaceholderURL(m_placeholderURL.get(), resumeData);
    resumeData = resumeDataWithPlaceholder.span();
#endif

    send(Messages::DownloadProxy::DidFail(error, resumeData));

    if (RefPtr extension = std::exchange(m_sandboxExtension, nullptr))
        extension->revoke();

    if (CheckedPtr downloadManager = m_downloadManager)
        downloadManager->downloadFinished(*this);
}

IPC::Connection* Download::messageSenderConnection() const
{
    if (CheckedPtr downloadManager = m_downloadManager)
        return downloadManager->downloadProxyConnection();
    return nullptr;
}

uint64_t Download::messageSenderDestinationID() const
{
    return m_downloadID.toUInt64();
}

#if !PLATFORM(COCOA)
void Download::platformCancelNetworkLoad(CompletionHandler<void(std::span<const uint8_t>)>&& completionHandler)
{
    completionHandler({ });
}

void Download::platformDestroyDownload()
{
}

void Download::platformDidFinish(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}
#endif

} // namespace WebKit

#undef DOWNLOAD_RELEASE_LOG
