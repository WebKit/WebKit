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

#ifndef NetworkProcessProxy_h
#define NetworkProcessProxy_h

#include "ChildProcessProxy.h"
#include "CustomProtocolManagerProxy.h"
#include "ProcessLauncher.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "WebProcessProxyMessages.h"
#include <memory>
#include <wtf/Deque.h>

namespace WebCore {
class AuthenticationChallenge;
class ResourceRequest;
class SecurityOrigin;
class SessionID;
}

namespace WebKit {

class DownloadProxy;
class DownloadProxyMap;
class WebProcessPool;
enum class WebsiteDataFetchOption;
enum class WebsiteDataType;
struct NetworkProcessCreationParameters;

class NetworkProcessProxy : public ChildProcessProxy, private ProcessThrottlerClient {
public:
    static Ref<NetworkProcessProxy> create(WebProcessPool&);
    ~NetworkProcessProxy();

    void getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>);

    DownloadProxy* createDownloadProxy(const WebCore::ResourceRequest&);

    void fetchWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, std::function<void (WebsiteData)> completionHandler);
    void deleteWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType>, std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, OptionSet<WebKit::WebsiteDataType>, const Vector<RefPtr<WebCore::SecurityOrigin>>& origins, const Vector<String>& cookieHostNames, std::function<void ()> completionHandler);

#if PLATFORM(COCOA)
    void setProcessSuppressionEnabled(bool);
#endif

    void processReadyToSuspend();

    void setIsHoldingLockedFiles(bool);

    ProcessThrottler& throttler() { return m_throttler; }

private:
    NetworkProcessProxy(WebProcessPool&);

    // ChildProcessProxy
    virtual void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    virtual void connectionWillOpen(IPC::Connection&) override;
    virtual void processWillShutDown(IPC::Connection&) override;

    void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&);
    void networkProcessCrashedOrFailedToLaunch();

    // ProcessThrottlerClient
    void sendProcessWillSuspendImminently() override;
    void sendPrepareToSuspend() override;
    void sendCancelPrepareToSuspend() override;
    void sendProcessDidResume() override;
    void didSetAssertionState(AssertionState) override;

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;
    virtual void didClose(IPC::Connection&) override;
    virtual void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    virtual IPC::ProcessType localProcessType() override { return IPC::ProcessType::UI; }
    virtual IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::Network; }

    // Message handlers
    void didReceiveNetworkProcessProxyMessage(IPC::Connection&, IPC::MessageDecoder&);
    void didCreateNetworkConnectionToWebProcess(const IPC::Attachment&);
    void didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const WebCore::AuthenticationChallenge&, uint64_t challengeID);
    void didFetchWebsiteData(uint64_t callbackID, const WebsiteData&);
    void didDeleteWebsiteData(uint64_t callbackID);
    void didDeleteWebsiteDataForOrigins(uint64_t callbackID);
    void logSampledDiagnosticMessage(uint64_t pageID, const String& message, const String& description);
    void logSampledDiagnosticMessageWithResult(uint64_t pageID, const String& message, const String& description, uint32_t result);
    void logSampledDiagnosticMessageWithValue(uint64_t pageID, const String& message, const String& description, const String& value);

    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    WebProcessPool& m_processPool;
    
    unsigned m_numPendingConnectionRequests;
    Deque<RefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>> m_pendingConnectionReplies;

    HashMap<uint64_t, std::function<void (WebsiteData)>> m_pendingFetchWebsiteDataCallbacks;
    HashMap<uint64_t, std::function<void ()>> m_pendingDeleteWebsiteDataCallbacks;
    HashMap<uint64_t, std::function<void ()>> m_pendingDeleteWebsiteDataForOriginsCallbacks;

    std::unique_ptr<DownloadProxyMap> m_downloadProxyMap;
    CustomProtocolManagerProxy m_customProtocolManagerProxy;
    ProcessThrottler m_throttler;
    ProcessThrottler::BackgroundActivityToken m_tokenForHoldingLockedFiles;
};

} // namespace WebKit

#endif // NetworkProcessProxy_h
