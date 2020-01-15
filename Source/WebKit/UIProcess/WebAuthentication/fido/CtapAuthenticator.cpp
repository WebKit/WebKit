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
#include "CtapAuthenticator.h"

#if ENABLE(WEB_AUTHN)

#include "CtapDriver.h"
#include "CtapHidDriver.h"
#include "U2fAuthenticator.h"
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
    if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet)
        cborCmd = encodeMakeCredenitalRequestAsCBOR(requestData().hash, WTF::get<PublicKeyCredentialCreationOptions>(requestData().options), m_info.options().userVerificationAvailability(), PinParameters { pin::kProtocolVersion, m_pinAuth });
    else
        cborCmd = encodeMakeCredenitalRequestAsCBOR(requestData().hash, WTF::get<PublicKeyCredentialCreationOptions>(requestData().options), m_info.options().userVerificationAvailability());
    driver().transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueMakeCredentialAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueMakeCredentialAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto response = readCTAPMakeCredentialResponse(data, WTF::get<PublicKeyCredentialCreationOptions>(requestData().options).attestation);
    if (!response) {
        auto error = getResponseCode(data);
        if (error == CtapDeviceResponseCode::kCtap2ErrCredentialExcluded)
            receiveRespond(ExceptionData { InvalidStateError, "At least one credential matches an entry of the excludeCredentials list in the authenticator."_s });
        // FIXME(205837)
        else if (error == CtapDeviceResponseCode::kCtap2ErrPinInvalid || error == CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid)
            getRetries();
        else
            receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }
    receiveRespond(response.releaseNonNull());
}

void CtapAuthenticator::getAssertion()
{
    ASSERT(!m_isDowngraded);
    Vector<uint8_t> cborCmd;
    if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet)
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, WTF::get<PublicKeyCredentialRequestOptions>(requestData().options), m_info.options().userVerificationAvailability(), PinParameters { pin::kProtocolVersion, m_pinAuth });
    else
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, WTF::get<PublicKeyCredentialRequestOptions>(requestData().options), m_info.options().userVerificationAvailability());
    driver().transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueGetAssertionAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto response = readCTAPGetAssertionResponse(data);
    if (!response) {
        auto error = getResponseCode(data);
        // FIXME(205837)
        if (error == CtapDeviceResponseCode::kCtap2ErrPinInvalid || error == CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid) {
            getRetries();
            return;
        }
        if (error != CtapDeviceResponseCode::kCtap2ErrInvalidCBOR && tryDowngrade())
            return;
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
    auto addResult = m_assertionResponses.add(response.releaseNonNull());
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    driver().transact(encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetNextAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueGetNextAssertionAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto response = readCTAPGetAssertionResponse(data);
    if (!response) {
        auto error = getResponseCode(data);
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }
    m_remainingAssertionResponses--;
    auto addResult = m_assertionResponses.add(response.releaseNonNull());
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    if (!m_remainingAssertionResponses) {
        if (auto* observer = this->observer()) {
            observer->selectAssertionResponse(m_assertionResponses, [this, weakThis = makeWeakPtr(*this)] (const AuthenticatorAssertionResponse& response) {
                ASSERT(RunLoop::isMain());
                if (!weakThis)
                    return;
                auto returnResponse = m_assertionResponses.take(const_cast<AuthenticatorAssertionResponse*>(&response));
                if (!returnResponse)
                    return;
                receiveRespond(WTFMove(*returnResponse));
            });
        }
        return;
    }

    driver().transact(encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetNextAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::getRetries()
{
    auto cborCmd = encodeAsCBOR(pin::RetriesRequest { });
    driver().transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
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
    driver().transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this), retries = retries->retries] (Vector<uint8_t>&& data) {
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
        observer->requestPin(retries, [weakThis = makeWeakPtr(*this), keyAgreement = WTFMove(*keyAgreement)] (const String& pin) {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;
            weakThis->continueGetPinTokenAfterRequestPin(pin, keyAgreement.peerKey);
        });
    }
}

void CtapAuthenticator::continueGetPinTokenAfterRequestPin(const String& pin, const CryptoKeyEC& peerKey)
{
    auto pinUtf8 = pin::validateAndConvertToUTF8(pin);
    if (!pinUtf8) {
        receiveRespond(ExceptionData { UnknownError, makeString("Pin is not valid: ", pin) });
        return;
    }
    auto tokenRequest = pin::TokenRequest::tryCreate(*pinUtf8, peerKey);
    if (!tokenRequest) {
        receiveRespond(ExceptionData { UnknownError, "Cannot create a TokenRequest."_s });
        return;
    }

    auto cborCmd = encodeAsCBOR(*tokenRequest);
    driver().transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this), tokenRequest = WTFMove(*tokenRequest)] (Vector<uint8_t>&& data) {
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
    // AuthenticatorCoordinator::create should always set it.
    auto& extensions = WTF::get<PublicKeyCredentialCreationOptions>(requestData().options).extensions;
    ASSERT(!!extensions);
    if (extensions->googleLegacyAppidSupport)
        tryDowngrade();
    return extensions->googleLegacyAppidSupport;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
