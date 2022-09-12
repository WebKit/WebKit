/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(IPC_TESTING_API)

#include "Connection.h"
#include "IPCConnectionTesterIdentifier.h"
#include "MessageReceiver.h"
#include <optional>
#include <wtf/HashMap.h>

namespace IPC {
class Connection;
}

namespace WebKit {

// Interface to test various IPC::Connection related activities.
class IPCConnectionTester final : public RefCounted<IPCConnectionTester>, private IPC::Connection::Client {
public:
    ~IPCConnectionTester();
    static Ref<IPCConnectionTester> create(IPC::Connection&, IPCConnectionTesterIdentifier, IPC::Connection::Handle&&);
    void stopListeningForIPC(Ref<IPCConnectionTester>&& refFromConnection);

    void sendAsyncMessages(uint32_t messageCount);
private:
    IPCConnectionTester(Ref<IPC::Connection>&&, IPCConnectionTesterIdentifier, IPC::Connection::Handle&&);
    void initialize();

    // IPC::Connection::Client overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final;

    // Messages.
    void asyncMessage(uint32_t value);
    void syncMessage(uint32_t value, CompletionHandler<void(uint32_t sameValue)>&&);

    const Ref<IPC::Connection> m_connection;
    const Ref<IPC::Connection> m_testedConnection;
    const IPCConnectionTesterIdentifier m_identifier;
    uint32_t m_previousAsyncMessageValue { 0 };
};

}

#endif
