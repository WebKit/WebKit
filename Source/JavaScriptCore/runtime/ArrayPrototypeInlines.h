/*
 * Copyright (C) 2022 Apple, Inc. All rights reserved.
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

#pragma once

#include "ArrayConstructor.h"
#include "ArrayPrototype.h"
#include "ExceptionHelpers.h"
#include "GetVM.h"

namespace JSC {

enum class SpeciesConstructResult : uint8_t {
    FastPath,
    Exception,
    CreatedObject
};

ALWAYS_INLINE bool arraySpeciesWatchpointIsValid(JSObject* thisObject)
{
    JSGlobalObject* globalObject = thisObject->globalObject();
    ArrayPrototype* arrayPrototype = globalObject->arrayPrototype();

    ASSERT(globalObject->arraySpeciesWatchpointSet().state() != ClearWatchpoint);

    return !thisObject->hasCustomProperties()
        && arrayPrototype == thisObject->getPrototypeDirect()
        && globalObject->arraySpeciesWatchpointSet().state() == IsWatched;
}

ALWAYS_INLINE std::pair<SpeciesConstructResult, JSObject*> speciesConstructArray(JSGlobalObject* globalObject, JSObject* thisObject, uint64_t length)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    constexpr std::pair<SpeciesConstructResult, JSObject*> exceptionResult { SpeciesConstructResult::Exception, nullptr };

    // ECMA 9.4.2.3: https://tc39.github.io/ecma262/#sec-arrayspeciescreate
    JSValue constructor = jsUndefined();
    bool thisIsArray = isArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, exceptionResult);
    if (LIKELY(thisIsArray)) {
        // Fast path in the normal case where the user has not set an own constructor and the Array.prototype.constructor is normal.
        // We need prototype check for subclasses of Array, which are Array objects but have a different prototype by default.
        bool isValid = arraySpeciesWatchpointIsValid(thisObject);
        RETURN_IF_EXCEPTION(scope, exceptionResult);
        if (LIKELY(isValid))
            return std::pair { SpeciesConstructResult::FastPath, nullptr };

        constructor = thisObject->get(globalObject, vm.propertyNames->constructor);
        RETURN_IF_EXCEPTION(scope, exceptionResult);
        if (constructor.isConstructor()) {
            JSObject* constructorObject = jsCast<JSObject*>(constructor);
            bool isArrayConstructorFromAnotherRealm = globalObject != constructorObject->globalObject()
                && constructorObject->inherits<ArrayConstructor>();
            if (isArrayConstructorFromAnotherRealm)
                return std::pair { SpeciesConstructResult::FastPath, nullptr };
        }
        if (constructor.isObject()) {
            constructor = constructor.get(globalObject, vm.propertyNames->speciesSymbol);
            RETURN_IF_EXCEPTION(scope, exceptionResult);
            if (constructor.isNull())
                return std::pair { SpeciesConstructResult::FastPath, nullptr };
        }
    } else {
        // If isArray is false, return ? ArrayCreate(length).
        return std::pair { SpeciesConstructResult::FastPath, nullptr };
    }

    if (constructor.isUndefined())
        return std::pair { SpeciesConstructResult::FastPath, nullptr };

    MarkedArgumentBuffer args;
    args.append(jsNumber(length));
    ASSERT(!args.hasOverflowed());
    JSObject* newObject = construct(globalObject, constructor, args, "Species construction did not get a valid constructor"_s);
    RETURN_IF_EXCEPTION(scope, exceptionResult);
    return std::pair { SpeciesConstructResult::CreatedObject, newObject };
}

} // namespace JSC
