/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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

#include "ButterflyInlines.h"
#include "JSCInlines.h"

namespace JSC {

static const PropertyOffset indexPropertyOffset = 100;
static const PropertyOffset inputPropertyOffset = 101;

static JSArray* tryCreateUninitializedRegExpMatchesArray(VM& vm, Structure* structure, unsigned initialLength)
{
    unsigned vectorLength = std::max(BASE_VECTOR_LEN, initialLength);
    if (vectorLength > MAX_STORAGE_VECTOR_LENGTH)
        return 0;

    void* temp;
    if (!vm.heap.tryAllocateStorage(0, Butterfly::totalSize(0, structure->outOfLineCapacity(), true, vectorLength * sizeof(EncodedJSValue)), &temp))
        return 0;
    Butterfly* butterfly = Butterfly::fromBase(temp, 0, structure->outOfLineCapacity());
    butterfly->setVectorLength(vectorLength);
    butterfly->setPublicLength(initialLength);

    return JSArray::createWithButterfly(vm, structure, butterfly);
}

JSArray* createRegExpMatchesArray(ExecState* exec, JSString* input, RegExp* regExp, MatchResult result)
{
    ASSERT(result);
    VM& vm = exec->vm();
    JSArray* array = tryCreateUninitializedRegExpMatchesArray(vm, exec->lexicalGlobalObject()->regExpMatchesArrayStructure(), regExp->numSubpatterns() + 1);
    RELEASE_ASSERT(array);

    SamplingRegion samplingRegion("Reifying substring properties");

    array->initializeIndex(vm, 0, jsSubstring(exec, input, result.start, result.end - result.start), ArrayWithContiguous);

    if (unsigned numSubpatterns = regExp->numSubpatterns()) {
        Vector<int, 32> subpatternResults;
        int position = regExp->match(vm, input->value(exec), result.start, subpatternResults);
        ASSERT_UNUSED(position, position >= 0 && static_cast<size_t>(position) == result.start);
        ASSERT(result.start == static_cast<size_t>(subpatternResults[0]));
        ASSERT(result.end == static_cast<size_t>(subpatternResults[1]));

        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            int start = subpatternResults[2 * i];
            if (start >= 0)
                array->initializeIndex(vm, i, jsSubstring(exec, input, start, subpatternResults[2 * i + 1] - start), ArrayWithContiguous);
            else
                array->initializeIndex(vm, i, jsUndefined(), ArrayWithContiguous);
        }
    }

    array->putDirect(vm, indexPropertyOffset, jsNumber(result.start));
    array->putDirect(vm, inputPropertyOffset, input);

    return array;
}

Structure* createRegExpMatchesArrayStructure(VM& vm, JSGlobalObject& globalObject)
{
    Structure* structure = globalObject.arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);
    PropertyOffset offset;
    structure = structure->addPropertyTransition(vm, structure, vm.propertyNames->index, 0, offset);
    ASSERT(offset == indexPropertyOffset);
    structure = structure->addPropertyTransition(vm, structure, vm.propertyNames->input, 0, offset);
    ASSERT(offset == inputPropertyOffset);
    return structure;
}

} // namespace JSC
