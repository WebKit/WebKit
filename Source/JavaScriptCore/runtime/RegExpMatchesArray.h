/*
 *  Copyright (C) 2008-2026 Apple Inc. All rights reserved.
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
#include "ObjectConstructor.h"
#include "RegExpInlines.h"
#include "RegExpObject.h"

namespace JSC {

static constexpr PropertyOffset RegExpMatchesArrayIndexPropertyOffset = firstOutOfLineOffset;
static constexpr PropertyOffset RegExpMatchesArrayInputPropertyOffset = firstOutOfLineOffset + 1;
static constexpr PropertyOffset RegExpMatchesArrayGroupsPropertyOffset = firstOutOfLineOffset + 2;
static constexpr PropertyOffset RegExpMatchesArrayIndicesPropertyOffset = firstOutOfLineOffset + 3;
static constexpr PropertyOffset RegExpMatchesIndicesGroupsPropertyOffset = firstOutOfLineOffset;

JSArray* createRegExpMatchesArrayWithGroupsOrIndices(VM&, JSGlobalObject*, JSString* input, RegExp*, const MatchResult&, std::span<const int> subpatternResults, unsigned numSubpatterns, bool hasNamedCaptures, bool createIndices);
JSArray* createRegExpMatchesArrayForPlainRegExpHavingABadTime(VM&, JSGlobalObject*, JSString*, const MatchResult&, std::span<const int> subpatternResults, unsigned numSubpatterns);

ALWAYS_INLINE JSArray* tryCreateUninitializedRegExpMatchesArray(ObjectInitializationScope& scope, GCDeferralContext* deferralContext, Structure* structure, unsigned initialLength)
{
    VM& vm = scope.vm();
    unsigned vectorLength = initialLength;
    if (vectorLength > MAX_STORAGE_VECTOR_LENGTH)
        return nullptr;

    const bool hasIndexingHeader = true;
    Butterfly* butterfly = Butterfly::tryCreateUninitialized(vm, nullptr, 0, structure->outOfLineCapacity(), hasIndexingHeader, vectorLength * sizeof(EncodedJSValue), deferralContext);
    if (!butterfly) [[unlikely]]
        return nullptr;

    butterfly->setVectorLength(vectorLength);
    butterfly->setPublicLength(initialLength);

    for (unsigned i = initialLength; i < vectorLength; ++i)
        butterfly->contiguous().atUnsafe(i).clear();

    JSArray* result = JSArray::createWithButterfly(vm, deferralContext, structure, butterfly);

    scope.notifyAllocated(result);
    return result;
}

ALWAYS_INLINE JSArray* createRegExpMatchesArrayForPlainRegExp(VM& vm, JSGlobalObject* globalObject, JSString* input, const MatchResult& result, std::span<const int> subpatternResults, unsigned numSubpatterns)
{
    Structure* matchStructure = globalObject->regExpMatchesArrayStructure();

    GCDeferralContext deferralContext(vm);
    ObjectInitializationScope matchesArrayScope(vm);

    // FIXME: This should handle array allocation errors gracefully.
    // https://bugs.webkit.org/show_bug.cgi?id=155144
    JSArray* array = tryCreateUninitializedRegExpMatchesArray(matchesArrayScope, &deferralContext, matchStructure, numSubpatterns + 1);
    RELEASE_ASSERT(array);

    array->putDirectOffset(vm, RegExpMatchesArrayIndexPropertyOffset, jsNumber(result.start));
    array->putDirectOffset(vm, RegExpMatchesArrayInputPropertyOffset, input);
    array->putDirectOffset(vm, RegExpMatchesArrayGroupsPropertyOffset, jsUndefined());

    ASSERT(!array->butterfly()->indexingHeader()->preCapacity(matchStructure));
    auto capacity = matchStructure->outOfLineCapacity();
    auto size = matchStructure->outOfLineSize();
    if (capacity > size) [[unlikely]]
        gcSafeZeroMemory(static_cast<JSValue*>(array->butterfly()->base(0, capacity)), (capacity - size) * sizeof(JSValue));

    array->initializeIndexWithoutBarrier(matchesArrayScope, 0, jsSubstringOfResolved(vm, &deferralContext, input, result.start, result.end - result.start), ArrayWithContiguous);
    for (unsigned i = 1; i <= numSubpatterns; ++i) {
        int start = subpatternResults[2 * i];
        int end = subpatternResults[2 * i + 1];
        JSValue value = jsUndefined();
        if (start >= 0 && end >= start)
            value = jsSubstringOfResolved(vm, &deferralContext, input, start, end - start);
        array->initializeIndexWithoutBarrier(matchesArrayScope, i, value, ArrayWithContiguous);
    }

    return array;
}

ALWAYS_INLINE JSArray* createRegExpMatchesArray(VM& vm, JSGlobalObject* globalObject, JSString* input, StringView inputValue, RegExp* regExp, unsigned startOffset, MatchResult& result)
{
    if constexpr (validateDFGDoesGC)
        vm.verifyCanGC();

    auto subpatternResults = regExp->ovectorSpan();
    int position = regExp->matchInline<Yarr::MatchFrom::VMThread>(globalObject, vm, inputValue, startOffset, subpatternResults);
    if (position == -1) {
        result = MatchResult::failed();
        return nullptr;
    }

    result.start = position;
    result.end = subpatternResults[1];

    unsigned numSubpatterns = regExp->numSubpatterns();
    bool hasNamedCaptures = regExp->hasNamedCaptures();
    bool createIndices = regExp->hasIndices();

    if (!hasNamedCaptures && !createIndices) [[likely]] {
        if (globalObject->isHavingABadTime()) [[unlikely]]
            return createRegExpMatchesArrayForPlainRegExpHavingABadTime(vm, globalObject, input, result, subpatternResults, numSubpatterns);
        return createRegExpMatchesArrayForPlainRegExp(vm, globalObject, input, result, subpatternResults, numSubpatterns);
    }

    return createRegExpMatchesArrayWithGroupsOrIndices(vm, globalObject, input, regExp, result, subpatternResults, numSubpatterns, hasNamedCaptures, createIndices);
}

JSArray* createEmptyRegExpMatchesArray(JSGlobalObject*, JSString*, RegExp*);
Structure* createRegExpMatchesArrayStructure(VM&, JSGlobalObject*);
Structure* createRegExpMatchesArrayWithIndicesStructure(VM&, JSGlobalObject*);
Structure* createRegExpMatchesIndicesArrayStructure(VM&, JSGlobalObject*);
Structure* createRegExpMatchesArraySlowPutStructure(VM&, JSGlobalObject*);
Structure* createRegExpMatchesArrayWithIndicesSlowPutStructure(VM&, JSGlobalObject*);
Structure* createRegExpMatchesIndicesArraySlowPutStructure(VM&, JSGlobalObject*);

} // namespace JSC
