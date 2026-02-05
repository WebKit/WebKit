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
#include <pal/crypto/CryptoDigest.h>
#include <wtf/EnumTraits.h>
#include <wtf/RunLoop.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

#define CTAP_RELEASE_LOG(fmt, ...) RELEASE_LOG(WebAuthn, "%p [aaguid=%s, transport=%s] - CtapAuthenticator::" fmt, this, aaguidForDebugging().utf8().data(), transportForDebugging().utf8().data(), ##__VA_ARGS__)
#define CTAP_RELEASE_LOG_WITH_THIS(thisPtr, fmt, ...) RELEASE_LOG(WebAuthn, "%p [aaguid=%s, transport=%s] - CtapAuthenticator::" fmt, thisPtr.get(), thisPtr->aaguidForDebugging().utf8().data(), thisPtr->transportForDebugging().utf8().data(), ##__VA_ARGS__)


namespace WebKit {
using namespace WebCore;
using namespace fido;

using UVAvailability = AuthenticatorSupportedOptions::UserVerificationAvailability;

namespace {

static Vector<Vector<PublicKeyCredentialDescriptor>> batchesForCredentials(Vector<PublicKeyCredentialDescriptor> credentials, uint32_t maxBatchSize, std::optional<uint32_t> maxCredentialIDLength)
{
    Vector<Vector<PublicKeyCredentialDescriptor>> batches;
    for (auto credential : credentials) {
        if (maxCredentialIDLength && BufferSource { credential.id } .length() > *maxCredentialIDLength)
            continue;
        if (!batches.size() || batches.last().size() >= maxBatchSize)
            batches.append({ });
        batches.last().append(credential);
    }

    return batches;
}

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
    case CtapDeviceResponseCode::kSuccess:
        return WebAuthenticationStatus::PINSuccessful;
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

bool isTerminalPinError(const CtapDeviceResponseCode& error)
{
    switch (error) {
    case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
    case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
    case CtapDeviceResponseCode::kCtap2ErrUserPresenceRequired:
        return true;
    default:
        return false;
    }
}

// Hash PRF input according to spec: SHA-256("WebAuthn PRF" || 0x00 || input)
static Vector<uint8_t> hashPRFInput(const BufferSource& input)
{
    constexpr uint8_t prefix[] = { 'W', 'e', 'b', 'A', 'u', 't', 'h', 'n', ' ', 'P', 'R', 'F' };
    constexpr uint8_t nullByte = 0x00;
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(std::span { prefix });
    crypto->addBytes(std::span { &nullByte, 1 });
    crypto->addBytes(input.span());
    return crypto->computeHash();
}

} // namespace

CtapAuthenticator::CtapAuthenticator(Ref<CtapDriver>&& driver, AuthenticatorGetInfoResponse&& info)
    : FidoAuthenticator(WTF::move(driver))
    , m_info(WTF::move(info))
{
}

void CtapAuthenticator::makeCredential()
{
    CTAP_RELEASE_LOG("makeCredential");
    ASSERT(!m_isDowngraded);

    auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    if (options.excludeCredentials.size() > 1) {
        uint32_t maxBatchSize = 1;
        if (m_info.maxCredentialCountInList())
            maxBatchSize = *m_info.maxCredentialCountInList();
        m_batches = batchesForCredentials(options.excludeCredentials, maxBatchSize, m_info.maxCredentialIDLength());
        ASSERT(m_batches.size());
        if (!m_batches.size())
            return continueMakeCredentialAfterCheckExcludedCredentials();
        m_currentBatch = 0;
        std::optional<PinParameters> pinParameters;
        if (!m_pinAuth.isEmpty())
            pinParameters = PinParameters { static_cast<uint8_t>(selectPinProtocol()), m_pinAuth };
        Vector<uint8_t> cborCmd = encodeSilentGetAssertion(options.rp.id, requestData().hash, m_batches[m_currentBatch], pinParameters);
        protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) mutable {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;
            weakThis->continueSilentlyCheckCredentials(WTF::move(data), [weakThis = WTF::move(weakThis)] (bool foundMatch) mutable {
                if (!weakThis)
                    return;
                weakThis->continueMakeCredentialAfterCheckExcludedCredentials(foundMatch);
            });
        });
    } else
        continueMakeCredentialAfterCheckExcludedCredentials();
}

void CtapAuthenticator::continueSilentlyCheckCredentials(Vector<uint8_t>&& data, CompletionHandler<void(bool)>&& completionHandler)
{
    auto error = getResponseCode(data);
    CTAP_RELEASE_LOG("continueSilentlyCheckCredentials: Got error code: %hhu from authenticator.", enumToUnderlyingType(error));

    if (error == CtapDeviceResponseCode::kSuccess)
        return completionHandler(true);
    if (error == CtapDeviceResponseCode::kCtap2ErrNoCredentials) {
        if (m_currentBatch + 1 >= m_batches.size())
            return completionHandler(false);
        m_currentBatch += 1;
        if (m_currentBatch >= m_batches.size())
            return continueMakeCredentialAfterCheckExcludedCredentials();
    }

    if (isTerminalPinError(error)) {
        CTAP_RELEASE_LOG("continueSilentlyCheckCredentials: Terminal error - notifying UI");
        if (RefPtr observer = this->observer())
            observer->authenticatorStatusUpdated(WebAuthenticationStatus::PinAuthBlocked);
        return completionHandler(false);
    }
    if (isPinError(error)) {
        if (!m_pinAuth.isEmpty()) {
            if (RefPtr observer = this->observer())
                observer->authenticatorStatusUpdated(toStatus(error));
        }
        if (tryRestartPin(error))
            return;
    }

    Vector<uint8_t> cborCmd;
    auto response = readCTAPGetAssertionResponse(data, AuthenticatorAttachment::CrossPlatform, m_hmacSecretRequest);
    std::optional<PinParameters> pinParameters;
    if (!m_pinAuth.isEmpty())
        pinParameters = PinParameters { static_cast<uint8_t>(selectPinProtocol()), m_pinAuth };
    WTF::switchOn(requestData().options, [&](const PublicKeyCredentialCreationOptions& options) {
        cborCmd = encodeSilentGetAssertion(options.rp.id, requestData().hash, m_batches[m_currentBatch], pinParameters);
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        cborCmd = encodeSilentGetAssertion(options.rpId, requestData().hash, m_batches[m_currentBatch], pinParameters);
    });

    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)](Vector<uint8_t>&& data) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return completionHandler(false);
        weakThis->continueSilentlyCheckCredentials(WTF::move(data), WTF::move(completionHandler));
    });
}

void CtapAuthenticator::continueMakeCredentialAfterCheckExcludedCredentials(bool includeCurrentBatch)
{
    Vector<uint8_t> cborCmd;
    auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    std::optional<Vector<PublicKeyCredentialDescriptor>> overrideExcludeCredentials;
    if (includeCurrentBatch) {
        ASSERT(m_currentBatch < m_batches.size());
        overrideExcludeCredentials = m_batches[m_currentBatch];
    } else if (!m_batches.isEmpty())
        overrideExcludeCredentials = Vector<PublicKeyCredentialDescriptor> { };
    auto internalUVAvailability = m_info.options().userVerificationAvailability();
    auto residentKeyAvailability = m_info.options().residentKeyAvailability();
    if (options.authenticatorSelection && options.authenticatorSelection->userVerification == UserVerificationRequirement::Required && !isUVSetup()) {
        performAuthenticatorSelectionForSetupPin();
        return;
    }
    Vector<String> authenticatorSupportedExtensions;
    if (m_info.extensions())
        authenticatorSupportedExtensions = *m_info.extensions();
    if (m_isKeyStoreFull || (m_info.remainingDiscoverableCredentials() && !m_info.remainingDiscoverableCredentials())) {
        if (options.authenticatorSelection && (options.authenticatorSelection->requireResidentKey || options.authenticatorSelection->residentKey == ResidentKeyRequirement::Required)) {
            protect(observer())->authenticatorStatusUpdated(WebAuthenticationStatus::KeyStoreFull);
            return;
        }
        residentKeyAvailability = AuthenticatorSupportedOptions::ResidentKeyAvailability::kNotSupported;
    }

    bool needsPRF = options.extensions && options.extensions->prf;
    CTAP_RELEASE_LOG("continueMakeCredentialAfterCheckExcludedCredentials: PRF check - extensions=%d, prf=%d", !!options.extensions, needsPRF);
    bool supportsHmacSecret = authenticatorSupportedExtensions.contains(kExtensionHmacSecret);
    std::optional<HmacSecretParameters> hmacSecretParams;
    if (needsPRF && supportsHmacSecret && options.extensions->prf->eval) {
        CTAP_RELEASE_LOG("continueMakeCredentialAfterCheckExcludedCredentials: PRF extension found with eval, preparing hmac-secret");
        bool supportsHmacSecretMc = authenticatorSupportedExtensions.contains(kExtensionHmacSecretMc);
        if (supportsHmacSecretMc) {
            hmacSecretParams = prepareHmacSecretParameters(*options.extensions->prf, std::nullopt);
            if (hmacSecretParams)
                CTAP_RELEASE_LOG("continueMakeCredentialAfterCheckExcludedCredentials: PRF hmac-secret-mc parameters prepared");
            else
                CTAP_RELEASE_LOG("continueMakeCredentialAfterCheckExcludedCredentials: PRF hmac-secret-mc parameters failed to prepare");
        } else
            CTAP_RELEASE_LOG("continueMakeCredentialAfterCheckExcludedCredentials: Authenticator doesn't support hmac-secret-mc");
    } else if (needsPRF) {
        CTAP_RELEASE_LOG("continueMakeCredentialAfterCheckExcludedCredentials: PRF requested but conditions not met - supportsHmacSecret=%d, hasEval=%d",
            supportsHmacSecret, options.extensions->prf->eval.has_value());
    }

    UserVerificationRequirement effectiveUVRequirement = (needsPRF && supportsHmacSecret)
        ? UserVerificationRequirement::Required
        : (options.authenticatorSelection ? options.authenticatorSelection->userVerification : UserVerificationRequirement::Discouraged);

    if (internalUVAvailability == UVAvailability::kSupportedAndConfigured && effectiveUVRequirement != UserVerificationRequirement::Discouraged && m_pinAuth.isEmpty())
        cborCmd = encodeMakeCredentialRequestAsCBOR(requestData().hash, options, internalUVAvailability, effectiveUVRequirement, residentKeyAvailability, authenticatorSupportedExtensions, std::nullopt, m_info.algorithms(), WTF::move(overrideExcludeCredentials), WTF::move(hmacSecretParams));
    else if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet && effectiveUVRequirement != UserVerificationRequirement::Discouraged)
        cborCmd = encodeMakeCredentialRequestAsCBOR(requestData().hash, options, internalUVAvailability, effectiveUVRequirement, residentKeyAvailability, authenticatorSupportedExtensions, PinParameters { static_cast<uint8_t>(selectPinProtocol()), m_pinAuth }, m_info.algorithms(), WTF::move(overrideExcludeCredentials), WTF::move(hmacSecretParams));
    else
        cborCmd = encodeMakeCredentialRequestAsCBOR(requestData().hash, options, internalUVAvailability, effectiveUVRequirement, residentKeyAvailability, authenticatorSupportedExtensions, std::nullopt, m_info.algorithms(), WTF::move(overrideExcludeCredentials), WTF::move(hmacSecretParams));
    CTAP_RELEASE_LOG("makeCredential: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    if (m_info.maxMsgSize() && cborCmd.size() >= *m_info.maxMsgSize())
        CTAP_RELEASE_LOG("CtapAuthenticator::makeCredential cmdSize = %lu maxMsgSize = %u", cborCmd.size(), *m_info.maxMsgSize());
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueMakeCredentialAfterResponseReceived(WTF::move(data));
    });
}

void CtapAuthenticator::continueMakeCredentialAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto error = getResponseCode(data);
    CTAP_RELEASE_LOG("continueMakeCredentialAfterResponseReceived: Got error code: %hhu from authenticator.", enumToUnderlyingType(error));
    auto response = readCTAPMakeCredentialResponse(data, AuthenticatorAttachment::CrossPlatform, transports(), std::get<PublicKeyCredentialCreationOptions>(requestData().options).attestation, m_hmacSecretRequest);
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
                protect(observer())->authenticatorStatusUpdated(WebAuthenticationStatus::KeyStoreFull);
            else if (!m_isKeyStoreFull) {
                m_isKeyStoreFull = true;
                makeCredential();
            }
            return;
        }

        if (isTerminalPinError(error)) {
            CTAP_RELEASE_LOG("continueMakeCredentialAfterResponseReceived: Terminal PIN error - notifying UI");
            if (RefPtr observer = this->observer())
                observer->authenticatorStatusUpdated(WebAuthenticationStatus::PinAuthBlocked);
            return;
        }

        if (isPinError(error)) {
            if (!m_pinAuth.isEmpty()) { // Skip the very first command that acts like wink.
                if (RefPtr observer = this->observer())
                    observer->authenticatorStatusUpdated(toStatus(error));
            }
            if (tryRestartPin(error))
                return;
        }

        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }
    auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);

    if (m_hmacSecretRequest)
        m_hmacSecretRequest = std::nullopt;

    if (options.extensions && options.extensions->credProps) {
        auto extensionOutputs = response->extensions();

        auto rkSupported = m_info.options().residentKeyAvailability() == AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported;
        auto rkRequested = options.authenticatorSelection && ((options.authenticatorSelection->residentKey && options.authenticatorSelection->residentKey != ResidentKeyRequirement::Discouraged) || options.authenticatorSelection->requireResidentKey);
        extensionOutputs.credProps = CredentialPropertiesOutput { rkSupported && rkRequested && !m_isKeyStoreFull };
        response->setExtensions(WTF::move(extensionOutputs));
    }
    receiveRespond(response.releaseNonNull());
}

void CtapAuthenticator::getAssertion()
{
    CTAP_RELEASE_LOG("getAssertion");

    auto& options = std::get<PublicKeyCredentialRequestOptions>(requestData().options);
    if (options.allowCredentials.size() > 1) {
        uint32_t maxBatchSize = 1;
        if (m_info.maxCredentialCountInList())
            maxBatchSize = *m_info.maxCredentialCountInList();
        m_batches = batchesForCredentials(options.allowCredentials, maxBatchSize, m_info.maxCredentialIDLength());
        ASSERT(m_batches.size());
        if (!m_batches.size())
            return continueGetAssertionAfterCheckAllowCredentials();
        m_currentBatch = 0;
        std::optional<PinParameters> pinParameters;
        if (!m_pinAuth.isEmpty())
            pinParameters = PinParameters { static_cast<uint8_t>(selectPinProtocol()), m_pinAuth };
        Vector<uint8_t> cborCmd = encodeSilentGetAssertion(options.rpId, requestData().hash, m_batches[m_currentBatch], pinParameters);
        protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) mutable {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;
            weakThis->continueSilentlyCheckCredentials(WTF::move(data), [weakThis = WTF::move(weakThis)] (bool foundCredentials) mutable {
                if (!weakThis)
                    return;
                if (!foundCredentials && weakThis->tryDowngrade())
                    return;
                weakThis->continueGetAssertionAfterCheckAllowCredentials();
            });
        });
    } else if (options.allowCredentials.size() == 1 && canDowngradeToU2f()) {
        std::optional<PinParameters> pinParameters;
        if (!m_pinAuth.isEmpty())
            pinParameters = PinParameters { static_cast<uint8_t>(selectPinProtocol()), m_pinAuth };
        Vector<uint8_t> cborCmd = encodeSilentGetAssertion(options.rpId, requestData().hash, options.allowCredentials, pinParameters);
        protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;
            auto error = getResponseCode(data);
            if (error == CtapDeviceResponseCode::kCtap2ErrNoCredentials && weakThis->tryDowngrade())
                return;
            weakThis->continueGetAssertionAfterCheckAllowCredentials();
        });
    } else
        continueGetAssertionAfterCheckAllowCredentials();
}

void CtapAuthenticator::continueGetAssertionAfterCheckAllowCredentials()
{
    ASSERT(!m_isDowngraded);
    Vector<uint8_t> cborCmd;
    auto& options = std::get<PublicKeyCredentialRequestOptions>(requestData().options);

    auto internalUVAvailability = m_info.options().userVerificationAvailability();
    Vector<String> authenticatorSupportedExtensions;
    if (m_info.extensions())
        authenticatorSupportedExtensions = *m_info.extensions();
    std::optional<Vector<PublicKeyCredentialDescriptor>> overrideAllowCredentials;
    if (options.allowCredentials.size() > 1 && m_currentBatch < m_batches.size())
        overrideAllowCredentials = m_batches[m_currentBatch];

    bool needsPRF = options.extensions && options.extensions->prf;
    CTAP_RELEASE_LOG("continueGetAssertionAfterCheckAllowCredentials: PRF check - extensions=%d, prf=%d", !!options.extensions, needsPRF);
    bool supportsHmacSecret = authenticatorSupportedExtensions.contains(kExtensionHmacSecret);

    std::optional<HmacSecretParameters> hmacSecretParams;
    if (needsPRF && supportsHmacSecret) {
        CTAP_RELEASE_LOG("continueGetAssertionAfterCheckAllowCredentials: PRF extension found, preparing hmac-secret");
        // Determine which credential ID to use for evalByCredential matching
        std::optional<Vector<uint8_t>> credentialId;
        if (overrideAllowCredentials && !overrideAllowCredentials->isEmpty())
            credentialId = overrideAllowCredentials->first().id.span();
        else if (!options.allowCredentials.isEmpty())
            credentialId = options.allowCredentials.first().id.span();
        hmacSecretParams = prepareHmacSecretParameters(*options.extensions->prf, credentialId);
        if (hmacSecretParams)
            CTAP_RELEASE_LOG("continueGetAssertionAfterCheckAllowCredentials: PRF hmac-secret parameters prepared");
        else
            CTAP_RELEASE_LOG("continueGetAssertionAfterCheckAllowCredentials: PRF hmac-secret parameters failed to prepare");
    } else if (needsPRF)
        CTAP_RELEASE_LOG("continueGetAssertionAfterCheckAllowCredentials: PRF requested authenticator does not support hmac-secret");

    CTAP_RELEASE_LOG("getAssertion uv: %hhu internalUvAvailability %d", static_cast<uint8_t>(options.userVerification), internalUVAvailability);

    UserVerificationRequirement effectiveUVRequirement = (needsPRF && supportsHmacSecret)
        ? UserVerificationRequirement::Required
        : options.userVerification;

    if (internalUVAvailability == UVAvailability::kSupportedAndConfigured && effectiveUVRequirement != UserVerificationRequirement::Discouraged && m_pinAuth.isEmpty())
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability, effectiveUVRequirement, authenticatorSupportedExtensions, std::nullopt, WTF::move(overrideAllowCredentials), WTF::move(hmacSecretParams));
    else if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet && effectiveUVRequirement != UserVerificationRequirement::Discouraged)
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability, effectiveUVRequirement, authenticatorSupportedExtensions, PinParameters { static_cast<uint8_t>(selectPinProtocol()), m_pinAuth }, WTF::move(overrideAllowCredentials), WTF::move(hmacSecretParams));
    else
        cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, options, internalUVAvailability, effectiveUVRequirement, authenticatorSupportedExtensions, std::nullopt, WTF::move(overrideAllowCredentials), WTF::move(hmacSecretParams));
    if (m_info.maxMsgSize() && cborCmd.size() >= *m_info.maxMsgSize())
        CTAP_RELEASE_LOG("getAssertion cmdSize = %lu maxMsgSize = %u", cborCmd.size(), *m_info.maxMsgSize());
    CTAP_RELEASE_LOG("getAssertion: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetAssertionAfterResponseReceived(WTF::move(data));
    });
}

void CtapAuthenticator::continueGetAssertionAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto response = readCTAPGetAssertionResponse(data, AuthenticatorAttachment::CrossPlatform, m_hmacSecretRequest);
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

        if (isTerminalPinError(error)) {
            CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: Terminal PIN error - notifying UI");
            if (RefPtr observer = this->observer())
                observer->authenticatorStatusUpdated(WebAuthenticationStatus::PinAuthBlocked);
            return;
        }

        if (isPinError(error)) {
            if (!m_pinAuth.isEmpty()) { // Skip the very first command that acts like wink.
                if (RefPtr observer = this->observer())
                    observer->authenticatorStatusUpdated(toStatus(error));
            }
            if (tryRestartPin(error))
                return;
        }

        if (error == CtapDeviceResponseCode::kCtap2ErrNoCredentials) {
            if (RefPtr observer = this->observer())
                observer->authenticatorStatusUpdated(WebAuthenticationStatus::NoCredentialsFound);
        }

        CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: No credentials found.");
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }
    CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: Get %lu credentials back.", response->numberOfCredentials());

    if (m_hmacSecretRequest)
        m_hmacSecretRequest = std::nullopt;

    if (response->numberOfCredentials() <= 1) {
        receiveRespond(response.releaseNonNull());
        return;
    }

    m_remainingAssertionResponses = response->numberOfCredentials() - 1;
    m_assertionResponses.reserveInitialCapacity(response->numberOfCredentials());
    m_assertionResponses.append(response.releaseNonNull());
    auto cborCmd = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion);
    CTAP_RELEASE_LOG("continueGetAssertionAfterResponseReceived: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetNextAssertionAfterResponseReceived(WTF::move(data));
    });
}

void CtapAuthenticator::continueGetNextAssertionAfterResponseReceived(Vector<uint8_t>&& data)
{
    auto error = getResponseCode(data);
    CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived: errorcode: %hhu", enumToUnderlyingType(error));
    auto response = readCTAPGetAssertionResponse(data, AuthenticatorAttachment::CrossPlatform, m_hmacSecretRequest);
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
            observer->selectAssertionResponse(Vector { m_assertionResponses }, WebAuthenticationSource::External, [weakThis = WeakPtr { *this }] (AuthenticatorAssertionResponse* response) {
                RELEASE_ASSERT(RunLoop::isMain());
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                auto result = protectedThis->m_assertionResponses.findIf([expectedResponse = response] (auto& response) {
                    return response.ptr() == expectedResponse;
                });
                if (result == notFound)
                    return;
                protectedThis->receiveRespond(protectedThis->m_assertionResponses[result].copyRef());
            });
        }
        return;
    }

    auto cborCmd = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion);
    CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetNextAssertionAfterResponseReceived(WTF::move(data));
    });
}

void CtapAuthenticator::getRetries()
{
    auto cborCmd = encodeAsCBOR(pin::RetriesRequest { selectPinProtocol() });
    CTAP_RELEASE_LOG("getRetries: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        CTAP_RELEASE_LOG_WITH_THIS(protectedThis, "getRetries: Response %s", base64EncodeToString(data).utf8().data());
        protectedThis->continueGetKeyAgreementAfterGetRetries(WTF::move(data));
    });
}

void CtapAuthenticator::continueGetKeyAgreementAfterGetRetries(Vector<uint8_t>&& data)
{
    CTAP_RELEASE_LOG("continueGetKeyAgreementAfterGetRetries");
    auto retries = pin::RetriesResponse::parse(data);
    if (!retries) {
        auto error = getResponseCode(data);
        CTAP_RELEASE_LOG("continueGetKeyAgreementAfterGetRetries: Error code: %hhu", enumToUnderlyingType(error));
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }

    auto cborCmd = encodeAsCBOR(pin::KeyAgreementRequest { selectPinProtocol() });
    CTAP_RELEASE_LOG("continueGetKeyAgreementAfterGetRetries: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }, retries = retries->retries] (Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        CTAP_RELEASE_LOG_WITH_THIS(protectedThis, "continueGetKeyAgreementAfterGetRetries: Response %s", base64EncodeToString(data).utf8().data());
        protectedThis->continueRequestPinAfterGetKeyAgreement(WTF::move(data), retries);
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
        m_cachedPeerKey = keyAgreement->peerKey.ptr();
        observer->requestPin(retries, [weakThis = WeakPtr { *this }, keyAgreement = WTF::move(*keyAgreement)] (const String& pin) {
            RELEASE_ASSERT(RunLoop::isMain());
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            CTAP_RELEASE_LOG_WITH_THIS(protectedThis, "continueRequestPinAfterGetKeyAgreement: Got pin from observer.");
            protectedThis->continueGetPinTokenAfterRequestPin(pin, keyAgreement.peerKey);
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
    auto tokenRequest = pin::TokenRequest::tryCreate(selectPinProtocol(), *pinUTF8, peerKey);
    if (!tokenRequest) {
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, "Cannot create a TokenRequest."_s });
        return;
    }

    auto cborCmd = encodeAsCBOR(*tokenRequest);
    CTAP_RELEASE_LOG("continueGetPinTokenAfterRequestPin: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }, tokenRequest = WTF::move(*tokenRequest)] (Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        CTAP_RELEASE_LOG_WITH_THIS(protectedThis, "continueGetPinTokenAfterRequestPin: Response %s", base64EncodeToString(data).utf8().data());
        protectedThis->continueRequestAfterGetPinToken(WTF::move(data), tokenRequest);
    });
}

void CtapAuthenticator::continueRequestAfterGetPinToken(Vector<uint8_t>&& data, const fido::pin::TokenRequest& tokenRequest)
{
    CTAP_RELEASE_LOG("continueGetNextAssertionAfterResponseReceived");
    auto token = pin::TokenResponse::parse(selectPinProtocol(), tokenRequest.sharedKey(), data);
    if (!token) {
        auto error = getResponseCode(data);

        if (isPinError(error)) {
            if (RefPtr observer = this->observer())
                observer->authenticatorStatusUpdated(toStatus(error));
            if (tryRestartPin(error))
                return;
        }

        if (RefPtr observer = this->observer())
            observer->authenticatorStatusUpdated(toStatus(CtapDeviceResponseCode::kSuccess));

        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }

    if (RefPtr observer = this->observer())
        observer->authenticatorStatusUpdated(toStatus(CtapDeviceResponseCode::kSuccess));

    m_pinAuth = token->pinAuth(selectPinProtocol(), requestData().hash);
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
    case CtapDeviceResponseCode::kCtap2ErrPinNotSet:
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinRequired:
        if (m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedButPinNotSet)
            performAuthenticatorSelectionForSetupPin();
        else
            getRetries();
        return true;
    default:
        return false;
    }
}

bool CtapAuthenticator::canDowngradeToU2f() const
{
    CTAP_RELEASE_LOG("canDowngradeToU2f");
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
    return isConvertible;
}

bool CtapAuthenticator::tryDowngrade()
{
    CTAP_RELEASE_LOG("tryDowngrade");
    if (!canDowngradeToU2f())
        return false;

    CTAP_RELEASE_LOG("tryDowngrade: Downgrading to U2F.");
    m_isDowngraded = true;
    driver().setProtocol(ProtocolVersion::kU2f);
    protect(observer())->downgrade(*this, U2fAuthenticator::create(releaseDriver()));
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

bool CtapAuthenticator::isUVSetup() const
{
    return m_info.options().clientPinAvailability() == AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet || m_info.options().userVerificationAvailability() == AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured;
}

fido::PINUVAuthProtocol CtapAuthenticator::selectPinProtocol() const
{
    if (auto& protocols = m_info.pinProtocol()) {
        CTAP_RELEASE_LOG("selectPinProtocol: Authenticator supports %zu protocols", protocols->size());
        for (auto protocol : *protocols)
            CTAP_RELEASE_LOG("selectPinProtocol: Available protocol %hhu", static_cast<uint8_t>(protocol));

        if (protocols->contains(fido::PINUVAuthProtocol::kPinProtocol2)) {
            CTAP_RELEASE_LOG("selectPinProtocol: Selected Protocol 2");
            return fido::PINUVAuthProtocol::kPinProtocol2;
        }
    }
    CTAP_RELEASE_LOG("selectPinProtocol: Defaulting to Protocol 1");
    return fido::PINUVAuthProtocol::kPinProtocol1;
}

void CtapAuthenticator::continueSetupPinAfterCommand(Vector<uint8_t>&& data, const String& pin, Ref<WebCore::CryptoKeyEC> peerKey)
{
    auto error = getResponseCode(data);
    if (error != fido::CtapDeviceResponseCode::kSuccess) {
        CTAP_RELEASE_LOG("continueSetupPinAfterCommand: Response of setPin was not successful: %s", base64EncodeToString(data).utf8().data());
        protect(observer())->authenticatorStatusUpdated(WebAuthenticationStatus::PinInvalid);
        return;
    }
    m_info.mutableOptions().setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet);
    auto pinUTF8 = pin::validateAndConvertToUTF8(pin);
    if (!pinUTF8) {
        CTAP_RELEASE_LOG("continueSetupPinAfterCommand: Unable to convert PIN, although it was successfully set.");
        protect(observer())->authenticatorStatusUpdated(WebAuthenticationStatus::PinInvalid);
        return;
    }
    auto tokenRequest = pin::TokenRequest::tryCreate(selectPinProtocol(), *pinUTF8, peerKey);
    if (!tokenRequest) {
        CTAP_RELEASE_LOG("continueSetupPinAfterCommand: Failed to create TokenRequest.");
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, "Cannot create a TokenRequest."_s });
        return;
    }

    auto cborCmd = encodeAsCBOR(*tokenRequest);
    CTAP_RELEASE_LOG("continueSetupPinAfterCommand: Sending %s", base64EncodeToString(cborCmd).utf8().data());
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }, tokenRequest = WTF::move(*tokenRequest)] (Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        CTAP_RELEASE_LOG_WITH_THIS(protectedThis, "continueGetPinTokenAfterRequestPin: Response %s", base64EncodeToString(data).utf8().data());
        protectedThis->continueRequestAfterGetPinToken(WTF::move(data), tokenRequest);
    });
}


void CtapAuthenticator::continueSetupPinAfterGetKeyAgreement(Vector<uint8_t>&& data, const String& pin)
{
    auto keyAgreement = pin::KeyAgreementResponse::parse(data);
    if (!keyAgreement) {
        auto error = getResponseCode(data);
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<uint8_t>(error)) });
        return;
    }
    auto setPinRequest = pin::SetPinRequest::tryCreate(selectPinProtocol(), pin, keyAgreement->peerKey);
    if (!setPinRequest) {
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, "Cannot create a SetPinRequest."_s });
        return;
    }
    m_pinAuth = setPinRequest->pinAuth();
    auto cborCmd = encodeAsCBOR(*setPinRequest);
    protect(driver())->transact(WTF::move(cborCmd), [weakThis = WeakPtr { *this }, pin, peerKey = WTF::move( keyAgreement->peerKey)](Vector<uint8_t>&& response) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->continueSetupPinAfterCommand(WTF::move(response), pin,  WTF::move(peerKey));
    });
}

void CtapAuthenticator::setupPin()
{
    RefPtr observer = this->observer();
    if (!observer)
        return;
    CTAP_RELEASE_LOG("setupPin: Requesting new pin from delegate");
    uint64_t minLength = m_info.minPINLength().value_or(4);
    observer->requestNewPin(minLength, [weakThis = WeakPtr { *this }] (const String& pin) mutable {
        ASSERT(RunLoop::isMain());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (auto minPINLength = protectedThis->m_info.minPINLength(); pin.length() < (minPINLength ? *minPINLength : 4)) {
            protect(protectedThis->observer())->authenticatorStatusUpdated(WebAuthenticationStatus::PINTooShort); // PINTooShort
            protectedThis->performAuthenticatorSelectionForSetupPin();
            return;
        }
        if (pin.sizeInBytes() > kPINMaxSizeInBytes) {
            protect(protectedThis->observer())->authenticatorStatusUpdated(WebAuthenticationStatus::PINTooLong); // PINTooLong
            protectedThis->performAuthenticatorSelectionForSetupPin();
            return;
        }
        auto cborCmd = encodeAsCBOR(pin::KeyAgreementRequest { protectedThis->selectPinProtocol() });
        CTAP_RELEASE_LOG_WITH_THIS(protectedThis, "setupPin: Sending %s", base64EncodeToString(cborCmd).utf8().data());
        protect(protectedThis->driver())->transact(WTF::move(cborCmd), [weakThis = WTF::move(weakThis), pin] (Vector<uint8_t>&& data) {
            ASSERT(RunLoop::isMain());
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            CTAP_RELEASE_LOG_WITH_THIS(protectedThis, "setupPin: Response %s", base64EncodeToString(data).utf8().data());
            protectedThis->continueSetupPinAfterGetKeyAgreement(WTF::move(data), pin);
        });
    });
}

void CtapAuthenticator::performAuthenticatorSelectionForSetupPin()
{
    CTAP_RELEASE_LOG("performAuthenticatorSelectionForSetupPin: Requesting gesture for authenticator selection");
    if (m_info.versions().contains(ProtocolVersion::kCtap21) || m_info.versions().contains(ProtocolVersion::kCtap21Pre)) {
        // We should perform the authenticatorSelector command
        protect(driver())->transact(encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorAuthenticatorSelection), [weakThis = WeakPtr { *this }, weakDriver = WeakPtr { driver() }] (Vector<uint8_t>&& response) mutable {
            ASSERT(RunLoop::isMain());
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setupPin();
        });
    } else {
        auto zeroLengthPinAuth = encodeBogusRequestForAuthenticatorSelection();
        // We should send a zeroLength pinAuth
        protect(driver())->transact(WTF::move(zeroLengthPinAuth), [weakThis = WeakPtr { *this }, weakDriver = WeakPtr { driver() }] (Vector<uint8_t>&& response) mutable {
            ASSERT(RunLoop::isMain());
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setupPin();
        });
    }
}

std::optional<HmacSecretParameters> CtapAuthenticator::prepareHmacSecretParameters(const AuthenticationExtensionsClientInputs::PRFInputs& prfInputs, const std::optional<Vector<uint8_t>>& credentialId)
{
    if (!m_cachedPeerKey) {
        CTAP_RELEASE_LOG("prepareHmacSecretParameters: No cached peer key available (likely first attempt before PIN)");
        return std::nullopt;
    }

    const AuthenticationExtensionsClientInputs::PRFValues* selectedValues = nullptr;

    if (prfInputs.evalByCredential && credentialId) {
        auto credentialIdString = base64URLEncodeToString(*credentialId);
        for (const auto& entry : *prfInputs.evalByCredential) {
            if (entry.key == credentialIdString) {
                selectedValues = &entry.value;
                CTAP_RELEASE_LOG("prepareHmacSecretParameters: Using evalByCredential for credential");
                break;
            }
        }
    }

    if (!selectedValues && prfInputs.eval) {
        selectedValues = &*prfInputs.eval;
        CTAP_RELEASE_LOG("prepareHmacSecretParameters: Using eval");
    }

    if (!selectedValues) {
        CTAP_RELEASE_LOG("prepareHmacSecretParameters: No applicable PRF values");
        return std::nullopt;
    }

    auto salt1 = hashPRFInput(selectedValues->first);
    std::optional<Vector<uint8_t>> salt2;
    if (selectedValues->second)
        salt2 = hashPRFInput(*selectedValues->second);

    auto hmacSecretRequest = pin::HmacSecretRequest::create(selectPinProtocol(), salt1, salt2, m_cachedPeerKey.copyRef());
    if (!hmacSecretRequest) {
        CTAP_RELEASE_LOG("prepareHmacSecretParameters: Failed to create HmacSecretRequest");
        return std::nullopt;
    }

    HmacSecretParameters params;
    auto& coseKeyMap = hmacSecretRequest->coseKey();
    cbor::CBORValue::MapValue clonedKeyAgreement;
    for (const auto& entry : coseKeyMap)
        clonedKeyAgreement[entry.first.clone()] = entry.second.clone();
    params.keyAgreement = WTF::move(clonedKeyAgreement);
    params.saltEnc = hmacSecretRequest->saltEnc();
    params.saltAuth = hmacSecretRequest->saltAuth();
    params.protocol = static_cast<int>(hmacSecretRequest->protocol());

    m_hmacSecretRequest = WTF::move(hmacSecretRequest);

    return params;
}



} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
