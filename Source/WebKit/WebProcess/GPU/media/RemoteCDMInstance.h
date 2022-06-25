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

#include "MessageReceiver.h"
#include "RemoteCDMFactory.h"
#include "RemoteCDMInstanceConfiguration.h"
#include "RemoteCDMInstanceIdentifier.h"
#include <WebCore/CDMInstance.h>

namespace WebKit {

class RemoteCDMInstance final : public WebCore::CDMInstance, private IPC::MessageReceiver {
public:
    virtual ~RemoteCDMInstance();
    static Ref<RemoteCDMInstance> create(WeakPtr<RemoteCDMFactory>&&, RemoteCDMInstanceIdentifier&&, RemoteCDMInstanceConfiguration&&);

    const RemoteCDMInstanceIdentifier& identifier() const { return m_identifier; }

private:
    RemoteCDMInstance(WeakPtr<RemoteCDMFactory>&&, RemoteCDMInstanceIdentifier&&, RemoteCDMInstanceConfiguration&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void unrequestedInitializationDataReceived(const String&, Ref<WebCore::SharedBuffer>&&);

    ImplementationType implementationType() const final { return ImplementationType::Remote; }
    void initializeWithConfiguration(const WebCore::CDMKeySystemConfiguration&, AllowDistinctiveIdentifiers, AllowPersistentState, SuccessCallback&&) final;
    void setServerCertificate(Ref<WebCore::SharedBuffer>&&, SuccessCallback&&) final;
    void setStorageDirectory(const String&) final;
    const String& keySystem() const final { return m_configuration.keySystem; }
    RefPtr<WebCore::CDMInstanceSession> createSession() final;
    void setClient(WeakPtr<WebCore::CDMInstanceClient>&& client) final { m_client = WTFMove(client); }
    void clearClient() final { m_client.clear(); }

    WeakPtr<RemoteCDMFactory> m_factory;
    RemoteCDMInstanceIdentifier m_identifier;
    RemoteCDMInstanceConfiguration m_configuration;
    WeakPtr<WebCore::CDMInstanceClient> m_client;
};

}

SPECIALIZE_TYPE_TRAITS_CDM_INSTANCE(WebKit::RemoteCDMInstance, WebCore::CDMInstance::ImplementationType::Remote)

#endif
