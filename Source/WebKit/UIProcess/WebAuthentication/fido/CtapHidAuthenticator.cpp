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
#include "CtapHidAuthenticator.h"

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include "CtapHidDriver.h"
#include <WebCore/DeviceRequestConverter.h>
#include <WebCore/DeviceResponseConverter.h>
#include <WebCore/ExceptionData.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebKit {
using namespace WebCore;
using namespace fido;

CtapHidAuthenticator::CtapHidAuthenticator(std::unique_ptr<CtapHidDriver>&& driver, AuthenticatorGetInfoResponse&& info)
    : m_driver(WTFMove(driver))
    , m_info(WTFMove(info))
{
    // FIXME(191520): We need a way to convert std::unique_ptr to UniqueRef.
    ASSERT(m_driver);
}

void CtapHidAuthenticator::makeCredential()
{
    // FIXME(192061)
    LOG_ERROR("Start making credentials.");
    auto cborCmd = encodeMakeCredenitalRequestAsCBOR(requestData().hash, requestData().creationOptions, m_info.options().userVerificationAvailability());
    m_driver->transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueMakeCredentialAfterResponseReceived(WTFMove(data));
    });
}

void CtapHidAuthenticator::continueMakeCredentialAfterResponseReceived(Vector<uint8_t>&& data) const
{
    auto response = readCTAPMakeCredentialResponse(data);
    if (!response) {
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", data.size() == 1 ? data[0] : -1) });
        return;
    }
    receiveRespond(WTFMove(*response));
}

void CtapHidAuthenticator::getAssertion()
{
    // FIXME(192061)
    LOG_ERROR("Start getting assertions.");
    auto cborCmd = encodeGetAssertionRequestAsCBOR(requestData().hash, requestData().requestOptions, m_info.options().userVerificationAvailability());
    m_driver->transact(WTFMove(cborCmd), [weakThis = makeWeakPtr(*this)](Vector<uint8_t>&& data) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueGetAssertionAfterResponseReceived(WTFMove(data));
    });
}

void CtapHidAuthenticator::continueGetAssertionAfterResponseReceived(Vector<uint8_t>&& data) const
{
    auto response = readCTAPGetAssertionResponse(data);
    if (!response) {
        receiveRespond(ExceptionData { UnknownError, makeString("Unknown internal error. Error code: ", data.size() == 1 ? data[0] : -1) });
        return;
    }
    receiveRespond(WTFMove(*response));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
