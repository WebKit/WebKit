/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "PaymentHandler.h"

#if ENABLE(PAYMENT_REQUEST)

#if ENABLE(APPLE_PAY_AMS_UI)
#include "ApplePayAMSUIPaymentHandler.h"
#endif

#if ENABLE(APPLE_PAY)
#include "ApplePayPaymentHandler.h"
#endif

namespace WebCore {

RefPtr<PaymentHandler> PaymentHandler::create(Document& document, PaymentRequest& paymentRequest, const PaymentRequest::MethodIdentifier& identifier)
{
#if ENABLE(APPLE_PAY)
    if (ApplePayPaymentHandler::handlesIdentifier(identifier))
        return adoptRef(new ApplePayPaymentHandler(document, identifier, paymentRequest));
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    if (ApplePayAMSUIPaymentHandler::handlesIdentifier(identifier))
        return adoptRef(new ApplePayAMSUIPaymentHandler(document, identifier, paymentRequest));
#endif

    UNUSED_PARAM(document);
    UNUSED_PARAM(paymentRequest);
    UNUSED_PARAM(identifier);
    return nullptr;
}

ExceptionOr<void> PaymentHandler::canCreateSession(Document& document)
{
#if ENABLE(APPLE_PAY)
    auto result = PaymentSession::canCreateSession(document);
    if (result.hasException())
        return Exception { ExceptionCode::SecurityError, result.releaseException().releaseMessage() };
#else
    UNUSED_PARAM(document);
#endif

    return { };
}

ExceptionOr<void> PaymentHandler::validateData(Document& document, JSC::JSValue data, const PaymentRequest::MethodIdentifier& identifier)
{
#if ENABLE(APPLE_PAY)
    if (ApplePayPaymentHandler::handlesIdentifier(identifier))
        return ApplePayPaymentHandler::validateData(document, data);
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    if (ApplePayAMSUIPaymentHandler::handlesIdentifier(identifier))
        return ApplePayAMSUIPaymentHandler::validateData(document, data);
#endif

    UNUSED_PARAM(document);
    UNUSED_PARAM(data);
    UNUSED_PARAM(identifier);
    return { };
}

bool PaymentHandler::hasActiveSession(Document& document)
{
#if ENABLE(APPLE_PAY)
    if (ApplePayPaymentHandler::hasActiveSession(document))
        return true;
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    if (ApplePayAMSUIPaymentHandler::hasActiveSession(document))
        return true;
#endif

    UNUSED_PARAM(document);
    return false;
}

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
