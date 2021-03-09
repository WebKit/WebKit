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
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class RemoteCDMInstanceProxy;
class RemoteCDMInstanceSessionProxy;
class RemoteCDMProxy;
struct RemoteCDMConfiguration;

class RemoteCDMFactoryProxy final : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteCDMFactoryProxy(GPUConnectionToWebProcess&);
    virtual ~RemoteCDMFactoryProxy();

    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }
    void didReceiveSyncMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& encoder) { didReceiveSyncMessage(connection, decoder, encoder); }
    void didReceiveCDMMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveCDMInstanceMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveCDMInstanceSessionMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncCDMMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);
    void didReceiveSyncCDMInstanceMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);
    void didReceiveSyncCDMInstanceSessionMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    void addProxy(const RemoteCDMIdentifier&, std::unique_ptr<RemoteCDMProxy>&&);
    void removeProxy(const RemoteCDMIdentifier&);

    void addInstance(const RemoteCDMInstanceIdentifier&, std::unique_ptr<RemoteCDMInstanceProxy>&&);
    void removeInstance(const RemoteCDMInstanceIdentifier&);
    RemoteCDMInstanceProxy* getInstance(const RemoteCDMInstanceIdentifier&);

    void addSession(const RemoteCDMInstanceSessionIdentifier&, std::unique_ptr<RemoteCDMInstanceSessionProxy>&&);
    void removeSession(const RemoteCDMInstanceSessionIdentifier&);

    GPUConnectionToWebProcess* gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }

private:
    friend class GPUProcessConnection;
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final { }
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    // Messages
    void createCDM(const String& keySystem, CompletionHandler<void(RemoteCDMIdentifier&&, RemoteCDMConfiguration&&)>&&);
    void supportsKeySystem(const String& keySystem, CompletionHandler<void(bool)>&&);

    WeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    HashMap<RemoteCDMIdentifier, std::unique_ptr<RemoteCDMProxy>> m_proxies;
    HashMap<RemoteCDMInstanceIdentifier, std::unique_ptr<RemoteCDMInstanceProxy>> m_instances;
    HashMap<RemoteCDMInstanceSessionIdentifier, std::unique_ptr<RemoteCDMInstanceSessionProxy>> m_sessions;
};

}

#endif
