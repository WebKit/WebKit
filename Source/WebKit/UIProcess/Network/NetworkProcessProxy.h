/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "APIWebsiteDataStore.h"
#include "ChildProcessProxy.h"
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManagerProxy.h"
#endif
#include "NetworkProcessProxyMessages.h"
#include "ProcessLauncher.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "UserContentControllerIdentifier.h"
#include "WebProcessProxyMessages.h"
#include <memory>
#include <wtf/Deque.h>

namespace PAL {
class SessionID;
}

namespace WebCore {
class AuthenticationChallenge;
class ProtectionSpace;
class ResourceRequest;
enum class ShouldSample : bool;
class SecurityOrigin;
struct SecurityOriginData;
}

namespace WebKit {

class DownloadProxy;
class DownloadProxyMap;
class WebProcessPool;
enum class WebsiteDataFetchOption;
enum class WebsiteDataType;
struct NetworkProcessCreationParameters;
class WebUserContentControllerProxy;
struct WebsiteData;

class NetworkProcessProxy final : public ChildProcessProxy, private ProcessThrottlerClient {
public:
    explicit NetworkProcessProxy(WebProcessPool&);
    ~NetworkProcessProxy();

    void getNetworkProcessConnection(WebProcessProxy&, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply&&);

    DownloadProxy* createDownloadProxy(const WebCore::ResourceRequest&);

    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, CompletionHandler<void(WebsiteData)>&&);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType>, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebKit::WebsiteDataType>, const Vector<WebCore::SecurityOriginData>& origins, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, CompletionHandler<void()>&&);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void updatePrevalentDomainsToBlockCookiesFor(PAL::SessionID, const Vector<String>& domainsToBlock, CompletionHandler<void()>&&);
    void setAgeCapForClientSideCookies(PAL::SessionID, Optional<Seconds>, CompletionHandler<void()>&&);
    void hasStorageAccessForFrame(PAL::SessionID, const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback);
    void getAllStorageAccessEntries(PAL::SessionID, CompletionHandler<void(Vector<String>&& domains)>&&);
    void grantStorageAccess(PAL::SessionID, const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback);
    void removeAllStorageAccess(PAL::SessionID, CompletionHandler<void()>&&);
    void setCacheMaxAgeCapForPrevalentResources(PAL::SessionID, Seconds, CompletionHandler<void()>&&);
    void resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID, CompletionHandler<void()>&&);
#endif

    void writeBlobToFilePath(const URL&, const String& path, CompletionHandler<void(bool)>&&);

    void processReadyToSuspend();
    
    void sendProcessDidTransitionToForeground();
    void sendProcessDidTransitionToBackground();

    void setIsHoldingLockedFiles(bool);
    
    void syncAllCookies();
    void didSyncAllCookies();

    ProcessThrottler& throttler() { return m_throttler; }
    WebProcessPool& processPool() { return m_processPool; }

#if ENABLE(CONTENT_EXTENSIONS)
    void didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy&);
#endif

    void addSession(Ref<WebsiteDataStore>&&);
    void removeSession(PAL::SessionID);

private:
    // ChildProcessProxy
    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    void networkProcessCrashed();
    void clearCallbackStates();

    // ProcessThrottlerClient
    void sendProcessWillSuspendImminently() override;
    void sendPrepareToSuspend() override;
    void sendCancelPrepareToSuspend() override;
    void sendProcessDidResume() override;
    void didSetAssertionState(AssertionState) override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // Message handlers
    void didReceiveNetworkProcessProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didCreateNetworkConnectionToWebProcess(const IPC::Attachment&);
    void didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, WebCore::AuthenticationChallenge&&, uint64_t challengeID);
    void didFetchWebsiteData(uint64_t callbackID, const WebsiteData&);
    void didDeleteWebsiteData(uint64_t callbackID);
    void didDeleteWebsiteDataForOrigins(uint64_t callbackID);
    void logDiagnosticMessage(uint64_t pageID, const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithResult(uint64_t pageID, const String& message, const String& description, uint32_t result, WebCore::ShouldSample);
    void logDiagnosticMessageWithValue(uint64_t pageID, const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample);
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void didLogUserInteraction(uint64_t contextId);
    void didUpdateBlockCookies(uint64_t contextId);
    void didSetAgeCapForClientSideCookies(uint64_t contextId);
    void storageAccessRequestResult(bool wasGranted, uint64_t contextId);
    void allStorageAccessEntriesResult(Vector<String>&& domains, uint64_t contextId);
    void didRemoveAllStorageAccess(uint64_t contextId);
    void didSetCacheMaxAgeCapForPrevalentResources(uint64_t contextId);
    void didResetCacheMaxAgeCapForPrevalentResources(uint64_t contextId);
#endif
    void retrieveCacheStorageParameters(PAL::SessionID);

#if ENABLE(CONTENT_EXTENSIONS)
    void contentExtensionRules(UserContentControllerIdentifier);
#endif

#if ENABLE(SANDBOX_EXTENSIONS)
    void getSandboxExtensionsForBlobFiles(const Vector<String>& paths, Messages::NetworkProcessProxy::GetSandboxExtensionsForBlobFiles::AsyncReply&&);
#endif

#if ENABLE(SERVICE_WORKER)
    void establishWorkerContextConnectionToNetworkProcess(WebCore::SecurityOriginData&&);
    void establishWorkerContextConnectionToNetworkProcessForExplicitSession(WebCore::SecurityOriginData&&, PAL::SessionID);
#endif

    WebsiteDataStore* websiteDataStoreFromSessionID(PAL::SessionID);

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    WebProcessPool& m_processPool;
    
    unsigned m_numPendingConnectionRequests;
    Deque<std::pair<WeakPtr<WebProcessProxy>, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>> m_pendingConnectionReplies;

    HashMap<uint64_t, CompletionHandler<void(WebsiteData)>> m_pendingFetchWebsiteDataCallbacks;
    HashMap<uint64_t, CompletionHandler<void()>> m_pendingDeleteWebsiteDataCallbacks;
    HashMap<uint64_t, CompletionHandler<void()>> m_pendingDeleteWebsiteDataForOriginsCallbacks;

    std::unique_ptr<DownloadProxyMap> m_downloadProxyMap;
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    LegacyCustomProtocolManagerProxy m_customProtocolManagerProxy;
#endif
    ProcessThrottler m_throttler;
    ProcessThrottler::BackgroundActivityToken m_tokenForHoldingLockedFiles;
    ProcessThrottler::BackgroundActivityToken m_syncAllCookiesToken;
    
    unsigned m_syncAllCookiesCounter { 0 };

    HashMap<uint64_t, CompletionHandler<void()>> m_updateBlockCookiesCallbackMap;
    HashMap<uint64_t, CompletionHandler<void(bool wasGranted)>> m_storageAccessResponseCallbackMap;
    HashMap<uint64_t, CompletionHandler<void()>> m_removeAllStorageAccessCallbackMap;
    HashMap<uint64_t, CompletionHandler<void(Vector<String>&& domains)>> m_allStorageAccessEntriesCallbackMap;

    HashMap<uint64_t, CompletionHandler<void()>> m_updateRuntimeSettingsCallbackMap;

#if ENABLE(CONTENT_EXTENSIONS)
    HashSet<WebUserContentControllerProxy*> m_webUserContentControllerProxies;
#endif

    HashMap<PAL::SessionID, RefPtr<WebsiteDataStore>> m_websiteDataStores;
};

} // namespace WebKit
