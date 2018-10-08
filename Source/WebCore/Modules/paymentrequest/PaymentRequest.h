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
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "PaymentDetailsInit.h"
#include "PaymentMethodChangeEvent.h"
#include "PaymentOptions.h"
#include "PaymentResponse.h"
#include "URL.h"
#include <wtf/Variant.h>

namespace WebCore {

class Document;
class Event;
class PaymentAddress;
class PaymentHandler;
class PaymentResponse;
enum class PaymentComplete;
enum class PaymentShippingType;
struct PaymentDetailsUpdate;
struct PaymentMethodData;

class PaymentRequest final : public RefCounted<PaymentRequest>, public ActiveDOMObject, public EventTargetWithInlineData {
public:
    using AbortPromise = DOMPromiseDeferred<void>;
    using CanMakePaymentPromise = DOMPromiseDeferred<IDLBoolean>;
    using ShowPromise = DOMPromiseDeferred<IDLInterface<PaymentResponse>>;

    static ExceptionOr<Ref<PaymentRequest>> create(Document&, Vector<PaymentMethodData>&&, PaymentDetailsInit&&, PaymentOptions&&);
    ~PaymentRequest();

    void show(Document&, RefPtr<DOMPromise>&& detailsPromise, ShowPromise&&);
    ExceptionOr<void> abort(AbortPromise&&);
    void canMakePayment(Document&, CanMakePaymentPromise&&);

    const String& id() const;
    PaymentAddress* shippingAddress() const { return m_shippingAddress.get(); }
    const String& shippingOption() const { return m_shippingOption; }
    std::optional<PaymentShippingType> shippingType() const;

    enum class State {
        Created,
        Interactive,
        Closed,
    };

    enum class UpdateReason {
        ShowDetailsResolved,
        ShippingAddressChanged,
        ShippingOptionChanged,
        PaymentMethodChanged,
    };

    State state() const { return m_state; }

    const PaymentOptions& paymentOptions() const { return m_options; }
    const PaymentDetailsInit& paymentDetails() const { return m_details; }
    const Vector<String>& serializedModifierData() const { return m_serializedModifierData; }

    void shippingAddressChanged(Ref<PaymentAddress>&&);
    void shippingOptionChanged(const String& shippingOption);
    void paymentMethodChanged(const String& methodName, PaymentMethodChangeEvent::MethodDetailsFunction&&);
    ExceptionOr<void> updateWith(UpdateReason, Ref<DOMPromise>&&);
    ExceptionOr<void> completeMerchantValidation(Event&, Ref<DOMPromise>&&);
    void accept(const String& methodName, PaymentResponse::DetailsFunction&&, Ref<PaymentAddress>&& shippingAddress, const String& payerName, const String& payerEmail, const String& payerPhone);
    void complete(std::optional<PaymentComplete>&&);
    void cancel();

    using MethodIdentifier = Variant<String, URL>;
    using RefCounted<PaymentRequest>::ref;
    using RefCounted<PaymentRequest>::deref;

private:
    struct Method {
        MethodIdentifier identifier;
        String serializedData;
    };

    PaymentRequest(Document&, PaymentOptions&&, PaymentDetailsInit&&, Vector<String>&& serializedModifierData, Vector<Method>&& serializedMethodData, String&& selectedShippingOption);

    void settleDetailsPromise(UpdateReason);
    void whenDetailsSettled(std::function<void()>&&);
    void abortWithException(Exception&&);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "PaymentRequest"; }
    bool canSuspendForDocumentSuspension() const final;
    void stop() final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return PaymentRequestEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    bool isPaymentRequest() const final { return true; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    PaymentOptions m_options;
    PaymentDetailsInit m_details;
    Vector<String> m_serializedModifierData;
    Vector<Method> m_serializedMethodData;
    String m_shippingOption;
    RefPtr<PaymentAddress> m_shippingAddress;
    State m_state { State::Created };
    std::optional<ShowPromise> m_showPromise;
    RefPtr<PaymentHandler> m_activePaymentHandler;
    RefPtr<DOMPromise> m_detailsPromise;
    RefPtr<DOMPromise> m_merchantSessionPromise;
    bool m_isUpdating { false };
    bool m_isCancelPending { false };
};

std::optional<PaymentRequest::MethodIdentifier> convertAndValidatePaymentMethodIdentifier(const String& identifier);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PaymentRequest)
    static bool isType(const WebCore::EventTarget& eventTarget) { return eventTarget.isPaymentRequest(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(PAYMENT_REQUEST)
