/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#include "ArgList.h"
#include "ArrayConventions.h"
#include "ButterflyInlines.h"
#include "JSCellInlines.h"
#include "JSObject.h"

namespace JSC {

class JSArray;
class LLIntOffsetsExtractor;

extern const ASCIILiteral LengthExceededTheMaximumArrayLengthError;

class JSArray : public JSNonFinalObject {
    friend class LLIntOffsetsExtractor;
    friend class Walker;
    friend class JIT;

public:
    typedef JSNonFinalObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetPropertyNames;

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, !inlineCapacity);
        return sizeof(JSArray);
    }
        
protected:
    explicit JSArray(VM& vm, Structure* structure, Butterfly* butterfly)
        : JSNonFinalObject(vm, structure, butterfly)
    {
    }

public:
    static JSArray* tryCreate(VM&, Structure*, unsigned initialLength = 0);
    static JSArray* tryCreate(VM&, Structure*, unsigned initialLength, unsigned vectorLengthHint);
    static JSArray* create(VM&, Structure*, unsigned initialLength = 0);
    static JSArray* createWithButterfly(VM&, GCDeferralContext*, Structure*, Butterfly*);

    // tryCreateUninitializedRestricted is used for fast construction of arrays whose size and
    // contents are known at time of creation. This is a restricted API for careful use only in
    // performance critical code paths. If you don't have a good reason to use it, you probably
    // shouldn't use it. Instead, you should go with
    //   - JSArray::tryCreate() or JSArray::create() instead of tryCreateUninitializedRestricted(), and
    //   - putDirectIndex() instead of initializeIndex().
    //
    // Clients of this interface must:
    //   - null-check the result (indicating out of memory, or otherwise unable to allocate vector).
    //   - call 'initializeIndex' for all properties in sequence, for 0 <= i < initialLength.
    //   - Provide a valid GCDefferalContext* if they might garbage collect when initializing properties,
    //     otherwise the caller can provide a null GCDefferalContext*.
    //   - Provide a local stack instance of ObjectInitializationScope at the call site.
    //
    JS_EXPORT_PRIVATE static JSArray* tryCreateUninitializedRestricted(ObjectInitializationScope&, GCDeferralContext*, Structure*, unsigned initialLength);
    static JSArray* tryCreateUninitializedRestricted(ObjectInitializationScope& scope, Structure* structure, unsigned initialLength)
    {
        return tryCreateUninitializedRestricted(scope, nullptr, structure, initialLength);
    }

    static void eagerlyInitializeButterfly(ObjectInitializationScope&, JSArray*, unsigned initialLength);

    JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool throwException);

    JS_EXPORT_PRIVATE static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);

    DECLARE_EXPORT_INFO;

    // OK if we know this is a JSArray, but not if it could be an object of a derived class; for RuntimeArray this always returns 0.
    unsigned length() const { return getArrayLength(); }

    // OK to use on new arrays, but not if it might be a RegExpMatchArray or RuntimeArray.
    JS_EXPORT_PRIVATE bool setLength(ExecState*, unsigned, bool throwException = false);

    void pushInline(ExecState*, JSValue);
    JS_EXPORT_PRIVATE void push(ExecState*, JSValue);
    JS_EXPORT_PRIVATE JSValue pop(ExecState*);

    JSArray* fastSlice(ExecState&, unsigned startIndex, unsigned count);

    bool canFastCopy(VM&, JSArray* otherArray);
    bool canDoFastIndexedAccess(VM&);
    // This function returns NonArray if the indexing types are not compatable for copying.
    IndexingType mergeIndexingTypeForCopying(IndexingType other);
    bool appendMemcpy(ExecState*, VM&, unsigned startIndex, JSArray* otherArray);

    enum ShiftCountMode {
        // This form of shift hints that we're doing queueing. With this assumption in hand,
        // we convert to ArrayStorage, which has queue optimizations.
        ShiftCountForShift,
            
        // This form of shift hints that we're just doing care and feeding on an array that
        // is probably typically used for ordinary accesses. With this assumption in hand,
        // we try to preserve whatever indexing type it has already.
        ShiftCountForSplice
    };

    bool shiftCountForShift(ExecState* exec, unsigned startIndex, unsigned count)
    {
        VM& vm = exec->vm();
        return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));
    }
    bool shiftCountForSplice(ExecState* exec, unsigned& startIndex, unsigned count)
    {
        return shiftCountWithAnyIndexingType(exec, startIndex, count);
    }
    template<ShiftCountMode shiftCountMode>
    bool shiftCount(ExecState* exec, unsigned& startIndex, unsigned count)
    {
        switch (shiftCountMode) {
        case ShiftCountForShift:
            return shiftCountForShift(exec, startIndex, count);
        case ShiftCountForSplice:
            return shiftCountForSplice(exec, startIndex, count);
        default:
            CRASH();
            return false;
        }
    }
        
    bool unshiftCountForShift(ExecState* exec, unsigned startIndex, unsigned count)
    {
        return unshiftCountWithArrayStorage(exec, startIndex, count, ensureArrayStorage(exec->vm()));
    }
    bool unshiftCountForSplice(ExecState* exec, unsigned startIndex, unsigned count)
    {
        return unshiftCountWithAnyIndexingType(exec, startIndex, count);
    }
    template<ShiftCountMode shiftCountMode>
    bool unshiftCount(ExecState* exec, unsigned startIndex, unsigned count)
    {
        switch (shiftCountMode) {
        case ShiftCountForShift:
            return unshiftCountForShift(exec, startIndex, count);
        case ShiftCountForSplice:
            return unshiftCountForSplice(exec, startIndex, count);
        default:
            CRASH();
            return false;
        }
    }

    JS_EXPORT_PRIVATE void fillArgList(ExecState*, MarkedArgumentBuffer&);
    JS_EXPORT_PRIVATE void copyToArguments(ExecState*, VirtualRegister firstElementDest, unsigned offset, unsigned length);

    JS_EXPORT_PRIVATE bool isIteratorProtocolFastAndNonObservable();

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, IndexingType indexingType)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ArrayType, StructureFlags), info(), indexingType);
    }
        
protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(jsDynamicCast<JSArray*>(vm, this));
        ASSERT_WITH_MESSAGE(type() == ArrayType || type() == DerivedArrayType, "Instance inheriting JSArray should have either ArrayType or DerivedArrayType");
    }

    static bool put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);

    static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    JS_EXPORT_PRIVATE static void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

private:
    bool isLengthWritable()
    {
        ArrayStorage* storage = arrayStorageOrNull();
        if (!storage)
            return true;
        SparseArrayValueMap* map = storage->m_sparseMap.get();
        return !map || !map->lengthIsReadOnly();
    }
        
    bool shiftCountWithAnyIndexingType(ExecState*, unsigned& startIndex, unsigned count);
    JS_EXPORT_PRIVATE bool shiftCountWithArrayStorage(VM&, unsigned startIndex, unsigned count, ArrayStorage*);

    bool unshiftCountWithAnyIndexingType(ExecState*, unsigned startIndex, unsigned count);
    bool unshiftCountWithArrayStorage(ExecState*, unsigned startIndex, unsigned count, ArrayStorage*);
    bool unshiftCountSlowCase(const AbstractLocker&, VM&, DeferGC&, bool, unsigned);

    bool setLengthWithArrayStorage(ExecState*, unsigned newLength, bool throwException, ArrayStorage*);
    void setLengthWritable(ExecState*, bool writable);
};

inline Butterfly* tryCreateArrayButterfly(VM& vm, JSObject* intendedOwner, unsigned initialLength)
{
    Butterfly* butterfly = Butterfly::tryCreate(
        vm, intendedOwner, 0, 0, true, baseIndexingHeaderForArrayStorage(initialLength),
        ArrayStorage::sizeFor(BASE_ARRAY_STORAGE_VECTOR_LEN));
    if (!butterfly)
        return nullptr;
    ArrayStorage* storage = butterfly->arrayStorage();
    storage->m_sparseMap.clear();
    storage->m_indexBias = 0;
    storage->m_numValuesInVector = 0;
    return butterfly;
}

inline JSArray* JSArray::tryCreate(VM& vm, Structure* structure, unsigned initialLength, unsigned vectorLengthHint)
{
    ASSERT(vectorLengthHint >= initialLength);
    unsigned outOfLineStorage = structure->outOfLineCapacity();

    Butterfly* butterfly;
    IndexingType indexingType = structure->indexingType();
    if (LIKELY(!hasAnyArrayStorage(indexingType))) {
        ASSERT(
            hasUndecided(indexingType)
            || hasInt32(indexingType)
            || hasDouble(indexingType)
            || hasContiguous(indexingType));

        if (UNLIKELY(vectorLengthHint > MAX_STORAGE_VECTOR_LENGTH))
            return nullptr;

        unsigned vectorLength = Butterfly::optimalContiguousVectorLength(structure, vectorLengthHint);
        void* temp = vm.jsValueGigacageAuxiliarySpace.allocateNonVirtual(
            vm,
            Butterfly::totalSize(0, outOfLineStorage, true, vectorLength * sizeof(EncodedJSValue)),
            nullptr, AllocationFailureMode::ReturnNull);
        if (!temp)
            return nullptr;
        butterfly = Butterfly::fromBase(temp, 0, outOfLineStorage);
        butterfly->setVectorLength(vectorLength);
        butterfly->setPublicLength(initialLength);
        if (hasDouble(indexingType))
            clearArray(butterfly->contiguousDouble().data(), vectorLength);
        else
            clearArray(butterfly->contiguous().data(), vectorLength);
    } else {
        ASSERT(
            indexingType == ArrayWithSlowPutArrayStorage
            || indexingType == ArrayWithArrayStorage);
        butterfly = tryCreateArrayButterfly(vm, nullptr, initialLength);
        if (!butterfly)
            return nullptr;
        for (unsigned i = 0; i < BASE_ARRAY_STORAGE_VECTOR_LEN; ++i)
            butterfly->arrayStorage()->m_vector[i].clear();
    }

    return createWithButterfly(vm, nullptr, structure, butterfly);
}

inline JSArray* JSArray::tryCreate(VM& vm, Structure* structure, unsigned initialLength)
{
    return tryCreate(vm, structure, initialLength, initialLength);
}

inline JSArray* JSArray::create(VM& vm, Structure* structure, unsigned initialLength)
{
    JSArray* result = JSArray::tryCreate(vm, structure, initialLength);
    RELEASE_ASSERT(result);

    return result;
}

inline JSArray* JSArray::createWithButterfly(VM& vm, GCDeferralContext* deferralContext, Structure* structure, Butterfly* butterfly)
{
    JSArray* array = new (NotNull, allocateCell<JSArray>(vm.heap, deferralContext)) JSArray(vm, structure, butterfly);
    array->finishCreation(vm);
    return array;
}

JSArray* asArray(JSValue);

inline JSArray* asArray(JSCell* cell)
{
    ASSERT(cell->inherits<JSArray>(cell->vm()));
    return jsCast<JSArray*>(cell);
}

inline JSArray* asArray(JSValue value)
{
    return asArray(value.asCell());
}

inline bool isJSArray(JSCell* cell)
{
    ASSERT((cell->classInfo(cell->vm()) == JSArray::info()) == (cell->type() == ArrayType));
    return cell->type() == ArrayType;
}

inline bool isJSArray(JSValue v) { return v.isCell() && isJSArray(v.asCell()); }

JS_EXPORT_PRIVATE JSArray* constructArray(ExecState*, Structure*, const ArgList& values);
JS_EXPORT_PRIVATE JSArray* constructArray(ExecState*, Structure*, const JSValue* values, unsigned length);
JS_EXPORT_PRIVATE JSArray* constructArrayNegativeIndexed(ExecState*, Structure*, const JSValue* values, unsigned length);

} // namespace JSC
