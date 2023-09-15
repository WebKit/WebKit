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

#if PLATFORM(IOS) || PLATFORM(VISION)
bool isPokerBros();
#endif

inline std::tuple<ASCIILiteral, JSString*> inferBuiltinTag(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

#if PLATFORM(IOS) || PLATFORM(VISION)
    static bool needsOldBuiltinTag = isPokerBros();
    if (UNLIKELY(needsOldBuiltinTag))
        return std::tuple { object->className(), nullptr };
#endif

    switch (object->type()) {
    case ArrayType:
    case DerivedArrayType:
        return std::tuple { "Array"_s, vm.smallStrings.objectArrayString() };
    case DirectArgumentsType:
    case ScopedArgumentsType:
    case ClonedArgumentsType:
        return std::tuple { "Arguments"_s, vm.smallStrings.objectArgumentsString() };
    case JSFunctionType:
    case InternalFunctionType:
        return std::tuple { "Function"_s, vm.smallStrings.objectFunctionString() };
    case ErrorInstanceType:
        return std::tuple { "Error"_s, vm.smallStrings.objectErrorString() };
    case JSDateType:
        return std::tuple { "Date"_s, vm.smallStrings.objectDateString() };
    case RegExpObjectType:
        return std::tuple { "RegExp"_s, vm.smallStrings.objectRegExpString() };
    case BooleanObjectType:
        return std::tuple { "Boolean"_s, vm.smallStrings.objectBooleanString() };
    case NumberObjectType:
        return std::tuple { "Number"_s, vm.smallStrings.objectNumberString() };
    case StringObjectType:
    case DerivedStringObjectType:
        return std::tuple { "String"_s, vm.smallStrings.objectStringString() };
    case FinalObjectType:
        return std::tuple { "Object"_s, vm.smallStrings.objectObjectString() };
    default: {
        bool objectIsArray = isArray(globalObject, object);
        RETURN_IF_EXCEPTION(scope, { });
        if (objectIsArray)
            return std::tuple { "Array"_s, vm.smallStrings.objectArrayString() };
        if (object->isCallable())
            return std::tuple { "Function"_s, vm.smallStrings.objectFunctionString() };
        return std::tuple { "Object"_s, vm.smallStrings.objectObjectString() };
    }
    }
}

ALWAYS_INLINE JSString* objectPrototypeToStringSlow(JSGlobalObject* globalObject, JSObject* thisObject)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [ tag, jsCommonTag ] = inferBuiltinTag(globalObject, thisObject);
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

    JSString* jsResult = nullptr;
    if (!jsTag) {
        if (jsCommonTag)
            jsResult = jsCommonTag;
        else
            jsTag = jsString(vm, AtomStringImpl::add(tag));
    }

    if (!jsResult) {
        jsResult = jsString(globalObject, vm.smallStrings.objectStringStart(), jsTag, vm.smallStrings.singleCharacterString(']'));
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    thisObject->structure()->cacheSpecialProperty(globalObject, vm, jsResult, CachedSpecialPropertyKey::ToStringTag, slot);
    return jsResult;
}

ALWAYS_INLINE JSString* objectPrototypeToString(JSGlobalObject* globalObject, JSValue thisValue)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (thisValue.isUndefined())
        return vm.smallStrings.objectUndefinedString();
    if (thisValue.isNull())
        return vm.smallStrings.objectNullString();

    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    Integrity::auditStructureID(thisObject->structureID());
    auto result = thisObject->structure()->cachedSpecialProperty(CachedSpecialPropertyKey::ToStringTag);
    if (result)
        return asString(result);

    RELEASE_AND_RETURN(scope, objectPrototypeToStringSlow(globalObject, thisObject));
}

inline Structure* ObjectPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC
