/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "MessageSenderInlines.h"
#include "PushMessageForTesting.h"
#include <memory>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>

using WebKit::WebPushD::PushMessageForTesting;

namespace WebPushTool {

enum class Action {
    StreamDebugMessages,
};

enum class PreferTestService : bool {
    No,
    Yes,
};

enum class Reconnect : bool {
    No,
    Yes,
};

enum class WaitForServiceToExist : bool {
    No,
    Yes,
};

class Connection final : public CanMakeWeakPtr<Connection>, public IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<Connection> create(std::optional<Action>, PreferTestService, Reconnect);
    Connection(std::optional<Action>, PreferTestService, Reconnect);
    ~Connection() final { }

    void connectToService(WaitForServiceToExist);

    void setPushMessage(std::unique_ptr<PushMessageForTesting>&& message) { m_pushMessage = WTFMove(message); }

private:
    void messageReceived(xpc_object_t);
    void connectionDropped();

    void startAction();
    void startDebugStreamAction();
    void sendPushMessage();

    void sendAuditToken();

    bool performSendWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&&) const final;
    bool performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&&, CompletionHandler<void(IPC::Decoder*)>&&) const final;
    IPC::Connection* messageSenderConnection() const final { return nullptr; }
    uint64_t messageSenderDestinationID() const final { return 0; }

    std::optional<Action> m_action;
    bool m_reconnect { false };
    RetainPtr<xpc_connection_t> m_connection;
    const char* m_serviceName;

    std::unique_ptr<PushMessageForTesting> m_pushMessage;
};

} // namespace WebPushTool
