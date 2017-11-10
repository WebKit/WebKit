/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInjectedScriptHost.h"

#include "DOMException.h"
#include "JSDOMException.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLCollection.h"
#include "JSNode.h"
#include "JSNodeList.h"

#if ENABLE(PAYMENT_REQUEST)
#include "JSPaymentRequest.h"
#include "JSPaymentShippingType.h"
#include "PaymentOptions.h"
#include "PaymentRequest.h"
#endif

namespace WebCore {

using namespace JSC;

JSValue WebInjectedScriptHost::subtype(ExecState* exec, JSValue value)
{
    VM& vm = exec->vm();
    if (value.inherits(vm, JSNode::info()))
        return jsNontrivialString(exec, ASCIILiteral("node"));
    if (value.inherits(vm, JSNodeList::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(vm, JSHTMLCollection::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(vm, JSDOMException::info()))
        return jsNontrivialString(exec, ASCIILiteral("error"));

    return jsUndefined();
}

#if ENABLE(PAYMENT_REQUEST)
static JSObject* constructInternalProperty(VM& vm, ExecState* exec, const String& name, JSValue value)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(exec, "name"), jsString(exec, name));
    object->putDirect(vm, Identifier::fromString(exec, "value"), value);
    return object;
}

static JSObject* objectForPaymentOptions(VM& vm, ExecState* exec, const PaymentOptions& paymentOptions)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(exec, "requestPayerName"), jsBoolean(paymentOptions.requestPayerName));
    object->putDirect(vm, Identifier::fromString(exec, "requestPayerEmail"), jsBoolean(paymentOptions.requestPayerEmail));
    object->putDirect(vm, Identifier::fromString(exec, "requestPayerPhone"), jsBoolean(paymentOptions.requestPayerPhone));
    object->putDirect(vm, Identifier::fromString(exec, "requestShipping"), jsBoolean(paymentOptions.requestShipping));
    object->putDirect(vm, Identifier::fromString(exec, "shippingType"), jsNontrivialString(exec, convertEnumerationToString(paymentOptions.shippingType)));
    return object;
}

static JSObject* objectForPaymentCurrencyAmount(VM& vm, ExecState* exec, const PaymentCurrencyAmount& paymentCurrencyAmount)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(exec, "currency"), jsString(exec, paymentCurrencyAmount.currency));
    object->putDirect(vm, Identifier::fromString(exec, "value"), jsString(exec, paymentCurrencyAmount.value));
    object->putDirect(vm, Identifier::fromString(exec, "currencySystem"), jsString(exec, paymentCurrencyAmount.currencySystem));
    return object;
}

static JSObject* objectForPaymentItem(VM& vm, ExecState* exec, const PaymentItem& paymentItem)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(exec, "label"), jsString(exec, paymentItem.label));
    object->putDirect(vm, Identifier::fromString(exec, "amount"), objectForPaymentCurrencyAmount(vm, exec, paymentItem.amount));
    object->putDirect(vm, Identifier::fromString(exec, "pending"), jsBoolean(paymentItem.pending));
    return object;
}

static JSObject* objectForPaymentShippingOption(VM& vm, ExecState* exec, const PaymentShippingOption& paymentShippingOption)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(exec, "id"), jsString(exec, paymentShippingOption.id));
    object->putDirect(vm, Identifier::fromString(exec, "label"), jsString(exec, paymentShippingOption.label));
    object->putDirect(vm, Identifier::fromString(exec, "amount"), objectForPaymentCurrencyAmount(vm, exec, paymentShippingOption.amount));
    object->putDirect(vm, Identifier::fromString(exec, "selected"), jsBoolean(paymentShippingOption.selected));
    return object;
}

static JSObject* objectForPaymentDetailsModifier(VM& vm, ExecState* exec, const PaymentDetailsModifier& modifier)
{
    auto* additionalDisplayItems = constructEmptyArray(exec, nullptr);
    for (unsigned i = 0; i < modifier.additionalDisplayItems.size(); ++i)
        additionalDisplayItems->putDirectIndex(exec, i, objectForPaymentItem(vm, exec, modifier.additionalDisplayItems[i]));

    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(exec, "supportedMethods"), jsString(exec, modifier.supportedMethods));
    object->putDirect(vm, Identifier::fromString(exec, "total"), !modifier.total ? jsNull() : objectForPaymentItem(vm, exec, *modifier.total));
    object->putDirect(vm, Identifier::fromString(exec, "additionalDisplayItems"), additionalDisplayItems);
    object->putDirect(vm, Identifier::fromString(exec, "data"), !modifier.data ? jsNull() : modifier.data.get());
    return object;
}

static JSObject* objectForPaymentDetails(VM& vm, ExecState* exec, const PaymentDetailsInit& paymentDetails)
{
    auto* displayItems = constructEmptyArray(exec, nullptr);
    for (unsigned i = 0; i < paymentDetails.displayItems.size(); ++i)
        displayItems->putDirectIndex(exec, i, objectForPaymentItem(vm, exec, paymentDetails.displayItems[i]));

    auto* shippingOptions = constructEmptyArray(exec, nullptr);
    for (unsigned i = 0; i < paymentDetails.shippingOptions.size(); ++i)
        shippingOptions->putDirectIndex(exec, i, objectForPaymentShippingOption(vm, exec, paymentDetails.shippingOptions[i]));

    auto* modifiers = constructEmptyArray(exec, nullptr);
    for (unsigned i = 0; i < paymentDetails.modifiers.size(); ++i)
        modifiers->putDirectIndex(exec, i, objectForPaymentDetailsModifier(vm, exec, paymentDetails.modifiers[i]));

    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(exec, "id"), jsString(exec, paymentDetails.id));
    object->putDirect(vm, Identifier::fromString(exec, "total"), objectForPaymentItem(vm, exec, paymentDetails.total));
    object->putDirect(vm, Identifier::fromString(exec, "displayItems"), displayItems);
    object->putDirect(vm, Identifier::fromString(exec, "shippingOptions"), shippingOptions);
    object->putDirect(vm, Identifier::fromString(exec, "modifiers"), modifiers);
    return object;
}

static JSString* jsStringForPaymentRequestState(VM& vm, ExecState* exec, PaymentRequest::State state)
{
    switch (state) {
    case PaymentRequest::State::Created:
        return jsNontrivialString(exec, ASCIILiteral("created"));
    case PaymentRequest::State::Interactive:
        return jsNontrivialString(exec, ASCIILiteral("interactive"));
    case PaymentRequest::State::Closed:
        return jsNontrivialString(exec, ASCIILiteral("closed"));
    }

    ASSERT_NOT_REACHED();
    return jsEmptyString(&vm);
}
#endif

JSValue WebInjectedScriptHost::getInternalProperties(VM& vm, ExecState* exec, JSC::JSValue value)
{
#if ENABLE(PAYMENT_REQUEST)
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (PaymentRequest* paymentRequest = JSPaymentRequest::toWrapped(vm, value)) {
        unsigned index = 0;
        auto* array = constructEmptyArray(exec, nullptr);
        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, ASCIILiteral("options"), objectForPaymentOptions(vm, exec, paymentRequest->paymentOptions())));
        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, ASCIILiteral("details"), objectForPaymentDetails(vm, exec, paymentRequest->paymentDetails())));
        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, ASCIILiteral("state"), jsStringForPaymentRequestState(vm, exec, paymentRequest->state())));
        RETURN_IF_EXCEPTION(scope, { });
        return array;
    }
#else
    UNUSED_PARAM(vm);
    UNUSED_PARAM(exec);
    UNUSED_PARAM(value);
#endif

    return { };
}

bool WebInjectedScriptHost::isHTMLAllCollection(JSC::VM& vm, JSC::JSValue value)
{
    return value.inherits(vm, JSHTMLAllCollection::info());
}

} // namespace WebCore
