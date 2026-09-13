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
#include <WebCore/CBORWriter.h>
#include <WebCore/CryptoKey.h>
#include <WebCore/CryptoKeyEC.h>
#include <WebCore/FidoConstants.h>
#include <WebCore/Pin.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <wtf/BlockPtr.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/Ref.h>
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
    initializeHmacSecretKey();
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
    auto task = makeBlockPtr([weakThis = WeakPtr { *this }, data = WTF::move(data), callback = WTF::move(callback)]() mutable {
        ASSERT(!RunLoop::isMain());
        RunLoop::mainSingleton().dispatch([weakThis, data = WTF::move(data), callback = WTF::move(callback)]() mutable {
            RefPtr protectedThis = weakThis;
            if (!protectedThis) {
                callback(DataSent::No);
                return;
            }

            protectedThis->assembleRequest(WTF::move(data));

            auto sent = DataSent::Yes;
            if (protectedThis->stagesMatch() && protectedThis->m_configuration.hid->error == Mock::HidError::DataNotSent)
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

            if (cmd == CtapRequestCommand::kAuthenticatorClientPin && requestMap) {
                auto subcommandIt = requestMap->getMap().find(CBORValue(static_cast<int64_t>(pin::RequestKey::kSubcommand)));
                if (subcommandIt != requestMap->getMap().end() && subcommandIt->second.isUnsigned()
                    && subcommandIt->second.getUnsigned() == static_cast<uint64_t>(pin::Subcommand::kGetKeyAgreement)) {
                    m_keyAgreementCount++;
                    if (m_configuration.hid->expectKeyAgreement) {
                        if (m_keyAgreementCount > 1) {
                            RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Unexpected extra key-agreement request");
                            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: More than one key-agreement request.");
                        }
                        m_respondWithSynthesizedKeyAgreement = true;
                    } else if (m_configuration.hid->supportHmacSecret) {
                        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Unexpected key-agreement request");
                        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Unexpected key-agreement request.");
                    }
                }
            }

            if (cmd == CtapRequestCommand::kAuthenticatorMakeCredential || cmd == CtapRequestCommand::kAuthenticatorGetAssertion) {
                if (m_configuration.hid->expectKeyAgreement && !m_keyAgreementCount) {
                    RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: make/get arrived before key agreement");
                    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Missing key-agreement request.");
                }
                if (requestMap)
                    handleHmacSecretRequest(cmd, requestMap->getMap());
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
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::ShortInitResponse) {
            // Emit an INIT response whose payload matches the requested nonce but is too short
            // to contain the subsequent 4-byte channel ID, to exercise the bounds check in
            // CtapHidDriver::continueAfterChannelAllocated().
            FidoHidInitPacket shortPacket(kHidBroadcastChannel, FidoHidDeviceCommand::kInit, WTF::move(payload), kHidInitNonceLength);
            receiveReport(shortPacket.getSerializedData());
            shouldContinueFeedReports();
            return;
        }
        payload.grow(kHidInitResponseSize);
        cryptographicallyRandomValues(payload.mutableSpan().subspan(writePosition, kCtapChannelIdSize));
        auto channel = kHidBroadcastChannel;
        if (stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongChannelId)
            channel--;
        FidoHidInitPacket initPacket(channel, FidoHidDeviceCommand::kInit, WTF::move(payload), payload.size());
        receiveReport(initPacket.getSerializedData());
        shouldContinueFeedReports();
        return;
    }

    std::optional<FidoHidMessage> message;
    if (m_stage == Mock::HidStage::Info && m_subStage == Mock::HidSubStage::Msg) {
        // FIXME(205839):
        Vector<uint8_t> infoData;
        if (m_configuration.hid->canDowngrade)
            infoData = encodeAsCBOR(AuthenticatorGetInfoResponse({ ProtocolVersion::kCtap2, ProtocolVersion::kU2f }, Vector<uint8_t>(FillWith { }, aaguidLength, 0u)));
        else {
            AuthenticatorGetInfoResponse infoResponse({ ProtocolVersion::kCtap2 }, Vector<uint8_t>(FillWith { }, aaguidLength, 0u));
            AuthenticatorSupportedOptions options;
            if (m_configuration.hid->supportClientPin || m_configuration.hid->supportHmacSecret) {
                StdSet<PINUVAuthProtocol> protocols;
                if (!m_configuration.hid->pinProtocols.isEmpty()) {
                    for (auto protocol : m_configuration.hid->pinProtocols) {
                        if (protocol == static_cast<uint8_t>(PINUVAuthProtocol::kPinProtocol1))
                            protocols.insert(PINUVAuthProtocol::kPinProtocol1);
                        else if (protocol == static_cast<uint8_t>(PINUVAuthProtocol::kPinProtocol2))
                            protocols.insert(PINUVAuthProtocol::kPinProtocol2);
                    }
                } else
                    protocols.insert(PINUVAuthProtocol::kPinProtocol1);

                infoResponse.setPinProtocols(WTF::move(protocols));
                if (m_configuration.hid->supportClientPin)
                    options.setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet);
            }
            if (m_configuration.hid->supportInternalUV)
                options.setUserVerificationAvailability(AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured);
            if (m_configuration.hid->supportHmacSecret || m_configuration.hid->supportHmacSecretMc) {
                Vector<String> extensions;
                if (m_configuration.hid->supportHmacSecret)
                    extensions.append(kExtensionHmacSecret);
                if (m_configuration.hid->supportHmacSecretMc)
                    extensions.append(kExtensionHmacSecretMc);
                infoResponse.setExtensions(WTF::move(extensions));
            }
            infoResponse.setOptions(WTF::move(options));
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
        else if (m_respondWithSynthesizedKeyAgreement) {
            m_respondWithSynthesizedKeyAgreement = false;
            message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, synthesizedKeyAgreementResponse());
        } else {
            ASSERT(!m_configuration.hid->payloadBase64.isEmpty());
            auto payload = base64Decode(m_configuration.hid->payloadBase64[0]);
            m_configuration.hid->payloadBase64.removeAt(0);
            if (!m_configuration.hid->isU2f) {
                if (!m_configuration.hid->hmacSecretOutputBase64.isEmpty())
                    *payload = injectHmacSecretOutput(WTF::move(*payload));
                message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, WTF::move(*payload));
            } else
                message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kMsg, WTF::move(*payload));
        }
    }

    ASSERT(message);
    bool isFirst = true;
    while (message->numPackets()) {
        auto report = message->popNextPacket();
        if (!isFirst && stagesMatch() && m_configuration.hid->error == Mock::HidError::WrongChannelId)
            report = FidoHidContinuationPacket(m_currentChannel - 1, 0, { }).getSerializedData();
        // Packets are feed asynchronously to mimic actual data transmission.
        RunLoop::mainSingleton().dispatch([report = WTF::move(report), weakThis = WeakPtr { *this }]() mutable {
            if (RefPtr protectedThis = weakThis)
                protectedThis->receiveReport(WTF::move(report));
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
        if (RefPtr protectedThis = weakThis)
            protectedThis->feedReports();
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
            m_expectedCommands.append(WTF::move(*decodedMessage));
        else
            RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Failed to decode expected command: %s", expectedCommandBase64.utf8().legacyCStringPointer());
    }

    RELEASE_LOG(WebAuthn, "MockHidConnection: Initialized %zu expected commands for validation", m_expectedCommands.size());
}

void MockHidConnection::validateExpectedCommand(const Vector<uint8_t>& actualCommand)
{
    if (m_currentExpectedCommandIndex >= m_expectedCommands.size()) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: VALIDATION FAILED - Received unexpected command beyond expected count. Expected %zu commands, but received command %zu. Content: %s", m_expectedCommands.size(), m_currentExpectedCommandIndex + 1, base64EncodeToString(actualCommand).utf8().legacyCStringPointer());
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Unexpected command.");
    }

    const auto& expectedCommand = m_expectedCommands[m_currentExpectedCommandIndex];
    if (actualCommand != expectedCommand) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: VALIDATION FAILED - Command mismatch at index %zu. Expected %s Actual %s", m_currentExpectedCommandIndex, base64EncodeToString(expectedCommand).utf8().legacyCStringPointer(), base64EncodeToString(actualCommand).utf8().legacyCStringPointer());
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Command did not match expected value.");
    }

    m_currentExpectedCommandIndex++;
}

void MockHidConnection::validateExpectedCommandsCompleted()
{
    if (m_configuration.hid && m_configuration.hid->expectKeyAgreement && !m_keyAgreementCount) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Expected a key-agreement request that was never sent");
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Missing key-agreement request.");
    }

    if (!m_configuration.hid || !m_configuration.hid->validateExpectedCommands)
        return;
    if (m_currentExpectedCommandIndex >= m_expectedCommands.size())
        return;

    for (size_t i = m_currentExpectedCommandIndex; i < m_expectedCommands.size(); ++i)
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Missing expected command %zu: %s", i, base64EncodeToString(m_expectedCommands[i]).utf8().legacyCStringPointer());
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: validateAllExpectedCommandsConsumed called - %zu of %zu commands consumed", m_currentExpectedCommandIndex, m_expectedCommands.size());
}

void MockHidConnection::initializeHmacSecretKey()
{
    if (!m_configuration.hid || (!m_configuration.hid->expectKeyAgreement && m_configuration.hid->hmacSecretOutputBase64.isEmpty()))
        return;

    auto keyPairResult = CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, true, CryptoKeyUsageDeriveBits);
    if (keyPairResult.hasException()) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Failed to generate hmac-secret key pair");
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Failed to generate hmac-secret key pair.");
        return;
    }

    auto keyPair = keyPairResult.releaseReturnValue();
    m_authenticatorPrivateKey = Ref { downcast<CryptoKeyEC>(*keyPair.privateKey) };
    m_authenticatorPublicKey = Ref { downcast<CryptoKeyEC>(*keyPair.publicKey) };
}

void MockHidConnection::handleHmacSecretRequest(CtapRequestCommand cmd, const CBORValue::MapValue& requestMap)
{
    auto extensionsKey = cmd == CtapRequestCommand::kAuthenticatorMakeCredential ? kCtapMakeCredentialExtensionsKey : kCtapGetAssertionExtensionsKey;
    auto pinAuthKey = cmd == CtapRequestCommand::kAuthenticatorMakeCredential ? kCtapMakeCredentialPinUvAuthParamKey : kCtapGetAssertionPinUvAuthParamKey;
    auto hmacSecretName = cmd == CtapRequestCommand::kAuthenticatorMakeCredential ? kExtensionHmacSecretMc : kExtensionHmacSecret;

    if (m_configuration.hid->rejectPinUvAuth && requestMap.find(CBORValue(pinAuthKey)) != requestMap.end()) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Unexpected pinUvAuth parameter");
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Unexpected pinUvAuth parameter.");
    }

    const CBORValue::MapValue* hmacSecretMap = nullptr;
    auto extensionsIt = requestMap.find(CBORValue(extensionsKey));
    if (extensionsIt != requestMap.end() && extensionsIt->second.isMap()) {
        auto hmacIt = extensionsIt->second.getMap().find(CBORValue(hmacSecretName));
        if (hmacIt != extensionsIt->second.getMap().end() && hmacIt->second.isMap())
            hmacSecretMap = &hmacIt->second.getMap();
    }

    if (m_configuration.hid->requireHmacSecretExtension && !hmacSecretMap) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Missing hmac-secret parameters");
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Missing hmac-secret parameters.");
    }
    if (m_configuration.hid->rejectHmacSecretExtension && hmacSecretMap) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Unexpected hmac-secret parameters");
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Unexpected hmac-secret parameters.");
    }
    if (!hmacSecretMap)
        return;

    auto keyAgreementIt = hmacSecretMap->find(CBORValue(kHmacSecretKeyAgreementKey));
    auto saltEncIt = hmacSecretMap->find(CBORValue(kHmacSecretSaltEncKey));
    auto saltAuthIt = hmacSecretMap->find(CBORValue(kHmacSecretSaltAuthKey));
    if (keyAgreementIt == hmacSecretMap->end() || !keyAgreementIt->second.isMap()
        || saltEncIt == hmacSecretMap->end() || !saltEncIt->second.isByteString()
        || saltAuthIt == hmacSecretMap->end() || !saltAuthIt->second.isByteString()) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Malformed hmac-secret parameters");
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Malformed hmac-secret parameters.");
    }

    CBORValue::MapValue clonedPeerKey;
    for (const auto& entry : keyAgreementIt->second.getMap())
        clonedPeerKey[entry.first.clone()] = entry.second.clone();
    m_pendingHmacSecretPeerCoseKey = WTF::move(clonedPeerKey);
    auto protocolIt = hmacSecretMap->find(CBORValue(kHmacSecretPinUvAuthProtocolKey));
    if (protocolIt != hmacSecretMap->end() && protocolIt->second.isInteger())
        m_pendingHmacSecretProtocol = static_cast<PINUVAuthProtocol>(protocolIt->second.getInteger());
    else
        m_pendingHmacSecretProtocol = PINUVAuthProtocol::kPinProtocol1;
}

Vector<uint8_t> MockHidConnection::synthesizedKeyAgreementResponse() const
{
    if (m_configuration.hid->keyAgreementError)
        return { static_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrOther) };

    if (m_configuration.hid->malformedKeyAgreement)
        return { static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess), 0xA1, 0x01, 0x00 };

    ASSERT(m_authenticatorPublicKey);
    Ref authenticatorPublicKey = *m_authenticatorPublicKey;
    auto rawPublicKey = authenticatorPublicKey->exportRaw();
    ASSERT(!rawPublicKey.hasException());

    CBORValue::MapValue responseMap;
    responseMap[CBORValue(static_cast<int64_t>(pin::ResponseKey::kKeyAgreement))] = CBORValue(pin::encodeCOSEPublicKey(rawPublicKey.releaseReturnValue()));
    auto encoded = CBORWriter::write(CBORValue(WTF::move(responseMap)));
    ASSERT(encoded);

    Vector<uint8_t> payload;
    payload.reserveInitialCapacity(1 + encoded->size());
    payload.append(static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess));
    payload.appendVector(*encoded);
    return payload;
}

Vector<uint8_t> MockHidConnection::injectHmacSecretOutput(Vector<uint8_t>&& payload) const
{
    if (!m_pendingHmacSecretPeerCoseKey || !m_authenticatorPrivateKey || !m_pendingHmacSecretProtocol)
        return WTF::move(payload);

    auto plaintext = base64Decode(m_configuration.hid->hmacSecretOutputBase64);
    if (!plaintext)
        return WTF::move(payload);

    Ref authenticatorPrivateKey = *m_authenticatorPrivateKey;
    auto encrypted = pin::encryptHmacSecretOutput(*m_pendingHmacSecretProtocol, authenticatorPrivateKey, *m_pendingHmacSecretPeerCoseKey, *plaintext);
    if (!encrypted) {
        RELEASE_LOG_ERROR(WebAuthn, "MockHidConnection: Failed to encrypt hmac-secret output");
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("MockHidConnection: Failed to encrypt hmac-secret output.");
        return WTF::move(payload);
    }

    if (payload.size() <= 1)
        return WTF::move(payload);

    auto decoded = CBORReader::read(payload.subspan(1));
    if (!decoded || !decoded->isMap())
        return WTF::move(payload);

    // Both makeCredential and getAssertion store authenticator data at key 2.
    auto& responseMap = decoded->getMap();
    auto authDataIt = responseMap.find(CBORValue(kCtapGetAssertionResponseAuthDataKey));
    if (authDataIt == responseMap.end() || !authDataIt->second.isByteString())
        return WTF::move(payload);

    auto authData = authDataIt->second.getByteString();
    constexpr size_t minAuthDataLength = rpIdHashLength + flagsLength + signCounterLength;
    if (authData.size() < minAuthDataLength)
        return WTF::move(payload);

    bool attested = authData[rpIdHashLength] & WebAuthn::attestedCredentialDataIncludedFlag;
    bool hadExtensions = authData[rpIdHashLength] & WebAuthn::extensionDataIncludedFlag;
    authData[rpIdHashLength] |= WebAuthn::extensionDataIncludedFlag;

    size_t extensionsOffset = minAuthDataLength;
    if (attested) {
        extensionsOffset += aaguidLength;
        if (authData.size() < extensionsOffset + credentialIdLengthLength)
            return WTF::move(payload);
        size_t credentialIdLength = (static_cast<size_t>(authData[extensionsOffset]) << 8) | static_cast<size_t>(authData[extensionsOffset + 1]);
        extensionsOffset += credentialIdLengthLength + credentialIdLength;
        if (authData.size() <= extensionsOffset)
            return WTF::move(payload);
        auto publicKeyResult = CBORReader::readWithBytesConsumed(authData.subspan(extensionsOffset));
        if (!publicKeyResult)
            return WTF::move(payload);
        extensionsOffset += publicKeyResult->second;
    }

    CBORValue::MapValue extensionsMap;
    if (hadExtensions && authData.size() > extensionsOffset) {
        auto existing = CBORReader::read(authData.subspan(extensionsOffset));
        if (existing && existing->isMap()) {
            for (const auto& entry : existing->getMap())
                extensionsMap[entry.first.clone()] = entry.second.clone();
        }
    }

    if (attested) {
        extensionsMap[CBORValue(kExtensionHmacSecretMc)] = CBORValue(*encrypted);
        extensionsMap[CBORValue(kExtensionHmacSecret)] = CBORValue(true);
    } else
        extensionsMap[CBORValue(kExtensionHmacSecret)] = CBORValue(*encrypted);

    auto encodedExtensions = CBORWriter::write(CBORValue(WTF::move(extensionsMap)));
    if (!encodedExtensions)
        return WTF::move(payload);

    Vector<uint8_t> newAuthData(authData.subspan(0, extensionsOffset));
    newAuthData.appendVector(*encodedExtensions);

    CBORValue::MapValue newResponseMap;
    for (const auto& entry : responseMap) {
        if (entry.first.isInteger() && entry.first.getInteger() == kCtapGetAssertionResponseAuthDataKey)
            newResponseMap[entry.first.clone()] = CBORValue(WTF::move(newAuthData));
        else
            newResponseMap[entry.first.clone()] = entry.second.clone();
    }

    auto encodedResponse = CBORWriter::write(CBORValue(WTF::move(newResponseMap)));
    if (!encodedResponse)
        return WTF::move(payload);

    Vector<uint8_t> rewritten;
    rewritten.reserveInitialCapacity(1 + encodedResponse->size());
    rewritten.append(payload[0]);
    rewritten.appendVector(*encodedResponse);
    return rewritten;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
