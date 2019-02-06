/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "U2fHidAuthenticator.h"

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include "CtapHidDriver.h"
#include <WebCore/ApduResponse.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/U2fCommandConstructor.h>
#include <WebCore/U2fResponseConverter.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebKit {
using namespace WebCore;
using namespace apdu;
using namespace fido;

namespace {
const unsigned retryTimeOutValueMs = 200;
}

U2fHidAuthenticator::U2fHidAuthenticator(std::unique_ptr<CtapHidDriver>&& driver)
    : m_driver(WTFMove(driver))
    , m_retryTimer(RunLoop::main(), this, &U2fHidAuthenticator::retryLastCommand)
{
    // FIXME(191520): We need a way to convert std::unique_ptr to UniqueRef.
    ASSERT(m_driver);
}

void U2fHidAuthenticator::makeCredential()
{
    if (!isConvertibleToU2fRegisterCommand(requestData().creationOptions)) {
        receiveRespond(ExceptionData { NotSupportedError, "Cannot convert the request to U2F command."_s });
        return;
    }
    if (!requestData().creationOptions.excludeCredentials.isEmpty()) {
        ASSERT(!m_nextListIndex);
        checkExcludeList(m_nextListIndex++);
        return;
    }
    issueRegisterCommand();
}

void U2fHidAuthenticator::checkExcludeList(size_t index)
{
    if (index >= requestData().creationOptions.excludeCredentials.size()) {
        issueRegisterCommand();
        return;
    }
    auto u2fCmd = convertToU2fCheckOnlySignCommand(requestData().hash, requestData().creationOptions, requestData().creationOptions.excludeCredentials[index]);
    ASSERT(u2fCmd);
    issueNewCommand(WTFMove(*u2fCmd), CommandType::CheckOnlyCommand);
}

void U2fHidAuthenticator::issueRegisterCommand()
{
    auto u2fCmd = convertToU2fRegisterCommand(requestData().hash, requestData().creationOptions);
    ASSERT(u2fCmd);
    issueNewCommand(WTFMove(*u2fCmd), CommandType::RegisterCommand);
}

void U2fHidAuthenticator::getAssertion()
{
    if (!isConvertibleToU2fSignCommand(requestData().requestOptions)) {
        receiveRespond(ExceptionData { NotSupportedError, "Cannot convert the request to U2F command."_s });
        return;
    }
    ASSERT(!m_nextListIndex);
    issueSignCommand(m_nextListIndex++);
}

void U2fHidAuthenticator::issueSignCommand(size_t index)
{
    if (index >= requestData().requestOptions.allowCredentials.size()) {
        receiveRespond(ExceptionData { NotAllowedError, "No credentials from the allowCredentials list is found in the authenticator."_s });
        return;
    }
    auto u2fCmd = convertToU2fSignCommand(requestData().hash, requestData().requestOptions, requestData().requestOptions.allowCredentials[index].idVector);
    ASSERT(u2fCmd);
    issueNewCommand(WTFMove(*u2fCmd), CommandType::SignCommand);
}

void U2fHidAuthenticator::issueNewCommand(Vector<uint8_t>&& command, CommandType type)
{
    m_lastCommand = WTFMove(command);
    m_lastCommandType = type;
    issueCommand(m_lastCommand, m_lastCommandType);
}

void U2fHidAuthenticator::issueCommand(const Vector<uint8_t>& command, CommandType type)
{
    m_driver->transact(Vector<uint8_t>(command), [weakThis = makeWeakPtr(*this), type](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->responseReceived(WTFMove(data), type);
    });
}

void U2fHidAuthenticator::responseReceived(Vector<uint8_t>&& response, CommandType type)
{
    auto apduResponse = ApduResponse::createFromMessage(response);
    if (!apduResponse) {
        receiveRespond(ExceptionData { UnknownError, "Couldn't parse the APDU response."_s });
        return;
    }

    switch (type) {
    case CommandType::RegisterCommand:
        continueRegisterCommandAfterResponseReceived(WTFMove(*apduResponse));
        return;
    case CommandType::CheckOnlyCommand:
        continueCheckOnlyCommandAfterResponseReceived(WTFMove(*apduResponse));
        return;
    case CommandType::BogusCommand:
        continueBogusCommandAfterResponseReceived(WTFMove(*apduResponse));
        return;
    case CommandType::SignCommand:
        continueSignCommandAfterResponseReceived(WTFMove(*apduResponse));
        return;
    }
    ASSERT_NOT_REACHED();
}

void U2fHidAuthenticator::continueRegisterCommandAfterResponseReceived(ApduResponse&& apduResponse)
{
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR: {
        auto response = readU2fRegisterResponse(requestData().creationOptions.rp.id, apduResponse.data());
        if (!response) {
            receiveRespond(ExceptionData { UnknownError, "Couldn't parse the U2F register response."_s });
            return;
        }
        receiveRespond(WTFMove(*response));
        return;
    }
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        // Polling is required during test of user presence.
        m_retryTimer.startOneShot(Seconds::fromMilliseconds(retryTimeOutValueMs));
        return;
    default:
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<unsigned>(apduResponse.status())) });
    }
}

void U2fHidAuthenticator::continueCheckOnlyCommandAfterResponseReceived(ApduResponse&& apduResponse)
{
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR:
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        issueNewCommand(constructBogusU2fRegistrationCommand(), CommandType::BogusCommand);
        return;
    default:
        checkExcludeList(m_nextListIndex++);
    }
}

void U2fHidAuthenticator::continueBogusCommandAfterResponseReceived(ApduResponse&& apduResponse)
{
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR:
        receiveRespond(ExceptionData { InvalidStateError, "At least one credential matches an entry of the excludeCredentials list in the authenticator."_s });
        return;
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        // Polling is required during test of user presence.
        m_retryTimer.startOneShot(Seconds::fromMilliseconds(retryTimeOutValueMs));
        return;
    default:
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<unsigned>(apduResponse.status())) });
    }
}

void U2fHidAuthenticator::continueSignCommandAfterResponseReceived(ApduResponse&& apduResponse)
{
    switch (apduResponse.status()) {
    case ApduResponse::Status::SW_NO_ERROR: {
        auto response = readU2fSignResponse(requestData().requestOptions.rpId, requestData().requestOptions.allowCredentials[m_nextListIndex - 1].idVector, apduResponse.data());
        if (!response) {
            receiveRespond(ExceptionData { UnknownError, "Couldn't parse the U2F sign response."_s });
            return;
        }
        receiveRespond(WTFMove(*response));
        return;
    }
    case ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
        // Polling is required during test of user presence.
        m_retryTimer.startOneShot(Seconds::fromMilliseconds(retryTimeOutValueMs));
        return;
    default:
        issueSignCommand(m_nextListIndex++);
    }
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
