/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include "JSValueInWrappedObject.h"
#include "PaymentAddress.h"
#include "PaymentComplete.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class PaymentRequest;
struct PaymentValidationErrors;

class PaymentResponse final : public ActiveDOMObject, public EventTargetWithInlineData, public RefCounted<PaymentResponse> {
public:
    using DetailsFunction = Function<JSC::Strong<JSC::JSObject>(JSC::ExecState&)>;

    static Ref<PaymentResponse> create(ScriptExecutionContext* context, PaymentRequest& request)
    {
        auto response = adoptRef(*new PaymentResponse(context, request));
        response->finishConstruction();
        return response;
    }

    ~PaymentResponse();

    const String& requestId() const { return m_requestId; }
    void setRequestId(const String& requestId) { m_requestId = requestId; }

    const String& methodName() const { return m_methodName; }
    void setMethodName(const String& methodName) { m_methodName = methodName; }

    const DetailsFunction& detailsFunction() const { return m_detailsFunction; }
    void setDetailsFunction(DetailsFunction&&);

    JSValueInWrappedObject& cachedDetails() { return m_cachedDetails; }

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
    void retry(PaymentValidationErrors&&, DOMPromiseDeferred<void>&&);
    void abortWithException(Exception&&);
    bool hasRetryPromise() const { return !!m_retryPromise; }
    void settleRetryPromise(ExceptionOr<void>&& = { });

    using RefCounted<PaymentResponse>::ref;
    using RefCounted<PaymentResponse>::deref;

private:
    PaymentResponse(ScriptExecutionContext*, PaymentRequest&);
    void finishConstruction();

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "PaymentResponse"; }
    bool canSuspendForDocumentSuspension() const final;
    void stop() final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return PaymentResponseEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    enum class State {
        Created,
        Completed,
        Stopped,
    };

    WeakPtr<PaymentRequest> m_request;
    String m_requestId;
    String m_methodName;
    DetailsFunction m_detailsFunction;
    JSValueInWrappedObject m_cachedDetails;
    RefPtr<PaymentAddress> m_shippingAddress;
    String m_shippingOption;
    String m_payerName;
    String m_payerEmail;
    String m_payerPhone;
    State m_state { State::Created };
    std::optional<DOMPromiseDeferred<void>> m_retryPromise;
    RefPtr<PendingActivity<PaymentResponse>> m_pendingActivity;
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
