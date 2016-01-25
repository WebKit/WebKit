/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2009, 2012-2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef JSObject_h
#define JSObject_h

#include "ArgList.h"
#include "ArrayConventions.h"
#include "ArrayStorage.h"
#include "Butterfly.h"
#include "CallFrame.h"
#include "ClassInfo.h"
#include "CommonIdentifiers.h"
#include "CopyBarrier.h"
#include "CustomGetterSetter.h"
#include "DeferGC.h"
#include "Heap.h"
#include "HeapInlines.h"
#include "IndexingHeaderInlines.h"
#include "JSCell.h"
#include "PropertySlot.h"
#include "PropertyStorage.h"
#include "PutDirectIndexMode.h"
#include "PutPropertySlot.h"

#include "Structure.h"
#include "VM.h"
#include "JSString.h"
#include "SparseArrayValueMap.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

inline JSCell* getJSFunction(JSValue value)
{
    if (value.isCell() && (value.asCell()->type() == JSFunctionType))
        return value.asCell();
    return 0;
}

class GetterSetter;
class InternalFunction;
class JSFunction;
class LLIntOffsetsExtractor;
class MarkedBlock;
class PropertyDescriptor;
class PropertyNameArray;
class Structure;
struct HashTable;
struct HashTableValue;

JS_EXPORT_PRIVATE JSObject* throwTypeError(ExecState*, const String&);
extern JS_EXPORTDATA const char* StrictModeReadonlyPropertyWriteError;

COMPILE_ASSERT(None < FirstInternalAttribute, None_is_below_FirstInternalAttribute);
COMPILE_ASSERT(ReadOnly < FirstInternalAttribute, ReadOnly_is_below_FirstInternalAttribute);
COMPILE_ASSERT(DontEnum < FirstInternalAttribute, DontEnum_is_below_FirstInternalAttribute);
COMPILE_ASSERT(DontDelete < FirstInternalAttribute, DontDelete_is_below_FirstInternalAttribute);
COMPILE_ASSERT(Accessor < FirstInternalAttribute, Accessor_is_below_FirstInternalAttribute);

class JSFinalObject;

class JSObject : public JSCell {
    friend class BatchedTransitionOptimizer;
    friend class JIT;
    friend class JSCell;
    friend class JSFinalObject;
    friend class MarkedBlock;
    JS_EXPORT_PRIVATE friend bool setUpStaticFunctionSlot(ExecState*, const HashTableValue*, JSObject*, PropertyName, PropertySlot&);

    enum PutMode {
        PutModePut,
        PutModeDefineOwnProperty,
    };

public:
    typedef JSCell Base;
        
    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);
    JS_EXPORT_PRIVATE static void copyBackingStore(JSCell*, CopyVisitor&, CopyToken);

    JS_EXPORT_PRIVATE static String className(const JSObject*);
    JS_EXPORT_PRIVATE static String calculatedClassName(JSObject*);

    JSValue prototype() const;
    JS_EXPORT_PRIVATE void setPrototype(VM&, JSValue prototype);
    JS_EXPORT_PRIVATE bool setPrototypeWithCycleCheck(ExecState*, JSValue prototype);
        
    bool mayInterceptIndexedAccesses()
    {
        return structure()->mayInterceptIndexedAccesses();
    }
        
    JSValue get(ExecState*, PropertyName) const;
    JSValue get(ExecState*, unsigned propertyName) const;

    bool fastGetOwnPropertySlot(ExecState*, VM&, Structure&, PropertyName, PropertySlot&);
    bool getPropertySlot(ExecState*, PropertyName, PropertySlot&);
    bool getPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    JS_EXPORT_PRIVATE static bool getOwnPropertySlotByIndex(JSObject*, ExecState*, unsigned propertyName, PropertySlot&);

    // The key difference between this and getOwnPropertySlot is that getOwnPropertySlot
    // currently returns incorrect results for the DOM window (with non-own properties)
    // being returned. Once this is fixed we should migrate code & remove this method.
    JS_EXPORT_PRIVATE bool getOwnPropertyDescriptor(ExecState*, PropertyName, PropertyDescriptor&);

    JS_EXPORT_PRIVATE bool allowsAccessFrom(ExecState*);

    unsigned getArrayLength() const
    {
        if (!hasIndexedProperties(indexingType()))
            return 0;
        return m_butterfly.get(this)->publicLength();
    }
        
    unsigned getVectorLength()
    {
        if (!hasIndexedProperties(indexingType()))
            return 0;
        return m_butterfly.get(this)->vectorLength();
    }
    
    static void putInline(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    
    JS_EXPORT_PRIVATE static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    JS_EXPORT_PRIVATE static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
        
    ALWAYS_INLINE void putByIndexInline(ExecState* exec, unsigned propertyName, JSValue value, bool shouldThrow)
    {
        if (canSetIndexQuickly(propertyName)) {
            setIndexQuickly(exec->vm(), propertyName, value);
            return;
        }
        methodTable(exec->vm())->putByIndex(this, exec, propertyName, value, shouldThrow);
    }
        
    // This is similar to the putDirect* methods:
    //  - the prototype chain is not consulted
    //  - accessors are not called.
    //  - it will ignore extensibility and read-only properties if PutDirectIndexLikePutDirect is passed as the mode (the default).
    // This method creates a property with attributes writable, enumerable and configurable all set to true.
    bool putDirectIndex(ExecState* exec, unsigned propertyName, JSValue value, unsigned attributes, PutDirectIndexMode mode)
    {
        if (!attributes && canSetIndexQuicklyForPutDirect(propertyName)) {
            setIndexQuickly(exec->vm(), propertyName, value);
            return true;
        }
        return putDirectIndexBeyondVectorLength(exec, propertyName, value, attributes, mode);
    }
    bool putDirectIndex(ExecState* exec, unsigned propertyName, JSValue value)
    {
        return putDirectIndex(exec, propertyName, value, 0, PutDirectIndexLikePutDirect);
    }

    // A non-throwing version of putDirect and putDirectIndex.
    JS_EXPORT_PRIVATE void putDirectMayBeIndex(ExecState*, PropertyName, JSValue);
        
    bool hasIndexingHeader() const
    {
        return structure()->hasIndexingHeader(this);
    }
    
    bool canGetIndexQuickly(unsigned i)
    {
        Butterfly* butterfly = m_butterfly.get(this);
        switch (indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            return false;
        case ALL_INT32_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            return i < butterfly->vectorLength() && butterfly->contiguous()[i];
        case ALL_DOUBLE_INDEXING_TYPES: {
            if (i >= butterfly->vectorLength())
                return false;
            double value = butterfly->contiguousDouble()[i];
            if (value != value)
                return false;
            return true;
        }
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return i < butterfly->arrayStorage()->vectorLength() && butterfly->arrayStorage()->m_vector[i];
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }
        
    JSValue getIndexQuickly(unsigned i)
    {
        Butterfly* butterfly = m_butterfly.get(this);
        switch (indexingType()) {
        case ALL_INT32_INDEXING_TYPES:
            return jsNumber(butterfly->contiguous()[i].get().asInt32());
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            return butterfly->contiguous()[i].get();
        case ALL_DOUBLE_INDEXING_TYPES:
            return JSValue(JSValue::EncodeAsDouble, butterfly->contiguousDouble()[i]);
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return butterfly->arrayStorage()->m_vector[i].get();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return JSValue();
        }
    }
        
    JSValue tryGetIndexQuickly(unsigned i) const
    {
        Butterfly* butterfly = m_butterfly.get(this);
        switch (indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            break;
        case ALL_INT32_INDEXING_TYPES:
            if (i < butterfly->publicLength()) {
                JSValue result = butterfly->contiguous()[i].get();
                ASSERT(result.isInt32() || !result);
                return result;
            }
            break;
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            if (i < butterfly->publicLength())
                return butterfly->contiguous()[i].get();
            break;
        case ALL_DOUBLE_INDEXING_TYPES: {
            if (i >= butterfly->publicLength())
                break;
            double result = butterfly->contiguousDouble()[i];
            if (result != result)
                break;
            return JSValue(JSValue::EncodeAsDouble, result);
        }
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            if (i < butterfly->arrayStorage()->vectorLength())
                return butterfly->arrayStorage()->m_vector[i].get();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        return JSValue();
    }
        
    JSValue getDirectIndex(ExecState* exec, unsigned i)
    {
        if (JSValue result = tryGetIndexQuickly(i))
            return result;
        PropertySlot slot(this);
        if (methodTable(exec->vm())->getOwnPropertySlotByIndex(this, exec, i, slot))
            return slot.getValue(exec, i);
        return JSValue();
    }
        
    JSValue getIndex(ExecState* exec, unsigned i) const
    {
        if (JSValue result = tryGetIndexQuickly(i))
            return result;
        return get(exec, i);
    }
        
    bool canSetIndexQuickly(unsigned i)
    {
        Butterfly* butterfly = m_butterfly.get(this);
        switch (indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            return false;
        case ALL_INT32_INDEXING_TYPES:
        case ALL_DOUBLE_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
        case NonArrayWithArrayStorage:
        case ArrayWithArrayStorage:
            return i < butterfly->vectorLength();
        case NonArrayWithSlowPutArrayStorage:
        case ArrayWithSlowPutArrayStorage:
            return i < butterfly->arrayStorage()->vectorLength()
                && !!butterfly->arrayStorage()->m_vector[i];
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }
        
    bool canSetIndexQuicklyForPutDirect(unsigned i)
    {
        switch (indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            return false;
        case ALL_INT32_INDEXING_TYPES:
        case ALL_DOUBLE_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return i < m_butterfly.get(this)->vectorLength();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }
        
    void setIndexQuickly(VM& vm, unsigned i, JSValue v)
    {
        Butterfly* butterfly = m_butterfly.get(this);
        switch (indexingType()) {
        case ALL_INT32_INDEXING_TYPES: {
            ASSERT(i < butterfly->vectorLength());
            if (!v.isInt32()) {
                convertInt32ToDoubleOrContiguousWhilePerformingSetIndex(vm, i, v);
                return;
            }
            FALLTHROUGH;
        }
        case ALL_CONTIGUOUS_INDEXING_TYPES: {
            ASSERT(i < butterfly->vectorLength());
            butterfly->contiguous()[i].set(vm, this, v);
            if (i >= butterfly->publicLength())
                butterfly->setPublicLength(i + 1);
            break;
        }
        case ALL_DOUBLE_INDEXING_TYPES: {
            ASSERT(i < butterfly->vectorLength());
            if (!v.isNumber()) {
                convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
                return;
            }
            double value = v.asNumber();
            if (value != value) {
                convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
                return;
            }
            butterfly->contiguousDouble()[i] = value;
            if (i >= butterfly->publicLength())
                butterfly->setPublicLength(i + 1);
            break;
        }
        case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
            ArrayStorage* storage = butterfly->arrayStorage();
            WriteBarrier<Unknown>& x = storage->m_vector[i];
            JSValue old = x.get();
            x.set(vm, this, v);
            if (!old) {
                ++storage->m_numValuesInVector;
                if (i >= storage->length())
                    storage->setLength(i + 1);
            }
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void initializeIndex(VM& vm, unsigned i, JSValue v)
    {
        initializeIndex(vm, i, v, indexingType());
    }

    void initializeIndex(VM& vm, unsigned i, JSValue v, IndexingType indexingType)
    {
        Butterfly* butterfly = m_butterfly.get(this);
        switch (indexingType) {
        case ALL_UNDECIDED_INDEXING_TYPES: {
            setIndexQuicklyToUndecided(vm, i, v);
            break;
        }
        case ALL_INT32_INDEXING_TYPES: {
            ASSERT(i < butterfly->publicLength());
            ASSERT(i < butterfly->vectorLength());
            if (!v.isInt32()) {
                convertInt32ToDoubleOrContiguousWhilePerformingSetIndex(vm, i, v);
                break;
            }
            FALLTHROUGH;
        }
        case ALL_CONTIGUOUS_INDEXING_TYPES: {
            ASSERT(i < butterfly->publicLength());
            ASSERT(i < butterfly->vectorLength());
            butterfly->contiguous()[i].set(vm, this, v);
            break;
        }
        case ALL_DOUBLE_INDEXING_TYPES: {
            ASSERT(i < butterfly->publicLength());
            ASSERT(i < butterfly->vectorLength());
            if (!v.isNumber()) {
                convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
                return;
            }
            double value = v.asNumber();
            if (value != value) {
                convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
                return;
            }
            butterfly->contiguousDouble()[i] = value;
            break;
        }
        case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
            ArrayStorage* storage = butterfly->arrayStorage();
            ASSERT(i < storage->length());
            ASSERT(i < storage->m_numValuesInVector);
            storage->m_vector[i].set(vm, this, v);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
        
    bool hasSparseMap()
    {
        switch (indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
        case ALL_INT32_INDEXING_TYPES:
        case ALL_DOUBLE_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            return false;
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return !!m_butterfly.get(this)->arrayStorage()->m_sparseMap;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }
        
    bool inSparseIndexingMode()
    {
        switch (indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
        case ALL_INT32_INDEXING_TYPES:
        case ALL_DOUBLE_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            return false;
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return m_butterfly.get(this)->arrayStorage()->inSparseMode();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }
        
    void enterDictionaryIndexingMode(VM&);

    // putDirect is effectively an unchecked vesion of 'defineOwnProperty':
    //  - the prototype chain is not consulted
    //  - accessors are not called.
    //  - attributes will be respected (after the call the property will exist with the given attributes)
    //  - the property name is assumed to not be an index.
    void putDirect(VM&, PropertyName, JSValue, unsigned attributes = 0);
    void putDirect(VM&, PropertyName, JSValue, PutPropertySlot&);
    void putDirectWithoutTransition(VM&, PropertyName, JSValue, unsigned attributes = 0);
    void putDirectNonIndexAccessor(VM&, PropertyName, JSValue, unsigned attributes);
    void putDirectAccessor(ExecState*, PropertyName, JSValue, unsigned attributes);
    JS_EXPORT_PRIVATE void putDirectCustomAccessor(VM&, PropertyName, JSValue, unsigned attributes);

    void putGetter(ExecState*, PropertyName, JSValue, unsigned attributes);
    void putSetter(ExecState*, PropertyName, JSValue, unsigned attributes);

    JS_EXPORT_PRIVATE bool hasProperty(ExecState*, PropertyName) const;
    JS_EXPORT_PRIVATE bool hasProperty(ExecState*, unsigned propertyName) const;
    bool hasOwnProperty(ExecState*, PropertyName) const;
    bool hasOwnProperty(ExecState*, unsigned) const;

    JS_EXPORT_PRIVATE static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    JS_EXPORT_PRIVATE static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);

    JS_EXPORT_PRIVATE static JSValue defaultValue(const JSObject*, ExecState*, PreferredPrimitiveType);

    JS_EXPORT_PRIVATE bool hasInstance(ExecState*, JSValue value, JSValue hasInstanceValue);
    bool hasInstance(ExecState*, JSValue);
    static bool defaultHasInstance(ExecState*, JSValue, JSValue prototypeProperty);

    JS_EXPORT_PRIVATE static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

    JS_EXPORT_PRIVATE static uint32_t getEnumerableLength(ExecState*, JSObject*);
    JS_EXPORT_PRIVATE static void getStructurePropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getGenericPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

    JSValue toPrimitive(ExecState*, PreferredPrimitiveType = NoPreference) const;
    bool getPrimitiveNumber(ExecState*, double& number, JSValue&) const;
    JS_EXPORT_PRIVATE double toNumber(ExecState*) const;
    JS_EXPORT_PRIVATE JSString* toString(ExecState*) const;

    JS_EXPORT_PRIVATE static JSValue toThis(JSCell*, ExecState*, ECMAMode);

    // This get function only looks at the property map.
    JSValue getDirect(VM& vm, PropertyName propertyName) const
    {
        Structure* structure = this->structure(vm);
        PropertyOffset offset = structure->get(vm, propertyName);
        checkOffset(offset, structure->inlineCapacity());
        return offset != invalidOffset ? getDirect(offset) : JSValue();
    }
    
    JSValue getDirect(VM& vm, PropertyName propertyName, unsigned& attributes) const
    {
        Structure* structure = this->structure(vm);
        PropertyOffset offset = structure->get(vm, propertyName, attributes);
        checkOffset(offset, structure->inlineCapacity());
        return offset != invalidOffset ? getDirect(offset) : JSValue();
    }

    PropertyOffset getDirectOffset(VM& vm, PropertyName propertyName)
    {
        Structure* structure = this->structure(vm);
        PropertyOffset offset = structure->get(vm, propertyName);
        checkOffset(offset, structure->inlineCapacity());
        return offset;
    }

    PropertyOffset getDirectOffset(VM& vm, PropertyName propertyName, unsigned& attributes)
    {
        Structure* structure = this->structure(vm);
        PropertyOffset offset = structure->get(vm, propertyName, attributes);
        checkOffset(offset, structure->inlineCapacity());
        return offset;
    }

    bool hasInlineStorage() const { return structure()->hasInlineStorage(); }
    ConstPropertyStorage inlineStorageUnsafe() const
    {
        return bitwise_cast<ConstPropertyStorage>(this + 1);
    }
    PropertyStorage inlineStorageUnsafe()
    {
        return bitwise_cast<PropertyStorage>(this + 1);
    }
    ConstPropertyStorage inlineStorage() const
    {
        ASSERT(hasInlineStorage());
        return inlineStorageUnsafe();
    }
    PropertyStorage inlineStorage()
    {
        ASSERT(hasInlineStorage());
        return inlineStorageUnsafe();
    }
        
    const Butterfly* butterfly() const { return m_butterfly.get(this); }
    Butterfly* butterfly() { return m_butterfly.get(this); }
        
    ConstPropertyStorage outOfLineStorage() const { return m_butterfly.get(this)->propertyStorage(); }
    PropertyStorage outOfLineStorage() { return m_butterfly.get(this)->propertyStorage(); }

    const WriteBarrierBase<Unknown>* locationForOffset(PropertyOffset offset) const
    {
        if (isInlineOffset(offset))
            return &inlineStorage()[offsetInInlineStorage(offset)];
        return &outOfLineStorage()[offsetInOutOfLineStorage(offset)];
    }

    WriteBarrierBase<Unknown>* locationForOffset(PropertyOffset offset)
    {
        if (isInlineOffset(offset))
            return &inlineStorage()[offsetInInlineStorage(offset)];
        return &outOfLineStorage()[offsetInOutOfLineStorage(offset)];
    }

    void transitionTo(VM&, Structure*);

    JS_EXPORT_PRIVATE bool removeDirect(VM&, PropertyName); // Return true if anything is removed.
    bool hasCustomProperties() { return structure()->didTransition(); }
    bool hasGetterSetterProperties() { return structure()->hasGetterSetterProperties(); }
    bool hasCustomGetterSetterProperties() { return structure()->hasCustomGetterSetterProperties(); }

    // putOwnDataProperty has 'put' like semantics, however this method:
    //  - assumes the object contains no own getter/setter properties.
    //  - provides no special handling for __proto__
    //  - does not walk the prototype chain (to check for accessors or non-writable properties).
    // This is used by JSLexicalEnvironment.
    bool putOwnDataProperty(VM&, PropertyName, JSValue, PutPropertySlot&);

    // Fast access to known property offsets.
    JSValue getDirect(PropertyOffset offset) const { return locationForOffset(offset)->get(); }
    void putDirect(VM& vm, PropertyOffset offset, JSValue value) { locationForOffset(offset)->set(vm, this, value); }
    void putDirectUndefined(PropertyOffset offset) { locationForOffset(offset)->setUndefined(); }

    JS_EXPORT_PRIVATE void putDirectNativeIntrinsicGetter(VM&, JSGlobalObject*, Identifier, NativeFunction, Intrinsic, unsigned attributes);
    JS_EXPORT_PRIVATE void putDirectNativeFunction(VM&, JSGlobalObject*, const PropertyName&, unsigned functionLength, NativeFunction, Intrinsic, unsigned attributes);
    JS_EXPORT_PRIVATE JSFunction* putDirectBuiltinFunction(VM&, JSGlobalObject*, const PropertyName&, FunctionExecutable*, unsigned attributes);
    JSFunction* putDirectBuiltinFunctionWithoutTransition(VM&, JSGlobalObject*, const PropertyName&, FunctionExecutable*, unsigned attributes);
    JS_EXPORT_PRIVATE void putDirectNativeFunctionWithoutTransition(VM&, JSGlobalObject*, const PropertyName&, unsigned functionLength, NativeFunction, Intrinsic, unsigned attributes);

    JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool shouldThrow);

    bool isGlobalObject() const;
    bool isErrorInstance() const;
    bool isWithScope() const;

    JS_EXPORT_PRIVATE void seal(VM&);
    JS_EXPORT_PRIVATE void freeze(VM&);
    JS_EXPORT_PRIVATE void preventExtensions(VM&);
    bool isSealed(VM& vm) { return structure(vm)->isSealed(vm); }
    bool isFrozen(VM& vm) { return structure(vm)->isFrozen(vm); }
    bool isExtensible() { return structure()->isExtensible(); }
    bool indexingShouldBeSparse()
    {
        return !isExtensible()
            || structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero();
    }

    bool staticFunctionsReified() { return structure()->staticFunctionsReified(); }
    void reifyStaticFunctionsForDelete(ExecState* exec);

    JS_EXPORT_PRIVATE Butterfly* growOutOfLineStorage(VM&, size_t oldSize, size_t newSize);
    void setButterflyWithoutChangingStructure(VM&, Butterfly*);
        
    void setStructure(VM&, Structure*);
    void setStructureAndButterfly(VM&, Structure*, Butterfly*);
    void setStructureAndReallocateStorageIfNecessary(VM&, unsigned oldCapacity, Structure*);
    void setStructureAndReallocateStorageIfNecessary(VM&, Structure*);

    JS_EXPORT_PRIVATE void convertToDictionary(VM&);

    void flattenDictionaryObject(VM& vm)
    {
        structure(vm)->flattenDictionaryStructure(vm, this);
    }
    void shiftButterflyAfterFlattening(VM&, size_t outOfLineCapacityBefore, size_t outOfLineCapacityAfter);

    JSGlobalObject* globalObject() const
    {
        ASSERT(structure()->globalObject());
        ASSERT(!isGlobalObject() || ((JSObject*)structure()->globalObject()) == this);
        return structure()->globalObject();
    }
        
    void switchToSlowPutArrayStorage(VM&);
        
    // The receiver is the prototype in this case. The following:
    //
    // asObject(foo->structure()->storedPrototype())->attemptToInterceptPutByIndexOnHoleForPrototype(...)
    //
    // is equivalent to:
    //
    // foo->attemptToInterceptPutByIndexOnHole(...);
    bool attemptToInterceptPutByIndexOnHoleForPrototype(ExecState*, JSValue thisValue, unsigned propertyName, JSValue, bool shouldThrow);
        
    // Returns 0 if int32 storage cannot be created - either because
    // indexing should be sparse, we're having a bad time, or because
    // we already have a more general form of storage (double,
    // contiguous, array storage).
    ContiguousJSValues ensureInt32(VM& vm)
    {
        if (LIKELY(hasInt32(indexingType())))
            return m_butterfly.get(this)->contiguousInt32();
            
        return ensureInt32Slow(vm);
    }
        
    // Returns 0 if double storage cannot be created - either because
    // indexing should be sparse, we're having a bad time, or because
    // we already have a more general form of storage (contiguous,
    // or array storage).
    ContiguousDoubles ensureDouble(VM& vm)
    {
        if (LIKELY(hasDouble(indexingType())))
            return m_butterfly.get(this)->contiguousDouble();
            
        return ensureDoubleSlow(vm);
    }
        
    // Returns 0 if contiguous storage cannot be created - either because
    // indexing should be sparse or because we're having a bad time.
    ContiguousJSValues ensureContiguous(VM& vm)
    {
        if (LIKELY(hasContiguous(indexingType())))
            return m_butterfly.get(this)->contiguous();
            
        return ensureContiguousSlow(vm);
    }

    // Ensure that the object is in a mode where it has array storage. Use
    // this if you're about to perform actions that would have required the
    // object to be converted to have array storage, if it didn't have it
    // already.
    ArrayStorage* ensureArrayStorage(VM& vm)
    {
        if (LIKELY(hasAnyArrayStorage(indexingType())))
            return m_butterfly.get(this)->arrayStorage();

        return ensureArrayStorageSlow(vm);
    }
        
    static size_t offsetOfInlineStorage();
        
    static ptrdiff_t butterflyOffset()
    {
        return OBJECT_OFFSETOF(JSObject, m_butterfly);
    }
        
    void* butterflyAddress()
    {
        return &m_butterfly;
    }

    DECLARE_EXPORT_INFO;

protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(inherits(info()));
        ASSERT(prototype().isNull() || Heap::heap(this) == Heap::heap(prototype()));
        ASSERT(structure()->isObject());
        ASSERT(classInfo());
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    // To instantiate objects you likely want JSFinalObject, below.
    // To create derived types you likely want JSNonFinalObject, below.
    JSObject(VM&, Structure*, Butterfly* = 0);
        
    void visitButterfly(SlotVisitor&, Butterfly*, size_t storageSize);
    void copyButterfly(CopyVisitor&, Butterfly*, size_t storageSize);

    // Call this if you know that the object is in a mode where it has array
    // storage. This will assert otherwise.
    ArrayStorage* arrayStorage()
    {
        ASSERT(hasAnyArrayStorage(indexingType()));
        return m_butterfly.get(this)->arrayStorage();
    }
        
    // Call this if you want to predicate some actions on whether or not the
    // object is in a mode where it has array storage.
    ArrayStorage* arrayStorageOrNull()
    {
        switch (indexingType()) {
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return m_butterfly.get(this)->arrayStorage();
                
        default:
            return 0;
        }
    }
        
    size_t butterflyTotalSize();
    size_t butterflyPreCapacity();

    Butterfly* createInitialUndecided(VM&, unsigned length);
    ContiguousJSValues createInitialInt32(VM&, unsigned length);
    ContiguousDoubles createInitialDouble(VM&, unsigned length);
    ContiguousJSValues createInitialContiguous(VM&, unsigned length);
        
    void convertUndecidedForValue(VM&, JSValue);
    void createInitialForValueAndSet(VM&, unsigned index, JSValue);
    void convertInt32ForValue(VM&, JSValue);
        
    ArrayStorage* createArrayStorage(VM&, unsigned length, unsigned vectorLength);
    ArrayStorage* createInitialArrayStorage(VM&);
        
    ContiguousJSValues convertUndecidedToInt32(VM&);
    ContiguousDoubles convertUndecidedToDouble(VM&);
    ContiguousJSValues convertUndecidedToContiguous(VM&);
    ArrayStorage* convertUndecidedToArrayStorage(VM&, NonPropertyTransition);
    ArrayStorage* convertUndecidedToArrayStorage(VM&);
        
    ContiguousDoubles convertInt32ToDouble(VM&);
    ContiguousJSValues convertInt32ToContiguous(VM&);
    ArrayStorage* convertInt32ToArrayStorage(VM&, NonPropertyTransition);
    ArrayStorage* convertInt32ToArrayStorage(VM&);
    
    ContiguousJSValues convertDoubleToContiguous(VM&);
    ArrayStorage* convertDoubleToArrayStorage(VM&, NonPropertyTransition);
    ArrayStorage* convertDoubleToArrayStorage(VM&);
        
    ArrayStorage* convertContiguousToArrayStorage(VM&, NonPropertyTransition);
    ArrayStorage* convertContiguousToArrayStorage(VM&);

        
    ArrayStorage* ensureArrayStorageExistsAndEnterDictionaryIndexingMode(VM&);
        
    bool defineOwnNonIndexProperty(ExecState*, PropertyName, const PropertyDescriptor&, bool throwException);

    template<IndexingType indexingShape>
    void putByIndexBeyondVectorLengthWithoutAttributes(ExecState*, unsigned propertyName, JSValue);
    void putByIndexBeyondVectorLengthWithArrayStorage(ExecState*, unsigned propertyName, JSValue, bool shouldThrow, ArrayStorage*);

    bool increaseVectorLength(VM&, unsigned newLength);
    void deallocateSparseIndexMap();
    bool defineOwnIndexedProperty(ExecState*, unsigned, const PropertyDescriptor&, bool throwException);
    SparseArrayValueMap* allocateSparseIndexMap(VM&);
        
    void notifyPresenceOfIndexedAccessors(VM&);
        
    bool attemptToInterceptPutByIndexOnHole(ExecState*, unsigned index, JSValue, bool shouldThrow);
        
    // Call this if you want setIndexQuickly to succeed and you're sure that
    // the array is contiguous.
    void ensureLength(VM& vm, unsigned length)
    {
        ASSERT(length < MAX_ARRAY_INDEX);
        ASSERT(hasContiguous(indexingType()) || hasInt32(indexingType()) || hasDouble(indexingType()) || hasUndecided(indexingType()));

        if (m_butterfly.get(this)->vectorLength() < length)
            ensureLengthSlow(vm, length);
            
        if (m_butterfly.get(this)->publicLength() < length)
            m_butterfly.get(this)->setPublicLength(length);
    }
        
    // Call this if you want to shrink the butterfly backing store, and you're
    // sure that the array is contiguous.
    void reallocateAndShrinkButterfly(VM&, unsigned length);
    
    template<IndexingType indexingShape>
    unsigned countElements(Butterfly*);
        
    // This is relevant to undecided, int32, double, and contiguous.
    unsigned countElements();
        
private:
    friend class LLIntOffsetsExtractor;
        
    // Nobody should ever ask any of these questions on something already known to be a JSObject.
    using JSCell::isAPIValueWrapper;
    using JSCell::isGetterSetter;
    void getObject();
    void getString(ExecState* exec);
    void isObject();
    void isString();
        
    Butterfly* createInitialIndexedStorage(VM&, unsigned length, size_t elementSize);
        
    ArrayStorage* enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(VM&, ArrayStorage*);
        
    template<PutMode>
    bool putDirectInternal(VM&, PropertyName, JSValue, unsigned attr, PutPropertySlot&);

    JS_EXPORT_PRIVATE NEVER_INLINE void putInlineSlow(ExecState*, PropertyName, JSValue, PutPropertySlot&);

    bool inlineGetOwnPropertySlot(VM&, Structure&, PropertyName, PropertySlot&);
    JS_EXPORT_PRIVATE void fillGetterPropertySlot(PropertySlot&, JSValue, unsigned, PropertyOffset);
    void fillCustomGetterPropertySlot(PropertySlot&, JSValue, unsigned, Structure&);

    JS_EXPORT_PRIVATE const HashTableValue* findPropertyHashEntry(PropertyName) const;
        
    void putIndexedDescriptor(ExecState*, SparseArrayEntry*, const PropertyDescriptor&, PropertyDescriptor& old);
        
    void putByIndexBeyondVectorLength(ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
    bool putDirectIndexBeyondVectorLengthWithArrayStorage(ExecState*, unsigned propertyName, JSValue, unsigned attributes, PutDirectIndexMode, ArrayStorage*);
    JS_EXPORT_PRIVATE bool putDirectIndexBeyondVectorLength(ExecState*, unsigned propertyName, JSValue, unsigned attributes, PutDirectIndexMode);
        
    unsigned getNewVectorLength(unsigned currentVectorLength, unsigned currentLength, unsigned desiredLength);
    unsigned getNewVectorLength(unsigned desiredLength);

    ArrayStorage* constructConvertedArrayStorageWithoutCopyingElements(VM&, unsigned neededLength);
        
    JS_EXPORT_PRIVATE void setIndexQuicklyToUndecided(VM&, unsigned index, JSValue);
    JS_EXPORT_PRIVATE void convertInt32ToDoubleOrContiguousWhilePerformingSetIndex(VM&, unsigned index, JSValue);
    JS_EXPORT_PRIVATE void convertDoubleToContiguousWhilePerformingSetIndex(VM&, unsigned index, JSValue);
        
    void ensureLengthSlow(VM&, unsigned length);
        
    ContiguousJSValues ensureInt32Slow(VM&);
    ContiguousDoubles ensureDoubleSlow(VM&);
    ContiguousJSValues ensureContiguousSlow(VM&);
    JS_EXPORT_PRIVATE ArrayStorage* ensureArrayStorageSlow(VM&);

protected:
    CopyBarrier<Butterfly> m_butterfly;
#if USE(JSVALUE32_64)
private:
    uint32_t m_padding;
#endif
};

// JSNonFinalObject is a type of JSObject that has some internal storage,
// but also preserves some space in the collector cell for additional
// data members in derived types.
class JSNonFinalObject : public JSObject {
    friend class JSObject;

public:
    typedef JSObject Base;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

protected:
    explicit JSNonFinalObject(VM& vm, Structure* structure, Butterfly* butterfly = 0)
        : JSObject(vm, structure, butterfly)
    {
    }

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(!this->structure()->hasInlineStorage());
        ASSERT(classInfo());
    }
};

class JSFinalObject;

// JSFinalObject is a type of JSObject that contains sufficent internal
// storage to fully make use of the colloctor cell containing it.
class JSFinalObject : public JSObject {
    friend class JSObject;

public:
    typedef JSObject Base;
    static const unsigned StructureFlags = Base::StructureFlags;

    static size_t allocationSize(size_t inlineCapacity)
    {
        return sizeof(JSObject) + inlineCapacity * sizeof(WriteBarrierBase<Unknown>);
    }

    static inline const TypeInfo typeInfo() { return TypeInfo(FinalObjectType, StructureFlags); }
    static const IndexingType defaultIndexingType = NonArray;
        
    static const unsigned defaultSize = 64;
    static inline unsigned defaultInlineCapacity()
    {
        return (defaultSize - allocationSize(0)) / sizeof(WriteBarrier<Unknown>);
    }

    static const unsigned maxSize = 512;
    static inline unsigned maxInlineCapacity()
    {
        return (maxSize - allocationSize(0)) / sizeof(WriteBarrier<Unknown>);
    }

    static JSFinalObject* create(ExecState*, Structure*, Butterfly* = nullptr);
    static JSFinalObject* create(VM&, Structure*);
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, unsigned inlineCapacity)
    {
        return Structure::create(vm, globalObject, prototype, typeInfo(), info(), defaultIndexingType, inlineCapacity);
    }

    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);

    DECLARE_EXPORT_INFO;

protected:
    void visitChildrenCommon(SlotVisitor&);
        
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(structure()->totalStorageCapacity() == structure()->inlineCapacity());
        ASSERT(classInfo());
    }

private:
    friend class LLIntOffsetsExtractor;

    explicit JSFinalObject(VM& vm, Structure* structure, Butterfly* butterfly = nullptr)
        : JSObject(vm, structure, butterfly)
    {
    }
};

JS_EXPORT_PRIVATE EncodedJSValue JSC_HOST_CALL objectPrivateFuncInstanceOf(ExecState*);

inline JSFinalObject* JSFinalObject::create(
    ExecState* exec, Structure* structure, Butterfly* butterfly)
{
    JSFinalObject* finalObject = new (
        NotNull, 
        allocateCell<JSFinalObject>(
            *exec->heap(),
            allocationSize(structure->inlineCapacity())
        )
    ) JSFinalObject(exec->vm(), structure, butterfly);
    finalObject->finishCreation(exec->vm());
    return finalObject;
}

inline JSFinalObject* JSFinalObject::create(VM& vm, Structure* structure)
{
    JSFinalObject* finalObject = new (NotNull, allocateCell<JSFinalObject>(vm.heap, allocationSize(structure->inlineCapacity()))) JSFinalObject(vm, structure);
    finalObject->finishCreation(vm);
    return finalObject;
}

inline bool isJSFinalObject(JSCell* cell)
{
    return cell->classInfo() == JSFinalObject::info();
}

inline bool isJSFinalObject(JSValue value)
{
    return value.isCell() && isJSFinalObject(value.asCell());
}

inline size_t JSObject::offsetOfInlineStorage()
{
    return sizeof(JSObject);
}

inline bool JSObject::isGlobalObject() const
{
    return type() == GlobalObjectType;
}

inline bool JSObject::isErrorInstance() const
{
    return type() == ErrorInstanceType;
}

inline bool JSObject::isWithScope() const
{
    return type() == WithScopeType;
}

inline void JSObject::setStructureAndButterfly(VM& vm, Structure* structure, Butterfly* butterfly)
{
    ASSERT(structure);
    ASSERT(!butterfly == (!structure->outOfLineCapacity() && !structure->hasIndexingHeader(this)));
    m_butterfly.set(vm, this, butterfly);
    setStructure(vm, structure);
}

inline void JSObject::setStructure(VM& vm, Structure* structure)
{
    ASSERT(structure);
    ASSERT(!m_butterfly == !(structure->outOfLineCapacity() || structure->hasIndexingHeader(this)));
    JSCell::setStructure(vm, structure);
}

inline void JSObject::setButterflyWithoutChangingStructure(VM& vm, Butterfly* butterfly)
{
    m_butterfly.set(vm, this, butterfly);
}

inline CallType getCallData(JSValue value, CallData& callData)
{
    CallType result = value.isCell() ? value.asCell()->methodTable()->getCallData(value.asCell(), callData) : CallTypeNone;
    ASSERT(result == CallTypeNone || value.isValidCallee());
    return result;
}

inline ConstructType getConstructData(JSValue value, ConstructData& constructData)
{
    ConstructType result = value.isCell() ? value.asCell()->methodTable()->getConstructData(value.asCell(), constructData) : ConstructTypeNone;
    ASSERT(result == ConstructTypeNone || value.isValidCallee());
    return result;
}

inline JSObject* asObject(JSCell* cell)
{
    ASSERT(cell->isObject());
    return jsCast<JSObject*>(cell);
}

inline JSObject* asObject(JSValue value)
{
    return asObject(value.asCell());
}

inline JSObject::JSObject(VM& vm, Structure* structure, Butterfly* butterfly)
    : JSCell(vm, structure)
    , m_butterfly(vm, this, butterfly)
{
    vm.heap.ascribeOwner(this, butterfly);
}

inline JSValue JSObject::prototype() const
{
    return structure()->storedPrototype();
}

ALWAYS_INLINE bool JSObject::inlineGetOwnPropertySlot(VM& vm, Structure& structure, PropertyName propertyName, PropertySlot& slot)
{
    unsigned attributes;
    PropertyOffset offset = structure.get(vm, propertyName, attributes);
    if (!isValidOffset(offset))
        return false;

    JSValue value = getDirect(offset);
    if (value.isCell()) {
        ASSERT(value);
        JSCell* cell = value.asCell();
        JSType type = cell->type();
        switch (type) {
        case GetterSetterType:
            fillGetterPropertySlot(slot, value, attributes, offset);
            return true;
        case CustomGetterSetterType:
            fillCustomGetterPropertySlot(slot, value, attributes, structure);
            return true;
        default:
            break;
        }
    }
    
    slot.setValue(this, attributes, value, offset);
    return true;
}

ALWAYS_INLINE void JSObject::fillCustomGetterPropertySlot(PropertySlot& slot, JSValue customGetterSetter, unsigned attributes, Structure& structure)
{
    if (structure.isDictionary()) {
        slot.setCustom(this, attributes, jsCast<CustomGetterSetter*>(customGetterSetter)->getter());
        return;
    }
    slot.setCacheableCustom(this, attributes, jsCast<CustomGetterSetter*>(customGetterSetter)->getter());
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    Structure& structure = *object->structure(vm);
    if (object->inlineGetOwnPropertySlot(vm, structure, propertyName, slot))
        return true;
    if (Optional<uint32_t> index = parseIndex(propertyName))
        return getOwnPropertySlotByIndex(object, exec, index.value(), slot);
    return false;
}

ALWAYS_INLINE bool JSObject::fastGetOwnPropertySlot(ExecState* exec, VM& vm, Structure& structure, PropertyName propertyName, PropertySlot& slot)
{
    if (LIKELY(!TypeInfo::overridesGetOwnPropertySlot(inlineTypeFlags())))
        return inlineGetOwnPropertySlot(vm, structure, propertyName, slot);
    return structure.classInfo()->methodTable.getOwnPropertySlot(this, exec, propertyName, slot);
}

// It may seem crazy to inline a function this large but it makes a big difference
// since this is function very hot in variable lookup
ALWAYS_INLINE bool JSObject::getPropertySlot(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    auto& structureIDTable = vm.heap.structureIDTable();
    JSObject* object = this;
    while (true) {
        Structure& structure = *structureIDTable.get(object->structureID());
        if (object->fastGetOwnPropertySlot(exec, vm, structure, propertyName, slot))
            return true;
        JSValue prototype = structure.storedPrototype();
        if (!prototype.isObject())
            break;
        object = asObject(prototype);
    }

    if (Optional<uint32_t> index = parseIndex(propertyName))
        return getPropertySlot(exec, index.value(), slot);
    return false;
}

ALWAYS_INLINE bool JSObject::getPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    auto& structureIDTable = vm.heap.structureIDTable();
    JSObject* object = this;
    while (true) {
        Structure& structure = *structureIDTable.get(object->structureID());
        if (structure.classInfo()->methodTable.getOwnPropertySlotByIndex(object, exec, propertyName, slot))
            return true;
        JSValue prototype = structure.storedPrototype();
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }
}

inline JSValue JSObject::get(ExecState* exec, PropertyName propertyName) const
{
    PropertySlot slot(this);
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);
    
    return jsUndefined();
}

inline JSValue JSObject::get(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot(this);
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);

    return jsUndefined();
}

template<JSObject::PutMode mode>
ALWAYS_INLINE bool JSObject::putDirectInternal(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(value.isGetterSetter() == !!(attributes & Accessor));
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    ASSERT(!parseIndex(propertyName));

    Structure* structure = this->structure(vm);
    if (structure->isDictionary()) {
        ASSERT(!structure->hasInferredTypes());
        
        unsigned currentAttributes;
        PropertyOffset offset = structure->get(vm, propertyName, currentAttributes);
        if (offset != invalidOffset) {
            if ((mode == PutModePut) && currentAttributes & ReadOnly)
                return false;

            putDirect(vm, offset, value);
            structure->didReplaceProperty(offset);
            slot.setExistingProperty(this, offset);

            if ((attributes & Accessor) != (currentAttributes & Accessor)) {
                ASSERT(!(attributes & ReadOnly));
                setStructure(vm, Structure::attributeChangeTransition(vm, structure, propertyName, attributes));
            }
            return true;
        }

        if ((mode == PutModePut) && !isExtensible())
            return false;

        DeferGC deferGC(vm.heap);
        Butterfly* newButterfly = butterfly();
        if (this->structure()->putWillGrowOutOfLineStorage())
            newButterfly = growOutOfLineStorage(vm, this->structure()->outOfLineCapacity(), this->structure()->suggestedNewOutOfLineStorageCapacity());
        offset = this->structure()->addPropertyWithoutTransition(vm, propertyName, attributes);
        setStructureAndButterfly(vm, this->structure(), newButterfly);

        validateOffset(offset);
        ASSERT(this->structure()->isValidOffset(offset));
        putDirect(vm, offset, value);
        slot.setNewProperty(this, offset);
        if (attributes & ReadOnly)
            this->structure()->setContainsReadOnlyProperties();
        return true;
    }

    PropertyOffset offset;
    size_t currentCapacity = this->structure()->outOfLineCapacity();
    Structure* newStructure = Structure::addPropertyTransitionToExistingStructure(
        structure, propertyName, attributes, offset);
    if (newStructure) {
        newStructure->willStoreValueForExistingTransition(
            vm, propertyName, value, slot.context() == PutPropertySlot::PutById);
        
        DeferGC deferGC(vm.heap);
        Butterfly* newButterfly = butterfly();
        if (currentCapacity != newStructure->outOfLineCapacity()) {
            ASSERT(newStructure != this->structure());
            newButterfly = growOutOfLineStorage(vm, currentCapacity, newStructure->outOfLineCapacity());
        }

        validateOffset(offset);
        ASSERT(newStructure->isValidOffset(offset));
        setStructureAndButterfly(vm, newStructure, newButterfly);
        putDirect(vm, offset, value);
        slot.setNewProperty(this, offset);
        return true;
    }

    unsigned currentAttributes;
    bool hasInferredType;
    offset = structure->get(vm, propertyName, currentAttributes, hasInferredType);
    if (offset != invalidOffset) {
        if ((mode == PutModePut) && currentAttributes & ReadOnly)
            return false;

        structure->didReplaceProperty(offset);
        if (UNLIKELY(hasInferredType)) {
            structure->willStoreValueForReplace(
                vm, propertyName, value, slot.context() == PutPropertySlot::PutById);
        }

        slot.setExistingProperty(this, offset);
        putDirect(vm, offset, value);

        if ((attributes & Accessor) != (currentAttributes & Accessor)) {
            ASSERT(!(attributes & ReadOnly));
            setStructure(vm, Structure::attributeChangeTransition(vm, structure, propertyName, attributes));
        }
        return true;
    }

    if ((mode == PutModePut) && !isExtensible())
        return false;

    // We want the structure transition watchpoint to fire after this object has switched
    // structure. This allows adaptive watchpoints to observe if the new structure is the one
    // we want.
    DeferredStructureTransitionWatchpointFire deferredWatchpointFire;
    
    newStructure = Structure::addPropertyTransition(
        vm, structure, propertyName, attributes, offset, slot.context(), &deferredWatchpointFire);
    newStructure->willStoreValueForNewTransition(
        vm, propertyName, value, slot.context() == PutPropertySlot::PutById);
    
    validateOffset(offset);
    ASSERT(newStructure->isValidOffset(offset));
    setStructureAndReallocateStorageIfNecessary(vm, newStructure);

    putDirect(vm, offset, value);
    slot.setNewProperty(this, offset);
    if (attributes & ReadOnly)
        newStructure->setContainsReadOnlyProperties();
    return true;
}

inline void JSObject::setStructureAndReallocateStorageIfNecessary(VM& vm, unsigned oldCapacity, Structure* newStructure)
{
    ASSERT(oldCapacity <= newStructure->outOfLineCapacity());
    
    if (oldCapacity == newStructure->outOfLineCapacity()) {
        setStructure(vm, newStructure);
        return;
    }

    DeferGC deferGC(vm.heap); 
    Butterfly* newButterfly = growOutOfLineStorage(
        vm, oldCapacity, newStructure->outOfLineCapacity());
    setStructureAndButterfly(vm, newStructure, newButterfly);
}

inline void JSObject::setStructureAndReallocateStorageIfNecessary(VM& vm, Structure* newStructure)
{
    setStructureAndReallocateStorageIfNecessary(
        vm, structure(vm)->outOfLineCapacity(), newStructure);
}

inline bool JSObject::putOwnDataProperty(VM& vm, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    ASSERT(!structure()->hasGetterSetterProperties());
    ASSERT(!structure()->hasCustomGetterSetterProperties());

    return putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot);
}

inline void JSObject::putDirect(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!value.isGetterSetter() && !(attributes & Accessor));
    ASSERT(!value.isCustomGetterSetter());
    PutPropertySlot slot(this);
    putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, value, attributes, slot);
}

inline void JSObject::putDirect(VM& vm, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    ASSERT(!value.isGetterSetter());
    ASSERT(!value.isCustomGetterSetter());
    putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, value, 0, slot);
}

inline void JSObject::putDirectWithoutTransition(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes)
{
    DeferGC deferGC(vm.heap);
    ASSERT(!value.isGetterSetter() && !(attributes & Accessor));
    ASSERT(!value.isCustomGetterSetter());
    Butterfly* newButterfly = m_butterfly.get(this);
    if (structure()->putWillGrowOutOfLineStorage())
        newButterfly = growOutOfLineStorage(vm, structure()->outOfLineCapacity(), structure()->suggestedNewOutOfLineStorageCapacity());
    Structure* structure = this->structure();
    PropertyOffset offset = structure->addPropertyWithoutTransition(vm, propertyName, attributes);
    bool shouldOptimize = false;
    structure->willStoreValueForNewTransition(vm, propertyName, value, shouldOptimize);
    setStructureAndButterfly(vm, structure, newButterfly);
    putDirect(vm, offset, value);
}

inline JSValue JSObject::toPrimitive(ExecState* exec, PreferredPrimitiveType preferredType) const
{
    return methodTable()->defaultValue(this, exec, preferredType);
}

ALWAYS_INLINE JSObject* Register::object() const
{
    return asObject(jsValue());
}

ALWAYS_INLINE Register& Register::operator=(JSObject* object)
{
    u.value = JSValue::encode(JSValue(object));
    return *this;
}

inline size_t offsetInButterfly(PropertyOffset offset)
{
    return offsetInOutOfLineStorage(offset) + Butterfly::indexOfPropertyStorage();
}

inline size_t JSObject::butterflyPreCapacity()
{
    if (UNLIKELY(hasIndexingHeader()))
        return butterfly()->indexingHeader()->preCapacity(structure());
    return 0;
}

inline size_t JSObject::butterflyTotalSize()
{
    Structure* structure = this->structure();
    Butterfly* butterfly = this->butterfly();
    size_t preCapacity;
    size_t indexingPayloadSizeInBytes;
    bool hasIndexingHeader = this->hasIndexingHeader();

    if (UNLIKELY(hasIndexingHeader)) {
        preCapacity = butterfly->indexingHeader()->preCapacity(structure);
        indexingPayloadSizeInBytes = butterfly->indexingHeader()->indexingPayloadSizeInBytes(structure);
    } else {
        preCapacity = 0;
        indexingPayloadSizeInBytes = 0;
    }

    return Butterfly::totalSize(preCapacity, structure->outOfLineCapacity(), hasIndexingHeader, indexingPayloadSizeInBytes);
}

inline int indexRelativeToBase(PropertyOffset offset)
{
    if (isOutOfLineOffset(offset))
        return offsetInOutOfLineStorage(offset) + Butterfly::indexOfPropertyStorage();
    ASSERT(!(JSObject::offsetOfInlineStorage() % sizeof(EncodedJSValue)));
    return JSObject::offsetOfInlineStorage() / sizeof(EncodedJSValue) + offsetInInlineStorage(offset);
}

inline int offsetRelativeToBase(PropertyOffset offset)
{
    if (isOutOfLineOffset(offset))
        return offsetInOutOfLineStorage(offset) * sizeof(EncodedJSValue) + Butterfly::offsetOfPropertyStorage();
    return JSObject::offsetOfInlineStorage() + offsetInInlineStorage(offset) * sizeof(EncodedJSValue);
}

// Returns the maximum offset (away from zero) a load instruction will encode.
inline size_t maxOffsetRelativeToBase(PropertyOffset offset)
{
    ptrdiff_t addressOffset = offsetRelativeToBase(offset);
#if USE(JSVALUE32_64)
    if (addressOffset >= 0)
        return static_cast<size_t>(addressOffset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag);
#endif
    return static_cast<size_t>(addressOffset);
}

COMPILE_ASSERT(!(sizeof(JSObject) % sizeof(WriteBarrierBase<Unknown>)), JSObject_inline_storage_has_correct_alignment);

ALWAYS_INLINE Identifier makeIdentifier(VM& vm, const char* name)
{
    return Identifier::fromString(&vm, name);
}

ALWAYS_INLINE Identifier makeIdentifier(VM&, const Identifier& name)
{
    return name;
}

// Helper for defining native functions, if you're not using a static hash table.
// Use this macro from within finishCreation() methods in prototypes. This assumes
// you've defined variables called exec, globalObject, and vm, and they
// have the expected meanings.
#define JSC_NATIVE_INTRINSIC_FUNCTION(jsName, cppName, attributes, length, intrinsic) \
    putDirectNativeFunction(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (length), cppName, \
        (intrinsic), (attributes))

#define JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(jsName, cppName, attributes, length, intrinsic) \
    putDirectNativeFunctionWithoutTransition(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (length), cppName, \
        (intrinsic), (attributes))

// As above, but this assumes that the function you're defining doesn't have an
// intrinsic.
#define JSC_NATIVE_FUNCTION(jsName, cppName, attributes, length) \
    JSC_NATIVE_INTRINSIC_FUNCTION(jsName, cppName, (attributes), (length), NoIntrinsic)

#define JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(jsName, cppName, attributes, length) \
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(jsName, cppName, (attributes), (length), NoIntrinsic)

// Identical helpers but for builtins. Note that currently, we don't support builtins that are
// also intrinsics, but we probably will do that eventually.
#define JSC_BUILTIN_FUNCTION(jsName, generatorName, attributes) \
    putDirectBuiltinFunction(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (generatorName)(vm), (attributes))

#define JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(jsName, generatorName, attributes) \
    putDirectBuiltinFunctionWithoutTransition(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (generatorName)(vm), (attributes))

// Helper for defining native getters on properties.
#define JSC_NATIVE_INTRINSIC_GETTER(jsName, cppName, attributes, intrinsic)  \
    putDirectNativeIntrinsicGetter(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (cppName), \
        (intrinsic), ((attributes) | Accessor))

#define JSC_NATIVE_GETTER(jsName, cppName, attributes) \
    JSC_NATIVE_INTRINSIC_GETTER((jsName), (cppName), (attributes), NoIntrinsic)


} // namespace JSC

#endif // JSObject_h
