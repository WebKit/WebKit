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

#pragma once

#include "DownloadID.h"
#include "MessageSender.h"
#include "NetworkLoadClient.h"
#include "SandboxExtension.h"
#include <WebCore/ProcessIdentifier.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {
class PendingDownload;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::PendingDownload> : std::true_type { };
}

namespace IPC {
class Connection;
}

namespace WebCore {
class ResourceResponse;

enum class FromDownloadAttribute : bool;
}

namespace WebKit {

class Download;
class NetworkLoad;
class NetworkLoadParameters;
class NetworkSession;

class PendingDownload : public NetworkLoadClient, public IPC::MessageSender, public CanMakeWeakPtr<PendingDownload> {
    WTF_MAKE_TZONE_ALLOCATED(PendingDownload);
public:
    PendingDownload(IPC::Connection*, NetworkLoadParameters&&, DownloadID, NetworkSession&, const String& suggestedName, WebCore::FromDownloadAttribute, std::optional<WebCore::ProcessIdentifier>);
    PendingDownload(IPC::Connection*, std::unique_ptr<NetworkLoad>&&, ResponseCompletionHandler&&, DownloadID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);
    virtual ~PendingDownload();

    void cancel(CompletionHandler<void(std::span<const uint8_t>)>&&);

#if PLATFORM(COCOA)
#if HAVE(MODERN_DOWNLOADPROGRESS)
    void publishProgress(const URL&, std::span<const uint8_t>);
#else
    void publishProgress(const URL&, SandboxExtension::Handle&&);
#endif
    void didBecomeDownload(const std::unique_ptr<Download>&);
#endif

private:    
    // NetworkLoadClient.
    void didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) override { }
    bool isSynchronous() const override { return false; }
    bool isAllowedToAskUserForCredentials() const final { return m_isAllowedToAskUserForCredentials; }
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) override;
    void didReceiveResponse(WebCore::ResourceResponse&&, PrivateRelayed, ResponseCompletionHandler&&) override;
    void didReceiveBuffer(const WebCore::FragmentedSharedBuffer&, uint64_t reportedEncodedDataLength) override { };
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) override { };
    void didFailLoading(const WebCore::ResourceError&) override;
    bool isDownloadTriggeredWithDownloadAttribute() const;

    // MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

private:
    std::unique_ptr<NetworkLoad> m_networkLoad;
    RefPtr<IPC::Connection> m_parentProcessConnection;
    bool m_isAllowedToAskUserForCredentials;
    bool m_isDownloadCancelled = false;
    WebCore::FromDownloadAttribute m_fromDownloadAttribute;
    std::optional<WebCore::ProcessIdentifier> m_webProcessID;

#if PLATFORM(COCOA)
    URL m_progressURL;
#if HAVE(MODERN_DOWNLOADPROGRESS)
    Vector<uint8_t> m_bookmarkData;
#else
    SandboxExtension::Handle m_progressSandboxExtension;
#endif
#endif
};

}
