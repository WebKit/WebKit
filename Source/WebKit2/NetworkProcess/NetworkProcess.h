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

#ifndef NetworkProcess_h
#define NetworkProcess_h

#if ENABLE(NETWORK_PROCESS)

#include "CacheModel.h"
#include "ChildProcess.h"
#include "DownloadManager.h"
#include "NetworkResourceLoadScheduler.h"
#include <wtf/Forward.h>

namespace WebCore {
    class RunLoop;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
struct NetworkProcessCreationParameters;

class NetworkProcess : ChildProcess, DownloadManager::Client {
    WTF_MAKE_NONCOPYABLE(NetworkProcess);
public:
    static NetworkProcess& shared();

    void initialize(CoreIPC::Connection::Identifier, WebCore::RunLoop*);

    void removeNetworkConnectionToWebProcess(NetworkConnectionToWebProcess*);

    NetworkResourceLoadScheduler& networkResourceLoadScheduler() { return m_networkResourceLoadScheduler; }

    DownloadManager& downloadManager();

private:
    NetworkProcess();
    ~NetworkProcess();

    void platformInitialize(const NetworkProcessCreationParameters&);

    // ChildProcess
    virtual bool shouldTerminate() OVERRIDE;

    // CoreIPC::Connection::Client
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&) OVERRIDE;
    virtual void didClose(CoreIPC::Connection*) OVERRIDE;
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference messageReceiverName, CoreIPC::StringReference messageName) OVERRIDE;

    // DownloadManager::Client
    virtual void didCreateDownload() OVERRIDE;
    virtual void didDestroyDownload() OVERRIDE;
    virtual CoreIPC::Connection* downloadProxyConnection() OVERRIDE;

    // Message Handlers
    void didReceiveNetworkProcessMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void initializeNetworkProcess(const NetworkProcessCreationParameters&);
    void createNetworkConnectionToWebProcess();
    void ensurePrivateBrowsingSession();
    void destroyPrivateBrowsingSession();
    void setCacheModel(uint32_t);
#if ENABLE(CUSTOM_PROTOCOLS)
    void registerSchemeForCustomProtocol(const String&);
    void unregisterSchemeForCustomProtocol(const String&);
#endif

    // Platform Helpers
    void platformSetCacheModel(CacheModel);

    // The connection to the UI process.
    RefPtr<CoreIPC::Connection> m_uiConnection;

    // Connections to WebProcesses.
    Vector<RefPtr<NetworkConnectionToWebProcess> > m_webProcessConnections;

    NetworkResourceLoadScheduler m_networkResourceLoadScheduler;

    String m_diskCacheDirectory;
    bool m_hasSetCacheModel;
    CacheModel m_cacheModel;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // NetworkProcess_h
