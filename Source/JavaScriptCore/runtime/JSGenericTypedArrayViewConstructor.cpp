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

    size_t count = static_cast<size_t>(view.length() / 2);
    for (size_t i = 0; i < count * 2; ++i) {
        int digit = parseDigit(view[i], 16);
        if (UNLIKELY(digit == -1))
            return JSValue::encode(throwSyntaxError(globalObject, scope, "Uint8Array.prototype.setFromHex requires a string containing only \"0123456789abcdefABCDEF\""_s));
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
