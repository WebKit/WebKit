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

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include <WebCore/FidoConstants.h>
#include <wtf/RunLoop.h>
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

    m_connection->send(m_requestMessage->popNextPacket(), [weakThis = makeWeakPtr(*this)](HidConnection::DataSent sent) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->write(sent);
    });
}

void CtapHidDriver::Worker::write(HidConnection::DataSent sent)
{
    ASSERT(m_state == State::Write);
    if (sent != HidConnection::DataSent::Yes) {
        returnMessage(std::nullopt);
        return;
    }

    if (!m_requestMessage->numPackets()) {
        m_state = State::Read;
        m_connection->registerDataReceivedCallback([weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) mutable {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;
            weakThis->read(data);
        });
        return;
    }

    m_connection->send(m_requestMessage->popNextPacket(), [weakThis = makeWeakPtr(*this)](HidConnection::DataSent sent) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->write(sent);
    });
}

void CtapHidDriver::Worker::read(const Vector<uint8_t>& data)
{
    ASSERT(m_state == State::Read);
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
            returnMessage(std::nullopt);
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
        returnMessage(WTFMove(m_responseMessage));
        return;
    }
}

void CtapHidDriver::Worker::returnMessage(std::optional<fido::FidoHidMessage>&& message)
{
    m_state = State::Idle;
    m_connection->unregisterDataReceivedCallback();
    m_callback(WTFMove(message));
}

CtapHidDriver::CtapHidDriver(UniqueRef<HidConnection>&& connection)
    : m_worker(makeUniqueRef<Worker>(WTFMove(connection)))
{
}

void CtapHidDriver::transact(Vector<uint8_t>&& data, ResponseCallback&& callback)
{
    ASSERT(m_state == State::Idle);
    m_state = State::AllocateChannel;
    m_requestData = WTFMove(data);
    m_responseCallback = WTFMove(callback);

    // Allocate a channel.
    // FIXME(191533): Get a real nonce.
    auto initCommand = FidoHidMessage::create(kHidBroadcastChannel, FidoHidDeviceCommand::kInit, { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 });
    ASSERT(initCommand);
    m_worker->transact(WTFMove(*initCommand), [weakThis = makeWeakPtr(*this)](std::optional<FidoHidMessage>&& response) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueAfterChannelAllocated(WTFMove(response));
    });
}

void CtapHidDriver::continueAfterChannelAllocated(std::optional<FidoHidMessage>&& message)
{
    ASSERT(m_state == State::AllocateChannel);
    if (!message) {
        returnResponse({ });
        return;
    }
    m_state = State::Ready;
    ASSERT(message->channelId() == m_channelId);

    // FIXME(191534): Check Payload including nonce and everything
    auto payload = message->getMessagePayload();
    size_t index = 8;
    ASSERT(payload.size() == kHidInitResponseSize);
    m_channelId = static_cast<uint32_t>(payload[index++]) << 24;
    m_channelId |= static_cast<uint32_t>(payload[index++]) << 16;
    m_channelId |= static_cast<uint32_t>(payload[index++]) << 8;
    m_channelId |= static_cast<uint32_t>(payload[index]);
    auto cmd = FidoHidMessage::create(m_channelId, FidoHidDeviceCommand::kCbor, m_requestData);
    ASSERT(cmd);
    m_worker->transact(WTFMove(*cmd), [weakThis = makeWeakPtr(*this)](std::optional<FidoHidMessage>&& response) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueAfterResponseReceived(WTFMove(response));
    });
}

void CtapHidDriver::continueAfterResponseReceived(std::optional<fido::FidoHidMessage>&& message)
{
    ASSERT(m_state == State::Ready);
    ASSERT(!message || message->channelId() == m_channelId);
    returnResponse(message ? message->getMessagePayload() : Vector<uint8_t>());
}

void CtapHidDriver::returnResponse(Vector<uint8_t>&& response)
{
    // Reset state before calling the response callback to avoid being deleted.
    m_state = State::Idle;
    m_channelId = fido::kHidBroadcastChannel;
    m_requestData = { };
    m_responseCallback(WTFMove(response));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
