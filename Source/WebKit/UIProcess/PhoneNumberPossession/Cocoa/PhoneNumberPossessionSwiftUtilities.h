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

#pragma once

#if ENABLE(PHONE_NUMBER_POSSESSION)

#include <JavaScriptCore/ArrayBuffer.h>
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/EscapableByteSpan.h>
#include <wtf/RefCountable.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct PhoneNumberPossessionControllerRequest {
    String relyingParty;
    WTF::VectorUInt8 challenge;
    bool includesPhoneNumber { false };
    std::optional<String> phoneNumberSuffixHint;
    std::optional<WTF::VectorUInt8> pnpiSuffixHint;
    std::optional<WTF::VectorUInt8> knownCarrierPubKey;
    String origin;
};

enum class PhoneNumberPossessionControllerError : uint8_t {
    None,
    Unavailable,
    UserCancelled,
    Cancelled,
    Failed,
};

struct PhoneNumberPossessionControllerResult {
    PhoneNumberPossessionControllerError error { PhoneNumberPossessionControllerError::None };
    RefPtr<JSC::ArrayBuffer> encryptedPNPIWithProof;
    bool hasAggregatorData { false };
    RefPtr<JSC::ArrayBuffer> encryptedProviderErrorInfo;
    RefPtr<JSC::ArrayBuffer> encryptedZKPEncryptionKey;
    RefPtr<JSC::ArrayBuffer> carrierPubKey;
    RefPtr<JSC::ArrayBuffer> encryptedMSISDN;
    RefPtr<JSC::ArrayBuffer> clientDataJSON;
    RefPtr<JSC::ArrayBuffer> portingZKP;
};

using PhoneNumberPossessionCanRequestCompletionHandler = WTF::RefCountable<CompletionHandler<void(bool)>>;
using PhoneNumberPossessionRequestCompletionHandler = WTF::RefCountable<CompletionHandler<void(PhoneNumberPossessionControllerResult)>>;

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
