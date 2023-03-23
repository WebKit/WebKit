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

#include "ArrayPrototypeInlines.h"
#include "IntegrityInlines.h"
#include "ObjectPrototype.h"

namespace JSC {

ALWAYS_INLINE JSString* objectPrototypeToString(JSGlobalObject* globalObject, JSValue thisValue)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (thisValue.isUndefined())
        return vm.smallStrings.objectUndefinedString();
    if (thisValue.isNull())
        return vm.smallStrings.objectNullString();

    // BigIntPrototype and SymbolPrototype have @@toStringTag.
    JSString* tag = nullptr;
    JSObject* startingPoint = nullptr;
    if (thisValue.isNumber()) {
        tag = vm.smallStrings.objectNumberString();
        startingPoint = globalObject->numberPrototype();
    } else if (thisValue.isString()) {
        tag = vm.smallStrings.objectStringString();
        startingPoint = globalObject->stringPrototype();
    } else if (thisValue.isBoolean()) {
        tag = vm.smallStrings.objectBooleanString();
        startingPoint = globalObject->booleanPrototype();
    } else if (thisValue.isObject()) {
        startingPoint = asObject(thisValue);
        switch (startingPoint->type()) {
        case ArrayType:
        case DerivedArrayType:
            tag = vm.smallStrings.objectArrayString();
            break;
        case DirectArgumentsType:
        case ScopedArgumentsType:
        case ClonedArgumentsType:
            tag = vm.smallStrings.objectArgumentsString();
            break;
        case JSFunctionType:
        case InternalFunctionType:
            tag = vm.smallStrings.objectFunctionString();
            break;
        case ErrorInstanceType:
            tag = vm.smallStrings.objectErrorString();
            break;
        case JSDateType:
            tag = vm.smallStrings.objectDateString();
            break;
        case RegExpObjectType:
            tag = vm.smallStrings.objectRegExpString();
            break;
        case BooleanObjectType:
            tag = vm.smallStrings.objectBooleanString();
            break;
        case NumberObjectType:
            tag = vm.smallStrings.objectNumberString();
            break;
        case StringObjectType:
        case DerivedStringObjectType:
            tag = vm.smallStrings.objectStringString();
            break;
        case FinalObjectType:
            tag = vm.smallStrings.objectObjectString();
            break;
        default:
            break;
        }
    }

    if (tag) {
        if (LIKELY(!startingPoint->mayHaveInterestingSymbols()))
            return tag;
    }

    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto result = thisObject->structure()->cachedSpecialProperty(CachedSpecialPropertyKey::ToStringTag);
    if (result)
        return asString(result);
    RELEASE_AND_RETURN(scope, objectPrototypeToStringSlow(globalObject, thisObject));
}

} // namespace JSC
