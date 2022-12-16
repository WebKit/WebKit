/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "U2fAuthenticator.h"
#include <WebCore/AuthenticationExtensionsClientOutputs.h>
#include <WebCore/AuthenticatorAttachment.h>
#include <WebCore/CryptoKeyAES.h>
#include <WebCore/CryptoKeyEC.h>
#include <WebCore/CryptoKeyHMAC.h>
#include <WebCore/DeviceRequestConverter.h>
#include <WebCore/DeviceResponseConverter.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/Pin.h>
#include <WebCore/U2fCommandConstructor.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringConcatenateNumbers.h>

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

CtapAuthenticator::CtapAuthenticator(std::unique_ptr<CtapDriver>&& driver, AuthenticatorGetInfoResponse&& info)
    : FidoAuthenticator(WTFMove(driver))
    , m_info(WTFMove(info))
{
}

void CtapAuthenticator::makeCredential()
{
    ASSERT(!m_isDowngraded);
    if (processGoogleLegacyAppIdSupportExtension())
        return;
    Vector<uint8_t> cborCmd;
    auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    auto internalUVAvailability = m_info.options().userVerificationAvailability();
    auto residentKeyAvailability = m_info.options().residentKeyAvailability();
    if (m_isKeyStoreFull || (m_info.remainingDiscoverableCredentials() && !m_info.remainingDiscoverableCredentials())) {
        if (options.authenticatorSelection && (options.authenticatorSelection->requireResidentKey || options.authenticatorSelection->residentKey == ResidentKeyRequirement::Required)) {
            observer()->authenticatorStatusUpdated(WebAuthenticationStatus::KeyStoreFull);
            return;
        }
        residentKeyAvailability = AuthenticatorSupportedOptions::ResidentKeyAvailability::kNotSupported;
    }
    // If UV is required, then either built-in uv or a pin will work.
    if (internalUVAvailability == UVAvailability::kSupportedAndConfigured && (!options.authenticatorSelection || options.authenticatorSelection->userVerification != UserVerificationRequirement::Discouraged) && m_pinAuth.isEmpty())
        cborCmd = encodeMakeCredenitalRequestAsCBOR(requestData().hash, options, internalUVAvailability, residentKeyAvailability);
    else if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet)
        cborCmd = encodeMakeCredenitalRequestAsCBOR(requestData().hash, options, internalUVAvailability, residentKeyAvailability, PinParameters { pin::kProtocolVersion, m_pinAuth });
    else
        cborCmd = encodeMakeCredenitalRequestAsCBOR(requestData().hash, options, internalUVAvailability, residentKeyAvailability);
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueMakeCredentialAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueMakeCredentialAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto response = readCTAPMakeCredentialResponse(data, AuthenticatorAttachment::CrossPlatform, transports(), std::get<PublicKeyCredentialCreationOptions>(requestData().options).attestation);
    if (!response) {
        auto error = getResponseCode(data);

        if (error == CtapDeviceResponseCode::kCtap2ErrActionTimeout) {
            makeCredential();
            return;
        }

        if (error == CtapDeviceResponseCode::kCtap2ErrCredentialExcluded) {
            receiveRespond(ExceptionData { InvalidStateError, "At least one credential matches an entry of the excludeCredentials list in the authenticator."_s });
            return;
        }
        if (error == CtapDeviceResponseCode::kCtap2ErrKeyStoreFull) {
            auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
            if (options.authenticatorSelection->requireResidentKey || options.authenticatorSelection->residentKey == ResidentKeyRequirement::Required) {
                observer()->authenticatorStatusUpdated(WebAuthenticationStatus::KeyStoreFull);
            } else if (!m_isKeyStoreFull) {
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

        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }
    auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    if (options.extensions && options.extensions->credProps) {
        auto extensionOutputs = response->extensions();
        
        auto rkSupported = m_info.options().residentKeyAvailability() == AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported;
        auto rkRequested = options.authenticatorSelection && ((options.authenticatorSelection->residentKey && options.authenticatorSelection->residentKey != ResidentKeyRequirement::Discouraged) || options.authenticatorSelection->requireResidentKey);
        extensionOutputs.credProps = AuthenticationExtensionsClientOutputs::CredentialPropertiesOutput { rkSupported && rkRequested && !m_isKeyStoreFull };
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
    // If UV is required, then either built-in uv or a pin will work.
    if (internalUVAvailability == UVAvailability::kSupportedAndConfigured && options.userVerification != UserVerificationRequirement::Discouraged && m_pinAuth.isEmpty())
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability);
    else if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet && options.userVerification != UserVerificationRequirement::Discouraged)
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability, PinParameters { pin::kProtocolVersion, m_pinAuth });
    else
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability);
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
    if (!response) {
        auto error = getResponseCode(data);

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

        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }

    if (response->numberOfCredentials() <= 1) {
        receiveRespond(response.releaseNonNull());
        return;
    }

    m_remainingAssertionResponses = response->numberOfCredentials() - 1;
    m_assertionResponses.reserveInitialCapacity(response->numberOfCredentials());
    m_assertionResponses.uncheckedAppend(response.releaseNonNull());
    driver().transact(encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetNextAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueGetNextAssertionAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto response = readCTAPGetAssertionResponse(data, AuthenticatorAttachment::CrossPlatform);
    if (!response) {
        auto error = getResponseCode(data);
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }
    m_remainingAssertionResponses--;
    m_assertionResponses.uncheckedAppend(response.releaseNonNull());

    if (!m_remainingAssertionResponses) {
        if (auto* observer = this->observer()) {
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

    driver().transact(encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetNextAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::getRetries()
{
    auto cborCmd = encodeAsCBOR(pin::RetriesRequest { });
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetKeyAgreementAfterGetRetries(WTFMove(data));
    });
}

void CtapAuthenticator::continueGetKeyAgreementAfterGetRetries(Vector<uint8_t>&& data)
{
    auto retries = pin::RetriesResponse::parse(data);
    if (!retries) {
        auto error = getResponseCode(data);
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }

    auto cborCmd = encodeAsCBOR(pin::KeyAgreementRequest { });
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }, retries = retries->retries] (Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueRequestPinAfterGetKeyAgreement(WTFMove(data), retries);
    });
}

void CtapAuthenticator::continueRequestPinAfterGetKeyAgreement(Vector<uint8_t>&& data, uint64_t retries)
{
    auto keyAgreement = pin::KeyAgreementResponse::parse(data);
    if (!keyAgreement) {
        auto error = getResponseCode(data);
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }

    if (auto* observer = this->observer()) {
        observer->requestPin(retries, [weakThis = WeakPtr { *this }, keyAgreement = WTFMove(*keyAgreement)] (const String& pin) {
            RELEASE_ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;
            weakThis->continueGetPinTokenAfterRequestPin(pin, keyAgreement.peerKey);
        });
    }
}

void CtapAuthenticator::continueGetPinTokenAfterRequestPin(const String& pin, const CryptoKeyEC& peerKey)
{
    if (pin.isNull()) {
        receiveRespond(ExceptionData { UnknownError, "Pin is null."_s });
        return;
    }

    auto pinUTF8 = pin::validateAndConvertToUTF8(pin);
    if (!pinUTF8) {
        // Fake a pin invalid response from the authenticator such that clients could show some error to the user.
        if (auto* observer = this->observer())
            observer->authenticatorStatusUpdated(WebAuthenticationStatus::PinInvalid);
        tryRestartPin(CtapDeviceResponseCode::kCtap2ErrPinInvalid);
        return;
    }
    auto tokenRequest = pin::TokenRequest::tryCreate(*pinUTF8, peerKey);
    if (!tokenRequest) {
        receiveRespond(ExceptionData { UnknownError, "Cannot create a TokenRequest."_s });
        return;
    }

    auto cborCmd = encodeAsCBOR(*tokenRequest);
    driver().transact(WTFMove(cborCmd), [weakThis = WeakPtr { *this }, tokenRequest = WTFMove(*tokenRequest)] (Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueRequestAfterGetPinToken(WTFMove(data), tokenRequest);
    });
}

void CtapAuthenticator::continueRequestAfterGetPinToken(Vector<uint8_t>&& data, const fido::pin::TokenRequest& tokenRequest)
{
    auto token = pin::TokenResponse::parse(tokenRequest.sharedKey(), data);
    if (!token) {
        auto error = getResponseCode(data);

        if (isPinError(error)) {
            if (auto* observer = this->observer())
                observer->authenticatorStatusUpdated(toStatus(error));
            if (tryRestartPin(error))
                return;
        }

        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
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

    m_isDowngraded = true;
    driver().setProtocol(ProtocolVersion::kU2f);
    observer()->downgrade(this, U2fAuthenticator::create(releaseDriver()));
    return true;
}

// Only U2F protocol is supported for Google legacy AppID support.
bool CtapAuthenticator::processGoogleLegacyAppIdSupportExtension()
{
    auto& extensions = std::get<PublicKeyCredentialCreationOptions>(requestData().options).extensions;
    if (!extensions) {
        // AuthenticatorCoordinator::create should always set it.
        ASSERT_NOT_REACHED();
        return false;
    }
    if (extensions->googleLegacyAppidSupport)
        tryDowngrade();
    return extensions->googleLegacyAppidSupport;
}

Vector<AuthenticatorTransport> CtapAuthenticator::transports() const
{
    
    if (auto& infoTransports = m_info.transports())
        return *infoTransports;
    return Vector { driver().transport() };
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
