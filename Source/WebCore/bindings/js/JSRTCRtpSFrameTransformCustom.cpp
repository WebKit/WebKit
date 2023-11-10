/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "JSRTCRtpSFrameTransform.h"

#if ENABLE(WEB_RTC)

#include "JSCryptoKey.h"
#include "JSDOMPromiseDeferred.h"
#include <JavaScriptCore/JSBigInt.h>

namespace WebCore {
using namespace JSC;

JSValue JSRTCRtpSFrameTransform::setEncryptionKey(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame, Ref<DeferredPromise>&& promise)
{
    auto& vm = getVM(&lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(callFrame.argumentCount() < 1)) {
        throwVMError(&lexicalGlobalObject, throwScope, createNotEnoughArgumentsError(&lexicalGlobalObject));
        return jsUndefined();
    }

    EnsureStillAliveScope argument0 = callFrame.uncheckedArgument(0);
    auto key = convert<IDLInterface<CryptoKey>>(lexicalGlobalObject, argument0.value(), [](auto& lexicalGlobalObject, auto& scope) {
        throwArgumentTypeError(lexicalGlobalObject, scope, 0, "key", "SFrameTransform", "setEncryptionKey", "CryptoKey");
    });
    RETURN_IF_EXCEPTION(throwScope, jsUndefined());

    EnsureStillAliveScope argument1 = callFrame.argument(1);
    std::optional<uint64_t> keyID;
    if (!argument1.value().isUndefined()) {
        if (argument1.value().isBigInt()) {
            if (argument1.value().asHeapBigInt()->length() > 1) {
                throwException(&lexicalGlobalObject, throwScope, createDOMException(&lexicalGlobalObject, ExceptionCode::RangeError, "Not a 64 bits integer"_s));
                return jsUndefined();
            }
            keyID = JSBigInt::toBigUInt64(argument1.value());
        } else
            keyID = std::optional<Converter<IDLUnsignedLongLong>::ReturnType>(convert<IDLUnsignedLongLong>(lexicalGlobalObject, argument1.value()));
    }
    RETURN_IF_EXCEPTION(throwScope, jsUndefined());
    throwScope.release();

    wrapped().setEncryptionKey(*key, keyID, WTFMove(promise));
    return jsUndefined();
}

}

#endif
