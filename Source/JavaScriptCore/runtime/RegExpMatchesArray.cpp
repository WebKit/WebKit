/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "RegExpMatchesArray.h"

namespace JSC {

JSArray* createEmptyRegExpMatchesArray(JSGlobalObject* globalObject, JSString* input, RegExp* regExp)
{
    VM& vm = globalObject->vm();
    JSArray* array;

    // FIXME: This should handle array allocation errors gracefully.
    // https://bugs.webkit.org/show_bug.cgi?id=155144
    
    GCDeferralContext deferralContext(vm.heap);
    ObjectInitializationScope scope(vm);

    if (UNLIKELY(globalObject->isHavingABadTime())) {
        array = JSArray::tryCreateUninitializedRestricted(scope, &deferralContext,
            regExp->hasIndices() ? globalObject->regExpMatchesArrayWithIndicesStructure() : globalObject->regExpMatchesArrayStructure(), regExp->numSubpatterns() + 1);
        // FIXME: we should probably throw an out of memory error here, but
        // when making this change we should check that all clients of this
        // function will correctly handle an exception being thrown from here.
        // https://bugs.webkit.org/show_bug.cgi?id=169786
        RELEASE_ASSERT(array);

        array->initializeIndexWithoutBarrier(scope, 0, jsEmptyString(vm));
        
        if (unsigned numSubpatterns = regExp->numSubpatterns()) {
            for (unsigned i = 1; i <= numSubpatterns; ++i)
                array->initializeIndexWithoutBarrier(scope, i, jsUndefined());
        }
    } else {
        array = tryCreateUninitializedRegExpMatchesArray(scope, &deferralContext,
            regExp->hasIndices() ? globalObject->regExpMatchesArrayWithIndicesStructure() : globalObject->regExpMatchesArrayStructure(), regExp->numSubpatterns() + 1);
        RELEASE_ASSERT(array);
        
        array->initializeIndexWithoutBarrier(scope, 0, jsEmptyString(vm), ArrayWithContiguous);
        
        if (unsigned numSubpatterns = regExp->numSubpatterns()) {
            for (unsigned i = 1; i <= numSubpatterns; ++i)
                array->initializeIndexWithoutBarrier(scope, i, jsUndefined(), ArrayWithContiguous);
        }
    }

    array->putDirectWithoutBarrier(RegExpMatchesArrayIndexPropertyOffset, jsNumber(-1));
    array->putDirectWithoutBarrier(RegExpMatchesArrayInputPropertyOffset, input);
    array->putDirectWithoutBarrier(RegExpMatchesArrayGroupsPropertyOffset, jsUndefined());
    if (regExp->hasIndices())
        array->putDirectWithoutBarrier(RegExpMatchesArrayIndicesPropertyOffset, jsUndefined());
    return array;
}

static Structure* createStructureImpl(VM& vm, JSGlobalObject* globalObject, IndexingType indexingType)
{
    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->index, 0, offset);
    ASSERT(offset == RegExpMatchesArrayIndexPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->input, 0, offset);
    ASSERT(offset == RegExpMatchesArrayInputPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->groups, 0, offset);
    ASSERT(offset == RegExpMatchesArrayGroupsPropertyOffset);
    return structure;
}

static Structure* createStructureWithIndicesImpl(VM& vm, JSGlobalObject* globalObject, IndexingType indexingType)
{
    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->index, 0, offset);
    ASSERT(offset == RegExpMatchesArrayIndexPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->input, 0, offset);
    ASSERT(offset == RegExpMatchesArrayInputPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->groups, 0, offset);
    ASSERT(offset == RegExpMatchesArrayGroupsPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->indices, 0, offset);
    ASSERT(offset == RegExpMatchesArrayIndicesPropertyOffset);
    return structure;
}

static Structure* createIndicesStructureImpl(VM& vm, JSGlobalObject* globalObject, IndexingType indexingType)
{
    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->groups, 0, offset);
    ASSERT(offset == RegExpMatchesIndicesGroupsPropertyOffset);
    return structure;
}

Structure* createRegExpMatchesArrayStructure(VM& vm, JSGlobalObject* globalObject)
{
    return createStructureImpl(vm, globalObject, ArrayWithContiguous);
}

Structure* createRegExpMatchesArrayWithIndicesStructure(VM& vm, JSGlobalObject* globalObject)
{
    return createStructureWithIndicesImpl(vm, globalObject, ArrayWithContiguous);
}

Structure* createRegExpMatchesIndicesArrayStructure(VM& vm, JSGlobalObject* globalObject)
{
    return createIndicesStructureImpl(vm, globalObject, ArrayWithContiguous);
}

Structure* createRegExpMatchesArraySlowPutStructure(VM& vm, JSGlobalObject* globalObject)
{
    return createStructureImpl(vm, globalObject, ArrayWithSlowPutArrayStorage);
}

Structure* createRegExpMatchesArrayWithIndicesSlowPutStructure(VM& vm, JSGlobalObject* globalObject)
{
    return createStructureWithIndicesImpl(vm, globalObject, ArrayWithSlowPutArrayStorage);
}

Structure* createRegExpMatchesIndicesArraySlowPutStructure(VM& vm, JSGlobalObject* globalObject)
{
    return createIndicesStructureImpl(vm, globalObject, ArrayWithSlowPutArrayStorage);
}

} // namespace JSC
