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

#pragma once

#include "DownloadID.h"
#include "DownloadManager.h"
#include "DownloadMonitor.h"
#include "MessageSender.h"
#include "NetworkDataTask.h"
#include "SandboxExtension.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/ResourceRequest.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSProgress;
OBJC_CLASS NSURLSessionDownloadTask;
#endif

namespace IPC {
class DataReference;
}

namespace WebCore {
class AuthenticationChallenge;
class BlobDataFileReference;
class Credential;
class ResourceError;
class ResourceHandle;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class DownloadMonitor;
class NetworkDataTask;
class NetworkSession;
class WebPage;

class Download : public IPC::MessageSender, public CanMakeWeakPtr<Download> {
    WTF_MAKE_NONCOPYABLE(Download); WTF_MAKE_FAST_ALLOCATED;
public:
    Download(DownloadManager&, DownloadID, NetworkDataTask&, NetworkSession&, const String& suggestedFilename = { });
#if PLATFORM(COCOA)
    Download(DownloadManager&, DownloadID, NSURLSessionDownloadTask*, NetworkSession&, const String& suggestedFilename = { });
#endif

    ~Download();

    void resume(const IPC::DataReference& resumeData, const String& path, SandboxExtension::Handle&&);
    void cancel();
#if PLATFORM(COCOA)
    void publishProgress(const URL&, SandboxExtension::Handle&&);
#endif

    DownloadID downloadID() const { return m_downloadID; }
    PAL::SessionID sessionID() const { return m_sessionID; }
    const String& suggestedName() const { return m_suggestedName; }

    void setSandboxExtension(RefPtr<SandboxExtension>&& sandboxExtension) { m_sandboxExtension = WTFMove(sandboxExtension); }
    void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&);
    void didCreateDestination(const String& path);
    void didReceiveData(uint64_t length);
    void didFinish();
    void didFail(const WebCore::ResourceError&, const IPC::DataReference& resumeData);
    void didCancel(const IPC::DataReference& resumeData);

    bool isAlwaysOnLoggingAllowed() const;
    bool wasCanceled() const { return m_wasCanceled; }

    void applicationDidEnterBackground() { m_monitor.applicationDidEnterBackground(); }
    void applicationWillEnterForeground() { m_monitor.applicationWillEnterForeground(); }
    DownloadManager& manager() const { return m_downloadManager; }

    unsigned testSpeedMultiplier() const { return m_testSpeedMultiplier; }

private:
    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    void platformCancelNetworkLoad();
    void platformDestroyDownload();

    DownloadManager& m_downloadManager;
    DownloadID m_downloadID;
    Ref<DownloadManager::Client> m_client;

    Vector<RefPtr<WebCore::BlobDataFileReference>> m_blobFileReferences;
    RefPtr<SandboxExtension> m_sandboxExtension;

    RefPtr<NetworkDataTask> m_download;
#if PLATFORM(COCOA)
    RetainPtr<NSURLSessionDownloadTask> m_downloadTask;
    RetainPtr<NSProgress> m_progress;
#endif
    PAL::SessionID m_sessionID;
    String m_suggestedName;
    bool m_wasCanceled { false };
    bool m_hasReceivedData { false };
    DownloadMonitor m_monitor { *this };
    unsigned m_testSpeedMultiplier { 1 };
};

} // namespace WebKit
