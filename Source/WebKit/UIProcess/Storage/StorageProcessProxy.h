/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "ChildProcessProxy.h"
#include "ProcessLauncher.h"
#include "WebProcessProxyMessages.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>

namespace WebCore {
class SecurityOrigin;
class SessionID;
struct SecurityOriginData;
}

namespace WebKit {

class WebProcessPool;
enum class WebsiteDataType;
struct WebsiteData;

class StorageProcessProxy : public ChildProcessProxy {
public:
    static Ref<StorageProcessProxy> create(WebProcessPool*);
    ~StorageProcessProxy();

    void fetchWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType>, WTF::Function<void(WebsiteData)>&& completionHandler);
    void deleteWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType>, std::chrono::system_clock::time_point modifiedSince, WTF::Function<void()>&& completionHandler);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, WTF::Function<void()>&& completionHandler);

    void getStorageProcessConnection(Ref<Messages::WebProcessProxy::GetStorageProcessConnection::DelayedReply>&&);

private:
    StorageProcessProxy(WebProcessPool*);

    // ChildProcessProxy
    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void processWillShutDown(IPC::Connection&) override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    void didReceiveStorageProcessProxyMessage(IPC::Connection&, IPC::Decoder&);

    // Message handlers
    void didCreateStorageToWebProcessConnection(const IPC::Attachment&);
    void didFetchWebsiteData(uint64_t callbackID, const WebsiteData&);
    void didDeleteWebsiteData(uint64_t callbackID);
    void didDeleteWebsiteDataForOrigins(uint64_t callbackID);
#if ENABLE(SANDBOX_EXTENSIONS)
    void getSandboxExtensionsForBlobFiles(uint64_t requestID, const Vector<String>& paths);
#endif

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    WebProcessPool* m_processPool;

    unsigned m_numPendingConnectionRequests;
    Deque<Ref<Messages::WebProcessProxy::GetStorageProcessConnection::DelayedReply>> m_pendingConnectionReplies;

    HashMap<uint64_t, WTF::Function<void (WebsiteData)>> m_pendingFetchWebsiteDataCallbacks;
    HashMap<uint64_t, WTF::Function<void ()>> m_pendingDeleteWebsiteDataCallbacks;
    HashMap<uint64_t, WTF::Function<void ()>> m_pendingDeleteWebsiteDataForOriginsCallbacks;
};

} // namespace WebKit
