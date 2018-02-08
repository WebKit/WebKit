/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(PAYMENT_REQUEST)

#include "JSDOMPromiseDeferred.h"
#include "PaymentAddress.h"
#include "PaymentComplete.h"

namespace WebCore {
    
class Document;
class PaymentRequest;

class PaymentResponse final : public RefCounted<PaymentResponse> {
public:
    static Ref<PaymentResponse> create(PaymentRequest& request)
    {
        return adoptRef(*new PaymentResponse(request));
    }

    ~PaymentResponse();

    const String& requestId() const { return m_requestId; }
    void setRequestId(const String& requestId) { m_requestId = requestId; }

    const String& methodName() const { return m_methodName; }
    void setMethodName(const String& methodName) { m_methodName = methodName; }

    const JSC::Strong<JSC::JSObject>& details() const { return m_details; }
    void setDetails(JSC::Strong<JSC::JSObject>&& details) { m_details = WTFMove(details); }

    PaymentAddress* shippingAddress() const { return m_shippingAddress.get(); }
    void setShippingAddress(PaymentAddress* shippingAddress) { m_shippingAddress = shippingAddress; }

    const String& shippingOption() const { return m_shippingOption; }
    void setShippingOption(const String& shippingOption) { m_shippingOption = shippingOption; }

    const String& payerName() const { return m_payerName; }
    void setPayerName(const String& payerName) { m_payerName = payerName; }

    const String& payerEmail() const { return m_payerEmail; }
    void setPayerEmail(const String& payerEmail) { m_payerEmail = payerEmail; }

    const String& payerPhone() const { return m_payerPhone; }
    void setPayerPhone(const String& payerPhone) { m_payerPhone = payerPhone; }

    void complete(std::optional<PaymentComplete>&&, DOMPromiseDeferred<void>&&);

private:
    explicit PaymentResponse(PaymentRequest&);

    Ref<PaymentRequest> m_request;
    String m_requestId;
    String m_methodName;
    // FIXME: The following use of JSC::Strong is incorrect and can lead to storage leaks
    // due to reference cycles; we should use JSValueInWrappedObject instead.
    JSC::Strong<JSC::JSObject> m_details;
    RefPtr<PaymentAddress> m_shippingAddress;
    String m_shippingOption;
    String m_payerName;
    String m_payerEmail;
    String m_payerPhone;
    bool m_completeCalled { false };
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
