/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include "CtapAuthenticator.h"

#if ENABLE(WEB_AUTHN)

#include "CtapDriver.h"
#include "CtapHidDriver.h"
#include "Logging.h"
#include "U2fAuthenticator.h"
#include <WebCore/AuthenticationExtensionsClientOutputs.h>
#include <WebCore/AuthenticatorAttachment.h>
#include <WebCore/CredentialPropertiesOutput.h>
#include <WebCore/CryptoKeyAES.h>
#include <WebCore/CryptoKeyHMAC.h>
#include <WebCore/DeviceRequestConverter.h>
#include <WebCore/DeviceResponseConverter.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/Pin.h>
#include <WebCore/U2fCommandConstructor.h>
#include <WebCore/WebAuthenticationUtils.h>
#include <wtf/EnumTraits.h>
#include <wtf/RunLoop.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

#define CTAP_RELEASE_LOG(fmt, ...) RELEASE_LOG(WebAuthn, "%p [aaguid=%s, transport=%s] - CtapAuthenticator::" fmt, this, aaguidForDebugging().utf8().data(), transportForDebugging().utf8().data(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;
using namespace fido;

using UVAvailability = AuthenticatorSupportedOptions::UserVerificationAvailability;

namespace {
WebAuthenticationStatus toStatus(const CtapDeviceResponseCode& error)
{
    switch (error) {
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinInvalid:
        return WebAuthenticationStatus::PinInvalid;
    case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        return WebAuthenticationStatus::PinAuthBlocked;
    case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        return WebAuthenticationStatus::PinBlocked;
    default:
        ASSERT_NOT_REACHED();
        return WebAuthenticationStatus::PinInvalid;
    }
}

bool isPinError(const CtapDeviceResponseCode& error)
{
    switch (error) {
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
    case CtapDeviceResponseCode::kCtap2ErrPinInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
    case CtapDeviceResponseCode::kCtap2ErrPinRequired:
        return true;
    default:
        return false;
    }
}

} // namespace

CtapAuthenticator::CtapAuthenticator(Ref<CtapDriver>&& driver, AuthenticatorGetInfoResponse&& info)
    : FidoAuthenticator(WTFMove(driver))
    , m_info(WTFMove(info))
{
}

void CtapAuthenticator::makeCredential()
{
    CTAP_RELEASE_LOG("makeCredential");
    ASSERT(!m_isDowngraded);
    Vector<uint8_t> cborCmd;
    auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    auto internalUVAvailability = m_info.options().userVerificationAvailability();
    auto residentKeyAvailability = m_info.options().residentKeyAvailability();
    Vector<String> authenticatorSupportedExtensions;
    if (m_isKeyStoreFull || (m_info.remainingDiscoverableCredentials() && !m_info.remainingDiscoverableCredentials())) {
        if (options.authenticatorSelection && (options.authenticatorSelection->requireResidentKey || options.authenticatorSelection->residentKey == ResidentKeyRequirement::Required)) {
            observer()->authenticatorStatusUpdated(WebAuthenticationStatus::KeyStoreFull);
            return;
        }
        residentKeyAvailability = AuthenticatorSupportedOptions::ResidentKeyAvailability::kNotSupported;
    }
    // If UV is required, then either built-in uv or a pin will work.
    if (internalUVAvailability == UVAvailability::kSupportedAndConfigured && (!options.authenticatorSelection || options.authenticatorSelection->userVerification != UserVerificationRequirement::Discouraged) && m_pinAuth.isEmpty())
        cborCmd = encodeMakeCredentialRequestAsCBOR(requestData().hash, options, internalUVAvailability, residentKeyAvailability, authenticatorSupportedExtensions);
    else if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet)
        cborCmd = encodeMakeCredentialRequestAsCBOR(requestData().hash, options, internalUVAvailability, residentKeyAvailability, authenticatorSupportedExtensions, PinParameters { pin::kProtocolVersion, m_pinAuth });
    else
        cborCmd = encodeMakeCredentialRequestAsCBOR(requestData().hash, options, internalUVAvailability, residentKeyAvailability, authenticatorSupportedExtensions);
    CTAP_RELEASE_LOG("makeCredential: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueMakeCredentialAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueMakeCredentialAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto error = getResponseCode(data);
    CTAP_RELEASE_LOG("continueMakeCredentialAfterResponseReceived: Got error code: %hhu from authenticator.", enumToUnderlyingType(error));
    auto response = readCTAPMakeCredentialResponse(data, AuthenticatorAttachment::CrossPlatform, transports(), std::get<PublicKeyCredentialCreationOptions>(requestData().options).attestation);
    if (!response) {
        CTAP_RELEASE_LOG("makeCredential: Failed to parse response %s", base64EncodeToString(data).utf8().data());

        if (error == CtapDeviceResponseCode::kCtap2ErrActionTimeout) {
            makeCredential();
            return;
        }

        if (error == CtapDeviceResponseCode::kCtap2ErrCredentialExcluded) {
            receiveRespond(ExceptionData { ExceptionCode::InvalidStateError, "At least one credential matches an entry of the excludeCredentials list in the authenticator."_s });
            return;
        }
        if (error == CtapDeviceResponseCode::kCtap2ErrKeyStoreFull) {
            auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
            if (options.authenticatorSelection->requireResidentKey || options.authenticatorSelection->residentKey == ResidentKeyRequirement::Required)
                observer()->authenticatorStatusUpdated(WebAuthenticationStatus::KeyStoreFull);
            else if (!m_isKeyStoreFull) {
                m_isKeyStoreFull = true;
                makeCredential();
            }
            return;
        }

        if (isPinError(error)) {
            if (!m_pinAuth.isEmpty() && observer()) // Skip the very first command that acts like wink.
                observer()->authenticatorStatusUpdated(toStatus(error));
            if (tryRestartPin(error))
                return;
        }

        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }
    auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    if (options.extensions && options.extensions->credProps) {
        auto extensionOutputs = response->extensions();
        
        auto rkSupported = m_info.options().residentKeyAvailability() == AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported;
        auto rkRequested = options.authenticatorSelection && ((options.authenticatorSelection->residentKey && options.authenticatorSelection->residentKey != ResidentKeyRequirement::Discouraged) || options.authenticatorSelection->requireResidentKey);
        extensionOutputs.credProps = CredentialPropertiesOutput { rkSupported && rkRequested && !m_isKeyStoreFull };
        response->setExtensions(WTFMove(extensionOutputs));
    }
    receiveRespond(response.releaseNonNull());
}

void CtapAuthenticator::getAssertion()
{
    ASSERT(!m_isDowngraded);
    Vector<uint8_t> cborCmd;
    auto& options = std::get<PublicKeyCredentialRequestOptions>(requestData().options);
    auto internalUVAvailability = m_info.options().userVerificationAvailability();
    Vector<String> authenticatorSupportedExtensions;
    // If UV is required, then either built-in uv or a pin will work.
    if (internalUVAvailability == UVAvailability::kSupportedAndConfigured && options.userVerification != UserVerificationRequirement::Discouraged && m_pinAuth.isEmpty())
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability, authenticatorSupportedExtensions);
    else if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet && options.userVerification != UserVerificationRequirement::Discouraged)
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability, authenticatorSupportedExtensions, PinParameters { pin::kProtocolVersion, m_pinAuth });
    else
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability, authenticatorSupportedExtensions);
    CTAP_RELEASE_LOG("getAssertion: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueGetAssertionAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto response = readCTAPGetAssertionResponse(data, AuthenticatorAttachment::CrossPlatform);
    auto error = getResponseCode(data);
    CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: errorcode: %hhu", enumToUnderlyingType(error));
    if (!response) {
        CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: Failed to parse response %s", base64EncodeToString(data).utf8().data());
        if (error == CtapDeviceResponseCode::kCtap2ErrActionTimeout) {
            getAssertion();
            return;
        }

        if (!isPinError(error) && tryDowngrade())
            return;

        if (isPinError(error)) {
            if (!m_pinAuth.isEmpty() && observer()) // Skip the very first command that acts like wink.
                observer()->authenticatorStatusUpdated(toStatus(error));
            if (tryRestartPin(error))
                return;
        }

        if (error == CtapDeviceResponseCode::kCtap2ErrNoCredentials && observer())
            observer()->authenticatorStatusUpdated(WebAuthenticationStatus::NoCredentialsFound);

        CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: No credentials found.");
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }
    CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: Get %lu credentials back.", response->numberOfCredentials());

    if (response->numberOfCredentials() <= 1) {
        receiveRespond(response.releaseNonNull());
        return;
    }

    m_remainingAssertionResponses = response->numberOfCredentials() - 1;
    m_assertionResponses.reserveInitialCapacity(response->numberOfCredentials());
    m_assertionResponses.append(response.releaseNonNull());
    auto cborCmd = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion);
    CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetNextAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueGetNextAssertionAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto error = getResponseCode(data);
    CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived: errorcode: %hhu", enumToUnderlyingType(error));
    auto response = readCTAPGetAssertionResponse(data, AuthenticatorAttachment::CrossPlatform);
    if (!response) {
        CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived: Unable to parse response: %s", base64EncodeToString(data).utf8().data());
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }
    m_remainingAssertionResponses--;
    m_assertionResponses.append(response.releaseNonNull());
    CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived: Remaining responses: %lu", m_remainingAssertionResponses);

    if (!m_remainingAssertionResponses) {
        if (RefPtr observer = this->observer()) {
            observer->selectAssertionResponse(Vector { m_assertionResponses }, WebAuthenticationSource::External, [this, weakThis = WeakPtr { *this }] (AuthenticatorAssertionResponse* response) {
                RELEASE_ASSERT(RunLoop::isMain());
                if (!weakThis)
                    return;
                auto result = m_assertionResponses.findIf([expectedResponse = response] (auto& response) {
                    return response.ptr() == expectedResponse;
                });
                if (result == notFound)
                    return;
                receiveRespond(m_assertionResponses[result].copyRef());
            });
        }
        return;
    }

    auto cborCmd = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion);
    CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetNextAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::getRetries()
{
    auto cborCmd = encodeAsCBOR(pin::RetriesRequest { });
    CTAP_RELEASE_LOG("getRetries: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }, this](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        CTAP_RELEASE_LOG("getRetries: Response %s", base64EncodeToString(data).utf8().data());
        weakThis->continueGetKeyAgreementAfterGetRetries(WTFMove(data));
    });
}

void CtapAuthenticator::continueGetKeyAgreementAfterGetRetries(Vector<uint8_t>&& data)
{
    CTAP_RELEASE_LOG("continueGetKeyAgreementAfterGetRetries");
    auto retries = pin::RetriesResponse::parse(data);
    if (!retries) {
        auto error = getResponseCode(data);
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }

    auto cborCmd = encodeAsCBOR(pin::KeyAgreementRequest { });
    CTAP_RELEASE_LOG("continueGetKeyAgreementAfterGetRetries: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }, this, retries = retries->retries] (Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        CTAP_RELEASE_LOG("continueGetKeyAgreementAfterGetRetries: Response %s", base64EncodeToString(data).utf8().data());
        weakThis->continueRequestPinAfterGetKeyAgreement(WTFMove(data), retries);
    });
}

void CtapAuthenticator::continueRequestPinAfterGetKeyAgreement(Vector<uint8_t>&& data, uint64_t retries)
{
    CTAP_RELEASE_LOG("continueRequestPinAfterGetKeyAgreement");
    auto keyAgreement = pin::KeyAgreementResponse::parse(data);
    if (!keyAgreement) {
        auto error = getResponseCode(data);
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }

    if (RefPtr observer = this->observer()) {
        CTAP_RELEASE_LOG("continueRequestPinAfterGetKeyAgreement: Requesting pin from observer.");
        observer->requestPin(retries, [weakThis = WeakPtr { *this }, this, keyAgreement = WTFMove(*keyAgreement)] (const String& pin) {
            RELEASE_ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;
            CTAP_RELEASE_LOG("continueRequestPinAfterGetKeyAgreement: Got pin from observer.");
            weakThis->continueGetPinTokenAfterRequestPin(pin, keyAgreement.peerKey);
        });
    }
}

void CtapAuthenticator::continueGetPinTokenAfterRequestPin(const String& pin, const CryptoKeyEC& peerKey)
{
    CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived");
    if (pin.isNull()) {
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, "Pin is null."_s });
        return;
    }

    auto pinUTF8 = pin::validateAndConvertToUTF8(pin);
    if (!pinUTF8) {
        // Fake a pin invalid response from the authenticator such that clients could show some error to the user.
        if (RefPtr observer = this->observer())
            observer->authenticatorStatusUpdated(WebAuthenticationStatus::PinInvalid);
        tryRestartPin(CtapDeviceResponseCode::kCtap2ErrPinInvalid);
        return;
    }
    auto tokenRequest = pin::TokenRequest::tryCreate(*pinUTF8, peerKey);
    if (!tokenRequest) {
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, "Cannot create a TokenRequest."_s });
        return;
    }

    auto cborCmd = encodeAsCBOR(*tokenRequest);
    CTAP_RELEASE_LOG("continueGetPinTokenAfterRequestPin: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }, this, tokenRequest = WTFMove(*tokenRequest)] (Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        CTAP_RELEASE_LOG("continueGetPinTokenAfterRequestPin: Response %s", base64EncodeToString(data).utf8().data());
        weakThis->continueRequestAfterGetPinToken(WTFMove(data), tokenRequest);
    });
}

void CtapAuthenticator::continueRequestAfterGetPinToken(Vector<uint8_t>&& data, const fido::pin::TokenRequest& tokenRequest)
{
    CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived");
    auto token = pin::TokenResponse::parse(tokenRequest.sharedKey(), data);
    if (!token) {
        auto error = getResponseCode(data);

        if (isPinError(error)) {
            if (RefPtr observer = this->observer())
                observer->authenticatorStatusUpdated(toStatus(error));
            if (tryRestartPin(error))
                return;
        }

        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }

    m_pinAuth = token->pinAuth(requestData().hash);
    WTF::switchOn(requestData().options, [&](const PublicKeyCredentialCreationOptions& options) {
        makeCredential();
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        getAssertion();
    });
}

bool CtapAuthenticator::tryRestartPin(const CtapDeviceResponseCode& error)
{
    CTAP_RELEASE_LOG("tryRestartPin: Error code: %hhu", enumToUnderlyingType(error));
    switch (error) {
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinRequired:
        getRetries();
        return true;
    default:
        return false;
    }
}

bool CtapAuthenticator::tryDowngrade()
{
    CTAP_RELEASE_LOG("tryDowngrade");
    if (m_info.versions().find(ProtocolVersion::kU2f) == m_info.versions().end())
        return false;
    if (!observer())
        return false;

    bool isConvertible = false;
    WTF::switchOn(requestData().options, [&](const PublicKeyCredentialCreationOptions& options) {
        isConvertible = isConvertibleToU2fRegisterCommand(options);
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        isConvertible = isConvertibleToU2fSignCommand(options);
    });
    if (!isConvertible)
        return false;

    CTAP_RELEASE_LOG("tryDowngrade: Downgrading to U2F.");
    m_isDowngraded = true;
    driver().setProtocol(ProtocolVersion::kU2f);
    observer()->downgrade(this, U2fAuthenticator::create(releaseDriver()));
    return true;
}

Vector<AuthenticatorTransport> CtapAuthenticator::transports() const
{
    
    if (auto& infoTransports = m_info.transports())
        return *infoTransports;
    return Vector { driver().transport() };
}

String CtapAuthenticator::aaguidForDebugging() const
{
    return WTF::UUID { std::span<const uint8_t, 16> { m_info.aaguid() } }.toString();
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
