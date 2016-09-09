/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "JSApplePayShippingMethodSelectedEvent.h"

#if ENABLE(APPLE_PAY)

#include <runtime/JSCInlines.h>
#include <runtime/ObjectConstructor.h>
#include <wtf/text/StringBuilder.h>

using namespace JSC;

namespace WebCore {

static JSValue toJS(ExecState& state, const PaymentRequest::ShippingMethod& shippingMethod)
{
    JSObject* object = constructEmptyObject(&state);

    object->putDirect(state.vm(), Identifier::fromString(&state, "label"), jsString(&state, shippingMethod.label));
    object->putDirect(state.vm(), Identifier::fromString(&state, "detail"), jsString(&state, shippingMethod.detail));

    StringBuilder amountString;
    amountString.appendNumber(shippingMethod.amount / 100);
    amountString.appendLiteral(".");

    unsigned decimals = shippingMethod.amount % 100;
    if (decimals < 10)
        amountString.appendLiteral("0");
    amountString.appendNumber(decimals);
    object->putDirect(state.vm(), Identifier::fromString(&state, "amount"), jsString(&state, amountString.toString()));

    object->putDirect(state.vm(), Identifier::fromString(&state, "identifier"), jsString(&state, shippingMethod.identifier));

    return object;
}

JSValue JSApplePayShippingMethodSelectedEvent::shippingMethod(ExecState& exec) const
{
    if (!m_shippingMethod)
        m_shippingMethod.set(exec.vm(), this, toJS(exec, wrapped().shippingMethod()));

    return m_shippingMethod.get();
}

}

#endif
