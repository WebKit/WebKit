/*
 *  Copyright (C) 2008, 2016 Apple Inc. All Rights Reserved.
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

#ifndef RegExpMatchesArray_h
#define RegExpMatchesArray_h

#include "ButterflyInlines.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "RegExpInlines.h"
#include "RegExpObject.h"

namespace JSC {

static const PropertyOffset RegExpMatchesArrayIndexPropertyOffset = 100;
static const PropertyOffset RegExpMatchesArrayInputPropertyOffset = 101;

ALWAYS_INLINE JSArray* tryCreateUninitializedRegExpMatchesArray(VM& vm, Structure* structure, unsigned initialLength)
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

ALWAYS_INLINE JSArray* createRegExpMatchesArray(
    VM& vm, JSGlobalObject* globalObject, JSString* input, const String& inputValue,
    RegExp* regExp, unsigned startOffset, MatchResult& result)
{
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
    
    if (UNLIKELY(globalObject->isHavingABadTime())) {
        array = JSArray::tryCreateUninitialized(vm, globalObject->regExpMatchesArrayStructure(), regExp->numSubpatterns() + 1);
        
        array->initializeIndex(vm, 0, jsSubstringOfResolved(vm, input, result.start, result.end - result.start));
        
        if (unsigned numSubpatterns = regExp->numSubpatterns()) {
            for (unsigned i = 1; i <= numSubpatterns; ++i) {
                int start = subpatternResults[2 * i];
                if (start >= 0)
                    array->initializeIndex(vm, i, JSRopeString::createSubstringOfResolved(vm, input, start, subpatternResults[2 * i + 1] - start));
                else
                    array->initializeIndex(vm, i, jsUndefined());
            }
        }
    } else {
        array = tryCreateUninitializedRegExpMatchesArray(vm, globalObject->regExpMatchesArrayStructure(), regExp->numSubpatterns() + 1);
        RELEASE_ASSERT(array);
        
        array->initializeIndex(vm, 0, jsSubstringOfResolved(vm, input, result.start, result.end - result.start), ArrayWithContiguous);
        
        if (unsigned numSubpatterns = regExp->numSubpatterns()) {
            for (unsigned i = 1; i <= numSubpatterns; ++i) {
                int start = subpatternResults[2 * i];
                if (start >= 0)
                    array->initializeIndex(vm, i, JSRopeString::createSubstringOfResolved(vm, input, start, subpatternResults[2 * i + 1] - start), ArrayWithContiguous);
                else
                    array->initializeIndex(vm, i, jsUndefined(), ArrayWithContiguous);
            }
        }
    }

    array->putDirect(vm, RegExpMatchesArrayIndexPropertyOffset, jsNumber(result.start));
    array->putDirect(vm, RegExpMatchesArrayInputPropertyOffset, input);

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

}

#endif // RegExpMatchesArray_h
