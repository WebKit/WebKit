/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include "HidConnection.h"
#include <WebCore/FidoConstants.h>
#include <WebCore/FidoHidMessage.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

// The following implements the CTAP HID protocol:
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#usb
// FSM: Idle => AllocateChannel => Ready
class CtapHidDriver : public CanMakeWeakPtr<CtapHidDriver> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CtapHidDriver);
public:
    using ResponseCallback = Function<void(Vector<uint8_t>&&)>;

    enum class State : uint8_t {
        Idle,
        AllocateChannel,
        Ready,
        // FIXME(191528)
        Busy
    };

    explicit CtapHidDriver(UniqueRef<HidConnection>&&);

    void transact(Vector<uint8_t>&& data, ResponseCallback&&);

private:
    // Worker is the helper that maintains the transaction.
    // https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#arbitration
    // FSM: Idle => Write => Read.
    class Worker : public CanMakeWeakPtr<Worker> {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(Worker);
    public:
        using MessageCallback = Function<void(Optional<fido::FidoHidMessage>&&)>;

        enum class State : uint8_t  {
            Idle,
            Write,
            Read
        };

        explicit Worker(UniqueRef<HidConnection>&&);
        ~Worker();

        void transact(fido::FidoHidMessage&&, MessageCallback&&);

    private:
        void write(HidConnection::DataSent);
        void read(const Vector<uint8_t>&);
        void returnMessage(Optional<fido::FidoHidMessage>&&);

        UniqueRef<HidConnection> m_connection;
        State m_state { State::Idle };
        Optional<fido::FidoHidMessage> m_requestMessage;
        Optional<fido::FidoHidMessage> m_responseMessage;
        MessageCallback m_callback;
    };

    void continueAfterChannelAllocated(Optional<fido::FidoHidMessage>&&);
    void continueAfterResponseReceived(Optional<fido::FidoHidMessage>&&);
    void returnResponse(Vector<uint8_t>&&);

    UniqueRef<Worker> m_worker;
    State m_state { State::Idle };
    uint32_t m_channelId { fido::kHidBroadcastChannel };
    // One request at a time.
    Vector<uint8_t> m_requestData;
    ResponseCallback m_responseCallback;
    Vector<uint8_t> m_nonce;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
