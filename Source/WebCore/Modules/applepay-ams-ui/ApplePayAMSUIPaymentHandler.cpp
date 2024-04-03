/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ApplePayAMSUIPaymentHandler.h"

#if ENABLE(APPLE_PAY_AMS_UI) && ENABLE(PAYMENT_REQUEST)

#include "ApplePayAMSUIRequest.h"
#include "Document.h"
#include "JSApplePayAMSUIRequest.h"
#include "JSDOMConvert.h"
#include "Page.h"

namespace WebCore {

static ExceptionOr<ApplePayAMSUIRequest> convertAndValidateApplePayAMSUIRequest(Document& document, JSC::JSValue data)
{
    if (data.isEmpty())
        return Exception { ExceptionCode::TypeError, "Missing payment method data."_s };

    auto throwScope = DECLARE_THROW_SCOPE(document.vm());
    auto applePayAMSUIRequest = convertDictionary<ApplePayAMSUIRequest>(*document.globalObject(), data);
    if (throwScope.exception())
        return Exception { ExceptionCode::ExistingExceptionError };

    if (!applePayAMSUIRequest.engagementRequest.startsWith('{'))
        return Exception { ExceptionCode::TypeError, "Member ApplePayAMSUIRequest.engagementRequest is required and must be a JSON-serializable object"_s };

    return WTFMove(applePayAMSUIRequest);
}

ExceptionOr<void> ApplePayAMSUIPaymentHandler::validateData(Document& document, JSC::JSValue data)
{
    auto requestOrException = convertAndValidateApplePayAMSUIRequest(document, data);
    if (requestOrException.hasException())
        return requestOrException.releaseException();

    return { };
}

bool ApplePayAMSUIPaymentHandler::handlesIdentifier(const PaymentRequest::MethodIdentifier& identifier)
{
    if (!std::holds_alternative<URL>(identifier))
        return false;

    auto& url = std::get<URL>(identifier);
    return url.host() == "apple.com"_s && url.path() == "/ams-ui"_s;
}

bool ApplePayAMSUIPaymentHandler::hasActiveSession(Document& document)
{
    auto* page = document.page();
    return page && page->hasActiveApplePayAMSUISession();
}

void ApplePayAMSUIPaymentHandler::finishSession(std::optional<bool>&& result)
{
    if (!result) {
        m_paymentRequest->reject(Exception { ExceptionCode::AbortError });
        return;
    }

    m_paymentRequest->accept(std::get<URL>(m_identifier).string(), [success = *result] (JSC::JSGlobalObject& lexicalGlobalObject) -> JSC::Strong<JSC::JSObject> {
        JSC::JSLockHolder lock { &lexicalGlobalObject };

        JSC::VM& vm = lexicalGlobalObject.vm();
        auto throwScope = DECLARE_THROW_SCOPE(vm);

        auto* object = constructEmptyObject(&lexicalGlobalObject);
        object->putDirect(vm, JSC::Identifier::fromString(vm, "success"_s), JSC::jsBoolean(success));

        RETURN_IF_EXCEPTION(throwScope, { });

        return { vm, object };
    });
}

ApplePayAMSUIPaymentHandler::ApplePayAMSUIPaymentHandler(Document& document, const PaymentRequest::MethodIdentifier& identifier, PaymentRequest& paymentRequest)
    : ContextDestructionObserver { &document }
    , m_identifier { identifier }
    , m_paymentRequest { paymentRequest }
{
    ASSERT(handlesIdentifier(m_identifier));
}

Document& ApplePayAMSUIPaymentHandler::document() const
{
    ASSERT(scriptExecutionContext());
    return downcast<Document>(*scriptExecutionContext());
}

Page& ApplePayAMSUIPaymentHandler::page() const
{
    ASSERT(document().page());
    return *document().page();
}

ExceptionOr<void> ApplePayAMSUIPaymentHandler::convertData(Document& document, JSC::JSValue data)
{
    auto requestOrException = convertAndValidateApplePayAMSUIRequest(document, data);
    if (requestOrException.hasException())
        return requestOrException.releaseException();

    m_applePayAMSUIRequest = requestOrException.releaseReturnValue();
    return { };
}

ExceptionOr<void> ApplePayAMSUIPaymentHandler::show(Document&)
{
    ASSERT(m_applePayAMSUIRequest);

    if (!page().startApplePayAMSUISession(page().mainFrameURL(), *this, *m_applePayAMSUIRequest))
        return Exception { ExceptionCode::AbortError };

    return { };
}

void ApplePayAMSUIPaymentHandler::hide()
{
    page().abortApplePayAMSUISession(*this);
}

void ApplePayAMSUIPaymentHandler::canMakePayment(Document& document, Function<void(bool)>&& completionHandler)
{
    auto* page = document.page();
    completionHandler(page && !page->hasActiveApplePayAMSUISession());
}

ExceptionOr<void> ApplePayAMSUIPaymentHandler::detailsUpdated(PaymentRequest::UpdateReason, String&& /* error */, AddressErrors&&, PayerErrorFields&&, JSC::JSObject* /* paymentMethodErrors */)
{
    ASSERT_NOT_REACHED_WITH_MESSAGE("ApplePayAMSUIPaymentHandler does not need shipping/payment info");
    return { };
}

ExceptionOr<void> ApplePayAMSUIPaymentHandler::merchantValidationCompleted(JSC::JSValue&& /* merchantSessionValue */)
{
    ASSERT_NOT_REACHED_WITH_MESSAGE("ApplePayAMSUIPaymentHandler does not need merchant validation");
    return { };
}

ExceptionOr<void> ApplePayAMSUIPaymentHandler::complete(Document&, std::optional<PaymentComplete>&&, String&&)
{
    hide();
    return { };
}

ExceptionOr<void> ApplePayAMSUIPaymentHandler::retry(PaymentValidationErrors&&)
{
    return show(document());
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
