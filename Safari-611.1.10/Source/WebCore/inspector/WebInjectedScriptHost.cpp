/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "JSEventListener.h"
#include "JSEventTarget.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLCollection.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include "JSWorker.h"
#include "Worker.h"

#if ENABLE(PAYMENT_REQUEST)
#include "JSPaymentRequest.h"
#include "JSPaymentShippingType.h"
#include "PaymentOptions.h"
#include "PaymentRequest.h"
#endif

namespace WebCore {

using namespace JSC;

JSValue WebInjectedScriptHost::subtype(JSGlobalObject* exec, JSValue value)
{
    VM& vm = exec->vm();
    if (value.inherits<JSNode>(vm))
        return jsNontrivialString(vm, "node"_s);
    if (value.inherits<JSNodeList>(vm))
        return jsNontrivialString(vm, "array"_s);
    if (value.inherits<JSHTMLCollection>(vm))
        return jsNontrivialString(vm, "array"_s);
    if (value.inherits<JSDOMException>(vm))
        return jsNontrivialString(vm, "error"_s);

    return jsUndefined();
}

static JSObject* constructInternalProperty(VM& vm, JSGlobalObject* exec, const String& name, JSValue value)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "name"), jsString(vm, name));
    object->putDirect(vm, Identifier::fromString(vm, "value"), value);
    return object;
}

#if ENABLE(PAYMENT_REQUEST)
static JSObject* objectForPaymentOptions(VM& vm, JSGlobalObject* exec, const PaymentOptions& paymentOptions)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "requestPayerName"), jsBoolean(paymentOptions.requestPayerName));
    object->putDirect(vm, Identifier::fromString(vm, "requestPayerEmail"), jsBoolean(paymentOptions.requestPayerEmail));
    object->putDirect(vm, Identifier::fromString(vm, "requestPayerPhone"), jsBoolean(paymentOptions.requestPayerPhone));
    object->putDirect(vm, Identifier::fromString(vm, "requestShipping"), jsBoolean(paymentOptions.requestShipping));
    object->putDirect(vm, Identifier::fromString(vm, "shippingType"), jsNontrivialString(vm, convertEnumerationToString(paymentOptions.shippingType)));
    return object;
}

static JSObject* objectForPaymentCurrencyAmount(VM& vm, JSGlobalObject* exec, const PaymentCurrencyAmount& paymentCurrencyAmount)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "currency"), jsString(vm, paymentCurrencyAmount.currency));
    object->putDirect(vm, Identifier::fromString(vm, "value"), jsString(vm, paymentCurrencyAmount.value));
    return object;
}

static JSObject* objectForPaymentItem(VM& vm, JSGlobalObject* exec, const PaymentItem& paymentItem)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "label"), jsString(vm, paymentItem.label));
    object->putDirect(vm, Identifier::fromString(vm, "amount"), objectForPaymentCurrencyAmount(vm, exec, paymentItem.amount));
    object->putDirect(vm, Identifier::fromString(vm, "pending"), jsBoolean(paymentItem.pending));
    return object;
}

static JSObject* objectForPaymentShippingOption(VM& vm, JSGlobalObject* exec, const PaymentShippingOption& paymentShippingOption)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "id"), jsString(vm, paymentShippingOption.id));
    object->putDirect(vm, Identifier::fromString(vm, "label"), jsString(vm, paymentShippingOption.label));
    object->putDirect(vm, Identifier::fromString(vm, "amount"), objectForPaymentCurrencyAmount(vm, exec, paymentShippingOption.amount));
    object->putDirect(vm, Identifier::fromString(vm, "selected"), jsBoolean(paymentShippingOption.selected));
    return object;
}

static JSObject* objectForPaymentDetailsModifier(VM& vm, JSGlobalObject* exec, const PaymentDetailsModifier& modifier)
{
    auto* additionalDisplayItems = constructEmptyArray(exec, nullptr);
    for (unsigned i = 0; i < modifier.additionalDisplayItems.size(); ++i)
        additionalDisplayItems->putDirectIndex(exec, i, objectForPaymentItem(vm, exec, modifier.additionalDisplayItems[i]));

    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "supportedMethods"), jsString(vm, modifier.supportedMethods));
    object->putDirect(vm, Identifier::fromString(vm, "total"), !modifier.total ? jsNull() : objectForPaymentItem(vm, exec, *modifier.total));
    object->putDirect(vm, Identifier::fromString(vm, "additionalDisplayItems"), additionalDisplayItems);
    object->putDirect(vm, Identifier::fromString(vm, "data"), !modifier.data ? jsNull() : modifier.data.get());
    return object;
}

static JSObject* objectForPaymentDetails(VM& vm, JSGlobalObject* exec, const PaymentDetailsInit& paymentDetails)
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
    object->putDirect(vm, Identifier::fromString(vm, "id"), jsString(vm, paymentDetails.id));
    object->putDirect(vm, Identifier::fromString(vm, "total"), objectForPaymentItem(vm, exec, paymentDetails.total));
    object->putDirect(vm, Identifier::fromString(vm, "displayItems"), displayItems);
    object->putDirect(vm, Identifier::fromString(vm, "shippingOptions"), shippingOptions);
    object->putDirect(vm, Identifier::fromString(vm, "modifiers"), modifiers);
    return object;
}

static JSString* jsStringForPaymentRequestState(VM& vm, PaymentRequest::State state)
{
    switch (state) {
    case PaymentRequest::State::Created:
        return jsNontrivialString(vm, "created"_s);
    case PaymentRequest::State::Interactive:
        return jsNontrivialString(vm, "interactive"_s);
    case PaymentRequest::State::Closed:
        return jsNontrivialString(vm, "closed"_s);
    }

    ASSERT_NOT_REACHED();
    return jsEmptyString(vm);
}
#endif

static JSObject* objectForEventTargetListeners(VM& vm, JSGlobalObject* exec, EventTarget* eventTarget)
{
    auto* scriptExecutionContext = eventTarget->scriptExecutionContext();
    if (!scriptExecutionContext)
        return nullptr;

    JSObject* listeners = nullptr;

    for (auto& eventType : eventTarget->eventTypes()) {
        unsigned listenersForEventIndex = 0;
        auto* listenersForEvent = constructEmptyArray(exec, nullptr);

        for (auto& eventListener : eventTarget->eventListeners(eventType)) {
            if (!is<JSEventListener>(eventListener->callback()))
                continue;

            auto& jsListener = downcast<JSEventListener>(eventListener->callback());
            if (&jsListener.isolatedWorld() != &currentWorld(*exec))
                continue;

            auto* jsFunction = jsListener.ensureJSFunction(*scriptExecutionContext);
            if (!jsFunction)
                continue;

            auto* propertiesForListener = constructEmptyObject(exec);
            propertiesForListener->putDirect(vm, Identifier::fromString(vm, "callback"), jsFunction);
            propertiesForListener->putDirect(vm, Identifier::fromString(vm, "capture"), jsBoolean(eventListener->useCapture()));
            propertiesForListener->putDirect(vm, Identifier::fromString(vm, "passive"), jsBoolean(eventListener->isPassive()));
            propertiesForListener->putDirect(vm, Identifier::fromString(vm, "once"), jsBoolean(eventListener->isOnce()));
            listenersForEvent->putDirectIndex(exec, listenersForEventIndex++, propertiesForListener);
        }

        if (listenersForEventIndex) {
            if (!listeners)
                listeners = constructEmptyObject(exec);
            listeners->putDirect(vm, Identifier::fromString(vm, eventType), listenersForEvent);
        }
    }

    return listeners;
}

JSValue WebInjectedScriptHost::getInternalProperties(VM& vm, JSGlobalObject* exec, JSC::JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto* worker = JSWorker::toWrapped(vm, value)) {
        unsigned index = 0;
        auto* array = constructEmptyArray(exec, nullptr);

        String name = worker->name();
        if (!name.isEmpty())
            array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "name"_s, jsString(vm, name)));

        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "terminated"_s, jsBoolean(worker->wasTerminated())));

        if (auto* listeners = objectForEventTargetListeners(vm, exec, worker))
            array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "listeners"_s, listeners));

        RETURN_IF_EXCEPTION(scope, { });
        return array;
    }

#if ENABLE(PAYMENT_REQUEST)
    if (PaymentRequest* paymentRequest = JSPaymentRequest::toWrapped(vm, value)) {
        unsigned index = 0;
        auto* array = constructEmptyArray(exec, nullptr);

        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "options"_s, objectForPaymentOptions(vm, exec, paymentRequest->paymentOptions())));
        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "details"_s, objectForPaymentDetails(vm, exec, paymentRequest->paymentDetails())));
        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "state"_s, jsStringForPaymentRequestState(vm, paymentRequest->state())));

        if (auto* listeners = objectForEventTargetListeners(vm, exec, paymentRequest))
            array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "listeners"_s, listeners));

        RETURN_IF_EXCEPTION(scope, { });
        return array;
    }
#endif

    if (auto* eventTarget = JSEventTarget::toWrapped(vm, value)) {
        unsigned index = 0;
        auto* array = constructEmptyArray(exec, nullptr);

        if (auto* listeners = objectForEventTargetListeners(vm, exec, eventTarget))
            array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "listeners"_s, listeners));

        RETURN_IF_EXCEPTION(scope, { });
        return array;
    }

    return { };
}

bool WebInjectedScriptHost::isHTMLAllCollection(JSC::VM& vm, JSC::JSValue value)
{
    return value.inherits<JSHTMLAllCollection>(vm);
}

} // namespace WebCore
