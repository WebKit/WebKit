/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "PhoneNumberPossessionRequest.h"

#if ENABLE(PHONE_NUMBER_POSSESSION)

#include "Exception.h"

namespace WebCore {

// Upper bounds that keep the IPC payload small. Callers apply any stricter rules before making a request.
static constexpr unsigned maximumPhoneNumberSuffixHintLength = 15;
static constexpr size_t maximumPNPISuffixHintLength = 64;
static constexpr size_t maximumCarrierPubKeyLength = 1024;

std::optional<Exception> validatePhoneNumberPossessionRequest(const PhoneNumberPossessionRequest& request)
{
    if (!request.challenge.byteLength())
        return Exception { ExceptionCode::TypeError, "The challenge must not be empty."_s };

    if (!request.phoneNumberSuffixHint.isNull() && (request.phoneNumberSuffixHint.isEmpty() || request.phoneNumberSuffixHint.length() > maximumPhoneNumberSuffixHintLength))
        return Exception { ExceptionCode::TypeError, "The phoneNumberSuffixHint must contain between 1 and 15 characters."_s };

    if (request.pnpiSuffixHint && (!request.pnpiSuffixHint->byteLength() || request.pnpiSuffixHint->byteLength() > maximumPNPISuffixHintLength))
        return Exception { ExceptionCode::TypeError, "The pnpiSuffixHint must be between 1 and 64 bytes long."_s };

    if (request.knownCarrierPubKey && (!request.knownCarrierPubKey->byteLength() || request.knownCarrierPubKey->byteLength() > maximumCarrierPubKeyLength))
        return Exception { ExceptionCode::TypeError, "The knownCarrierPubKey must be between 1 and 1024 bytes long."_s };

    return std::nullopt;
}

} // namespace WebCore

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
