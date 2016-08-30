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
#include "JSApplePaySession.h"

#if ENABLE(APPLE_PAY)

#include "ArrayValue.h"
#include "Dictionary.h"
#include "JSDOMBinding.h"
#include "JSDOMConvert.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

JSValue JSApplePaySession::completeShippingMethodSelection(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = state.thisValue();
    JSApplePaySession* castedThis = jsDynamicCast<JSApplePaySession*>(thisValue);
    if (UNLIKELY(!castedThis))
        return JSValue::decode(throwThisTypeError(state, scope, "ApplePaySession", "completeShippingMethodSelection"));

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSApplePaySession::info());
    auto& impl = castedThis->wrapped();
    if (UNLIKELY(state.argumentCount() < 3))
        return JSValue::decode(throwVMError(&state, scope, createNotEnoughArgumentsError(&state)));

    ExceptionCode ec = 0;
    uint16_t status = convert<uint16_t>(state, state.argument(0), NormalConversion);
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    Dictionary newTotal = { &state, state.argument(1) };
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    ArrayValue newLineItems { &state, state.argument(2) };
    if (UNLIKELY(state.hadException()))
        return jsUndefined();
    impl.completeShippingMethodSelection(status, newTotal, newLineItems, ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

JSValue JSApplePaySession::completeShippingContactSelection(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = state.thisValue();
    JSApplePaySession* castedThis = jsDynamicCast<JSApplePaySession*>(thisValue);
    if (UNLIKELY(!castedThis))
        return JSValue::decode(throwThisTypeError(state, scope, "ApplePaySession", "completeShippingContactSelection"));

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSApplePaySession::info());
    auto& impl = castedThis->wrapped();
    if (UNLIKELY(state.argumentCount() < 4))
        return JSValue::decode(throwVMError(&state, scope, createNotEnoughArgumentsError(&state)));

    ExceptionCode ec = 0;
    uint16_t status = convert<uint16_t>(state, state.argument(0), NormalConversion);
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    ArrayValue newShippingMethods { &state, state.argument(1) };
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    Dictionary newTotal = { &state, state.argument(2) };
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    ArrayValue newLineItems { &state, state.argument(3) };
    if (UNLIKELY(state.hadException()))
        return jsUndefined();
    impl.completeShippingContactSelection(status, newShippingMethods, newTotal, newLineItems, ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

JSValue JSApplePaySession::completePaymentMethodSelection(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = state.thisValue();
    JSApplePaySession* castedThis = jsDynamicCast<JSApplePaySession*>(thisValue);
    if (UNLIKELY(!castedThis))
        return JSValue::decode(throwThisTypeError(state, scope, "ApplePaySession", "completePaymentMethodSelection"));

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSApplePaySession::info());
    auto& impl = castedThis->wrapped();
    if (UNLIKELY(state.argumentCount() < 2))
        return JSValue::decode(throwVMError(&state, scope, createNotEnoughArgumentsError(&state)));

    ExceptionCode ec = 0;
    Dictionary newTotal = { &state, state.argument(0) };
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    ArrayValue newLineItems { &state, state.argument(1) };
    if (UNLIKELY(state.hadException()))
        return jsUndefined();
    impl.completePaymentMethodSelection(newTotal, newLineItems, ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

}

#endif
