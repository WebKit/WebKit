/*
 *  Copyright (C) 2008-2018 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "ButterflyInlines.h"
#include "GCDeferralContextInlines.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "RegExpInlines.h"
#include "RegExpObject.h"

namespace JSC {

static const PropertyOffset RegExpMatchesArrayIndexPropertyOffset = 100;
static const PropertyOffset RegExpMatchesArrayInputPropertyOffset = 101;
static const PropertyOffset RegExpMatchesArrayGroupsPropertyOffset = 102;

ALWAYS_INLINE JSArray* tryCreateUninitializedRegExpMatchesArray(ObjectInitializationScope& scope, GCDeferralContext* deferralContext, Structure* structure, unsigned initialLength)
{
    VM& vm = scope.vm();
    unsigned vectorLength = initialLength;
    if (vectorLength > MAX_STORAGE_VECTOR_LENGTH)
        return 0;

    const bool hasIndexingHeader = true;
    Butterfly* butterfly = Butterfly::tryCreateUninitialized(vm, nullptr, 0, structure->outOfLineCapacity(), hasIndexingHeader, vectorLength * sizeof(EncodedJSValue), deferralContext);
    if (UNLIKELY(!butterfly))
        return nullptr;

    butterfly->setVectorLength(vectorLength);
    butterfly->setPublicLength(initialLength);

    for (unsigned i = initialLength; i < vectorLength; ++i)
        butterfly->contiguous().atUnsafe(i).clear();

    JSArray* result = JSArray::createWithButterfly(vm, deferralContext, structure, butterfly);

    const bool createUninitialized = true;
    scope.notifyAllocated(result, createUninitialized);
    return result;
}

ALWAYS_INLINE JSArray* createRegExpMatchesArray(
    VM& vm, JSGlobalObject* globalObject, JSString* input, const String& inputValue,
    RegExp* regExp, unsigned startOffset, MatchResult& result)
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(vm.heap.expectDoesGC());

    Vector<int, 32> subpatternResults;
    int position = regExp->matchInline(vm, inputValue, startOffset, subpatternResults);
    if (position == -1) {
        result = MatchResult::failed();
        return nullptr;
    }

    result.start = position;
    result.end = subpatternResults[1];
    
    JSArray* array;

    // FIXME: This should handle array allocation errors gracefully.
    // https://bugs.webkit.org/show_bug.cgi?id=155144
    
    unsigned numSubpatterns = regExp->numSubpatterns();
    bool hasNamedCaptures = regExp->hasNamedCaptures();
    JSObject* groups = nullptr;
    Structure* matchStructure = globalObject->regExpMatchesArrayStructure();
    if (hasNamedCaptures) {
        groups = JSFinalObject::create(vm, JSFinalObject::createStructure(vm, globalObject, globalObject->objectPrototype(), 0));
        matchStructure = globalObject->regExpMatchesArrayWithGroupsStructure();
    }

    auto setProperties = [&] () {
        array->putDirect(vm, RegExpMatchesArrayIndexPropertyOffset, jsNumber(result.start));
        array->putDirect(vm, RegExpMatchesArrayInputPropertyOffset, input);
        if (hasNamedCaptures)
            array->putDirect(vm, RegExpMatchesArrayGroupsPropertyOffset, groups);

        ASSERT(!array->butterfly()->indexingHeader()->preCapacity(matchStructure));
        auto capacity = matchStructure->outOfLineCapacity();
        auto size = matchStructure->outOfLineSize();
        memset(array->butterfly()->base(0, capacity), 0, (capacity - size) * sizeof(JSValue));
    };

    if (UNLIKELY(globalObject->isHavingABadTime())) {
        GCDeferralContext deferralContext(vm.heap);
        ObjectInitializationScope scope(vm);
        array = JSArray::tryCreateUninitializedRestricted(scope, &deferralContext, matchStructure, numSubpatterns + 1);

        // FIXME: we should probably throw an out of memory error here, but
        // when making this change we should check that all clients of this
        // function will correctly handle an exception being thrown from here.
        // https://bugs.webkit.org/show_bug.cgi?id=169786
        RELEASE_ASSERT(array);

        setProperties();
        
        array->initializeIndexWithoutBarrier(scope, 0, jsSubstringOfResolved(vm, &deferralContext, input, result.start, result.end - result.start));
        
        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            int start = subpatternResults[2 * i];
            JSValue value;
            if (start >= 0)
                value = jsSubstringOfResolved(vm, &deferralContext, input, start, subpatternResults[2 * i + 1] - start);
            else
                value = jsUndefined();
            array->initializeIndexWithoutBarrier(scope, i, value);
        }
    } else {
        GCDeferralContext deferralContext(vm.heap);
        ObjectInitializationScope scope(vm);
        array = tryCreateUninitializedRegExpMatchesArray(scope, &deferralContext, matchStructure, numSubpatterns + 1);

        // FIXME: we should probably throw an out of memory error here, but
        // when making this change we should check that all clients of this
        // function will correctly handle an exception being thrown from here.
        // https://bugs.webkit.org/show_bug.cgi?id=169786
        RELEASE_ASSERT(array);
        
        setProperties();
        
        array->initializeIndexWithoutBarrier(scope, 0, jsSubstringOfResolved(vm, &deferralContext, input, result.start, result.end - result.start), ArrayWithContiguous);
        
        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            int start = subpatternResults[2 * i];
            JSValue value;
            if (start >= 0)
                value = jsSubstringOfResolved(vm, &deferralContext, input, start, subpatternResults[2 * i + 1] - start);
            else
                value = jsUndefined();
            array->initializeIndexWithoutBarrier(scope, i, value, ArrayWithContiguous);
        }
    }

    // Now the object is safe to scan by GC.

    // We initialize the groups object late as it could allocate, which with the current API could cause
    // allocations.
    if (groups) {
        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            String groupName = regExp->getCaptureGroupName(i);
            if (!groupName.isEmpty())
                groups->putDirect(vm, Identifier::fromString(&vm, groupName), array->getIndexQuickly(i));
        }
    }
    return array;
}

inline JSArray* createRegExpMatchesArray(ExecState* exec, JSGlobalObject* globalObject, JSString* string, RegExp* regExp, unsigned startOffset)
{
    MatchResult ignoredResult;
    return createRegExpMatchesArray(globalObject->vm(), globalObject, string, string->value(exec), regExp, startOffset, ignoredResult);
}
JSArray* createEmptyRegExpMatchesArray(JSGlobalObject*, JSString*, RegExp*);
Structure* createRegExpMatchesArrayStructure(VM&, JSGlobalObject*);
Structure* createRegExpMatchesArraySlowPutStructure(VM&, JSGlobalObject*);
Structure* createRegExpMatchesArrayWithGroupsStructure(VM&, JSGlobalObject*);
Structure* createRegExpMatchesArrayWithGroupsSlowPutStructure(VM&, JSGlobalObject*);

} // namespace JSC
