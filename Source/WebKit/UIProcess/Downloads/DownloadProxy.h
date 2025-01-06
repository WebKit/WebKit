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
#include "DownloadID.h"
#include "IdentifierTypes.h"
#include "SandboxExtension.h"
#include "UseDownloadPlaceholder.h"
#include "WebsiteDataStore.h"
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
class ProcessAssertion;
class WebPageProxy;

enum class AllowOverwrite : bool;

struct FrameInfoData;

class DownloadProxy : public API::ObjectImpl<API::Object::Type::Download>, public IPC::MessageReceiver {
public:
    using DecideDestinationCallback = CompletionHandler<void(String, SandboxExtension::Handle, AllowOverwrite, WebKit::UseDownloadPlaceholder, const URL&, SandboxExtension::Handle, std::span<const uint8_t>, std::span<const uint8_t>)>;

    template<typename... Args> static Ref<DownloadProxy> create(Args&&... args)
    {
        return adoptRef(*new DownloadProxy(std::forward<Args>(args)...));
    }
    ~DownloadProxy();

    void ref() const final { API::ObjectImpl<API::Object::Type::Download>::ref(); }
    void deref() const final { API::ObjectImpl<API::Object::Type::Download>::deref(); }

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
    void didFail(const WebCore::ResourceError&, std::span<const uint8_t> resumeData);
#if HAVE(MODERN_DOWNLOADPROGRESS)
    void didReceivePlaceholderURL(const URL&, std::span<const uint8_t> bookmarkData, WebKit::SandboxExtensionHandle&&, CompletionHandler<void()>&&);
    void didReceiveFinalURL(const URL&, std::span<const uint8_t> bookmarkData, WebKit::SandboxExtensionHandle&&);
    void didStartUpdatingProgress();
#endif
    void willSendRequest(WebCore::ResourceRequest&& redirectRequest, const WebCore::ResourceResponse& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&&);
    void decideDestinationWithSuggestedFilename(const WebCore::ResourceResponse&, String&& suggestedFilename, DecideDestinationCallback&&);

private:
    explicit DownloadProxy(DownloadProxyMap&, WebsiteDataStore&, API::DownloadClient&, const WebCore::ResourceRequest&, const FrameInfoData&, WebPageProxy*);

    Ref<API::DownloadClient> protectedClient() const;
    RefPtr<WebsiteDataStore> protectedDataStore() { return m_dataStore; }

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

#if HAVE(MODERN_DOWNLOADPROGRESS)
    static Vector<uint8_t> bookmarkDataForURL(const URL&);
    static Vector<uint8_t> activityAccessToken();
#endif

    WeakPtr<DownloadProxyMap> m_downloadProxyMap;
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
    bool m_downloadIsCancelled { false };
    Ref<API::FrameInfo> m_frameInfo;
    CompletionHandler<void(DownloadProxy*)> m_didStartCallback;
#if PLATFORM(COCOA)
    RetainPtr<NSProgress> m_progress;
#endif
#if HAVE(MODERN_DOWNLOADPROGRESS)
    RefPtr<ProcessAssertion> m_assertion;
#endif
};

} // namespace WebKit
