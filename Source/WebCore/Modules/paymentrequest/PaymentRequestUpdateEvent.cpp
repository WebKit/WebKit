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

#include "config.h"
#include "PaymentRequestUpdateEvent.h"

#if ENABLE(PAYMENT_REQUEST)

#include "EventNames.h"
#include "PaymentRequest.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PaymentRequestUpdateEvent);

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(const AtomString& type, const PaymentRequestUpdateEventInit& eventInit)
    : Event { type, eventInit, IsTrusted::No }
{
    ASSERT(!isTrusted());
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(const AtomString& type)
    : Event { type, CanBubble::No, IsCancelable::No }
{
    ASSERT(isTrusted());
}

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent() = default;

ExceptionOr<void> PaymentRequestUpdateEvent::updateWith(Ref<DOMPromise>&& detailsPromise)
{
    if (!isTrusted())
        return Exception { ExceptionCode::InvalidStateError };

    if (m_waitForUpdate)
        return Exception { ExceptionCode::InvalidStateError };

    stopPropagation();
    stopImmediatePropagation();

    PaymentRequest::UpdateReason reason;
    if (type() == eventNames().shippingaddresschangeEvent)
        reason = PaymentRequest::UpdateReason::ShippingAddressChanged;
    else if (type() == eventNames().shippingoptionchangeEvent)
        reason = PaymentRequest::UpdateReason::ShippingOptionChanged;
    else if (type() == eventNames().paymentmethodchangeEvent)
        reason = PaymentRequest::UpdateReason::PaymentMethodChanged;
    else {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::TypeError };
    }

    auto exception = downcast<PaymentRequest>(target())->updateWith(reason, WTFMove(detailsPromise));
    if (exception.hasException())
        return exception.releaseException();

    m_waitForUpdate = true;
    return { };
}

EventInterface PaymentRequestUpdateEvent::eventInterface() const
{
    return PaymentRequestUpdateEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
