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

#include "config.h"
#include "CtapHidDriver.h"

#if ENABLE(WEB_AUTHN)

#include <WebCore/FidoConstants.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WeakRandomNumber.h>
#include <wtf/text/Base64.h>

namespace WebKit {
using namespace fido;

CtapHidDriver::Worker::Worker(UniqueRef<HidConnection>&& connection)
    : m_connection(WTFMove(connection))
{
    m_connection->initialize();
}

CtapHidDriver::Worker::~Worker()
{
    m_connection->terminate();
}

void CtapHidDriver::Worker::transact(fido::FidoHidMessage&& requestMessage, MessageCallback&& callback)
{
    ASSERT(m_state == State::Idle);
    m_state = State::Write;
    m_requestMessage = WTFMove(requestMessage);
    m_responseMessage.reset();
    m_callback = WTFMove(callback);

    // HidConnection could hold data from other applications, and thereofore invalidate it before each transaction.
    m_connection->invalidateCache();
    m_connection->send(m_requestMessage->popNextPacket(), [weakThis = WeakPtr { *this }](HidConnection::DataSent sent) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->write(sent);
    });
}

void CtapHidDriver::Worker::write(HidConnection::DataSent sent)
{
    if (m_state != State::Write)
        return;
    if (sent != HidConnection::DataSent::Yes) {
        m_responseMessage = std::nullopt;
        returnMessage();
        return;
    }

    if (!m_requestMessage->numPackets()) {
        m_state = State::Read;
        m_connection->registerDataReceivedCallback([weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) mutable {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;
            weakThis->read(data);
        });
        return;
    }

    m_connection->send(m_requestMessage->popNextPacket(), [weakThis = WeakPtr { *this }](HidConnection::DataSent sent) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->write(sent);
    });
}

void CtapHidDriver::Worker::read(const Vector<uint8_t>& data)
{
    if (m_state != State::Read)
        return;
    if (!m_responseMessage) {
        m_responseMessage = FidoHidMessage::createFromSerializedData(data);
        // The first few reports could be for other applications, and therefore ignore those.
        if (!m_responseMessage || m_responseMessage->channelId() != m_requestMessage->channelId()) {
            LOG_ERROR("Couldn't parse a hid init packet: %s", m_responseMessage ? "wrong channel id." : "bad data.");
            m_responseMessage.reset();
            return;
        }
    } else {
        if (!m_responseMessage->addContinuationPacket(data)) {
            LOG_ERROR("Couldn't parse a hid continuation packet.");
            m_responseMessage = std::nullopt;
            returnMessage();
            return;
        }
    }

    if (m_responseMessage->messageComplete()) {
        // A KeepAlive cmd could be sent between a request and a response to indicate that
        // the authenticator is waiting for user consent. Keep listening for the response.
        if (m_responseMessage->cmd() == FidoHidDeviceCommand::kKeepAlive) {
            m_responseMessage.reset();
            return;
        }
        returnMessage();
        return;
    }
}

void CtapHidDriver::Worker::returnMessage()
{
    // Reset state before calling the response callback to avoid being deleted.
    auto callback = WTFMove(m_callback);
    auto message = WTFMove(m_responseMessage);
    reset();
    callback(WTFMove(message));
}

void CtapHidDriver::Worker::reset()
{
    m_connection->unregisterDataReceivedCallback();
    m_callback = nullptr;
    m_responseMessage = std::nullopt;
    m_requestMessage = std::nullopt;
    m_state = State::Idle;
}

// This implements CTAPHID_CANCEL which violates the transaction semantics:
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#usb-hid-cancel
void CtapHidDriver::Worker::cancel(fido::FidoHidMessage&& requestMessage)
{
    reset();
    m_connection->invalidateCache();
    ASSERT(requestMessage.numPackets() == 1);
    m_connection->sendSync(requestMessage.popNextPacket());
}

CtapHidDriver::CtapHidDriver(UniqueRef<HidConnection>&& connection)
    : CtapDriver(WebCore::AuthenticatorTransport::Usb)
    , m_worker(makeUniqueRef<Worker>(WTFMove(connection)))
    , m_nonce(kHidInitNonceLength)
{
}

void CtapHidDriver::transact(Vector<uint8_t>&& data, ResponseCallback&& callback)
{
    ASSERT(m_state == State::Idle);
    m_state = State::AllocateChannel;
    m_channelId = kHidBroadcastChannel;
    m_requestData = WTFMove(data);
    m_responseCallback = WTFMove(callback);

    // Allocate a channel.
    // Use a pseudo random nonce instead of a cryptographically strong one as the nonce
    // is mainly for identifications.
    size_t steps = kHidInitNonceLength / sizeof(uint32_t);
    ASSERT(!(kHidInitNonceLength % sizeof(uint32_t)) && steps >= 1);
    for (size_t i = 0; i < steps; ++i) {
        uint32_t weakRandom = weakRandomNumber<uint32_t>();
        memcpy(m_nonce.data() + i * sizeof(uint32_t), &weakRandom, sizeof(uint32_t));
    }

    auto initCommand = FidoHidMessage::create(m_channelId, FidoHidDeviceCommand::kInit, m_nonce);
    ASSERT(initCommand);
    m_worker->transact(WTFMove(*initCommand), [weakThis = WeakPtr { *this }](std::optional<FidoHidMessage>&& response) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueAfterChannelAllocated(WTFMove(response));
    });
}

void CtapHidDriver::continueAfterChannelAllocated(std::optional<FidoHidMessage>&& message)
{
    if (m_state != State::AllocateChannel)
        return;
    if (!message) {
        returnResponse({ });
        return;
    }
    ASSERT(message->channelId() == m_channelId);

    auto payload = message->getMessagePayload();
    ASSERT(payload.size() == kHidInitResponseSize);
    // Restart the transaction in the next run loop when nonce mismatches.
    if (memcmp(payload.data(), m_nonce.data(), m_nonce.size())) {
        m_state = State::Idle;
        RunLoop::main().dispatch([weakThis = WeakPtr { *this }, data = WTFMove(m_requestData), callback = WTFMove(m_responseCallback)]() mutable {
            if (!weakThis)
                return;
            weakThis->transact(WTFMove(data), WTFMove(callback));
        });
        return;
    }

    m_state = State::Ready;
    auto index = kHidInitNonceLength;
    m_channelId = static_cast<uint32_t>(payload[index++]) << 24;
    m_channelId |= static_cast<uint32_t>(payload[index++]) << 16;
    m_channelId |= static_cast<uint32_t>(payload[index++]) << 8;
    m_channelId |= static_cast<uint32_t>(payload[index]);
    // FIXME(191534): Check the rest of the payload.
    auto cmd = FidoHidMessage::create(m_channelId, protocol() == ProtocolVersion::kCtap ? FidoHidDeviceCommand::kCbor : FidoHidDeviceCommand::kMsg, m_requestData);
    ASSERT(cmd);
    m_worker->transact(WTFMove(*cmd), [weakThis = WeakPtr { *this }](std::optional<FidoHidMessage>&& response) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueAfterResponseReceived(WTFMove(response));
    });
}

void CtapHidDriver::continueAfterResponseReceived(std::optional<fido::FidoHidMessage>&& message)
{
    if (m_state != State::Ready)
        return;
    ASSERT(!message || message->channelId() == m_channelId);
    returnResponse(message ? message->getMessagePayload() : Vector<uint8_t>());
}

void CtapHidDriver::returnResponse(Vector<uint8_t>&& response)
{
    // Reset state before calling the response callback to avoid being deleted.
    auto responseCallback = WTFMove(m_responseCallback);
    reset();
    responseCallback(WTFMove(response));
}

void CtapHidDriver::reset()
{
    m_responseCallback = nullptr;
    m_channelId = fido::kHidBroadcastChannel;
    m_state = State::Idle;
}

void CtapHidDriver::cancel()
{
    if (m_state == State::Idle || protocol() != ProtocolVersion::kCtap)
        return;
    // Cancel any outstanding requests.
    if (m_state == State::Ready) {
        auto cancelCommand = FidoHidMessage::create(m_channelId, FidoHidDeviceCommand::kCancel, { });
        m_worker->cancel(WTFMove(*cancelCommand));
    }
    reset();
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
