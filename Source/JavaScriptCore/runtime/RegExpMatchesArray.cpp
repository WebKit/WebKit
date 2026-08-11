/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
    
    GCDeferralContext deferralContext(vm);
    ObjectInitializationScope scope(vm);

    if (globalObject->isHavingABadTime()) [[unlikely]] {
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

JSArray* createRegExpMatchesArrayWithGroupsOrIndices(VM& vm, JSGlobalObject* globalObject, JSString* input, RegExp* regExp, const MatchResult& result, std::span<const int> subpatternResults, unsigned numSubpatterns, bool hasNamedCaptures, bool createIndices)
{
    JSArray* array;
    JSArray* indicesArray = nullptr;

    // FIXME: This should handle array allocation errors gracefully.
    // https://bugs.webkit.org/show_bug.cgi?id=155144

    Structure* groupsStructure = nullptr;
    JSObject* groups = nullptr;
    JSObject* indicesGroups = nullptr;
    if (hasNamedCaptures) {
        groupsStructure = regExp->ensureGroupsStructure(vm, globalObject);
        Structure* structureForGroups = groupsStructure ? groupsStructure : globalObject->nullPrototypeObjectStructure();
        groups = constructEmptyObject(vm, structureForGroups);
        if (createIndices)
            indicesGroups = constructEmptyObject(vm, structureForGroups);
    }
    Structure* matchStructure = createIndices ? globalObject->regExpMatchesArrayWithIndicesStructure() : globalObject->regExpMatchesArrayStructure();

    auto setProperties = [&] () {
        array->putDirectOffset(vm, RegExpMatchesArrayIndexPropertyOffset, jsNumber(result.start));
        array->putDirectOffset(vm, RegExpMatchesArrayInputPropertyOffset, input);
        array->putDirectOffset(vm, RegExpMatchesArrayGroupsPropertyOffset, hasNamedCaptures ? groups : jsUndefined());

        ASSERT(!array->butterfly()->indexingHeader()->preCapacity(matchStructure));
        auto capacity = matchStructure->outOfLineCapacity();
        auto size = matchStructure->outOfLineSize();
        gcSafeZeroMemory(static_cast<JSValue*>(array->butterfly()->base(0, capacity)), (capacity - size) * sizeof(JSValue));

        if (createIndices) {
            array->putDirectOffset(vm, RegExpMatchesArrayIndicesPropertyOffset, indicesArray);

            Structure* indicesStructure = globalObject->regExpMatchesIndicesArrayStructure();

            indicesArray->putDirectOffset(vm, RegExpMatchesIndicesGroupsPropertyOffset, indicesGroups ? indicesGroups : jsUndefined());

            ASSERT(!indicesArray->butterfly()->indexingHeader()->preCapacity(indicesStructure));
            auto indicesCapacity = indicesStructure->outOfLineCapacity();
            auto indicesSize = indicesStructure->outOfLineSize();
            gcSafeZeroMemory(static_cast<JSValue*>(indicesArray->butterfly()->base(0, indicesCapacity)), (indicesCapacity - indicesSize) * sizeof(JSValue));
        }
    };

    auto createIndexArray = [&] (GCDeferralContext& deferralContext, int start, int end) {
        ObjectInitializationScope scope(vm);

        JSArray* result = JSArray::tryCreateUninitializedRestricted(scope, &deferralContext, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 2);
        result->initializeIndexWithoutBarrier(scope, 0, jsNumber(start));
        result->initializeIndexWithoutBarrier(scope, 1, jsNumber(end));

        return result;
    };

    if (globalObject->isHavingABadTime()) [[unlikely]] {
        GCDeferralContext deferralContext(vm);
        ObjectInitializationScope matchesArrayScope(vm);
        ObjectInitializationScope indicesArrayScope(vm);
        array = JSArray::tryCreateUninitializedRestricted(matchesArrayScope, &deferralContext, matchStructure, numSubpatterns + 1);

        if (createIndices)
            indicesArray = JSArray::tryCreateUninitializedRestricted(indicesArrayScope, &deferralContext, globalObject->regExpMatchesIndicesArrayStructure(), numSubpatterns + 1);

        // FIXME: we should probably throw an out of memory error here, but
        // when making this change we should check that all clients of this
        // function will correctly handle an exception being thrown from here.
        // https://bugs.webkit.org/show_bug.cgi?id=169786
        RELEASE_ASSERT(array);

        setProperties();

        array->initializeIndexWithoutBarrier(matchesArrayScope, 0, jsSubstringOfResolved(vm, &deferralContext, input, result.start, result.end - result.start));

        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            int start = subpatternResults[2 * i];
            int end = subpatternResults[2 * i + 1];
            JSValue value;
            if (start >= 0 && end >= start)
                value = jsSubstringOfResolved(vm, &deferralContext, input, start, end - start);
            else
                value = jsUndefined();
            array->initializeIndexWithoutBarrier(matchesArrayScope, i, value);
        }

        if (createIndices) {
            for (unsigned i = 0; i <= numSubpatterns; ++i) {
                int start = subpatternResults[2 * i];
                int end = subpatternResults[2 * i + 1];
                JSValue value;
                if (start >= 0 && end >= start)
                    indicesArray->initializeIndexWithoutBarrier(indicesArrayScope, i, createIndexArray(deferralContext, start, end));
                else
                    indicesArray->initializeIndexWithoutBarrier(indicesArrayScope, i, jsUndefined());
            }
        }
    } else {
        GCDeferralContext deferralContext(vm);
        ObjectInitializationScope matchesArrayScope(vm);
        ObjectInitializationScope indicesArrayScope(vm);
        array = tryCreateUninitializedRegExpMatchesArray(matchesArrayScope, &deferralContext, matchStructure, numSubpatterns + 1);

        if (createIndices)
            indicesArray = tryCreateUninitializedRegExpMatchesArray(indicesArrayScope, &deferralContext, globalObject->regExpMatchesIndicesArrayStructure(), numSubpatterns + 1);

        // FIXME: we should probably throw an out of memory error here, but
        // when making this change we should check that all clients of this
        // function will correctly handle an exception being thrown from here.
        // https://bugs.webkit.org/show_bug.cgi?id=169786
        RELEASE_ASSERT(array);

        setProperties();

        array->initializeIndexWithoutBarrier(matchesArrayScope, 0, jsSubstringOfResolved(vm, &deferralContext, input, result.start, result.end - result.start), ArrayWithContiguous);

        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            int start = subpatternResults[2 * i];
            int end = subpatternResults[2 * i + 1];
            JSValue value;
            if (start >= 0 && end >= start)
                value = jsSubstringOfResolved(vm, &deferralContext, input, start, end - start);
            else
                value = jsUndefined();
            array->initializeIndexWithoutBarrier(matchesArrayScope, i, value, ArrayWithContiguous);
        }

        if (createIndices) {
            for (unsigned i = 0; i <= numSubpatterns; ++i) {
                int start = subpatternResults[2 * i];
                int end = subpatternResults[2 * i + 1];
                if (start >= 0 && end >= start)
                    indicesArray->initializeIndexWithoutBarrier(indicesArrayScope, i, createIndexArray(deferralContext, start, end));
                else
                    indicesArray->initializeIndexWithoutBarrier(indicesArrayScope, i, jsUndefined());
            }
        }
    }

    // Now the object is safe to scan by GC.

    // We initialize the groups and indices objects late as they could allocate, which with the current API could cause
    // allocations.
    if (hasNamedCaptures) {
        bool hasDuplicateNamedCaptureGroups = regExp->hasDuplicateNamedCaptureGroups();
        PropertyOffset groupOffset = 0;
        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            const AtomString& groupName = regExp->getCaptureGroupNameForSubpatternId(i);
            if (!groupName.isEmpty()) {
                unsigned captureIndex = i;
                if (hasDuplicateNamedCaptureGroups) [[unlikely]]
                    captureIndex = regExp->subpatternIdForGroupName(groupName, subpatternResults);

                JSValue value;
                if (captureIndex > 0)
                    value = array->getIndexQuickly(captureIndex);
                else
                    value = jsUndefined();
                JSValue indicesValue;
                if (createIndices)
                    indicesValue = captureIndex > 0 ? indicesArray->getIndexQuickly(captureIndex) : jsUndefined();

                if (groupsStructure) {
                    groups->putDirectOffset(vm, groupOffset, value);
                    if (createIndices)
                        indicesGroups->putDirectOffset(vm, groupOffset, indicesValue);
                    groupOffset++;
                } else {
                    Identifier ident = Identifier::fromString(vm, groupName);
                    groups->putDirect(vm, ident, value);
                    if (createIndices)
                        indicesGroups->putDirect(vm, ident, indicesValue);
                }
            }
        }
    }

    return array;
}

JSArray* createRegExpMatchesArrayForPlainRegExpHavingABadTime(VM& vm, JSGlobalObject* globalObject, JSString* input, const MatchResult& result, std::span<const int> subpatternResults, unsigned numSubpatterns)
{
    Structure* matchStructure = globalObject->regExpMatchesArrayStructure();

    GCDeferralContext deferralContext(vm);
    ObjectInitializationScope matchesArrayScope(vm);

    // FIXME: This should handle array allocation errors gracefully.
    // https://bugs.webkit.org/show_bug.cgi?id=155144
    JSArray* array = JSArray::tryCreateUninitializedRestricted(matchesArrayScope, &deferralContext, matchStructure, numSubpatterns + 1);
    RELEASE_ASSERT(array);

    array->putDirectOffset(vm, RegExpMatchesArrayIndexPropertyOffset, jsNumber(result.start));
    array->putDirectOffset(vm, RegExpMatchesArrayInputPropertyOffset, input);
    array->putDirectOffset(vm, RegExpMatchesArrayGroupsPropertyOffset, jsUndefined());

    ASSERT(!array->butterfly()->indexingHeader()->preCapacity(matchStructure));
    auto capacity = matchStructure->outOfLineCapacity();
    auto size = matchStructure->outOfLineSize();
    if (capacity > size) [[unlikely]]
        gcSafeZeroMemory(static_cast<JSValue*>(array->butterfly()->base(0, capacity)), (capacity - size) * sizeof(JSValue));

    array->initializeIndexWithoutBarrier(matchesArrayScope, 0, jsSubstringOfResolved(vm, &deferralContext, input, result.start, result.end - result.start));
    for (unsigned i = 1; i <= numSubpatterns; ++i) {
        int start = subpatternResults[2 * i];
        int end = subpatternResults[2 * i + 1];
        JSValue value = jsUndefined();
        if (start >= 0 && end >= start)
            value = jsSubstringOfResolved(vm, &deferralContext, input, start, end - start);
        array->initializeIndexWithoutBarrier(matchesArrayScope, i, value);
    }

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
