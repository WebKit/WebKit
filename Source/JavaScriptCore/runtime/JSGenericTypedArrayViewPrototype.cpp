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

#include "JSGenericTypedArrayView.h"
#include "JSGenericTypedArrayViewConstructor.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "ObjectConstructor.h"
#include "ParseInt.h"
#include <wtf/text/Base64.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

JSC_DEFINE_HOST_FUNCTION(uint8ArrayPrototypeSetFromBase64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSUint8Array* uint8Array = jsDynamicCast<JSUint8Array*>(callFrame->thisValue());
    if (UNLIKELY(!uint8Array))
        return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromBase64 requires that |this| be a Uint8Array"_s);

    JSString* jsString = jsDynamicCast<JSString*>(callFrame->argument(0));
    if (UNLIKELY(!jsString))
        return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromBase64 requires a string"_s);

    auto alphabet = WTF::Alphabet::Base64;
    auto lastChunkHandling = WTF::LastChunkHandling::Loose;

    JSValue optionsValue = callFrame->argument(1);
    if (!optionsValue.isUndefined()) {
        JSObject* optionsObject = jsDynamicCast<JSObject*>(optionsValue);
        if (UNLIKELY(!optionsValue.isObject()))
            return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromBase64 requires that options be an object"_s);

        JSValue alphabetValue = optionsObject->get(globalObject, vm.propertyNames->alphabet);
        RETURN_IF_EXCEPTION(scope, { });
        if (!alphabetValue.isUndefined()) {
            JSString* alphabetString = jsDynamicCast<JSString*>(alphabetValue);
            if (UNLIKELY(!alphabetString))
                return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromBase64 requires that alphabet be \"base64\" or \"base64url\""_s);

            StringView alphabetStringView = alphabetString->view(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (alphabetStringView == "base64url"_s)
                alphabet = WTF::Alphabet::Base64URL;
            else if (alphabetStringView != "base64"_s)
                return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromBase64 requires that alphabet be \"base64\" or \"base64url\""_s);
        }

        JSValue lastChunkHandlingValue = optionsObject->get(globalObject, vm.propertyNames->lastChunkHandling);
        RETURN_IF_EXCEPTION(scope, { });
        if (!lastChunkHandlingValue.isUndefined()) {
            JSString* lastChunkHandlingString = jsDynamicCast<JSString*>(lastChunkHandlingValue);
            if (UNLIKELY(!lastChunkHandlingString))
                return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromBase64 requires that lastChunkHandling be \"loose\", \"strict\", or \"stop-before-partial\""_s);

            StringView lastChunkHandlingStringView = lastChunkHandlingString->view(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (lastChunkHandlingStringView == "strict"_s)
                lastChunkHandling = WTF::LastChunkHandling::Strict;
            else if (lastChunkHandlingStringView == "stop-before-partial"_s)
                lastChunkHandling = WTF::LastChunkHandling::StopBeforePartial;
            else if (lastChunkHandlingStringView != "loose"_s)
                return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromBase64 requires that lastChunkHandling be \"loose\", \"strict\", or \"stop-before-partial\""_s);
        }
    }

    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> byteLengthGetter;
    if (UNLIKELY(isIntegerIndexedObjectOutOfBounds(uint8Array, byteLengthGetter)))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    StringView view = jsString->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto [shouldThrowError, readLength, writeLength] = fromBase64(view, std::span { uint8Array->typedVector(), uint8Array->length() }, alphabet, lastChunkHandling);
    ASSERT(readLength <= view.length());
    if (UNLIKELY(shouldThrowError == WTF::FromBase64ShouldThrowError::Yes))
        return JSValue::encode(throwSyntaxError(globalObject, scope, "Uint8Array.prototype.setFromBase64 requires a valid base64 string"_s));

    JSObject* resultObject = constructEmptyObject(globalObject);
    resultObject->putDirect(vm, vm.propertyNames->read, jsNumber(readLength));
    resultObject->putDirect(vm, vm.propertyNames->written, jsNumber(writeLength));

    return JSValue::encode(resultObject);
}

JSC_DEFINE_HOST_FUNCTION(uint8ArrayPrototypeSetFromHex, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSUint8Array* uint8Array = jsDynamicCast<JSUint8Array*>(callFrame->thisValue());
    if (UNLIKELY(!uint8Array))
        return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromHex requires that |this| be a Uint8Array"_s);

    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> byteLengthGetter;
    if (UNLIKELY(isIntegerIndexedObjectOutOfBounds(uint8Array, byteLengthGetter)))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    JSString* jsString = jsDynamicCast<JSString*>(callFrame->argument(0));
    if (UNLIKELY(!jsString))
        return throwVMTypeError(globalObject, scope, "Uint8Array.prototype.setFromHex requires a string"_s);
    if (UNLIKELY(jsString->length() % 2))
        return JSValue::encode(throwSyntaxError(globalObject, scope, "Uint8Array.prototype.setFromHex requires a string of even length"_s));

    StringView view = jsString->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint8_t* data = uint8Array->typedVector();
    size_t writtenCount = std::min(static_cast<size_t>(view.length() / 2), uint8Array->length());
    size_t readCount = writtenCount * 2;
    auto result = std::span { data, data + writtenCount };

    bool success = false;
    if (view.is8Bit())
        success = decodeHex(view.span8().subspan(0, readCount), result) == WTF::notFound;
    else
        success = decodeHex(view.span16().subspan(0, readCount), result) == WTF::notFound;

    if (UNLIKELY(!success))
        return JSValue::encode(throwSyntaxError(globalObject, scope, "Uint8Array.prototype.setFromHex requires a string containing only \"0123456789abcdefABCDEF\""_s));

    JSObject* resultObject = constructEmptyObject(globalObject);
    resultObject->putDirect(vm, vm.propertyNames->read, jsNumber(readCount));
    resultObject->putDirect(vm, vm.propertyNames->written, jsNumber(writtenCount));
    return JSValue::encode(resultObject);
}

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
            RETURN_IF_EXCEPTION(scope, { });
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
    auto result = base64EncodeToStringReturnNullIfOverflow({ data, length }, options);
    if (UNLIKELY(result.isNull())) {
        throwOutOfMemoryError(globalObject, scope, "generated stirng is too long"_s);
        return { };
    }

    return JSValue::encode(jsString(vm, WTFMove(result)));
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

    const uint8_t* data = uint8Array->typedVector();
    size_t length = uint8Array->length();
    const auto* end = data + length;

    if (!length)
        return JSValue::encode(jsEmptyString(vm));

    if ((length * 2) > static_cast<size_t>(StringImpl::MaxLength)) {
        throwOutOfMemoryError(globalObject, scope, "generated stirng is too long"_s);
        return { };
    }

    std::span<LChar> buffer;
    auto result = StringImpl::createUninitialized(length * 2, buffer);
    LChar* bufferEnd = std::to_address(buffer.end());
    constexpr size_t stride = 8; // Because loading uint8x8_t.
    if (length >= stride) {
        auto encodeVector = [&](auto input) {
            // Hex conversion characters are only 16 characters. This perfectly fits in vqtbl1q_u8's table lookup.
            // Thus, this function leverages vqtbl1q_u8 to convert vector characters in a bulk manner.
            //
            // L => low nibble (4bits)
            // H => high nibble (4bits)
            //
            // original uint8x8_t : LHLHLHLHLHLHLHLH
            // widen uint16x8_t   : 00LH00LH00LH00LH00LH00LH00LH00LH
            // high               : LH00LH00LH00LH00LH00LH00LH00LH00
            // low                : 000L000L000L000L000L000L000L000L
            // merged             : LH0LLH0LLH0LLH0LLH0LLH0LLH0LLH0L
            // masked             : 0H0L0H0L0H0L0H0L0H0L0H0L0H0L0H0L
            constexpr simde_uint8x16_t characters { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
            auto widen = simde_vmovl_u8(input);
            auto high = simde_vshlq_n_u16(widen, 8);
            auto low = simde_vshrq_n_u16(widen, 4);
            auto merged = SIMD::bitOr(high, low);
            auto masked = SIMD::bitAnd(simde_vreinterpretq_u8_u16(merged), SIMD::splat<uint8_t>(0xf));
            return simde_vqtbl1q_u8(characters, masked);
        };

        const auto* cursor = data;
        auto* output = buffer.data();
        for (; cursor + (stride - 1) < end; cursor += stride, output += stride * 2)
            simde_vst1q_u8(output, encodeVector(simde_vld1_u8(cursor)));
        if (cursor < end)
            simde_vst1q_u8(bufferEnd - stride * 2, encodeVector(simde_vld1_u8(end - stride)));
    } else {
        const auto* cursor = data;
        auto* output = buffer.data();
        for (; cursor < end; cursor += 1, output += 2) {
            auto character = *cursor;
            *output = radixDigits[character / 16];
            *(output + 1) = radixDigits[character % 16];
        }
    }

    return JSValue::encode(jsNontrivialString(vm, WTFMove(result)));
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
