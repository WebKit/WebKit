/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef NetworkProcess_h
#define NetworkProcess_h

#include "CacheModel.h"
#include "ChildProcess.h"
#include "DownloadManager.h"
#include "MessageReceiverMap.h"
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/MemoryPressureHandler.h>
#include <WebCore/SessionID.h>
#include <memory>
#include <wtf/Forward.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(IOS)
#include "WebSQLiteDatabaseTracker.h"
#endif

namespace WebCore {
class CertificateInfo;
class NetworkStorageSession;
class SecurityOrigin;
class SessionID;
struct SecurityOriginData;
}

namespace WebKit {
class AuthenticationManager;
class NetworkConnectionToWebProcess;
class NetworkProcessSupplement;
struct NetworkProcessCreationParameters;

class NetworkProcess : public ChildProcess, private DownloadManager::Client {
    WTF_MAKE_NONCOPYABLE(NetworkProcess);
    friend class NeverDestroyed<NetworkProcess>;
    friend class NeverDestroyed<DownloadManager>;
public:
    static NetworkProcess& singleton();

    template <typename T>
    T* supplement()
    {
        return static_cast<T*>(m_supplements.get(T::supplementName()));
    }

    template <typename T>
    void addSupplement()
    {
        m_supplements.add(T::supplementName(), std::make_unique<T>(this));
    }

    void removeNetworkConnectionToWebProcess(NetworkConnectionToWebProcess*);

    AuthenticationManager& authenticationManager();
    DownloadManager& downloadManager();
    bool canHandleHTTPSServerTrustEvaluation() const { return m_canHandleHTTPSServerTrustEvaluation; }

    void processWillSuspendImminently(bool& handled);
    void prepareToSuspend();
    void cancelPrepareToSuspend();
    void processDidResume();

    // Diagnostic messages logging.
    void logDiagnosticMessage(uint64_t webPageID, const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithResult(uint64_t webPageID, const String& message, const String& description, WebCore::DiagnosticLoggingResultType, WebCore::ShouldSample);
    void logDiagnosticMessageWithValue(uint64_t webPageID, const String& message, const String& description, const String& value, WebCore::ShouldSample);

#if USE(CFURLCACHE)
    static Vector<Ref<WebCore::SecurityOrigin>> cfURLCacheOrigins();
    static void clearCFURLCacheForOrigins(const Vector<WebCore::SecurityOriginData>&);
#endif

#if PLATFORM(COCOA)
    void clearHSTSCache(WebCore::NetworkStorageSession&, std::chrono::system_clock::time_point modifiedSince);
#endif

    void prefetchDNS(const String&);

private:
    NetworkProcess();
    ~NetworkProcess();

    void platformInitializeNetworkProcess(const NetworkProcessCreationParameters&);

    virtual void terminate() override;
    void platformTerminate();

    void lowMemoryHandler(WebCore::Critical);
    void platformLowMemoryHandler(WebCore::Critical);

    // ChildProcess
    virtual void initializeProcess(const ChildProcessInitializationParameters&) override;
    virtual void initializeProcessName(const ChildProcessInitializationParameters&) override;
    virtual void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&) override;
    virtual void initializeConnection(IPC::Connection*) override;
    virtual bool shouldTerminate() override;

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;
    virtual void didClose(IPC::Connection&) override;
    virtual void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    virtual IPC::ProcessType localProcessType() override { return IPC::ProcessType::Network; }
    virtual IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::UI; }

    // DownloadManager::Client
    virtual void didCreateDownload() override;
    virtual void didDestroyDownload() override;
    virtual IPC::Connection* downloadProxyConnection() override;
    virtual AuthenticationManager& downloadsAuthenticationManager() override;

    // Message Handlers
    void didReceiveNetworkProcessMessage(IPC::Connection&, IPC::MessageDecoder&);
    void didReceiveSyncNetworkProcessMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);
    void initializeNetworkProcess(const NetworkProcessCreationParameters&);
    void createNetworkConnectionToWebProcess();
    void ensurePrivateBrowsingSession(WebCore::SessionID);
    void destroyPrivateBrowsingSession(WebCore::SessionID);

    void fetchWebsiteData(WebCore::SessionID, uint64_t websiteDataTypes, uint64_t callbackID);
    void deleteWebsiteData(WebCore::SessionID, uint64_t websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, uint64_t websiteDataTypes, const Vector<WebCore::SecurityOriginData>& origins, const Vector<String>& cookieHostNames, uint64_t callbackID);

    // FIXME: This should take a session ID so we can identify which disk cache to delete.
    void clearDiskCache(std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler);

    void downloadRequest(WebCore::SessionID, DownloadID, const WebCore::ResourceRequest&);
    void resumeDownload(WebCore::SessionID, DownloadID, const IPC::DataReference& resumeData, const String& path, const SandboxExtension::Handle&);
    void cancelDownload(DownloadID);
#if USE(NETWORK_SESSION)
    void continueCanAuthenticateAgainstProtectionSpace(DownloadID, bool canAuthenticate);
    void continueWillSendRequest(DownloadID, const WebCore::ResourceRequest&);
#endif
    void setCacheModel(uint32_t);
    void allowSpecificHTTPSCertificateForHost(const WebCore::CertificateInfo&, const String& host);
    void setCanHandleHTTPSServerTrustEvaluation(bool);
    void getNetworkProcessStatistics(uint64_t callbackID);
    void clearCacheForAllOrigins(uint32_t cachesToClear);

#if USE(SOUP)
    void setIgnoreTLSErrors(bool);
    void userPreferredLanguagesChanged(const Vector<String>&);
#endif

    // Platform Helpers
    void platformSetCacheModel(CacheModel);

    // Connections to WebProcesses.
    Vector<RefPtr<NetworkConnectionToWebProcess>> m_webProcessConnections;

    String m_diskCacheDirectory;
    bool m_hasSetCacheModel;
    CacheModel m_cacheModel;
    int64_t m_diskCacheSizeOverride { -1 };
    bool m_diskCacheIsDisabledForTesting;
    bool m_canHandleHTTPSServerTrustEvaluation;

    typedef HashMap<const char*, std::unique_ptr<NetworkProcessSupplement>, PtrHash<const char*>> NetworkProcessSupplementMap;
    NetworkProcessSupplementMap m_supplements;

#if PLATFORM(COCOA)
    void platformInitializeNetworkProcessCocoa(const NetworkProcessCreationParameters&);

    // FIXME: We'd like to be able to do this without the #ifdef, but WorkQueue + BinarySemaphore isn't good enough since
    // multiple requests to clear the cache can come in before previous requests complete, and we need to wait for all of them.
    // In the future using WorkQueue and a counting semaphore would work, as would WorkQueue supporting the libdispatch concept of "work groups".
    dispatch_group_t m_clearCacheDispatchGroup;
#endif

#if PLATFORM(IOS)
    WebSQLiteDatabaseTracker m_webSQLiteDatabaseTracker;
#endif
};

} // namespace WebKit

#endif // NetworkProcess_h
