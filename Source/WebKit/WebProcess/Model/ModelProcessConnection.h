/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_PROCESS)

#include "Connection.h"
#include "MessageReceiverMap.h"
#include <wtf/AbstractThreadSafeRefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebPage;
struct ModelProcessConnectionInfo;
struct WebPageCreationParameters;


class ModelProcessConnection
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ModelProcessConnection>
    , public IPC::Connection::Client {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ModelProcessConnection);
public:
    static RefPtr<ModelProcessConnection> create(IPC::Connection& parentConnection);
    ~ModelProcessConnection();

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    IPC::Connection& connection() { return m_connection.get(); }
    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }

#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> auditToken();
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void createVisibilityPropagationContextForPage(WebPage&);
    void destroyVisibilityPropagationContextForPage(WebPage&);
#endif

    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel);

    class Client : public AbstractThreadSafeRefCountedAndCanMakeWeakPtr {
    public:
        virtual ~Client() = default;

        virtual void modelProcessConnectionDidClose(ModelProcessConnection&) { }
    };
    void addClient(const Client& client) { m_clients.add(client); }

    static constexpr Seconds defaultTimeout = 3_s;

private:
    ModelProcessConnection(IPC::Connection::Identifier&&);
    bool waitForDidInitialize();
    void invalidate();

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t indexOfObjectFailingDecoding) override;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    // Messages.
    void didInitialize(std::optional<ModelProcessConnectionInfo>&&);

    // The connection from the web process to the model process.
    Ref<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
    bool m_hasInitialized { false };
#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> m_auditToken;
#endif

    ThreadSafeWeakHashSet<Client> m_clients;
};

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
