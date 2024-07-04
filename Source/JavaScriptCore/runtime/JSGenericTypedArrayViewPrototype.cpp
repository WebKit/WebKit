/*
 * Copyright (C) 2024 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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
#include "JSGenericTypedArrayViewPrototype.h"

#include "ParseInt.h"
#include <wtf/text/Base64.h>

namespace JSC {

JSC_DEFINE_HOST_FUNCTION(uint8ArrayPrototypeToBase64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSUint8Array* uint8Array = jsDynamicCast<JSUint8Array*>(callFrame->thisValue());
    if (UNLIKELY(!uint8Array))
        return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.toBase64 requires that |this| be a Uint8Array"_s);

    OptionSet<Base64EncodeOption> options;

    JSValue optionsValue = callFrame->argument(0);
    if (!optionsValue.isUndefined()) {
        JSObject* optionsObject = jsDynamicCast<JSObject*>(optionsValue);
        if (UNLIKELY(!optionsObject))
            return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.toBase64 requires that options be an object"_s);

        JSValue alphabetValue = optionsObject->get(globalObject, vm.propertyNames->alphabet);
        RETURN_IF_EXCEPTION(scope, { });
        if (!alphabetValue.isUndefined()) {
            JSString* alphabetString = jsDynamicCast<JSString*>(alphabetValue);
            if (UNLIKELY(!alphabetString))
                return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.toBase64 requires that alphabet be \"base64\" or \"base64url\""_s);

            StringView alphabetStringView = alphabetString->view(globalObject);
            if (alphabetStringView == "base64url"_s)
                options.add(Base64EncodeOption::URL);
            else if (alphabetStringView != "base64"_s)
                return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.toBase64 requires that alphabet be \"base64\" or \"base64url\""_s);
        }

        JSValue omitPaddingValue = optionsObject->get(globalObject, vm.propertyNames->omitPadding);
        RETURN_IF_EXCEPTION(scope, { });
        if (omitPaddingValue.toBoolean(globalObject))
            options.add(Base64EncodeOption::OmitPadding);
    }

    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> byteLengthGetter;
    if (UNLIKELY(isIntegerIndexedObjectOutOfBounds(uint8Array, byteLengthGetter)))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    const uint8_t* data = uint8Array->typedVector();
    size_t length = uint8Array->length();
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, base64EncodeToString({ data, length }, options))));
}

JSC_DEFINE_HOST_FUNCTION(uint8ArrayPrototypeToHex, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSUint8Array* uint8Array = jsDynamicCast<JSUint8Array*>(callFrame->thisValue());
    if (UNLIKELY(!uint8Array))
        return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.toHex requires that |this| be a Uint8Array"_s);

    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> byteLengthGetter;
    if (UNLIKELY(isIntegerIndexedObjectOutOfBounds(uint8Array, byteLengthGetter)))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    StringBuilder builder;
    builder.reserveCapacity(uint8Array->length() * 2);

    const uint8_t* data = uint8Array->typedVector();
    size_t length = uint8Array->length();
    for (size_t i = 0; i < length; ++i) {
        builder.append(radixDigits[data[i] / 16]);
        builder.append(radixDigits[data[i] % 16]);
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, builder.toString())));
}

} // namespace JSC
