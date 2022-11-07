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

#include "APIObject.h"
#include "Connection.h"
#include "DataReference.h"
#include "DownloadID.h"
#include "IdentifierTypes.h"
#include "SandboxExtension.h"
#include <WebCore/ResourceRequest.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS NSProgress;

namespace API {
class Data;
class DownloadClient;
class FrameInfo;
}

namespace WebCore {
class AuthenticationChallenge;
class IntRect;
class ProtectionSpace;
class ResourceError;
class ResourceResponse;
}

namespace WebKit {

class DownloadProxyMap;
class WebPageProxy;
class WebsiteDataStore;

enum class AllowOverwrite : bool;

struct FrameInfoData;

class DownloadProxy : public API::ObjectImpl<API::Object::Type::Download>, public IPC::MessageReceiver {
public:

    template<typename... Args> static Ref<DownloadProxy> create(Args&&... args)
    {
        return adoptRef(*new DownloadProxy(std::forward<Args>(args)...));
    }
    ~DownloadProxy();

    DownloadID downloadID() const { return m_downloadID; }
    const WebCore::ResourceRequest& request() const { return m_request; }
    API::Data* legacyResumeData() const { return m_legacyResumeData.get(); }

    void cancel(CompletionHandler<void(API::Data*)>&&);

    void invalidate();
    void processDidClose();

    void didReceiveDownloadProxyMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncDownloadProxyMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    WebPageProxy* originatingPage() const;

    void setRedirectChain(Vector<URL>&& redirectChain) { m_redirectChain = WTFMove(redirectChain); }
    const Vector<URL>& redirectChain() const { return m_redirectChain; }

    void setWasUserInitiated(bool value) { m_wasUserInitiated = value; }
    bool wasUserInitiated() const { return m_wasUserInitiated; }

    const String& destinationFilename() const { return m_destinationFilename; }
    void setDestinationFilename(const String& d) { m_destinationFilename = d; }

#if USE(SYSTEM_PREVIEW)
    bool isSystemPreviewDownload() const { return request().isSystemPreview(); }
    WebCore::SystemPreviewInfo systemPreviewDownloadInfo() const { RELEASE_ASSERT(request().systemPreviewInfo().has_value()); return *request().systemPreviewInfo(); }
#endif

#if PLATFORM(COCOA)
    void publishProgress(const URL&);
    void setProgress(NSProgress *progress) { m_progress = progress; }
    NSProgress *progress() const { return m_progress.get(); }
#endif
#if PLATFORM(MAC)
    void updateQuarantinePropertiesIfPossible();
#endif
    API::FrameInfo& frameInfo() { return m_frameInfo.get(); }

    API::DownloadClient& client() { return m_client.get(); }
    void setClient(Ref<API::DownloadClient>&&);
    void setDidStartCallback(CompletionHandler<void(DownloadProxy*)>&& callback) { m_didStartCallback = WTFMove(callback); }
    void setSuggestedFilename(const String& suggestedFilename) { m_suggestedFilename = suggestedFilename; }

    // Message handlers.
    void didStart(const WebCore::ResourceRequest&, const String& suggestedFilename);
    void didReceiveAuthenticationChallenge(WebCore::AuthenticationChallenge&&, AuthenticationChallengeIdentifier);
    void didReceiveData(uint64_t bytesWritten, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite);
    void shouldDecodeSourceDataOfMIMEType(const String& mimeType, bool& result);
    void didCreateDestination(const String& path);
    void didFinish();
    void didFail(const WebCore::ResourceError&, const IPC::DataReference& resumeData);
    void willSendRequest(WebCore::ResourceRequest&& redirectRequest, const WebCore::ResourceResponse& redirectResponse);
    void decideDestinationWithSuggestedFilename(const WebCore::ResourceResponse&, String&& suggestedFilename, CompletionHandler<void(String, SandboxExtension::Handle, AllowOverwrite)>&&);

private:
    explicit DownloadProxy(DownloadProxyMap&, WebsiteDataStore&, API::DownloadClient&, const WebCore::ResourceRequest&, const FrameInfoData&, WebPageProxy*);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    DownloadProxyMap& m_downloadProxyMap;
    RefPtr<WebsiteDataStore> m_dataStore;
    Ref<API::DownloadClient> m_client;
    DownloadID m_downloadID;

    RefPtr<API::Data> m_legacyResumeData;
    WebCore::ResourceRequest m_request;
    String m_suggestedFilename;
    String m_destinationFilename;

    WeakPtr<WebPageProxy> m_originatingPage;
    Vector<URL> m_redirectChain;
    bool m_wasUserInitiated { true };
    Ref<API::FrameInfo> m_frameInfo;
    CompletionHandler<void(DownloadProxy*)> m_didStartCallback;
#if PLATFORM(COCOA)
    RetainPtr<NSProgress> m_progress;
#endif
};

} // namespace WebKit
