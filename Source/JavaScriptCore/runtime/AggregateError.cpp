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
#include "AggregateError.h"

#include "ExceptionScope.h"
#include "IteratorOperations.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"

namespace JSC {

ErrorInstance* createAggregateError(JSGlobalObject* globalObject, VM& vm, Structure* structure, JSValue errors, JSValue message, JSValue options, ErrorInstance::SourceAppender appender, RuntimeType type, bool useCurrentFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    String messageString = message.isUndefined() ? String() : message.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSValue cause;
    if (options.isObject()) {
        // Since `throw undefined;` is valid, we need to distinguish the case where `cause` is an explicit undefined.
        cause = asObject(options)->getIfPropertyExists(globalObject, vm.propertyNames->cause);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    MarkedArgumentBuffer errorsList;
    forEachInIterable(globalObject, errors, [&] (VM&, JSGlobalObject*, JSValue nextValue) {
        errorsList.append(nextValue);
        if (UNLIKELY(errorsList.hasOverflowed()))
            throwOutOfMemoryError(globalObject, scope);
    });
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* array = constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), errorsList);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* error = ErrorInstance::create(vm, structure, messageString, cause, appender, type, ErrorType::AggregateError, useCurrentFrame);
    error->putDirect(vm, vm.propertyNames->errors, array, static_cast<unsigned>(PropertyAttribute::DontEnum));
    return error;
}

} // namespace JSC
