/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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

#include "JSPropertyNameEnumerator.h"
#include "StructureCreateInlines.h"
#include "VMInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

inline Structure* JSPropertyNameEnumerator::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

inline JSPropertyNameEnumerator* propertyNameEnumerator(JSGlobalObject* globalObject, JSObject* base)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint32_t indexedLength = base->getEnumerableLength();

    Structure* structure = base->structure();
    if (!indexedLength) {
        uintptr_t enumeratorAndFlag = structure->cachedPropertyNameEnumeratorAndFlag();
        if (enumeratorAndFlag) {
            if (!(enumeratorAndFlag & StructureRareData::cachedPropertyNameEnumeratorIsValidatedViaTraversingFlag))
                return std::bit_cast<JSPropertyNameEnumerator*>(enumeratorAndFlag);
            structure->prototypeChain(vm, globalObject, base); // Refresh cached structure chain.
            if (auto* enumerator = structure->cachedPropertyNameEnumerator())
                return enumerator;
        }
    }

    uint32_t numberStructureProperties = 0;
    PropertyNameArrayBuilder propertyNames(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    getEnumerablePropertyNames(globalObject, base, propertyNames, indexedLength, numberStructureProperties);
    RETURN_IF_EXCEPTION(scope, nullptr);

    ASSERT(propertyNames.size() < UINT32_MAX);

    bool sawPolyProto;
    bool successfullyNormalizedChain = normalizePrototypeChain(globalObject, base, sawPolyProto) != InvalidPrototypeChain;

    Structure* structureAfterGettingPropertyNames = base->structure();
    if (!structureAfterGettingPropertyNames->canAccessPropertiesQuicklyForEnumeration()) {
        indexedLength = 0;
        numberStructureProperties = 0;
    }

    JSPropertyNameEnumerator* enumerator = nullptr;
    if (!indexedLength && !propertyNames.size())
        enumerator = vm.emptyPropertyNameEnumerator();
    else {
        enumerator = JSPropertyNameEnumerator::tryCreate(vm, structureAfterGettingPropertyNames, indexedLength, numberStructureProperties, WTF::move(propertyNames));
        if (!enumerator) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }
    }
    if (!indexedLength && successfullyNormalizedChain && structureAfterGettingPropertyNames == structure) {
        StructureChain* chain = structure->prototypeChain(vm, globalObject, base);
        if (structure->canCachePropertyNameEnumerator(vm))
            structure->setCachedPropertyNameEnumerator(vm, enumerator, chain);
    }
    return enumerator;
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
