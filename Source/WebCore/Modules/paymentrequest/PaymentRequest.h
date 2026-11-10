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
#include "EventTargetInterfaces.h"
#include "IDLTypes.h"
#include "PaymentDetailsInit.h"
#include "PaymentMethodChangeEvent.h"
#include "PaymentOptions.h"
#include "PaymentResponse.h"
#include <wtf/URL.h>

namespace WebCore {

class Document;
class Event;
class PaymentAddress;
class PaymentHandler;
class PaymentRequestUpdateEvent;
class PaymentResponse;
enum class PaymentComplete;
enum class PaymentShippingType;
struct PaymentDetailsUpdate;
struct PaymentMethodData;
template<typename IDLType> class DOMPromiseDeferred;
template<typename> class ExceptionOr;

class PaymentRequest final : public ActiveDOMObject, public EventTarget, public RefCounted<PaymentRequest> {
    WTF_MAKE_TZONE_ALLOCATED(PaymentRequest);
public:
    using AbortPromise = DOMPromiseDeferred<void>;
    using CanMakePaymentPromise = DOMPromiseDeferred<IDLBoolean>;
    using ShowPromise = DOMPromiseDeferred<IDLInterface<PaymentResponse>>;

    static ExceptionOr<Ref<PaymentRequest>> create(Document&, Vector<PaymentMethodData>&&, PaymentDetailsInit&&, PaymentOptions&&);
    ~PaymentRequest();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    void show(Document&, RefPtr<DOMPromise>&& detailsPromise, ShowPromise&&);
    void abort(AbortPromise&&);
    void canMakePayment(Document&, CanMakePaymentPromise&&);

    NODELETE const RefPtr<StringImpl>& id() const;
    PaymentAddress* shippingAddress() const { return m_shippingAddress.get(); }
    const RefPtr<StringImpl>& shippingOption() const LIFETIME_BOUND { return m_shippingOption; }
    NODELETE std::optional<PaymentShippingType> shippingType() const;

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

    const PaymentOptions& paymentOptions() const LIFETIME_BOUND { return m_options; }
    const PaymentDetailsInit& paymentDetails() const LIFETIME_BOUND { return m_details; }
    const Vector<RefPtr<StringImpl>>& serializedModifierData() const LIFETIME_BOUND { return m_serializedModifierData; }

    void shippingAddressChanged(Ref<PaymentAddress>&&);
    void shippingOptionChanged(StringImpl* shippingOption);
    void paymentMethodChanged(StringImpl* methodName, PaymentMethodChangeEvent::MethodDetailsFunction&&);
    ExceptionOr<void> updateWith(UpdateReason, Ref<DOMPromise>&&);
    ExceptionOr<void> completeMerchantValidation(Event&, Ref<DOMPromise>&&);
    void accept(StringImpl* methodName, PaymentResponse::DetailsFunction&&);
    void accept(StringImpl* methodName, PaymentResponse::DetailsFunction&&, Ref<PaymentAddress>&& shippingAddress, StringImpl* payerName, StringImpl* payerEmail, StringImpl* payerPhone);
    void reject(Exception&&);
    ExceptionOr<void> complete(Document&, std::optional<PaymentComplete>&&, RefPtr<StringImpl>&& serializedData);
    ExceptionOr<void> retry(PaymentValidationErrors&&);
    void cancel();

    using MethodIdentifier = Variant<RefPtr<StringImpl>, URL>;

private:
    struct Method {
        MethodIdentifier identifier;
        RefPtr<StringImpl> serializedData;
    };

    struct PaymentHandlerWithPendingActivity {
        Ref<PaymentHandler> paymentHandler;
        Ref<PendingActivity<PaymentRequest>> pendingActivity;
    };

    PaymentRequest(Document&, PaymentOptions&&, PaymentDetailsInit&&, Vector<RefPtr<StringImpl>>&& serializedModifierData, Vector<Method>&& serializedMethodData, RefPtr<StringImpl>&& selectedShippingOption);

    void dispatchAndCheckUpdateEvent(Ref<PaymentRequestUpdateEvent>&&);
    void settleDetailsPromise(UpdateReason);
    void whenDetailsSettled(std::function<void()>&&);
    void abortWithException(Exception&&);
    PaymentHandler* activePaymentHandler() { return m_activePaymentHandler ? m_activePaymentHandler->paymentHandler.ptr() : nullptr; }
    void settleShowPromise(ExceptionOr<PaymentResponse&>&&);
    void closeActivePaymentHandler();

    // ActiveDOMObject
    void stop() final;
    void suspend(ReasonForSuspension) final;

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::PaymentRequest; }
    NODELETE ScriptExecutionContext* scriptExecutionContext() const final;
    bool isPaymentRequest() const final { return true; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    PaymentOptions m_options;
    PaymentDetailsInit m_details;
    Vector<RefPtr<StringImpl>> m_serializedModifierData;
    Vector<Method> m_serializedMethodData;
    RefPtr<StringImpl> m_shippingOption;
    RefPtr<PaymentAddress> m_shippingAddress;
    State m_state { State::Created };
    std::unique_ptr<ShowPromise> m_showPromise;
    std::optional<PaymentHandlerWithPendingActivity> m_activePaymentHandler;
    RefPtr<DOMPromise> m_detailsPromise;
    RefPtr<DOMPromise> m_merchantSessionPromise;
    RefPtr<PaymentResponse> m_response;
    bool m_isUpdating { false };
    bool m_isCancelPending { false };
};

std::optional<PaymentRequest::MethodIdentifier> convertAndValidatePaymentMethodIdentifier(StringImpl* identifier);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PaymentRequest)
    static bool isType(const WebCore::EventTarget& eventTarget) { return eventTarget.isPaymentRequest(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(PAYMENT_REQUEST)
