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

#include "Logging.h"
#include <WebCore/AuthenticatorGetInfoResponse.h>
#include <WebCore/CBORReader.h>
#include <WebCore/FidoConstants.h>
#include <WebCore/Pin.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <wtf/BlockPtr.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/RunLoop.h>
#include <wtf/darwin/DispatchExtras.h>
#include <wtf/text/Base64.h>

namespace WebKit {
using Mock = WebCore::MockWebAuthenticationConfiguration;
using namespace WebCore;
using namespace cbor;
using namespace fido;

Ref<MockHidConnection> MockHidConnection::create(IOHIDDeviceRef device, const WebCore::MockWebAuthenticationConfiguration& configuration)
{
    return adoptRef(*new MockHidConnection(device, configuration));
}

MockHidConnection::MockHidConnection(IOHIDDeviceRef device, const MockWebAuthenticationConfiguration& configuration)
    : HidConnection(device)
    , m_configuration(configuration)
{
    initializeExpectedCommands();
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
    auto task = makeBlockPtr([weakThis = WeakPtr { *this }, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
        ASSERT(!RunLoop::isMain());
        RunLoop::mainSingleton().dispatch([weakThis, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
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
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), task.get());
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
        if (m_configuration.hid && m_configuration.hid->validateExpectedCommands)
            validateExpectedCommand(m_requestMessage->getMessagePayload());

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
            payload.removeAt(0);
            auto requestMap = CBORReader::read(payload);
            ASSERT(requestMap || cmd == CtapRequestCommand::kAuthenticatorGetNextAssertion);

            if (cmd == CtapRequestCommand::kAuthenticatorMakeCredential) {
                auto it = requestMap->getMap().find(CBORValue(kCtapMakeCredentialRequestOptionsKey)); // Find options.
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
                auto it = requestMap->getMap().find(CBORValue(kCtapGetAssertionRequestOptionsKey)); // Find options.
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
    m_requestMessage = std::nullopt;
    if (m_configuration.hid->fastDataArrival)
        feedReports();
}

void MockHidConnection::feedReports()
{
    if (m_subStage == Mock::HidSubStage::Init) {
        Vector<uint8_t> payload;
        payload.reserveInitialCapacity(kHidInitResponseSize);
        payload.appendVector(m_nonce);
        size_t writePosition = payload.size();
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongNonce)
            payload[0]--;
        payload.grow(kHidInitResponseSize);
        cryptographicallyRandomValues(payload.mutableSpan().subspan(writePosition, kCtapChannelIdSize));
        auto channel = kHidBroadcastChannel;
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongChannelId)
            channel--;
        FidoHidInitPacket initPacket(channel, FidoHidDeviceCommand::kInit, WTFMove(payload), payload.size());
        receiveReport(initPacket.getSerializedData());
        shouldContinueFeedReports();
        return;
    }

    std::optional<FidoHidMessage> message;
    if (m_stage == Mock::HidStage::Info && m_subStage == Mock::HidSubStage::Msg) {
        // FIXME(205839):
        Vector<uint8_t> infoData;
        if (m_configuration.hid->canDowngrade)
            infoData = encodeAsCBOR(AuthenticatorGetInfoResponse({ ProtocolVersion::kCtap2, ProtocolVersion::kU2f }, Vector<uint8_t>(aaguidLength, 0u)));
        else {
            AuthenticatorGetInfoResponse infoResponse({ ProtocolVersion::kCtap2 }, Vector<uint8_t>(aaguidLength, 0u));
            AuthenticatorSupportedOptions options;
            if (m_configuration.hid->supportClientPin) {
                infoResponse.setPinProtocols({ pin::kProtocolVersion });
                options.setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet);
            }
            if (m_configuration.hid->supportInternalUV)
                options.setUserVerificationAvailability(AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured);
            infoResponse.setOptions(WTFMove(options));
            infoResponse.setMaxCredentialCountInList(m_configuration.hid->maxCredentialCountInList);
            infoResponse.setMaxCredentialIDLength(m_configuration.hid->maxCredentialIdLength);
            infoData = encodeAsCBOR(infoResponse);
        }
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
            FidoHidInitPacket initPacket(m_currentChannel, FidoHidDeviceCommand::kKeepAlive, { kCtapKeepAliveStatusProcessing }, 1);
            receiveReport(initPacket.getSerializedData());
            continueFeedReports();
            return;
        }
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::UnsupportedOptions && (m_requireResidentKey || m_requireUserVerification))
            message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, { static_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrUnsupportedOption) });
        else {
            ASSERT(!m_configuration.hid->payloadBase64.isEmpty());
            auto payload = base64Decode(m_configuration.hid->payloadBase64[0]);
            m_configuration.hid->payloadBase64.removeAt(0);
            if (!m_configuration.hid->isU2f)
                message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, WTFMove(*payload));
            else
                message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kMsg, WTFMove(*payload));
        }
    }

    ASSERT(message);
    bool isFirst = true;
    while (message->numPackets()) {
        auto report = message->popNextPacket();
        if (!isFirst && stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongChannelId)
            report = FidoHidContinuationPacket(m_currentChannel - 1, 0, { }).getSerializedData();
        // Packets are feed asynchronously to mimic actual data transmission.
        RunLoop::mainSingleton().dispatch([report = WTFMove(report), weakThis = WeakPtr { *this }]() mutable {
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
    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }]() mutable {
        if (!weakThis)
            return;
        weakThis->feedReports();
    });
}

void MockHidConnection::initializeExpectedCommands()
{
    if (!m_configuration.hid || !m_configuration.hid->validateExpectedCommands)
        return;

    m_expectedCommands.clear();
    m_currentExpectedCommandIndex = 0;

    for (const auto& expectedCommandBase64 : m_configuration.hid->expectedCommandsBase64) {
        auto decodedMessage = base64Decode(expectedCommandBase64);
        if (decodedMessage)
            m_expectedCommands.append(WTFMove(*decodedMessage));
        else
            RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Failed to decode expected command: %s", expectedCommandBase64.utf8().data());
    }

    RELEASE_LOG(WebAuthn, "MockHidConnection: Initialized %zu expected commands for validation", m_expectedCommands.size());
}

void MockHidConnection::validateExpectedCommand(const Vector<uint8_t>& actualCommand)
{
    if (m_currentExpectedCommandIndex >= m_expectedCommands.size()) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: VALIDATION FAILED - Received unexpected command beyond expected count. Expected %zu commands, but received command %zu. Content: %s", m_expectedCommands.size(), m_currentExpectedCommandIndex + 1, base64EncodeToString(actualCommand).utf8().data());
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Unexpected command.");
    }

    const auto& expectedCommand = m_expectedCommands[m_currentExpectedCommandIndex];
    if (actualCommand != expectedCommand) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: VALIDATION FAILED - Command mismatch at index %zu. Expected %s Actual %s", m_currentExpectedCommandIndex, base64EncodeToString(expectedCommand).utf8().data(), base64EncodeToString(actualCommand).utf8().data());
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Command did not match expected value.");
    }

    m_currentExpectedCommandIndex++;
}

void MockHidConnection::validateExpectedCommandsCompleted()
{
    if (!m_configuration.hid || !m_configuration.hid->validateExpectedCommands)
        return;
    if (m_currentExpectedCommandIndex >= m_expectedCommands.size())
        return;

    for (size_t i = m_currentExpectedCommandIndex; i < m_expectedCommands.size(); ++i)
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Missing expected command %zu: %s", i, base64EncodeToString(m_expectedCommands[i]).utf8().data());
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: validateAllExpectedCommandsConsumed called - %zu of %zu commands consumed", m_currentExpectedCommandIndex, m_expectedCommands.size());
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
