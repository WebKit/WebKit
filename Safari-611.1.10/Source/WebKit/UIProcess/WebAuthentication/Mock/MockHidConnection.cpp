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

#if ENABLE(WEB_AUTHN)

#include <WebCore/AuthenticatorGetInfoResponse.h>
#include <WebCore/CBORReader.h>
#include <WebCore/FidoConstants.h>
#include <WebCore/Pin.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <wtf/BlockPtr.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/RunLoop.h>
#include <wtf/text/Base64.h>

namespace WebKit {
using Mock = WebCore::MockWebAuthenticationConfiguration;
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
    setIsInitialized(true);
}

void MockHidConnection::terminate()
{
    setIsInitialized(false);
}

auto MockHidConnection::sendSync(const Vector<uint8_t>& data) -> DataSent
{
    ASSERT(isInitialized());
    if (m_configuration.hid->expectCancel) {
        auto message = FidoHidMessage::createFromSerializedData(data);
        ASSERT_UNUSED(message, message);
        ASSERT(message->cmd() == FidoHidDeviceCommand::kCancel);
        LOG_ERROR("Request cancelled.");
    }
    return DataSent::Yes;
}

void MockHidConnection::send(Vector<uint8_t>&& data, DataSentCallback&& callback)
{
    ASSERT(isInitialized());
    auto task = makeBlockPtr([weakThis = makeWeakPtr(*this), data = WTFMove(data), callback = WTFMove(callback)]() mutable {
        ASSERT(!RunLoop::isMain());
        RunLoop::main().dispatch([weakThis, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
            if (!weakThis) {
                callback(DataSent::No);
                return;
            }

            weakThis->assembleRequest(WTFMove(data));

            auto sent = DataSent::Yes;
            if (weakThis->stagesMatch() && weakThis->m_configuration.hid->error == Mock::HidError::DataNotSent)
                sent = DataSent::No;
            callback(sent);
        });
    });
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), task.get());
}

void MockHidConnection::registerDataReceivedCallbackInternal()
{
    if (stagesMatch() && m_configuration.hid->error == Mock::HidError::EmptyReport) {
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
        m_subStage = Mock::HidSubStage::Init;
        if (previousSubStage == Mock::HidSubStage::Msg)
            m_stage = Mock::HidStage::Request;
    }
    if (m_requestMessage->cmd() == FidoHidDeviceCommand::kCbor || m_requestMessage->cmd() == FidoHidDeviceCommand::kMsg)
        m_subStage = Mock::HidSubStage::Msg;

    if (m_stage == Mock::HidStage::Request && m_subStage == Mock::HidSubStage::Msg) {
        // Make sure we issue different msg cmd for CTAP and U2F.
        if (m_configuration.hid->canDowngrade && !m_configuration.hid->isU2f)
            m_configuration.hid->isU2f = m_requestMessage->cmd() == FidoHidDeviceCommand::kMsg;
        ASSERT(m_configuration.hid->isU2f ^ (m_requestMessage->cmd() != FidoHidDeviceCommand::kMsg));

        // Set options.
        if (m_requestMessage->cmd() == FidoHidDeviceCommand::kCbor) {
            m_requireResidentKey = false;
            m_requireUserVerification = false;

            auto payload = m_requestMessage->getMessagePayload();
            ASSERT(payload.size());
            auto cmd = static_cast<CtapRequestCommand>(payload[0]);
            payload.remove(0);
            auto requestMap = CBORReader::read(payload);
            ASSERT(requestMap || cmd == CtapRequestCommand::kAuthenticatorGetNextAssertion);

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
    }

    // Store nonce.
    if (m_subStage == Mock::HidSubStage::Init) {
        m_nonce = m_requestMessage->getMessagePayload();
        ASSERT(m_nonce.size() == kHidInitNonceLength);
    }

    m_currentChannel = m_requestMessage->channelId();
    m_requestMessage = WTF::nullopt;
    if (m_configuration.hid->fastDataArrival)
        feedReports();
}

void MockHidConnection::feedReports()
{
    using namespace MockHidConnectionInternal;

    if (m_subStage == Mock::HidSubStage::Init) {
        Vector<uint8_t> payload;
        payload.reserveInitialCapacity(kHidInitResponseSize);
        payload.appendVector(m_nonce);
        size_t writePosition = payload.size();
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongNonce)
            payload[0]--;
        payload.grow(kHidInitResponseSize);
        cryptographicallyRandomValues(payload.data() + writePosition, CtapChannelIdSize);
        auto channel = kHidBroadcastChannel;
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongChannelId)
            channel--;
        FidoHidInitPacket initPacket(channel, FidoHidDeviceCommand::kInit, WTFMove(payload), payload.size());
        receiveReport(initPacket.getSerializedData());
        shouldContinueFeedReports();
        return;
    }

    Optional<FidoHidMessage> message;
    if (m_stage == Mock::HidStage::Info && m_subStage == Mock::HidSubStage::Msg) {
        // FIXME(205839):
        Vector<uint8_t> infoData;
        if (m_configuration.hid->canDowngrade)
            infoData = encodeAsCBOR(AuthenticatorGetInfoResponse({ ProtocolVersion::kCtap, ProtocolVersion::kU2f }, Vector<uint8_t>(aaguidLength, 0u)));
        else if (m_configuration.hid->supportClientPin) {
            AuthenticatorGetInfoResponse infoResponse({ ProtocolVersion::kCtap }, Vector<uint8_t>(aaguidLength, 0u));
            infoResponse.setPinProtocols({ pin::kProtocolVersion });
            AuthenticatorSupportedOptions options;
            options.setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet);
            infoResponse.setOptions(WTFMove(options));
            infoData = encodeAsCBOR(infoResponse);
        } else
            infoData = encodeAsCBOR(AuthenticatorGetInfoResponse({ ProtocolVersion::kCtap }, Vector<uint8_t>(aaguidLength, 0u)));
        infoData.insert(0, static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)); // Prepend status code.
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongChannelId)
            message = FidoHidMessage::create(m_currentChannel - 1, FidoHidDeviceCommand::kCbor, infoData);
        else {
            if (!m_configuration.hid->isU2f)
                message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, infoData);
            else
                message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kError, { static_cast<uint8_t>(CtapDeviceResponseCode::kCtap1ErrInvalidCommand) });
        }
    }

    if (m_stage == Mock::HidStage::Request && m_subStage == Mock::HidSubStage::Msg) {
        if (m_configuration.hid->expectCancel)
            return;
        if (m_configuration.hid->keepAlive) {
            m_configuration.hid->keepAlive = false;
            FidoHidInitPacket initPacket(m_currentChannel, FidoHidDeviceCommand::kKeepAlive, { CtapKeepAliveStatusProcessing }, 1);
            receiveReport(initPacket.getSerializedData());
            continueFeedReports();
            return;
        }
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::UnsupportedOptions && (m_requireResidentKey || m_requireUserVerification))
            message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, { static_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrUnsupportedOption) });
        else {
            Vector<uint8_t> payload;
            ASSERT(!m_configuration.hid->payloadBase64.isEmpty());
            auto status = base64Decode(m_configuration.hid->payloadBase64[0], payload);
            m_configuration.hid->payloadBase64.remove(0);
            ASSERT_UNUSED(status, status);
            if (!m_configuration.hid->isU2f)
                message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, payload);
            else
                message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kMsg, payload);
        }
    }

    ASSERT(message);
    bool isFirst = true;
    while (message->numPackets()) {
        auto report = message->popNextPacket();
        if (!isFirst && stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongChannelId)
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
    m_configuration.hid->error = Mock::HidError::Success;
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

#endif // ENABLE(WEB_AUTHN)
