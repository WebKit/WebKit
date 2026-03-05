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

#include <wtf/ArgumentCoder.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS PKPaymentMerchantSession;

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENTMERCHANTSESSION)

namespace WebKit {

struct CoreIPCPKPaymentMerchantSessionData {
    RetainPtr<NSString> merchantIdentifier;
    RetainPtr<NSString> merchantSessionIdentifier;
    RetainPtr<NSString> nonce;
    RetainPtr<NSNumber> epochTimestamp;
    RetainPtr<NSNumber> expiresAt;
    RetainPtr<NSString> domainName;
    RetainPtr<NSString> displayName;
    RetainPtr<NSData> signature;
    RetainPtr<NSString> retryNonce;
    RetainPtr<NSString> initiativeContext;
    RetainPtr<NSString> initiative;
    RetainPtr<NSData> ampEnrollmentPinning;
    RetainPtr<NSString> operationalAnalyticsIdentifier;
    std::optional<Vector<RetainPtr<NSString>>> signedFields;
#if ENABLE(APPLE_PAY_DELEGATED_REQUEST)
    std::optional<bool> isDelegatedSession;
    RetainPtr<NSString> delegateDisplayName;
#endif
};

class CoreIPCPKPaymentMerchantSession {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCPKPaymentMerchantSession);
public:
    CoreIPCPKPaymentMerchantSession(PKPaymentMerchantSession *);
    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCPKPaymentMerchantSession>;
    CoreIPCPKPaymentMerchantSession(std::optional<CoreIPCPKPaymentMerchantSessionData>&&);

    std::optional<CoreIPCPKPaymentMerchantSessionData> m_data;
};

} // namespace WebKit

#endif
