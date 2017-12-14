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
#include "MessageSender.h"
#include "SandboxExtension.h"
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/ResourceRequest.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

#if USE(NETWORK_SESSION)
#include "NetworkDataTask.h"
#include <WebCore/AuthenticationChallenge.h>
#if PLATFORM(COCOA)
OBJC_CLASS NSURLSessionDownloadTask;
#endif
#else // USE(NETWORK_SESSION)
#if PLATFORM(COCOA)
OBJC_CLASS NSURLDownload;
OBJC_CLASS WKDownloadAsDelegate;
#endif
#endif // USE(NETWORK_SESSION)

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

class DownloadManager;
class NetworkDataTask;
class NetworkSession;
class WebPage;

class Download : public IPC::MessageSender {
    WTF_MAKE_NONCOPYABLE(Download); WTF_MAKE_FAST_ALLOCATED;
public:
#if USE(NETWORK_SESSION)
    Download(DownloadManager&, DownloadID, NetworkDataTask&, const PAL::SessionID& sessionID, const String& suggestedFilename = { });
#if PLATFORM(COCOA)
    Download(DownloadManager&, DownloadID, NSURLSessionDownloadTask*, const PAL::SessionID& sessionID, const String& suggestedFilename = { });
#endif
#else
    Download(DownloadManager&, DownloadID, const WebCore::ResourceRequest&, const String& suggestedFilename = { });
#endif

    ~Download();

#if !USE(NETWORK_SESSION)
    void setBlobFileReferences(Vector<RefPtr<WebCore::BlobDataFileReference>>&& fileReferences) { m_blobFileReferences = WTFMove(fileReferences); }

    void start();
    void startWithHandle(WebCore::ResourceHandle*, const WebCore::ResourceResponse&);
#endif
    void resume(const IPC::DataReference& resumeData, const String& path, SandboxExtension::Handle&&);
    void cancel();

    DownloadID downloadID() const { return m_downloadID; }
    const String& suggestedName() const { return m_suggestedName; }

#if USE(NETWORK_SESSION)
    void setSandboxExtension(RefPtr<SandboxExtension>&& sandboxExtension) { m_sandboxExtension = WTFMove(sandboxExtension); }
    void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&);
#else
    const WebCore::ResourceRequest& request() const { return m_request; }
    void didReceiveAuthenticationChallenge(const WebCore::AuthenticationChallenge&);
    void didStart();
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceResponse&&);
    void didReceiveResponse(const WebCore::ResourceResponse&);
    bool shouldDecodeSourceDataOfMIMEType(const String& mimeType);
    String decideDestinationWithSuggestedFilename(const String& filename, bool& allowOverwrite);
    void decideDestinationWithSuggestedFilenameAsync(const String&);
    void didDecideDownloadDestination(const String& destinationPath, SandboxExtension::Handle&&, bool allowOverwrite);
    void continueDidReceiveResponse();
    void platformDidFinish();
#endif
    void didCreateDestination(const String& path);
    void didReceiveData(uint64_t length);
    void didFinish();
    void didFail(const WebCore::ResourceError&, const IPC::DataReference& resumeData);
    void didCancel(const IPC::DataReference& resumeData);

private:
    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() override;
    uint64_t messageSenderDestinationID() override;

#if !USE(NETWORK_SESSION)
    void startNetworkLoad();
    void startNetworkLoadWithHandle(WebCore::ResourceHandle*, const WebCore::ResourceResponse&);
    void platformInvalidate();
#endif
    void platformCancelNetworkLoad();

    bool isAlwaysOnLoggingAllowed() const;

    DownloadManager& m_downloadManager;
    DownloadID m_downloadID;

    Vector<RefPtr<WebCore::BlobDataFileReference>> m_blobFileReferences;
    RefPtr<SandboxExtension> m_sandboxExtension;

#if USE(NETWORK_SESSION)
    RefPtr<NetworkDataTask> m_download;
#if PLATFORM(COCOA)
    RetainPtr<NSURLSessionDownloadTask> m_downloadTask;
#endif
    PAL::SessionID m_sessionID;
#else // USE(NETWORK_SESSION)
    WebCore::ResourceRequest m_request;
    String m_responseMIMEType;
#if PLATFORM(COCOA)
    RetainPtr<NSURLDownload> m_nsURLDownload;
    RetainPtr<WKDownloadAsDelegate> m_delegate;
#endif
    std::unique_ptr<WebCore::ResourceHandleClient> m_downloadClient;
    RefPtr<WebCore::ResourceHandle> m_resourceHandle;
#endif // USE(NETWORK_SESSION)
    String m_suggestedName;
    bool m_hasReceivedData { false };
};

} // namespace WebKit
