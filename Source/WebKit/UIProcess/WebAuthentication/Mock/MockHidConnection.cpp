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
#include "MockHidConnection.h"

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include <WebCore/AuthenticatorGetInfoResponse.h>
#include <WebCore/CBORReader.h>
#include <WebCore/FidoConstants.h>
#include <wtf/BlockPtr.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/RunLoop.h>
#include <wtf/text/Base64.h>

namespace WebKit {
using Mock = MockWebAuthenticationConfiguration::Hid;
using namespace WebCore;
using namespace cbor;
using namespace fido;

namespace MockHidConnectionInternal {
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#mandatory-commands
const size_t CtapChannelIdSize = 4;
const uint8_t CtapKeepAliveStatusProcessing = 1;
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#commands
const int64_t CtapMakeCredentialRequestOptionsKey = 7;
const int64_t CtapGetAssertionRequestOptionsKey = 5;
}

MockHidConnection::MockHidConnection(IOHIDDeviceRef device, const MockWebAuthenticationConfiguration& configuration)
    : HidConnection(device)
    , m_configuration(configuration)
{
}

void MockHidConnection::initialize()
{
    m_initialized = true;
}

void MockHidConnection::terminate()
{
    m_terminated = true;
}

void MockHidConnection::send(Vector<uint8_t>&& data, DataSentCallback&& callback)
{
    ASSERT(m_initialized);
    auto task = BlockPtr<void()>::fromCallable([weakThis = makeWeakPtr(*this), data = WTFMove(data), callback = WTFMove(callback)]() mutable {
        ASSERT(!RunLoop::isMain());
        RunLoop::main().dispatch([weakThis, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
            if (!weakThis)
                return;

            weakThis->assembleRequest(WTFMove(data));

            auto sent = DataSent::Yes;
            if (weakThis->stagesMatch() && weakThis->m_configuration.hid->error == Mock::Error::DataNotSent)
                sent = DataSent::No;
            callback(sent);
        });
    });
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), task.get());
}

void MockHidConnection::registerDataReceivedCallbackInternal()
{
    if (stagesMatch() && m_configuration.hid->error == Mock::Error::EmptyReport) {
        receiveReport({ });
        shouldContinueFeedReports();
        return;
    }
    if (!m_configuration.hid->fastDataArrival)
        feedReports();
}

void MockHidConnection::assembleRequest(Vector<uint8_t>&& data)
{
    if (!m_requestMessage) {
        m_requestMessage = FidoHidMessage::createFromSerializedData(data);
        ASSERT(m_requestMessage);
    } else {
        auto status = m_requestMessage->addContinuationPacket(data);
        ASSERT_UNUSED(status, status);
    }

    if (m_requestMessage->messageComplete())
        parseRequest();
}

void MockHidConnection::parseRequest()
{
    using namespace MockHidConnectionInternal;

    ASSERT(m_requestMessage);
    // Set stages.
    if (m_requestMessage->cmd() == FidoHidDeviceCommand::kInit) {
        auto previousSubStage = m_subStage;
        m_subStage = Mock::SubStage::Init;
        if (previousSubStage == Mock::SubStage::Msg)
            m_stage = Mock::Stage::Request;
    }
    if (m_requestMessage->cmd() == FidoHidDeviceCommand::kCbor)
        m_subStage = Mock::SubStage::Msg;

    // Set options.
    if (m_stage == Mock::Stage::Request && m_subStage == Mock::SubStage::Msg) {
        m_requireResidentKey = false;
        m_requireUserVerification = false;

        auto payload = m_requestMessage->getMessagePayload();
        ASSERT(payload.size());
        auto cmd = static_cast<CtapRequestCommand>(payload[0]);
        payload.remove(0);
        auto requestMap = CBORReader::read(payload);
        ASSERT(requestMap);

        if (cmd == CtapRequestCommand::kAuthenticatorMakeCredential) {
            auto it = requestMap->getMap().find(CBORValue(CtapMakeCredentialRequestOptionsKey)); // Find options.
            if (it != requestMap->getMap().end()) {
                auto& optionMap = it->second.getMap();

                auto itr = optionMap.find(CBORValue(kResidentKeyMapKey));
                if (itr != optionMap.end())
                    m_requireResidentKey = itr->second.getBool();

                itr = optionMap.find(CBORValue(kUserVerificationMapKey));
                if (itr != optionMap.end())
                    m_requireUserVerification = itr->second.getBool();
            }
        }

        if (cmd == CtapRequestCommand::kAuthenticatorGetAssertion) {
            auto it = requestMap->getMap().find(CBORValue(CtapGetAssertionRequestOptionsKey)); // Find options.
            if (it != requestMap->getMap().end()) {
                auto& optionMap = it->second.getMap();
                auto itr = optionMap.find(CBORValue(kUserVerificationMapKey));
                if (itr != optionMap.end())
                    m_requireUserVerification = itr->second.getBool();
            }
        }
    }

    // Store nonce.
    if (m_subStage == Mock::SubStage::Init) {
        m_nonce = m_requestMessage->getMessagePayload();
        ASSERT(m_nonce.size() == kHidInitNonceLength);
    }

    m_currentChannel = m_requestMessage->channelId();
    m_requestMessage = std::nullopt;
    if (m_configuration.hid->fastDataArrival)
        feedReports();
}

void MockHidConnection::feedReports()
{
    using namespace MockHidConnectionInternal;

    if (m_subStage == Mock::SubStage::Init) {
        Vector<uint8_t> payload;
        payload.reserveInitialCapacity(kHidInitResponseSize);
        payload.appendVector(m_nonce);
        if (stagesMatch() && m_configuration.hid->error == Mock::Error::WrongNonce)
            payload[0]--;
        payload.grow(kHidInitResponseSize);
        cryptographicallyRandomValues(payload.data() + payload.size(), CtapChannelIdSize);
        auto channel = kHidBroadcastChannel;
        if (stagesMatch() && m_configuration.hid->error == Mock::Error::WrongChannelId)
            channel--;
        FidoHidInitPacket initPacket(channel, FidoHidDeviceCommand::kInit, WTFMove(payload), payload.size());
        receiveReport(initPacket.getSerializedData());
        shouldContinueFeedReports();
        return;
    }

    std::optional<FidoHidMessage> message;
    if (m_stage == Mock::Stage::Info && m_subStage == Mock::SubStage::Msg) {
        auto infoData = encodeAsCBOR(AuthenticatorGetInfoResponse({ ProtocolVersion::kCtap }, Vector<uint8_t>(kAaguidLength, 0u)));
        infoData.insert(0, static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)); // Prepend status code.
        if (stagesMatch() && m_configuration.hid->error == Mock::Error::WrongChannelId)
            message = FidoHidMessage::create(m_currentChannel - 1, FidoHidDeviceCommand::kCbor, infoData);
        else
            message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, infoData);
    }

    if (m_stage == Mock::Stage::Request && m_subStage == Mock::SubStage::Msg) {
        if (m_configuration.hid->keepAlive) {
            m_configuration.hid->keepAlive = false;
            FidoHidInitPacket initPacket(m_currentChannel, FidoHidDeviceCommand::kKeepAlive, { CtapKeepAliveStatusProcessing }, 1);
            receiveReport(initPacket.getSerializedData());
            continueFeedReports();
            return;
        }
        if (stagesMatch() && m_configuration.hid->error == Mock::Error::UnsupportedOptions && (m_requireResidentKey || m_requireUserVerification))
            message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, { static_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrUnsupportedOption) });
        else {
            Vector<uint8_t> payload;
            auto status = base64Decode(m_configuration.hid->payloadBase64, payload);
            ASSERT_UNUSED(status, status);
            message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, payload);
        }
    }

    ASSERT(message);
    bool isFirst = true;
    while (message->numPackets()) {
        auto report = message->popNextPacket();
        if (!isFirst && stagesMatch() && m_configuration.hid->error == Mock::Error::WrongChannelId)
            report = FidoHidContinuationPacket(m_currentChannel - 1, 0, { }).getSerializedData();
        // Packets are feed asynchronously to mimic actual data transmission.
        RunLoop::main().dispatch([report = WTFMove(report), weakThis = makeWeakPtr(*this)]() mutable {
            if (!weakThis)
                return;
            weakThis->receiveReport(WTFMove(report));
        });
        isFirst = false;
    }
}

bool MockHidConnection::stagesMatch() const
{
    return m_configuration.hid->stage == m_stage && m_configuration.hid->subStage == m_subStage;
}

void MockHidConnection::shouldContinueFeedReports()
{
    if (!m_configuration.hid->continueAfterErrorData)
        return;
    m_configuration.hid->continueAfterErrorData = false;
    m_configuration.hid->error = Mock::Error::Success;
    continueFeedReports();
}

void MockHidConnection::continueFeedReports()
{
    // Send actual response for the next run.
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this)]() mutable {
        if (!weakThis)
            return;
        weakThis->feedReports();
    });
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
