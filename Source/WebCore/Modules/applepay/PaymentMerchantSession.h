/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#if ENABLE(APPLE_PAY)

#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

namespace JSC {
class CallFrame;
class JSGlobalObject;
class JSValue;
}

OBJC_CLASS PKPaymentMerchantSession;

namespace WebCore {

class PaymentMerchantSession {
public:
    PaymentMerchantSession() = default;
    explicit PaymentMerchantSession(RetainPtr<PKPaymentMerchantSession>&& pkPaymentMerchantSession)
        : m_pkPaymentMerchantSession { WTFMove(pkPaymentMerchantSession) }
    {
    }

    static std::optional<PaymentMerchantSession> fromJS(JSC::JSGlobalObject&, JSC::JSValue, String& errorMessage);

    PKPaymentMerchantSession *pkPaymentMerchantSession() const { return m_pkPaymentMerchantSession.get(); }

private:
    RetainPtr<PKPaymentMerchantSession> m_pkPaymentMerchantSession;
};

}

#endif
