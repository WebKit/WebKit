/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "MerchantValidationEvent.h"

#if ENABLE(PAYMENT_REQUEST)

#include "Document.h"
#include "PaymentRequest.h"

namespace WebCore {

Ref<MerchantValidationEvent> MerchantValidationEvent::create(const AtomicString& type, const String& methodName, URL&& validationURL)
{
    return adoptRef(*new MerchantValidationEvent(type, methodName, WTFMove(validationURL)));
}

ExceptionOr<Ref<MerchantValidationEvent>> MerchantValidationEvent::create(Document& document, const AtomicString& type, Init&& eventInit)
{
    URL validationURL { document.url(), eventInit.validationURL };
    if (!validationURL.isValid())
        return Exception { TypeError };

    auto methodName = WTFMove(eventInit.methodName);
    if (!methodName.isEmpty()) {
        auto validatedMethodName = convertAndValidatePaymentMethodIdentifier(methodName);
        if (!validatedMethodName)
            return Exception { RangeError, makeString('"', methodName, "\" is an invalid payment method identifier.") };
    }

    return adoptRef(*new MerchantValidationEvent(type, WTFMove(methodName), WTFMove(validationURL), WTFMove(eventInit)));
}

MerchantValidationEvent::MerchantValidationEvent(const AtomicString& type, const String& methodName, URL&& validationURL)
    : Event { type, Event::CanBubble::No, Event::IsCancelable::No }
    , m_methodName { methodName }
    , m_validationURL { WTFMove(validationURL) }
{
    ASSERT(isTrusted());
    ASSERT(m_validationURL.isValid());
}

MerchantValidationEvent::MerchantValidationEvent(const AtomicString& type, String&& methodName, URL&& validationURL, Init&& eventInit)
    : Event { type, WTFMove(eventInit), IsTrusted::No }
    , m_methodName { WTFMove(methodName) }
    , m_validationURL { WTFMove(validationURL) }
{
    ASSERT(!isTrusted());
    ASSERT(m_validationURL.isValid());
}

EventInterface MerchantValidationEvent::eventInterface() const
{
    return MerchantValidationEventInterfaceType;
}

ExceptionOr<void> MerchantValidationEvent::complete(Ref<DOMPromise>&& merchantSessionPromise)
{
    if (!isTrusted())
        return Exception { InvalidStateError };

    if (m_isCompleted)
        return Exception { InvalidStateError };

    auto exception = downcast<PaymentRequest>(target())->completeMerchantValidation(*this, WTFMove(merchantSessionPromise));
    if (exception.hasException())
        return exception.releaseException();

    m_isCompleted = true;
    return { };
}

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
