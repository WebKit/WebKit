/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "VirtualHidConnection.h"

#if ENABLE(WEB_AUTHN)

#include "VirtualAuthenticatorManager.h"
#include "VirtualAuthenticatorUtils.h"
#include <WebCore/CBORReader.h>
#include <WebCore/CBORWriter.h>
#include <WebCore/FidoConstants.h>
#include <WebCore/WebAuthenticationUtils.h>
#include <wtf/BlockPtr.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace cbor;
using namespace fido;
using namespace WebCore;

const size_t CtapChannelIdSize = 4;

Ref<VirtualHidConnection> VirtualHidConnection::create(const String& authenticatorId, const VirtualAuthenticatorConfiguration& configuration, const WeakPtr<VirtualAuthenticatorManager>& manager)
{
    return adoptRef(*new VirtualHidConnection(authenticatorId, configuration, manager));
}

VirtualHidConnection::VirtualHidConnection(const String& authenticatorId, const VirtualAuthenticatorConfiguration& configuration, const WeakPtr<VirtualAuthenticatorManager>& manager)
    : HidConnection(nullptr)
    , m_manager(manager)
    , m_configuration(configuration)
    , m_authenticatorId(authenticatorId)
{
}

void VirtualHidConnection::initialize()
{
    setIsInitialized(true);
}

void VirtualHidConnection::terminate()
{
    setIsInitialized(false);
}

auto VirtualHidConnection::sendSync(const Vector<uint8_t>& data) -> DataSent
{
    ASSERT(isInitialized());
    return DataSent::Yes;
}

void VirtualHidConnection::send(Vector<uint8_t>&& data, DataSentCallback&& callback)
{
    ASSERT(isInitialized());
    auto task = makeBlockPtr([weakThis = WeakPtr { *this }, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
        ASSERT(!RunLoop::isMain());
        RunLoop::main().dispatch([weakThis, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
            if (!weakThis) {
                callback(DataSent::No);
                return;
            }

            weakThis->assembleRequest(WTFMove(data));

            callback(DataSent::Yes);
        });
    });
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), task.get());
}

void VirtualHidConnection::assembleRequest(Vector<uint8_t>&& data)
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

void VirtualHidConnection::receiveHidMessage(fido::FidoHidMessage&& message)
{
    while (message.numPackets()) {
        auto report = message.popNextPacket();
        RunLoop::main().dispatch([report = WTFMove(report), weakThis = WeakPtr { *this }]() mutable {
            if (!weakThis)
                return;
            weakThis->receiveReport(WTFMove(report));
        });
    }
}

void VirtualHidConnection::recieveResponseCode(fido::CtapDeviceResponseCode code)
{
    Vector<uint8_t> buffer;
    buffer.append(static_cast<uint8_t>(code));

    auto message = FidoHidMessage::create(m_currentChannel, FidoHidDeviceCommand::kCbor, WTFMove(buffer));
    receiveHidMessage(WTFMove(*message));
}

void VirtualHidConnection::parseRequest()
{
    if (!m_requestMessage)
        return;
    RefPtr manager = m_manager.get();
    if (!manager)
        return;
    m_currentChannel = m_requestMessage->channelId();
    switch (m_requestMessage->cmd()) {
    case FidoHidDeviceCommand::kInit: {
        m_nonce = m_requestMessage->getMessagePayload();
        ASSERT(m_nonce.size() == kHidInitNonceLength);
        Vector<uint8_t> payload;
        payload.reserveInitialCapacity(kHidInitResponseSize);
        payload.appendVector(m_nonce);
        size_t writePosition = payload.size();
        payload.grow(kHidInitResponseSize);
        cryptographicallyRandomValues(payload.mutableSpan().subspan(writePosition, CtapChannelIdSize));
        auto channel = kHidBroadcastChannel;
        FidoHidInitPacket initPacket(channel, FidoHidDeviceCommand::kInit, WTFMove(payload), payload.size());
        receiveReport(initPacket.getSerializedData());
        break;
    }
    case FidoHidDeviceCommand::kCbor: {
        auto payload = m_requestMessage->getMessagePayload();
        ASSERT(payload.size());
        auto cmd = static_cast<CtapRequestCommand>(payload[0]);
        payload.remove(0);
        auto requestMap = CBORReader::read(payload);
        ASSERT(requestMap || cmd == CtapRequestCommand::kAuthenticatorGetNextAssertion);
        if (cmd == CtapRequestCommand::kAuthenticatorMakeCredential) {
            VirtualCredential credential;
            auto it = requestMap->getMap().find(CBORValue(kCtapMakeCredentialRequestOptionsKey)); // Find options.
            if (it != requestMap->getMap().end()) {
                auto& optionMap = it->second.getMap();

                auto itr = optionMap.find(CBORValue(kResidentKeyMapKey));
                if (itr != optionMap.end()) {
                    auto requireResidentKey = itr->second.getBool();
                    // https://fidoalliance.org/specs/fido-v2.1-rd-20210309/fido-client-to-authenticator-protocol-v2.1-rd-20210309.html#op-makecred-step-options
                    if (requireResidentKey && !m_configuration.hasResidentKey) {
                        recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrUnsupportedOption);
                        return;
                    }
                    if (requireResidentKey)
                        credential.isResidentCredential = true;
                }

                itr = optionMap.find(CBORValue(kUserVerificationMapKey));
                if (itr != optionMap.end()) {
                    auto requireUserVerification = itr->second.getBool();
                    if (requireUserVerification && !m_configuration.hasUserVerification) {
                        recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrInvalidOption);
                        return;
                    }
                    if (requireUserVerification && !m_configuration.isUserVerified) {
                        recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrOperationDenied);
                        return;
                    }
                    credential.isVerificationRequired = requireUserVerification;
                }
            }
            it = requestMap->getMap().find(CBORValue(kCtapMakeCredentialRpKey));
            if (it == requestMap->getMap().end()) {
                recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrMissingParameter);
                return;
            }
            auto& rpEntity = it->second.getMap();
            it = rpEntity.find(CBORValue(kEntityIdMapKey));
            if (it == rpEntity.end()) {
                recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrMissingParameter);
                return;
            }
            credential.rpId = it->second.getString();
            auto privateKey = createPrivateKey();
            credential.privateKey = base64PrivateKey(privateKey);

            it = requestMap->getMap().find(CBORValue(kCtapMakeCredentialUserKey));
            if (it == requestMap->getMap().end()) {
                recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrMissingParameter);
                return;
            }
            auto& userEntity = it->second.getMap();
            it = userEntity.find(CBORValue(kEntityIdMapKey));
            if (it == userEntity.end()) {
                recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrMissingParameter);
                return;
            }
            credential.userHandle = it->second.getByteString();
            auto credentialIdAndCosePubKey = credentialIdAndCosePubKeyForPrivateKey(privateKey);
            credential.credentialId = credentialIdAndCosePubKey.first;
            manager->addCredential(m_authenticatorId, credential);

            auto attestedCredentialData = buildAttestedCredentialData(Vector<uint8_t>(aaguidLength, 0), credentialIdAndCosePubKey.first, credentialIdAndCosePubKey.second);

            auto authenticatorData = buildAuthData(credential.rpId, flagsForConfig(m_configuration), credential.signCount, attestedCredentialData);
            CBORValue::MapValue response;
            response[CBORValue(1)] = CBORValue("none");
            response[CBORValue(2)] = CBORValue(authenticatorData);
            auto attObj = buildAttestationMap(WTFMove(authenticatorData), String { emptyString() }, { }, AttestationConveyancePreference::None);
            response[CBORValue(3)] = CBORValue(attObj);
            auto payload = CBORWriter::write(CBORValue(response));
            Vector<uint8_t> buffer;
            buffer.reserveCapacity(payload->size() + 1);
            buffer.append(static_cast<uint8_t>(fido::CtapDeviceResponseCode::kSuccess));
            buffer.appendVector(*payload);

            auto message = FidoHidMessage::create(m_requestMessage->channelId(), FidoHidDeviceCommand::kCbor, WTFMove(buffer));
            receiveHidMessage(WTFMove(*message));
        } else if (cmd == CtapRequestCommand::kAuthenticatorGetAssertion) {
            auto it = requestMap->getMap().find(CBORValue(kCtapGetAssertionRpIdKey));
            if (it == requestMap->getMap().end()) {
                recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrMissingParameter);
                return;
            }
            auto rpId = it->second.getString();
            (void)rpId;
            it = requestMap->getMap().find(CBORValue(kCtapGetAssertionClientDataHashKey));
            if (it == requestMap->getMap().end()) {
                recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrMissingParameter);
                return;
            }
            auto clientDataHash = it->second.getByteString();
            (void)clientDataHash;
            Vector<Vector<uint8_t>> allowList;
            it = requestMap->getMap().find(CBORValue(kCtapGetAssertionAllowListKey));
            if (it != requestMap->getMap().end()) {
                const auto& cborAllowList = it->second.getArray();
                if (cborAllowList.isEmpty()) {
                    recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrInvalidOption);
                    return;
                }

                for (const auto& cborCredential : cborAllowList) {
                    auto& credMap = cborCredential.getMap();
                    auto itr = credMap.find(CBORValue(fido::kEntityIdMapKey));
                    if (itr == credMap.end() || !itr->second.isByteString()) {
                        recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrInvalidOption);
                        return;
                    }
                    allowList.append(itr->second.getByteString());
                }
            }

            it = requestMap->getMap().find(CBORValue(kCtapGetAssertionRequestOptionsKey));
            if (it != requestMap->getMap().end()) {
                auto& optionMap = it->second.getMap();

                auto itr = optionMap.find(CBORValue(kUserVerificationMapKey));
                if (itr != optionMap.end()) {
                    auto requireUserVerification = itr->second.getBool();
                    if (requireUserVerification && !m_configuration.hasUserVerification) {
                        recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrInvalidOption);
                        return;
                    }
                    if (requireUserVerification && !m_configuration.isUserVerified) {
                        recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrOperationDenied);
                        return;
                    }
                }

                itr = optionMap.find(CBORValue(kUserPresenceMapKey));
                if (itr != optionMap.end()) {
                    auto requireUserPresence = itr->second.getBool();
                    if (requireUserPresence && !m_configuration.isUserConsenting) {
                        recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrOperationDenied);
                        return;
                    }
                }
            }
            auto matchingCredentials = manager->credentialsMatchingList(m_authenticatorId, rpId, allowList);
            if (matchingCredentials.isEmpty()) {
                recieveResponseCode(CtapDeviceResponseCode::kCtap2ErrNoCredentials);
                return;
            }
            auto& credential = matchingCredentials[0];
            CBORValue::MapValue response;

            response[CBORValue(1)] = CBORValue(buildCredentialDescriptor(credential.credentialId));
            auto key = privateKeyFromBase64(credential.privateKey);
            auto credentialIdAndCosePubKey = credentialIdAndCosePubKeyForPrivateKey(key);
            auto attestedCredentialData = buildAttestedCredentialData(Vector<uint8_t>(aaguidLength, 0), credentialIdAndCosePubKey.first, credentialIdAndCosePubKey.second);
            auto authData = buildAuthData(rpId, flagsForConfig(m_configuration), credential.signCount, attestedCredentialData);
            response[CBORValue(2)] = CBORValue(authData);
            response[CBORValue(3)] = CBORValue(signatureForPrivateKey(key, authData, clientDataHash));
            if (credential.userHandle)
                response[CBORValue(4)] = CBORValue(buildUserEntityMap(*credential.userHandle, emptyString(), emptyString()));
            response[CBORValue(5)] = CBORValue((int64_t)matchingCredentials.size());
            auto payload = CBORWriter::write(CBORValue(response));
            Vector<uint8_t> buffer;
            buffer.reserveCapacity(payload->size() + 1);
            buffer.append(static_cast<uint8_t>(fido::CtapDeviceResponseCode::kSuccess));
            buffer.appendVector(*payload);

            auto message = FidoHidMessage::create(m_requestMessage->channelId(), FidoHidDeviceCommand::kCbor, WTFMove(buffer));
            receiveHidMessage(WTFMove(*message));
        }
        break;
    }
    default:
        break;
    }
    m_requestMessage = std::nullopt;
}


} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
