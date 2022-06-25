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

#if ENABLE(BUILT_IN_NOTIFICATIONS)

#include "DaemonConnection.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebPushDaemonConstants.h"

namespace WebKit {

class NetworkNotificationManager;
class NetworkSession;

namespace WebPushD {

struct ConnectionTraits {
    using MessageType = WebPushD::MessageType;
    static constexpr const char* protocolVersionKey { WebPushD::protocolVersionKey };
    static constexpr uint64_t protocolVersionValue { WebPushD::protocolVersionValue };
    static constexpr const char* protocolEncodedMessageKey { WebPushD::protocolEncodedMessageKey };
};

class Connection : public Daemon::ConnectionToMachService<ConnectionTraits> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Connection(CString&& machServiceName, NetworkNotificationManager&, WebPushDaemonConnectionConfiguration&&);

    void debugMessage(const String&);
    void setConfiguration(WebPushDaemonConnectionConfiguration&&);

private:
    void newConnectionWasInitialized() const final;
#if PLATFORM(COCOA)
    RetainPtr<xpc_object_t> dictionaryFromMessage(MessageType, Daemon::EncodedMessage&&) const final;
    void connectionReceivedEvent(xpc_object_t) final;
#endif
    void sendDebugModeIsEnabledMessageIfNecessary() const;

    NetworkSession& networkSession() const;

    NetworkNotificationManager& m_notificationManager;
    WebPushDaemonConnectionConfiguration m_configuration;

    template<MessageType messageType, typename... Args>
    void sendMessage(Args&&...) const;
    template<MessageType messageType, typename... Args, typename... ReplyArgs>
    void sendMessageWithReply(CompletionHandler<void(ReplyArgs...)>&&, Args&&...) const;
};

} // namespace WebPushD
} // namespace WebKit

#endif // ENABLE(BUILT_IN_NOTIFICATIONS)

