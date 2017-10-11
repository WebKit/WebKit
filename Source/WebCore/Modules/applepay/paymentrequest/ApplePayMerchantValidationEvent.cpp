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

#include "config.h"
#include "ApplePayMerchantValidationEvent.h"

#if ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)

#include "Document.h"
#include "MainFrame.h"
#include "PaymentCoordinator.h"
#include "PaymentMerchantSession.h"
#include <runtime/JSCJSValueInlines.h>
#include <runtime/JSObject.h>

namespace WebCore {

Ref<ApplePayMerchantValidationEvent> ApplePayMerchantValidationEvent::create(const AtomicString& type, const URL& validationURL)
{
    return adoptRef(*new ApplePayMerchantValidationEvent(type, validationURL));
}

ApplePayMerchantValidationEvent::ApplePayMerchantValidationEvent(const AtomicString& type, const URL& validationURL)
    : ApplePayValidateMerchantEvent { type, validationURL }
{
}

ExceptionOr<void> ApplePayMerchantValidationEvent::complete(Document& document, JSC::JSValue merchantSessionValue)
{
    if (m_isCompleted)
        return Exception { InvalidStateError };
    m_isCompleted = true;

    auto& paymentCoordinator = document.frame()->mainFrame().paymentCoordinator();
    if (!paymentCoordinator.hasActiveSession())
        return Exception { InvalidStateError };

    if (!merchantSessionValue.isObject())
        return Exception { TypeError };

    String errorMessage;
    auto merchantSession = PaymentMerchantSession::fromJS(*document.execState(), asObject(merchantSessionValue), errorMessage);
    if (!merchantSession)
        return Exception { TypeError, WTFMove(errorMessage) };

    paymentCoordinator.completeMerchantValidation(*merchantSession);
    return { };
}

EventInterface ApplePayMerchantValidationEvent::eventInterface() const
{
    return ApplePayMerchantValidationEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
