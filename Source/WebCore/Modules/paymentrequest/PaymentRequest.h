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

#pragma once

#if ENABLE(PAYMENT_REQUEST)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "PaymentDetailsInit.h"
#include "PaymentOptions.h"

namespace WebCore {

class Document;
class PaymentAddress;
class PaymentResponse;
enum class PaymentShippingType;
struct PaymentMethodData;

class PaymentRequest final : public RefCounted<PaymentRequest>, public ActiveDOMObject, public EventTargetWithInlineData {
public:
    static ExceptionOr<Ref<PaymentRequest>> create(Document&, Vector<PaymentMethodData>&&, PaymentDetailsInit&&, PaymentOptions&&);
    ~PaymentRequest();

    void show(DOMPromiseDeferred<IDLInterface<PaymentResponse>>&&);
    void abort(DOMPromiseDeferred<void>&&);
    void canMakePayment(DOMPromiseDeferred<IDLBoolean>&&);

    const String& id() const;
    PaymentAddress* shippingAddress() const { return m_shippingAddress.get(); }
    const String& shippingOption() const { return m_shippingOption; }
    std::optional<PaymentShippingType> shippingType() const;

    using RefCounted<PaymentRequest>::ref;
    using RefCounted<PaymentRequest>::deref;

private:
    PaymentRequest(Document&, PaymentOptions&&, PaymentDetailsInit&&, Vector<String>&& serializedModifierData, HashMap<String, String>&& serializedMethodData, String&& selectedShippingOption);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "PaymentRequest"; }
    bool canSuspendForDocumentSuspension() const final { return true; }
    void stop() final { }

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return PaymentRequestEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    PaymentOptions m_options;
    PaymentDetailsInit m_details;
    Vector<String> m_serializedModifierData;
    HashMap<String, String> m_serializedMethodData;
    String m_shippingOption;
    RefPtr<PaymentAddress> m_shippingAddress;
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
