/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "ObjectConstructor.h"

namespace JSC {

inline Structure* ObjectConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

ALWAYS_INLINE bool canPerformFastPropertyNameEnumerationForJSONStringifyWithSideEffect(Structure* structure)
{
    // We do not check GetterSetter, CustomGetterSetter. GetterSetter and CustomGetterSetter will be invoked, and we fall back to the a bit more generic mode when we detect structure transition.
    if (structure->typeInfo().overridesGetOwnPropertySlot())
        return false;
    if (structure->typeInfo().overridesAnyFormOfGetOwnPropertyNames())
        return false;
    if (hasIndexedProperties(structure->indexingType()))
        return false;
    if (structure->isUncacheableDictionary())
        return false;
    if (structure->hasNonReifiedStaticProperties())
        return false;
    return true;
}

ALWAYS_INLINE void objectAssignIndexedPropertiesFast(JSGlobalObject* globalObject, JSObject* target, JSObject* source)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    source->forEachIndexedProperty(globalObject, [&](unsigned index, JSValue value) {
        target->putDirectIndex(globalObject, index, value);
        RETURN_IF_EXCEPTION(scope, IterationStatus::Done);
        return IterationStatus::Continue;
    });
    RETURN_IF_EXCEPTION(scope, void());
}

ALWAYS_INLINE bool objectAssignFast(JSGlobalObject* globalObject, JSObject* target, JSObject* source, Vector<RefPtr<UniquedStringImpl>, 8>& properties, MarkedArgumentBuffer& values)
{
    // |source| Structure does not have any getters. And target can perform fast put.
    // So enumerating properties and putting properties are non observable.

    // FIXME: It doesn't seem like we should have to do this in two phases, but
    // we're running into crashes where it appears that source is transitioning
    // under us, and even ends up in a state where it has a null butterfly. My
    // leading hypothesis here is that we fire some value replacement watchpoint
    // that ends up transitioning the structure underneath us.
    // https://bugs.webkit.org/show_bug.cgi?id=187837

    // FIXME: This fast path is very similar to ObjectConstructor' one. But extracting it to a function caused performance
    // regression in object-assign-replace. Since the code is small and fast path, we keep both.

    // Do not clear since Vector::clear shrinks the backing store.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    properties.resize(0);
    values.clear();
    bool canUseFastPath = source->fastForEachPropertyWithSideEffectFreeFunctor(vm, [&](const PropertyTableEntry& entry) -> bool {
        if (entry.attributes() & PropertyAttribute::DontEnum)
            return true;

        PropertyName propertyName(entry.key());
        if (propertyName.isPrivateName())
            return true;

        properties.append(entry.key());
        values.appendWithCrashOnOverflow(source->getDirect(entry.offset()));

        return true;
    });
    if (!canUseFastPath)
        return false;

    if (source->canHaveExistingOwnIndexedProperties()) {
        objectAssignIndexedPropertiesFast(globalObject, target, source);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Actually, assigning with empty object (option for example) is common. (`Object.assign(defaultOptions, passedOptions)` where `passedOptions` is empty object.)
    if (properties.size())
        target->putOwnDataPropertyBatching(vm, properties.data(), values.data(), properties.size());
    return true;
}


} // namespace JSC
