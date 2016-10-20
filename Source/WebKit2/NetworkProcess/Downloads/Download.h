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

#ifndef Download_h
#define Download_h

#include "DownloadID.h"
#include "MessageSender.h"
#include "SandboxExtension.h"
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SessionID.h>
#include <memory>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

#if USE(NETWORK_SESSION)
#if PLATFORM(COCOA)
OBJC_CLASS NSURLSessionDownloadTask;
typedef NSURLSessionDownloadTask* PlatformDownloadTaskRef;
#elif USE(SOUP)
namespace WebKit {
class NetworkDataTask;
}
typedef WebKit::NetworkDataTask* PlatformDownloadTaskRef;
#endif
#else // USE(NETWORK_SESSION)
#if PLATFORM(COCOA)
OBJC_CLASS NSURLDownload;
OBJC_CLASS WKDownloadAsDelegate;
#endif
#endif // USE(NETWORK_SESSION)

#if USE(CFURLCONNECTION)
#include <CFNetwork/CFURLDownloadPriv.h>
#endif

namespace IPC {
class DataReference;
}

namespace WebCore {
class AuthenticationChallenge;
class Credential;
class ResourceError;
class ResourceHandle;
class ResourceResponse;
}

namespace WebKit {

class DownloadManager;
class NetworkSession;
class WebPage;

class Download : public IPC::MessageSender {
    WTF_MAKE_NONCOPYABLE(Download); WTF_MAKE_FAST_ALLOCATED;
public:
#if USE(NETWORK_SESSION)
    Download(DownloadManager&, DownloadID, PlatformDownloadTaskRef, const WebCore::SessionID& sessionID, const String& suggestedFilename = { });
#endif
    Download(DownloadManager&, DownloadID, const WebCore::ResourceRequest&, const String& suggestedFilename = { });

    ~Download();

    void start();
    void startWithHandle(WebCore::ResourceHandle*, const WebCore::ResourceResponse&);
    void resume(const IPC::DataReference& resumeData, const String& path, const SandboxExtension::Handle&);
    void cancel();

    DownloadID downloadID() const { return m_downloadID; }
    const String& suggestedName() const { return m_suggestedName; }
    const WebCore::ResourceRequest& request() const { return m_request; }

#if USE(NETWORK_SESSION)
    void setSandboxExtension(RefPtr<SandboxExtension>&& sandboxExtension) { m_sandboxExtension = WTFMove(sandboxExtension); }
#else
    void didReceiveAuthenticationChallenge(const WebCore::AuthenticationChallenge&);
#endif
    void didStart();
    void didReceiveResponse(const WebCore::ResourceResponse&);
    void didReceiveData(uint64_t length);
    bool shouldDecodeSourceDataOfMIMEType(const String& mimeType);
#if !USE(NETWORK_SESSION)
    String decideDestinationWithSuggestedFilename(const String& filename, bool& allowOverwrite);
#endif
    void decideDestinationWithSuggestedFilenameAsync(const String&);
    void didDecideDownloadDestination(const String& destinationPath, const SandboxExtension::Handle&, bool allowOverwrite);
    void continueDidReceiveResponse();
    void didCreateDestination(const String& path);
    void didFinish();
    void platformDidFinish();
    void didFail(const WebCore::ResourceError&, const IPC::DataReference& resumeData);
    void didCancel(const IPC::DataReference& resumeData);

private:
    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() override;
    uint64_t messageSenderDestinationID() override;

#if !USE(NETWORK_SESSION)
    void startNetworkLoad();
    void startNetworkLoadWithHandle(WebCore::ResourceHandle*, const WebCore::ResourceResponse&);
#endif
    void cancelNetworkLoad();

    void platformInvalidate();

    bool isAlwaysOnLoggingAllowed() const;

    DownloadManager& m_downloadManager;
    DownloadID m_downloadID;
    WebCore::ResourceRequest m_request;

    RefPtr<SandboxExtension> m_sandboxExtension;

#if USE(NETWORK_SESSION)
#if PLATFORM(COCOA)
    RetainPtr<NSURLSessionDownloadTask> m_download;
#elif USE(SOUP)
    RefPtr<NetworkDataTask> m_download;
#endif
    WebCore::SessionID m_sessionID;
#else // USE(NETWORK_SESSION)
#if PLATFORM(COCOA)
    RetainPtr<NSURLDownload> m_nsURLDownload;
    RetainPtr<WKDownloadAsDelegate> m_delegate;
#endif
#endif // USE(NETWORK_SESSION)
#if USE(CFURLCONNECTION)
    RetainPtr<CFURLDownloadRef> m_download;
#endif
    std::unique_ptr<WebCore::ResourceHandleClient> m_downloadClient;
    RefPtr<WebCore::ResourceHandle> m_resourceHandle;
    String m_suggestedName;
    bool m_hasReceivedData { false };
};

} // namespace WebKit

#endif // Download_h
