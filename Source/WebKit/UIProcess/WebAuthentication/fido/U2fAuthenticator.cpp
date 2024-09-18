/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "U2fAuthenticator.h"

#if ENABLE(WEB_AUTHN)

#include "CtapDriver.h"
#include "Logging.h"
#include <WebCore/ApduResponse.h>
#include <WebCore/AuthenticatorAttachment.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/U2fCommandConstructor.h>
#include <WebCore/U2fResponseConverter.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

#define U2F_RELEASE_LOG(fmt, ...) RELEASE_LOG(WebAuthn, "%p [transport=%s] - U2fAuthenticator::" fmt, this, transportForDebugging().utf8().data(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;
using namespace apdu;
using namespace fido;

namespace {
const unsigned retryTimeOutValueMs = 200;
}

U2fAuthenticator::U2fAuthenticator(std::unique_ptr<CtapDriver>&& driver)
    : FidoAuthenticator(WTFMove(driver))
    , m_retryTimer(RunLoop::main(), this, &U2fAuthenticator::retryLastCommand)
{
}

void U2fAuthenticator::makeCredential()
{
    U2F_RELEASE_LOG("makeCredential");
    auto& creationOptions = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    if (!isConvertibleToU2fRegisterCommand(creationOptions))
        return;
    if (!creationOptions.excludeCredentials.isEmpty()) {
        ASSERT(!m_nextListIndex);
        checkExcludeList(m_nextListIndex++);
        return;
    }
    issueRegisterCommand();
}

void U2fAuthenticator::checkExcludeList(size_t index)
{
    U2F_RELEASE_LOG("checkExcludeList");
    auto& creationOptions = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    if (index >= creationOptions.excludeCredentials.size()) {
        issueRegisterCommand();
        return;
    }
    auto u2fCmd = convertToU2fCheckOnlySignCommand(requestData().hash, creationOptions, creationOptions.excludeCredentials[index]);
    ASSERT(u2fCmd);
    issueNewCommand(WTFMove(*u2fCmd), CommandType::CheckOnlyCommand);
}

void U2fAuthenticator::issueRegisterCommand()
{
    auto u2fCmd = convertToU2fRegisterCommand(requestData().hash, std::get<PublicKeyCredentialCreationOptions>(requestData().options));
    if (!u2fCmd) {
        U2F_RELEASE_LOG("issueRegisterCommand: request not convertible to U2F.");
        return;
    }
    U2F_RELEASE_LOG("issueRegisterCommand: Sending %s", base64EncodeToString(*u2fCmd).utf8().data());
    issueNewCommand(WTFMove(*u2fCmd), CommandType::RegisterCommand);
}

void U2fAuthenticator::getAssertion()
{
    U2F_RELEASE_LOG("getAssertion: Sending bogus sign.");
    if (!isConvertibleToU2fSignCommand(std::get<PublicKeyCredentialRequestOptions>(requestData().options))) {
        U2F_RELEASE_LOG("getAssertion: command not convertible.");
        return;
    }
    ASSERT(!m_nextListIndex);
    issueSignCommand(m_nextListIndex++);
}

void U2fAuthenticator::issueSignCommand(size_t index)
{
    auto& requestOptions = std::get<PublicKeyCredentialRequestOptions>(requestData().options);
    if (index >= requestOptions.allowCredentials.size()) {
        U2F_RELEASE_LOG("issueSignCommand: Sending bogus sign.");
        issueNewCommand(constructBogusU2fRegistrationCommand(), CommandType::BogusCommandNoCredentials);
        return;
    }
    auto u2fCmd = convertToU2fSignCommand(requestData().hash, requestOptions, requestOptions.allowCredentials[index].id, m_isAppId);
    if (!u2fCmd) {
        U2F_RELEASE_LOG("issueSignCommand: request not convertible to U2F.");
        return;
    }
    U2F_RELEASE_LOG("issueSignCommand: index: %lu Sending %s", index, base64EncodeToString(*u2fCmd).utf8().data());
    issueNewCommand(WTFMove(*u2fCmd), CommandType::SignCommand);
}

void U2fAuthenticator::issueNewCommand(Vector<uint8_t>&& command, CommandType type)
{
    U2F_RELEASE_LOG("issueNewCommand");
    m_lastCommand = WTFMove(command);
    m_lastCommandType = type;
    issueCommand(m_lastCommand, m_lastCommandType);
}

void U2fAuthenticator::issueCommand(const Vector<uint8_t>& command, CommandType type)
{
    U2F_RELEASE_LOG("issueCommand: Sending %s", base64EncodeToString(command).utf8().data());
    driver().transact(Vector<uint8_t>(command), [weakThis = WeakPtr { *this }, type](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->responseReceived(WTFMove(data), type);
    });
}

void U2fAuthenticator::responseReceived(Vector<uint8_t>&& response, CommandType type)
{
    auto apduResponse = ApduResponse::createFromMessage(response);
    if (!apduResponse) {
        U2F_RELEASE_LOG("responseReceived: Failed to parse response: %s", base64EncodeToString(response).utf8().data());
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, "Couldn't parse the APDU response."_s });
        return;
    }
    U2F_RELEASE_LOG("responseReceived: Got response for command type: %hhu", enumToUnderlyingType(type));

    switch (type) {
    case CommandType::RegisterCommand:
        continueRegisterCommandAfterResponseReceived(WTFMove(*apduResponse));
        return;
    case CommandType::CheckOnlyCommand:
        continueCheckOnlyCommandAfterResponseReceived(WTFMove(*apduResponse));
        return;
    case CommandType::BogusCommandExcludeCredentialsMatch:
        continueBogusCommandExcludeCredentialsMatchAfterResponseReceived(WTFMove(*apduResponse));
        return;
    case CommandType::BogusCommandNoCredentials:
        continueBogusCommandNoCredentialsAfterResponseReceived(WTFMove(*apduResponse));
        return;
    case CommandType::SignCommand:
        continueSignCommandAfterResponseReceived(WTFMove(*apduResponse));
        return;
    }
    ASSERT_NOT_REACHED();
}

void U2fAuthenticator::continueRegisterCommandAfterResponseReceived(ApduResponse&& apduResponse)
{
    U2F_RELEASE_LOG("continueRegisterCommandAfterResponseReceived: Status %hu", enumToUnderlyingType(apduResponse.status()));
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR: {
        auto& options = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
        ASSERT(options.rp.id);
        auto response = readU2fRegisterResponse(*options.rp.id, apduResponse.data(), AuthenticatorAttachment::CrossPlatform, { driver().transport() }, options.attestation);
        if (!response) {
            receiveRespond(ExceptionData { ExceptionCode::UnknownError, "Couldn't parse the U2F register response."_s });
            return;
        }
        receiveRespond(response.releaseNonNull());
        return;
    }
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        // Polling is required during test of user presence.
        m_retryTimer.startOneShot(Seconds::fromMilliseconds(retryTimeOutValueMs));
        return;
    default:
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<unsigned>(apduResponse.status())) });
    }
}

void U2fAuthenticator::continueCheckOnlyCommandAfterResponseReceived(ApduResponse&& apduResponse)
{
    U2F_RELEASE_LOG("continueCheckOnlyCommandAfterResponseReceived: Status %hu", enumToUnderlyingType(apduResponse.status()));
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR:
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        issueNewCommand(constructBogusU2fRegistrationCommand(), CommandType::BogusCommandExcludeCredentialsMatch);
        return;
    default:
        checkExcludeList(m_nextListIndex++);
    }
}

void U2fAuthenticator::continueBogusCommandExcludeCredentialsMatchAfterResponseReceived(ApduResponse&& apduResponse)
{
    U2F_RELEASE_LOG("continueBogusCommandExcludeCredentialsMatchAfterResponseReceived: Status %hu", enumToUnderlyingType(apduResponse.status()));
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR:
        receiveRespond(ExceptionData { ExceptionCode::InvalidStateError, "At least one credential matches an entry of the excludeCredentials list in the authenticator."_s });
        return;
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        // Polling is required during test of user presence.
        m_retryTimer.startOneShot(Seconds::fromMilliseconds(retryTimeOutValueMs));
        return;
    default:
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<unsigned>(apduResponse.status())) });
    }
}

void U2fAuthenticator::continueBogusCommandNoCredentialsAfterResponseReceived(ApduResponse&& apduResponse)
{
    U2F_RELEASE_LOG("continueBogusCommandNoCredentialsAfterResponseReceived: Status %hu", enumToUnderlyingType(apduResponse.status()));
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR:
        if (auto* observer = this->observer())
            observer->authenticatorStatusUpdated(WebAuthenticationStatus::NoCredentialsFound);
        receiveRespond(ExceptionData { ExceptionCode::NotAllowedError, "No credentials from the allowCredentials list is found in the authenticator."_s });
        return;
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        // Polling is required during test of user presence.
        m_retryTimer.startOneShot(Seconds::fromMilliseconds(retryTimeOutValueMs));
        return;
    default:
        receiveRespond(ExceptionData { ExceptionCode::UnknownError, makeString("Unknown internal error. Error code: "_s, static_cast<unsigned>(apduResponse.status())) });
    }
}

void U2fAuthenticator::continueSignCommandAfterResponseReceived(ApduResponse&& apduResponse)
{
    U2F_RELEASE_LOG("continueSignCommandAfterResponseReceived: Status %hu", enumToUnderlyingType(apduResponse.status()));
    auto& requestOptions = std::get<PublicKeyCredentialRequestOptions>(requestData().options);
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR: {
        RefPtr<AuthenticatorAssertionResponse> response;
        if (m_isAppId) {
            ASSERT(requestOptions.extensions && !requestOptions.extensions->appid.isNull());
            response = readU2fSignResponse(requestOptions.extensions->appid, requestOptions.allowCredentials[m_nextListIndex - 1].id, apduResponse.data(), AuthenticatorAttachment::CrossPlatform);
        } else
            response = readU2fSignResponse(requestOptions.rpId, requestOptions.allowCredentials[m_nextListIndex - 1].id, apduResponse.data(), AuthenticatorAttachment::CrossPlatform);
        if (!response) {
            receiveRespond(ExceptionData { ExceptionCode::UnknownError, "Couldn't parse the U2F sign response."_s });
            return;
        }
        if (m_isAppId)
            response->setExtensions({ m_isAppId, std::nullopt, std::nullopt, std::nullopt });

        receiveRespond(response.releaseNonNull());
        return;
    }
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        // Polling is required during test of user presence.
        m_retryTimer.startOneShot(Seconds::fromMilliseconds(retryTimeOutValueMs));
        return;
    case ApduResponse::Status::SW_WRONG_DATA:
        if (requestOptions.extensions && !requestOptions.extensions->appid.isNull()) {
            if (!m_isAppId) {
                m_isAppId = true;
                issueSignCommand(m_nextListIndex - 1);
                return;
            }
            m_isAppId = false;
        }
        issueSignCommand(m_nextListIndex++);
        return;
    default:
        issueSignCommand(m_nextListIndex++);
    }
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
