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
#include "AggregateErrorPrototype.h"

#include "CallFrame.h"
#include "Error.h"
#include "JSCell.h"
#include "JSObject.h"
#include "ThrowScope.h"
#include <wtf/Locker.h>

namespace JSC {

static EncodedJSValue JSC_HOST_CALL aggregateErrorPrototypeAccessorErrors(JSGlobalObject*, CallFrame*);

AggregateErrorPrototype::AggregateErrorPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void AggregateErrorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm, errorTypeName(ErrorType::AggregateError));
    ASSERT(inherits(vm, info()));

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("errors", aggregateErrorPrototypeAccessorErrors, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
}

EncodedJSValue JSC_HOST_CALL aggregateErrorPrototypeAccessorErrors(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* aggregateError = jsDynamicCast<AggregateError*>(vm, thisValue);
    if (!aggregateError)
        return throwVMTypeError(globalObject, scope, "The AggregateError.prototype.errors getter can only be called on a AggregateError object"_s);

    auto* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned index = 0;
    for (const auto& error : aggregateError->errors()) {
        result->putDirectIndex(globalObject, index++, error.get());
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(result);
}

} // namespace JSC
