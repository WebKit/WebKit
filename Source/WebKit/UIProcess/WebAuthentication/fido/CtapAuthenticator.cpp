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
#include <WebCore/DeviceRequestConverter.h>
#include <WebCore/DeviceResponseConverter.h>
#include <WebCore/ExceptionData.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebKit {
using namespace WebCore;
using namespace fido;

CtapAuthenticator::CtapAuthenticator(std::unique_ptr<CtapDriver>&& driver, AuthenticatorGetInfoResponse&& info)
    : m_driver(WTFMove(driver))
    , m_info(WTFMove(info))
{
    // FIXME(191520): We need a way to convert std::unique_ptr to UniqueRef.
    ASSERT(m_driver);
}

void CtapAuthenticator::makeCredential()
{
    ASSERT(!m_isDowngraded);
    auto cborCmd = encodeMakeCredenitalRequestAsCBOR(requestData().hash, requestData().creationOptions, m_info.options().userVerificationAvailability());
    m_driver->transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueMakeCredentialAfterResponseReceived(WTFMove(data));
    });
}

void CtapAuthenticator::continueMakeCredentialAfterResponseReceived(Vector<uint8_t>&& data) const
{
    auto response = readCTAPMakeCredentialResponse(data, requestData().creationOptions.attestation);
    if (!response) {
        auto error = getResponseCode(data);
        if (error == CtapDeviceResponseCode::kCtap2ErrCredentialExcluded)
            receiveRespond(ExceptionData { InvalidStateError, "At least one credential matches an entry of the excludeCredentials list in the authenticator."_s });
        else
            receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }
    receiveRespond(WTFMove(*response));
}

void CtapAuthenticator::getAssertion()
{
    ASSERT(!m_isDowngraded);
    auto cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, requestData().requestOptions, m_info.options().userVerificationAvailability());
    m_driver->transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
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
        if (error != CtapDeviceResponseCode::kCtap2ErrInvalidCBOR && tryDowngrade())
            return;
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", static_cast<uint8_t>(error)) });
        return;
    }
    receiveRespond(WTFMove(*response));
}

bool CtapAuthenticator::tryDowngrade()
{
    if (m_info.versions().find(ProtocolVersion::kU2f) == m_info.versions().end())
        return false;
    if (!observer())
        return false;

    m_isDowngraded = true;
    m_driver->setProtocol(ProtocolVersion::kU2f);
    observer()->downgrade(this, U2fAuthenticator::create(WTFMove(m_driver)));
    return true;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
