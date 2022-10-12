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

#if PLATFORM(IOS)
bool isPokerBros();
#endif

inline ASCIILiteral inferBuiltinTag(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = getVM(globalObject);
#if PLATFORM(IOS)
    static bool needsOldBuiltinTag = isPokerBros();
    if (UNLIKELY(needsOldBuiltinTag))
        return object->className();
#endif
    auto scope = DECLARE_THROW_SCOPE(vm);
    bool objectIsArray = isArray(globalObject, object);
    RETURN_IF_EXCEPTION(scope, { });
    if (objectIsArray)
        return "Array"_s;
    if (object->isCallable())
        return "Function"_s;
    JSType type = object->type();
    if (TypeInfo::isArgumentsType(type)
        || type == ErrorInstanceType
        || type == BooleanObjectType
        || type == NumberObjectType
        || type == StringObjectType
        || type == DerivedStringObjectType
        || type == JSDateType
        || type == RegExpObjectType)
        return object->className();
    return "Object"_s;
}

ALWAYS_INLINE JSString* objectPrototypeToStringSlow(JSGlobalObject* globalObject, JSObject* thisObject)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASCIILiteral tag = inferBuiltinTag(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSString* jsTag = nullptr;

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::Get);
    bool hasProperty = thisObject->getPropertySlot(globalObject, vm.propertyNames->toStringTagSymbol, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue tagValue = slot.getValue(globalObject, vm.propertyNames->toStringTagSymbol);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (tagValue.isString())
            jsTag = asString(tagValue);
    }

    if (!jsTag)
        jsTag = jsString(vm, AtomStringImpl::add(tag));

    JSString* jsResult = jsString(globalObject, vm.smallStrings.objectStringStart(), jsTag, vm.smallStrings.singleCharacterString(']'));
    RETURN_IF_EXCEPTION(scope, nullptr);
    thisObject->structure()->cacheSpecialProperty(globalObject, vm, jsResult, CachedSpecialPropertyKey::ToStringTag, slot);
    return jsResult;
}

ALWAYS_INLINE JSString* objectPrototypeToString(JSGlobalObject* globalObject, JSValue thisValue)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (thisValue.isUndefined())
        return vm.smallStrings.undefinedObjectString();
    if (thisValue.isNull())
        return vm.smallStrings.nullObjectString();

    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    Integrity::auditStructureID(thisObject->structureID());
    auto result = thisObject->structure()->cachedSpecialProperty(CachedSpecialPropertyKey::ToStringTag);
    if (result)
        return asString(result);

    RELEASE_AND_RETURN(scope, objectPrototypeToStringSlow(globalObject, thisObject));
}

} // namespace JSC
