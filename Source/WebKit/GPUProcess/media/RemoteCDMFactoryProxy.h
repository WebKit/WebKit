/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "MessageReceiver.h"
#include "RemoteCDMIdentifier.h"
#include "RemoteCDMInstanceIdentifier.h"
#include "RemoteCDMInstanceSessionIdentifier.h"
#include <WebCore/CDMPrivate.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class RemoteCDMInstanceProxy;
class RemoteCDMInstanceSessionProxy;
class RemoteCDMProxy;
struct RemoteCDMConfiguration;

class RemoteCDMFactoryProxy final : public RefCounted<RemoteCDMFactoryProxy>, public IPC::MessageReceiver, WebCore::CDMPrivateClient {
    WTF_MAKE_TZONE_ALLOCATED(RemoteCDMFactoryProxy);
public:
    static Ref<RemoteCDMFactoryProxy> create(GPUConnectionToWebProcess& gpuConnection)
    {
        return adoptRef(*new RemoteCDMFactoryProxy(gpuConnection));
    }
    virtual ~RemoteCDMFactoryProxy();

    void clear();

    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }
    bool didReceiveSyncMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) { return didReceiveSyncMessage(connection, decoder, encoder); }
    void didReceiveCDMMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveCDMInstanceMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveCDMInstanceSessionMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncCDMMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);
    bool didReceiveSyncCDMInstanceMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);
    bool didReceiveSyncCDMInstanceSessionMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void addProxy(const RemoteCDMIdentifier&, RefPtr<RemoteCDMProxy>&&);
    void removeProxy(const RemoteCDMIdentifier&);

    void addInstance(const RemoteCDMInstanceIdentifier&, Ref<RemoteCDMInstanceProxy>&&);
    void removeInstance(const RemoteCDMInstanceIdentifier&);
    RemoteCDMInstanceProxy* getInstance(const RemoteCDMInstanceIdentifier&);

    void addSession(const RemoteCDMInstanceSessionIdentifier&, Ref<RemoteCDMInstanceSessionProxy>&&);
    void removeSession(const RemoteCDMInstanceSessionIdentifier&);

    RefPtr<GPUConnectionToWebProcess> gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }

    bool allowsExitUnderMemoryPressure() const;

    const String& mediaKeysStorageDirectory() const;
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const;
#endif

private:
    RemoteCDMFactoryProxy(GPUConnectionToWebProcess&);

    friend class GPUProcessConnection;
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // Messages
    void createCDM(const String& keySystem, CompletionHandler<void(std::optional<RemoteCDMIdentifier>&&, RemoteCDMConfiguration&&)>&&);
    void supportsKeySystem(const String& keySystem, CompletionHandler<void(bool)>&&);

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    HashMap<RemoteCDMIdentifier, RefPtr<RemoteCDMProxy>> m_proxies;
    HashMap<RemoteCDMInstanceIdentifier, Ref<RemoteCDMInstanceProxy>> m_instances;
    HashMap<RemoteCDMInstanceSessionIdentifier, Ref<RemoteCDMInstanceSessionProxy>> m_sessions;

#if !RELEASE_LOG_DISABLED
    mutable RefPtr<Logger> m_logger;
#endif
};

}

#endif
