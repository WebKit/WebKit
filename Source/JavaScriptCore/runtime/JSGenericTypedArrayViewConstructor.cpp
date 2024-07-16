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
#include "JSGenericTypedArrayViewConstructor.h"

#include "ParseInt.h"
#include <wtf/text/Base64.h>

namespace JSC {

JSC_DEFINE_HOST_FUNCTION(uint8ArrayConstructorFromBase64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* jsString = jsDynamicCast<JSString*>(callFrame->argument(0));
    if (UNLIKELY(!jsString))
        return throwVMTypeError(globalObject, scope, "Uint8Array.fromBase64 requires a string"_s);

    auto alphabet = Alphabet::Base64;
    auto lastChunkHandling = LastChunkHandling::Loose;

    JSValue optionsValue = callFrame->argument(1);
    if (!optionsValue.isUndefined()) {
        JSObject* optionsObject = jsDynamicCast<JSObject*>(optionsValue);
        if (UNLIKELY(!optionsValue.isObject()))
            return throwVMTypeError(globalObject, scope, "Uint8Array.fromBase64 requires that options be an object"_s);

        JSValue alphabetValue = optionsObject->get(globalObject, vm.propertyNames->alphabet);
        RETURN_IF_EXCEPTION(scope, { });
        if (!alphabetValue.isUndefined()) {
            JSString* alphabetString = jsDynamicCast<JSString*>(alphabetValue);
            if (UNLIKELY(!alphabetString))
                return throwVMTypeError(globalObject, scope, "Uint8Array.fromBase64 requires that alphabet be \"base64\" or \"base64url\""_s);

            StringView alphabetStringView = alphabetString->view(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (alphabetStringView == "base64url"_s)
                alphabet = Alphabet::Base64URL;
            else if (alphabetStringView != "base64"_s)
                return throwVMTypeError(globalObject, scope, "Uint8Array.fromBase64 requires that alphabet be \"base64\" or \"base64url\""_s);
        }

        JSValue lastChunkHandlingValue = optionsObject->get(globalObject, vm.propertyNames->lastChunkHandling);
        RETURN_IF_EXCEPTION(scope, { });
        if (!lastChunkHandlingValue.isUndefined()) {
            JSString* lastChunkHandlingString = jsDynamicCast<JSString*>(lastChunkHandlingValue);
            if (UNLIKELY(!lastChunkHandlingString))
                return throwVMTypeError(globalObject, scope, "Uint8Array.fromBase64 requires that lastChunkHandling be \"loose\", \"strict\", or \"stop-before-partial\""_s);

            StringView lastChunkHandlingStringView = lastChunkHandlingString->view(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (lastChunkHandlingStringView == "strict"_s)
                lastChunkHandling = LastChunkHandling::Strict;
            else if (lastChunkHandlingStringView == "stop-before-partial"_s)
                lastChunkHandling = LastChunkHandling::StopBeforePartial;
            else if (lastChunkHandlingStringView != "loose"_s)
                return throwVMTypeError(globalObject, scope, "Uint8Array.fromBase64 requires that lastChunkHandling be \"loose\", \"strict\", or \"stop-before-partial\""_s);
        }
    }

    StringView view = jsString->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = fromBase64(view, std::numeric_limits<size_t>::max(), alphabet, lastChunkHandling);
    if (!result)
        return JSValue::encode(throwSyntaxError(globalObject, scope, "Uint8Array.fromBase64 requires a valid base64 string"_s));

    ASSERT(result->first <= view.length());

    Structure* structure = globalObject->typedArrayStructure(TypeUint8, false);
    JSUint8Array* uint8Array = JSUint8Array::create(vm, structure, Uint8Array::create(result->second.span()));
    return JSValue::encode(uint8Array);
}

JSC_DEFINE_HOST_FUNCTION(uint8ArrayConstructorFromHex, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* jsString = jsDynamicCast<JSString*>(callFrame->argument(0));
    if (UNLIKELY(!jsString))
        return throwVMTypeError(globalObject, scope, "Uint8Array.fromHex requires a string"_s);
    if (UNLIKELY(jsString->length() % 2))
        return JSValue::encode(throwSyntaxError(globalObject, scope, "Uint8Array.fromHex requires a string of even length"_s));

    StringView view = jsString->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t count = static_cast<size_t>(view.length() / 2);
    for (size_t i = 0; i < count * 2; ++i) {
        int digit = parseDigit(view[i], 16);
        if (UNLIKELY(digit == -1))
            return JSValue::encode(throwSyntaxError(globalObject, scope, "Uint8Array.prototype.fromHex requires a string containing only \"0123456789abcdefABCDEF\""_s));
    }

    JSUint8Array* uint8Array = JSUint8Array::create(globalObject, globalObject->typedArrayStructure(TypeUint8, false), count);
    uint8_t* data = uint8Array->typedVector();

    size_t read = 0;
    size_t written = 0;
    for (size_t i = 0; i < count; ++i) {
        int tens = parseDigit(view[read++], 16);
        int ones = parseDigit(view[read++], 16);
        data[written++] = (tens * 16) + ones;
    }
    ASSERT(read == count * 2);
    ASSERT(written == count);

    return JSValue::encode(uint8Array);
}

} // namespace JSC
