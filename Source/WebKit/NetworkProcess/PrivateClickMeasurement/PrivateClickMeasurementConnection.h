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

#include "DaemonConnection.h"
#include "PrivateClickMeasurementManagerInterface.h"

namespace WebKit {

class NetworkSession;

namespace PCM {

enum class MessageType : uint8_t;

struct ConnectionTraits {
    using MessageType = WebKit::PCM::MessageType;
    static constexpr const char* protocolVersionKey { PCM::protocolVersionKey };
    static constexpr uint64_t protocolVersionValue { PCM::protocolVersionValue };
    static constexpr const char* protocolEncodedMessageKey { PCM::protocolEncodedMessageKey };
};

class Connection : public Daemon::ConnectionToMachService<ConnectionTraits> {
public:
    Connection(CString&& machServiceName, NetworkSession&);

private:
    void newConnectionWasInitialized() const final;
#if PLATFORM(COCOA)
    RetainPtr<xpc_object_t> dictionaryFromMessage(MessageType, Daemon::EncodedMessage&&) const final;
    void connectionReceivedEvent(xpc_object_t) final;
#endif
    void sendDebugModeIsEnabledMessageIfNecessary() const;

    WeakPtr<NetworkSession> m_networkSession;
};

} // namespace PCM

} // namespace WebKit
