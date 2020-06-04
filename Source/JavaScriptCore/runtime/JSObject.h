/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "ArrayConventions.h"
#include "ArrayStorage.h"
#include "Butterfly.h"
#include "CPU.h"
#include "CagedBarrierPtr.h"
#include "CallFrame.h"
#include "ClassInfo.h"
#include "CustomGetterSetter.h"
#include "DOMAttributeGetterSetter.h"
#include "DeletePropertySlot.h"
#include "Heap.h"
#include "IndexingHeaderInlines.h"
#include "JSCast.h"
#include "MathCommon.h"
#include "ObjectInitializationScope.h"
#include "PropertySlot.h"
#include "PropertyStorage.h"
#include "PutDirectIndexMode.h"
#include "PutPropertySlot.h"
#include "Structure.h"
#include "StructureTransitionTable.h"
#include "VM.h"
#include "JSString.h"
#include "SparseArrayValueMap.h"
#include <wtf/StdLibExtras.h>

namespace JSC {
namespace DOMJIT {
class Signature;
}

inline JSCell* getJSFunction(JSValue value)
{
    if (value.isCell() && (value.asCell()->type() == JSFunctionType))
        return value.asCell();
    return nullptr;
}

class Exception;
class GetterSetter;
class InternalFunction;
class JSFunction;
class LLIntOffsetsExtractor;
class MarkedBlock;
class PropertyDescriptor;
class PropertyNameArray;
class Structure;
class ThrowScope;
struct HashTable;
struct HashTableValue;

JS_EXPORT_PRIVATE bool isTerminatedExecutionException(VM&, Exception*);
JS_EXPORT_PRIVATE Exception* throwTypeError(JSGlobalObject*, ThrowScope&, const String&);
extern JS_EXPORT_PRIVATE const ASCIILiteral NonExtensibleObjectPropertyDefineError;
extern JS_EXPORT_PRIVATE const ASCIILiteral ReadonlyPropertyWriteError;
extern JS_EXPORT_PRIVATE const ASCIILiteral ReadonlyPropertyChangeError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnableToDeletePropertyError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnconfigurablePropertyChangeAccessMechanismError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnconfigurablePropertyChangeConfigurabilityError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnconfigurablePropertyChangeEnumerabilityError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnconfigurablePropertyChangeWritabilityError;

COMPILE_ASSERT(PropertyAttribute::None < FirstInternalAttribute, None_is_below_FirstInternalAttribute);
COMPILE_ASSERT(PropertyAttribute::ReadOnly < FirstInternalAttribute, ReadOnly_is_below_FirstInternalAttribute);
COMPILE_ASSERT(PropertyAttribute::DontEnum < FirstInternalAttribute, DontEnum_is_below_FirstInternalAttribute);
COMPILE_ASSERT(PropertyAttribute::DontDelete < FirstInternalAttribute, DontDelete_is_below_FirstInternalAttribute);
COMPILE_ASSERT(PropertyAttribute::Accessor < FirstInternalAttribute, Accessor_is_below_FirstInternalAttribute);

class JSFinalObject;

class JSObject : public JSCell {
    friend class BatchedTransitionOptimizer;
    friend class JIT;
    friend class JSCell;
    friend class JSFinalObject;
    friend class MarkedBlock;
    JS_EXPORT_PRIVATE friend bool setUpStaticFunctionSlot(VM&, const HashTableValue*, JSObject*, PropertyName, PropertySlot&);

    enum PutMode {
        PutModePut,
        PutModeDefineOwnProperty,
    };

public:
    using Base = JSCell;

    // This is a super dangerous method for JITs. Sometimes the JITs will want to create either a
    // JSFinalObject or a JSArray. This is the method that will do that.
    static JSObject* createRawObject(VM& vm, Structure* structure, Butterfly* = nullptr);

    JS_EXPORT_PRIVATE static size_t estimatedSize(JSCell*, VM&);
    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);
    JS_EXPORT_PRIVATE static void analyzeHeap(JSCell*, HeapAnalyzer&);

    JS_EXPORT_PRIVATE static String className(const JSObject*, VM&);
    JS_EXPORT_PRIVATE static String calculatedClassName(JSObject*);

    // This function is what Object.prototype.toString() will use to get the name of
    // an object when using Symbol.toStringTag fails. For the most part there is no
    // difference between this and className(). The main use case is for new JS language
    // objects to set the default tag to "Object".
    JS_EXPORT_PRIVATE static String toStringName(const JSObject*, JSGlobalObject*);

    // This is the fully virtual [[GetPrototypeOf]] internal function defined
    // in the ECMAScript 6 specification. Use this when doing a [[GetPrototypeOf]] 
    // operation as dictated in the specification.
    JSValue getPrototype(VM&, JSGlobalObject*);
    JS_EXPORT_PRIVATE static JSValue getPrototype(JSObject*, JSGlobalObject*);
    // This gets the prototype directly off of the structure. This does not do
    // dynamic dispatch on the getPrototype method table method. It is not valid 
    // to use this when performing a [[GetPrototypeOf]] operation in the specification.
    // It is valid to use though when you know that you want to directly get it
    // without consulting the method table. This is akin to getting the [[Prototype]]
    // internal field directly as described in the specification.
    JSValue getPrototypeDirect(VM&) const;

    // This sets the prototype without checking for cycles and without
    // doing dynamic dispatch on [[SetPrototypeOf]] operation in the specification.
    // It is not valid to use this when performing a [[SetPrototypeOf]] operation in
    // the specification. It is valid to use though when you know that you want to directly
    // set it without consulting the method table and when you definitely won't
    // introduce a cycle in the prototype chain. This is akin to setting the
    // [[Prototype]] internal field directly as described in the specification.
    JS_EXPORT_PRIVATE void setPrototypeDirect(VM&, JSValue prototype);
private:
    // This is OrdinarySetPrototypeOf in the specification. Section 9.1.2.1
    // https://tc39.github.io/ecma262/#sec-ordinarysetprototypeof
    JS_EXPORT_PRIVATE bool setPrototypeWithCycleCheck(VM&, JSGlobalObject*, JSValue prototype, bool shouldThrowIfCantSet);
public:
    // This is the fully virtual [[SetPrototypeOf]] internal function defined
    // in the ECMAScript 6 specification. Use this when doing a [[SetPrototypeOf]] 
    // operation as dictated in the specification.
    bool setPrototype(VM&, JSGlobalObject*, JSValue prototype, bool shouldThrowIfCantSet = false);
    JS_EXPORT_PRIVATE static bool setPrototype(JSObject*, JSGlobalObject*, JSValue prototype, bool shouldThrowIfCantSet);
        
    inline bool mayInterceptIndexedAccesses(VM&);

    JSValue get(JSGlobalObject*, PropertyName) const;
    JSValue get(JSGlobalObject*, unsigned propertyName) const;
    JSValue get(JSGlobalObject*, uint64_t propertyName) const;

    template<bool checkNullStructure = false>
    bool getPropertySlot(JSGlobalObject*, PropertyName, PropertySlot&);
    bool getPropertySlot(JSGlobalObject*, unsigned propertyName, PropertySlot&);
    bool getPropertySlot(JSGlobalObject*, uint64_t propertyName, PropertySlot&);
    template<typename CallbackWhenNoException> typename std::result_of<CallbackWhenNoException(bool, PropertySlot&)>::type getPropertySlot(JSGlobalObject*, PropertyName, CallbackWhenNoException) const;
    template<typename CallbackWhenNoException> typename std::result_of<CallbackWhenNoException(bool, PropertySlot&)>::type getPropertySlot(JSGlobalObject*, PropertyName, PropertySlot&, CallbackWhenNoException) const;

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    JS_EXPORT_PRIVATE static bool getOwnPropertySlotByIndex(JSObject*, JSGlobalObject*, unsigned propertyName, PropertySlot&);
    bool getOwnPropertySlotInline(JSGlobalObject*, PropertyName, PropertySlot&);
    static void doPutPropertySecurityCheck(JSObject*, JSGlobalObject*, PropertyName, PutPropertySlot&);

    // The key difference between this and getOwnPropertySlot is that getOwnPropertySlot
    // currently returns incorrect results for the DOM window (with non-own properties)
    // being returned. Once this is fixed we should migrate code & remove this method.
    JS_EXPORT_PRIVATE bool getOwnPropertyDescriptor(JSGlobalObject*, PropertyName, PropertyDescriptor&);

    unsigned getArrayLength() const
    {
        if (!hasIndexedProperties(indexingType()))
            return 0;
        return m_butterfly->publicLength();
    }
        
    unsigned getVectorLength()
    {
        if (!hasIndexedProperties(indexingType()))
            return 0;
        return m_butterfly->vectorLength();
    }
    
    static bool putInlineForJSObject(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    
    JS_EXPORT_PRIVATE static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    // putByIndex assumes that the receiver is this JSCell object.
    JS_EXPORT_PRIVATE static bool putByIndex(JSCell*, JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow);
        
    // This performs the ECMAScript Set() operation.
    ALWAYS_INLINE bool putByIndexInline(JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
    {
        VM& vm = getVM(globalObject);
        if (canSetIndexQuickly(propertyName, value)) {
            setIndexQuickly(vm, propertyName, value);
            return true;
        }
        return methodTable(vm)->putByIndex(this, globalObject, propertyName, value, shouldThrow);
    }

    ALWAYS_INLINE bool putByIndexInline(JSGlobalObject* globalObject, uint64_t propertyName, JSValue value, bool shouldThrow)
    {
        if (LIKELY(propertyName <= MAX_ARRAY_INDEX))
            return putByIndexInline(globalObject, static_cast<uint32_t>(propertyName), value, shouldThrow);

        ASSERT(propertyName <= maxSafeInteger());
        PutPropertySlot slot(this, shouldThrow);
        return putInlineForJSObject(this, globalObject, Identifier::from(getVM(globalObject), propertyName), value, slot);
    }
        
    // This is similar to the putDirect* methods:
    //  - the prototype chain is not consulted
    //  - accessors are not called.
    //  - it will ignore extensibility and read-only properties if PutDirectIndexLikePutDirect is passed as the mode (the default).
    // This method creates a property with attributes writable, enumerable and configurable all set to true if attributes is zero,
    // otherwise, it creates a property with the provided attributes. Semantically, this is performing defineOwnProperty.
    bool putDirectIndex(JSGlobalObject* globalObject, unsigned propertyName, JSValue value, unsigned attributes, PutDirectIndexMode mode)
    {
        ASSERT(!value.isCustomGetterSetter());
        auto canSetIndexQuicklyForPutDirect = [&] () -> bool {
            switch (indexingMode()) {
            case ALL_BLANK_INDEXING_TYPES:
            case ALL_UNDECIDED_INDEXING_TYPES:
                return false;
            case ALL_WRITABLE_INT32_INDEXING_TYPES:
            case ALL_WRITABLE_DOUBLE_INDEXING_TYPES:
            case ALL_WRITABLE_CONTIGUOUS_INDEXING_TYPES:
            case ALL_ARRAY_STORAGE_INDEXING_TYPES:
                return propertyName < m_butterfly->vectorLength();
            default:
                if (isCopyOnWrite(indexingMode()))
                    return false;
                RELEASE_ASSERT_NOT_REACHED();
                return false;
            }
        };
        
        if (!attributes && canSetIndexQuicklyForPutDirect()) {
            setIndexQuickly(getVM(globalObject), propertyName, value);
            return true;
        }
        return putDirectIndexSlowOrBeyondVectorLength(globalObject, propertyName, value, attributes, mode);
    }
    // This is semantically equivalent to performing defineOwnProperty(propertyName, {configurable:true, writable:true, enumerable:true, value:value}).
    bool putDirectIndex(JSGlobalObject* globalObject, unsigned propertyName, JSValue value)
    {
        return putDirectIndex(globalObject, propertyName, value, 0, PutDirectIndexLikePutDirect);
    }

    ALWAYS_INLINE bool putDirectIndex(JSGlobalObject* globalObject, uint64_t propertyName, JSValue value, unsigned attributes, PutDirectIndexMode mode)
    {
        if (LIKELY(propertyName <= MAX_ARRAY_INDEX))
            return putDirectIndex(globalObject, static_cast<uint32_t>(propertyName), value, attributes, mode);
        return putDirect(getVM(globalObject), Identifier::from(getVM(globalObject), propertyName), value, attributes);
    }

    // A generally non-throwing version of putDirect and putDirectIndex.
    // However, it's only guaranteed to not throw based on what the receiver is.
    // For example, if the receiver is a ProxyObject, this is not guaranteed, since
    // it may call into arbitrary JS code. It's the responsibility of the user of
    // this API to ensure that the receiver object is a well known type if they
    // want to ensure that this won't throw an exception.
    JS_EXPORT_PRIVATE bool putDirectMayBeIndex(JSGlobalObject*, PropertyName, JSValue);
        
    bool hasIndexingHeader(VM& vm) const
    {
        return structure(vm)->hasIndexingHeader(this);
    }

    bool canGetIndexQuicklyForTypedArray(unsigned) const;
    JSValue getIndexQuicklyForTypedArray(unsigned) const;
    
    bool canGetIndexQuickly(unsigned i) const
    {
        const Butterfly* butterfly = this->butterfly();
        switch (indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
            return canGetIndexQuicklyForTypedArray(i);
        case ALL_UNDECIDED_INDEXING_TYPES:
            return false;
        case ALL_INT32_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            return i < butterfly->vectorLength() && butterfly->contiguous().at(this, i);
        case ALL_DOUBLE_INDEXING_TYPES: {
            if (i >= butterfly->vectorLength())
                return false;
            double value = butterfly->contiguousDouble().at(this, i);
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

    bool canGetIndexQuickly(uint64_t i) const
    {
        ASSERT(i <= maxSafeInteger());
        if (LIKELY(i <= MAX_ARRAY_INDEX))
            return canGetIndexQuickly(static_cast<uint32_t>(i));
        return false;
    }
        
    JSValue getIndexQuickly(unsigned i) const
    {
        const Butterfly* butterfly = this->butterfly();
        switch (indexingType()) {
        case ALL_INT32_INDEXING_TYPES:
            return jsNumber(butterfly->contiguous().at(this, i).get().asInt32());
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            return butterfly->contiguous().at(this, i).get();
        case ALL_DOUBLE_INDEXING_TYPES:
            return JSValue(JSValue::EncodeAsDouble, butterfly->contiguousDouble().at(this, i));
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return butterfly->arrayStorage()->m_vector[i].get();
        case ALL_BLANK_INDEXING_TYPES:
            return getIndexQuicklyForTypedArray(i);
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return JSValue();
        }
    }
        
    JSValue tryGetIndexQuickly(unsigned i) const
    {
        const Butterfly* butterfly = this->butterfly();
        switch (indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
            if (canGetIndexQuicklyForTypedArray(i))
                return getIndexQuicklyForTypedArray(i);
            break;
        case ALL_UNDECIDED_INDEXING_TYPES:
            break;
        case ALL_INT32_INDEXING_TYPES:
            if (i < butterfly->publicLength()) {
                JSValue result = butterfly->contiguous().at(this, i).get();
                ASSERT(result.isInt32() || !result);
                return result;
            }
            break;
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            if (i < butterfly->publicLength())
                return butterfly->contiguous().at(this, i).get();
            break;
        case ALL_DOUBLE_INDEXING_TYPES: {
            if (i >= butterfly->publicLength())
                break;
            double result = butterfly->contiguousDouble().at(this, i);
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

    JSValue tryGetIndexQuickly(uint64_t i) const
    {
        ASSERT(i <= maxSafeInteger());
        if (LIKELY(i <= MAX_ARRAY_INDEX))
            return tryGetIndexQuickly(static_cast<uint32_t>(i));
        return JSValue();
    }
        
    JSValue getDirectIndex(JSGlobalObject* globalObject, unsigned i)
    {
        if (JSValue result = tryGetIndexQuickly(i))
            return result;
        PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
        if (methodTable(getVM(globalObject))->getOwnPropertySlotByIndex(this, globalObject, i, slot))
            return slot.getValue(globalObject, i);
        return JSValue();
    }
        
    JSValue getIndex(JSGlobalObject* globalObject, unsigned i) const
    {
        if (JSValue result = tryGetIndexQuickly(i))
            return result;
        return get(globalObject, i);
    }

    bool canSetIndexQuicklyForTypedArray(unsigned, JSValue) const;
    void setIndexQuicklyForTypedArray(unsigned, JSValue);
        
    bool canSetIndexQuickly(unsigned i, JSValue value)
    {
        Butterfly* butterfly = this->butterfly();
        switch (indexingMode()) {
        case ALL_BLANK_INDEXING_TYPES:
            return canSetIndexQuicklyForTypedArray(i, value);
        case ALL_UNDECIDED_INDEXING_TYPES:
            return false;
        case ALL_WRITABLE_INT32_INDEXING_TYPES:
        case ALL_WRITABLE_DOUBLE_INDEXING_TYPES:
        case ALL_WRITABLE_CONTIGUOUS_INDEXING_TYPES:
        case NonArrayWithArrayStorage:
        case ArrayWithArrayStorage:
            return i < butterfly->vectorLength();
        case NonArrayWithSlowPutArrayStorage:
        case ArrayWithSlowPutArrayStorage:
            return i < butterfly->arrayStorage()->vectorLength()
                && !!butterfly->arrayStorage()->m_vector[i];
        default:
            if (isCopyOnWrite(indexingMode()))
                return false;
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }
        
    void setIndexQuickly(VM& vm, unsigned i, JSValue v)
    {
        Butterfly* butterfly = m_butterfly.get();
        ASSERT(!isCopyOnWrite(indexingMode()));
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
            butterfly->contiguous().at(this, i).setWithoutWriteBarrier(v);
            if (i >= butterfly->publicLength())
                butterfly->setPublicLength(i + 1);
            vm.heap.writeBarrier(this, v);
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
            butterfly->contiguousDouble().at(this, i) = value;
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
        case ALL_BLANK_INDEXING_TYPES:
            setIndexQuicklyForTypedArray(i, v);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void initializeIndex(ObjectInitializationScope& scope, unsigned i, JSValue v)
    {
        initializeIndex(scope, i, v, indexingType());
    }

    // NOTE: Clients of this method may call it more than once for any index, and this is supposed
    // to work.
    ALWAYS_INLINE void initializeIndex(ObjectInitializationScope& scope, unsigned i, JSValue v, IndexingType indexingType)
    {
        VM& vm = scope.vm();
        Butterfly* butterfly = m_butterfly.get();
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
            butterfly->contiguous().at(this, i).set(vm, this, v);
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
            butterfly->contiguousDouble().at(this, i) = value;
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
        
    void initializeIndexWithoutBarrier(ObjectInitializationScope& scope, unsigned i, JSValue v)
    {
        initializeIndexWithoutBarrier(scope, i, v, indexingType());
    }

    // This version of initializeIndex is for cases where you know that you will not need any
    // barriers. This implies not having any data format conversions.
    ALWAYS_INLINE void initializeIndexWithoutBarrier(ObjectInitializationScope&, unsigned i, JSValue v, IndexingType indexingType)
    {
        Butterfly* butterfly = m_butterfly.get();
        switch (indexingType) {
        case ALL_UNDECIDED_INDEXING_TYPES: {
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        case ALL_INT32_INDEXING_TYPES: {
            ASSERT(i < butterfly->publicLength());
            ASSERT(i < butterfly->vectorLength());
            RELEASE_ASSERT(v.isInt32());
            FALLTHROUGH;
        }
        case ALL_CONTIGUOUS_INDEXING_TYPES: {
            ASSERT(i < butterfly->publicLength());
            ASSERT(i < butterfly->vectorLength());
            butterfly->contiguous().at(this, i).setWithoutWriteBarrier(v);
            break;
        }
        case ALL_DOUBLE_INDEXING_TYPES: {
            ASSERT(i < butterfly->publicLength());
            ASSERT(i < butterfly->vectorLength());
            RELEASE_ASSERT(v.isNumber());
            double value = v.asNumber();
            RELEASE_ASSERT(value == value);
            butterfly->contiguousDouble().at(this, i) = value;
            break;
        }
        case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
            ArrayStorage* storage = butterfly->arrayStorage();
            ASSERT(i < storage->length());
            ASSERT(i < storage->m_numValuesInVector);
            storage->m_vector[i].setWithoutWriteBarrier(v);
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
            return !!m_butterfly->arrayStorage()->m_sparseMap;
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
            return m_butterfly->arrayStorage()->inSparseMode();
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
    bool putDirect(VM&, PropertyName, JSValue, unsigned attributes = 0);
    bool putDirect(VM&, PropertyName, JSValue, PutPropertySlot&);
    void putDirectWithoutTransition(VM&, PropertyName, JSValue, unsigned attributes = 0);
    bool putDirectNonIndexAccessor(VM&, PropertyName, GetterSetter*, unsigned attributes);
    void putDirectNonIndexAccessorWithoutTransition(VM&, PropertyName, GetterSetter*, unsigned attributes);
    bool putDirectAccessor(JSGlobalObject*, PropertyName, GetterSetter*, unsigned attributes);
    JS_EXPORT_PRIVATE bool putDirectCustomAccessor(VM&, PropertyName, JSValue, unsigned attributes);

    bool putGetter(JSGlobalObject*, PropertyName, JSValue, unsigned attributes);
    bool putSetter(JSGlobalObject*, PropertyName, JSValue, unsigned attributes);

    JS_EXPORT_PRIVATE bool hasProperty(JSGlobalObject*, PropertyName) const;
    JS_EXPORT_PRIVATE bool hasProperty(JSGlobalObject*, unsigned propertyName) const;
    bool hasProperty(JSGlobalObject*, uint64_t propertyName) const;
    bool hasPropertyGeneric(JSGlobalObject*, PropertyName, PropertySlot::InternalMethodType) const;
    bool hasPropertyGeneric(JSGlobalObject*, unsigned propertyName, PropertySlot::InternalMethodType) const;
    bool hasOwnProperty(JSGlobalObject*, PropertyName, PropertySlot&) const;
    bool hasOwnProperty(JSGlobalObject*, PropertyName) const;
    bool hasOwnProperty(JSGlobalObject*, unsigned) const;

    JS_EXPORT_PRIVATE static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    JS_EXPORT_PRIVATE static bool deletePropertyByIndex(JSCell*, JSGlobalObject*, unsigned propertyName);
    bool deleteProperty(JSGlobalObject*, PropertyName);
    bool deleteProperty(JSGlobalObject*, uint32_t propertyName);
    bool deleteProperty(JSGlobalObject*, uint64_t propertyName);

    JS_EXPORT_PRIVATE static JSValue defaultValue(const JSObject*, JSGlobalObject*, PreferredPrimitiveType);
    JSValue ordinaryToPrimitive(JSGlobalObject*, PreferredPrimitiveType) const;

    JS_EXPORT_PRIVATE bool hasInstance(JSGlobalObject*, JSValue value, JSValue hasInstanceValue);
    JS_EXPORT_PRIVATE bool hasInstance(JSGlobalObject*, JSValue);
    static bool defaultHasInstance(JSGlobalObject*, JSValue, JSValue prototypeProperty);

    JS_EXPORT_PRIVATE static void getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getOwnNonIndexPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);

    JS_EXPORT_PRIVATE static uint32_t getEnumerableLength(JSGlobalObject*, JSObject*);
    JS_EXPORT_PRIVATE static void getStructurePropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getGenericPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);

    JS_EXPORT_PRIVATE JSValue toPrimitive(JSGlobalObject*, PreferredPrimitiveType = NoPreference) const;
    bool getPrimitiveNumber(JSGlobalObject*, double& number, JSValue&) const;
    JS_EXPORT_PRIVATE double toNumber(JSGlobalObject*) const;
    JS_EXPORT_PRIVATE JSString* toString(JSGlobalObject*) const;

    JS_EXPORT_PRIVATE static JSValue toThis(JSCell*, JSGlobalObject*, ECMAMode);

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
        
    const Butterfly* butterfly() const { return m_butterfly.get(); }
    Butterfly* butterfly() { return m_butterfly.get(); }
    
    ConstPropertyStorage outOfLineStorage() const { return m_butterfly->propertyStorage(); }
    PropertyStorage outOfLineStorage() { return m_butterfly->propertyStorage(); }

    ALWAYS_INLINE const WriteBarrierBase<Unknown>* locationForOffset(PropertyOffset offset) const
    {
        if (isInlineOffset(offset))
            return &inlineStorage()[offsetInInlineStorage(offset)];
        return &outOfLineStorage()[offsetInOutOfLineStorage(offset)];
    }

    ALWAYS_INLINE WriteBarrierBase<Unknown>* locationForOffset(PropertyOffset offset)
    {
        if (isInlineOffset(offset))
            return &inlineStorage()[offsetInInlineStorage(offset)];
        return &outOfLineStorage()[offsetInOutOfLineStorage(offset)];
    }

    void transitionTo(VM&, Structure*);

    bool hasCustomProperties(VM& vm) { return structure(vm)->didTransition(); }
    bool hasGetterSetterProperties(VM& vm) { return structure(vm)->hasGetterSetterProperties(); }
    bool hasCustomGetterSetterProperties(VM& vm) { return structure(vm)->hasCustomGetterSetterProperties(); }

    // putOwnDataProperty has 'put' like semantics, however this method:
    //  - assumes the object contains no own getter/setter properties.
    //  - provides no special handling for __proto__
    //  - does not walk the prototype chain (to check for accessors or non-writable properties).
    // This is used by JSLexicalEnvironment.
    bool putOwnDataProperty(VM&, PropertyName, JSValue, PutPropertySlot&);
    bool putOwnDataPropertyMayBeIndex(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
private:
    void validatePutOwnDataProperty(VM&, PropertyName, JSValue);
public:

    // Fast access to known property offsets.
    ALWAYS_INLINE JSValue getDirect(PropertyOffset offset) const { return locationForOffset(offset)->get(); }
    JSValue getDirectConcurrently(Structure* expectedStructure, PropertyOffset) const;
    void putDirect(VM& vm, PropertyOffset offset, JSValue value) { locationForOffset(offset)->set(vm, this, value); }
    void putDirectWithoutBarrier(PropertyOffset offset, JSValue value) { locationForOffset(offset)->setWithoutWriteBarrier(value); }
    void putDirectUndefined(PropertyOffset offset) { locationForOffset(offset)->setUndefined(); }

    JS_EXPORT_PRIVATE bool putDirectNativeIntrinsicGetter(VM&, JSGlobalObject*, Identifier, NativeFunction, Intrinsic, unsigned attributes);
    JS_EXPORT_PRIVATE void putDirectNativeIntrinsicGetterWithoutTransition(VM&, JSGlobalObject*, Identifier, NativeFunction, Intrinsic, unsigned attributes);
    JS_EXPORT_PRIVATE bool putDirectNativeFunction(VM&, JSGlobalObject*, const PropertyName&, unsigned functionLength, NativeFunction, Intrinsic, unsigned attributes);
    JS_EXPORT_PRIVATE bool putDirectNativeFunction(VM&, JSGlobalObject*, const PropertyName&, unsigned functionLength, NativeFunction, Intrinsic, const DOMJIT::Signature*, unsigned attributes);
    JS_EXPORT_PRIVATE void putDirectNativeFunctionWithoutTransition(VM&, JSGlobalObject*, const PropertyName&, unsigned functionLength, NativeFunction, Intrinsic, unsigned attributes);

    JS_EXPORT_PRIVATE JSFunction* putDirectBuiltinFunction(VM&, JSGlobalObject*, const PropertyName&, FunctionExecutable*, unsigned attributes);
    JSFunction* putDirectBuiltinFunctionWithoutTransition(VM&, JSGlobalObject*, const PropertyName&, FunctionExecutable*, unsigned attributes);

    JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);

    bool isEnvironment() const;
    bool isGlobalObject() const;
    bool isJSLexicalEnvironment() const;
    bool isGlobalLexicalEnvironment() const;
    bool isStrictEvalActivation() const;
    bool isWithScope() const;

    bool isErrorInstance() const;

    JS_EXPORT_PRIVATE void seal(VM&);
    JS_EXPORT_PRIVATE void freeze(VM&);
    JS_EXPORT_PRIVATE static bool preventExtensions(JSObject*, JSGlobalObject*);
    JS_EXPORT_PRIVATE static bool isExtensible(JSObject*, JSGlobalObject*);
    bool isSealed(VM& vm) { return structure(vm)->isSealed(vm); }
    bool isFrozen(VM& vm) { return structure(vm)->isFrozen(vm); }

    JS_EXPORT_PRIVATE bool anyObjectInChainMayInterceptIndexedAccesses(VM&) const;
    JS_EXPORT_PRIVATE bool prototypeChainMayInterceptStoreTo(VM&, PropertyName);
    bool needsSlowPutIndexing(VM&) const;

private:
    NonPropertyTransition suggestedArrayStorageTransition(VM&) const;
public:
    // You should only call isStructureExtensible() when:
    // - Performing this check in a way that isn't described in the specification 
    //   as calling the virtual [[IsExtensible]] trap.
    // - When you're guaranteed that object->methodTable(vm)->isExtensible isn't
    //   overridden.
    ALWAYS_INLINE bool isStructureExtensible(VM& vm) { return structure(vm)->isStructureExtensible(); }
    // You should call this when performing [[IsExtensible]] trap in a place
    // that is described in the specification. This performs the fully virtual
    // [[IsExtensible]] trap.
    bool isExtensible(JSGlobalObject*);
    bool indexingShouldBeSparse(VM& vm)
    {
        return !isStructureExtensible(vm)
            || structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero();
    }

    bool staticPropertiesReified(VM& vm) { return structure(vm)->staticPropertiesReified(); }
    void reifyAllStaticProperties(JSGlobalObject*);

    JS_EXPORT_PRIVATE Butterfly* allocateMoreOutOfLineStorage(VM&, size_t oldSize, size_t newSize);

    // Call this when you do not need to change the structure.
    void setButterfly(VM&, Butterfly*);
    
    // Call this if you do need to change the structure, or if you changed something about a structure
    // in-place.
    void nukeStructureAndSetButterfly(VM&, StructureID oldStructureID, Butterfly*);

    void setStructure(VM&, Structure*);

    JS_EXPORT_PRIVATE void convertToDictionary(VM&);

    void flattenDictionaryObject(VM& vm)
    {
        structure(vm)->flattenDictionaryStructure(vm, this);
    }
    void shiftButterflyAfterFlattening(const GCSafeConcurrentJSLocker&, VM&, Structure* structure, size_t outOfLineCapacityAfter);

    JSGlobalObject* globalObject() const
    {
        ASSERT(structure()->globalObject());
        ASSERT(!isGlobalObject() || ((JSObject*)structure()->globalObject()) == this);
        return structure()->globalObject();
    }
        
    JSGlobalObject* globalObject(VM& vm) const
    {
        ASSERT(structure(vm)->globalObject());
        ASSERT(!isGlobalObject() || ((JSObject*)structure(vm)->globalObject()) == this);
        return structure(vm)->globalObject();
    }
        
    void switchToSlowPutArrayStorage(VM&);
        
    // The receiver is the prototype in this case. The following:
    //
    // asObject(foo->structure()->storedPrototype())->attemptToInterceptPutByIndexOnHoleForPrototype(...)
    //
    // is equivalent to:
    //
    // foo->attemptToInterceptPutByIndexOnHole(...);
    bool attemptToInterceptPutByIndexOnHoleForPrototype(JSGlobalObject*, JSValue thisValue, unsigned propertyName, JSValue, bool shouldThrow, bool& putResult);
        
    // Returns 0 if int32 storage cannot be created - either because
    // indexing should be sparse, we're having a bad time, or because
    // we already have a more general form of storage (double,
    // contiguous, array storage).
    ContiguousJSValues tryMakeWritableInt32(VM& vm)
    {
        if (LIKELY(hasInt32(indexingType()) && !isCopyOnWrite(indexingMode())))
            return m_butterfly->contiguousInt32();
            
        return tryMakeWritableInt32Slow(vm);
    }
        
    // Returns 0 if double storage cannot be created - either because
    // indexing should be sparse, we're having a bad time, or because
    // we already have a more general form of storage (contiguous,
    // or array storage).
    ContiguousDoubles tryMakeWritableDouble(VM& vm)
    {
        if (LIKELY(hasDouble(indexingType()) && !isCopyOnWrite(indexingMode())))
            return m_butterfly->contiguousDouble();
            
        return tryMakeWritableDoubleSlow(vm);
    }
        
    // Returns 0 if contiguous storage cannot be created - either because
    // indexing should be sparse or because we're having a bad time.
    ContiguousJSValues tryMakeWritableContiguous(VM& vm)
    {
        if (LIKELY(hasContiguous(indexingType()) && !isCopyOnWrite(indexingMode())))
            return m_butterfly->contiguous();
            
        return tryMakeWritableContiguousSlow(vm);
    }

    // Ensure that the object is in a mode where it has array storage. Use
    // this if you're about to perform actions that would have required the
    // object to be converted to have array storage, if it didn't have it
    // already.
    ArrayStorage* ensureArrayStorage(VM& vm)
    {
        if (LIKELY(hasAnyArrayStorage(indexingType())))
            return m_butterfly->arrayStorage();

        return ensureArrayStorageSlow(vm);
    }

    void ensureWritable(VM& vm)
    {
        if (isCopyOnWrite(indexingMode()))
            convertFromCopyOnWrite(vm);
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

    JS_EXPORT_PRIVATE JSValue getMethod(JSGlobalObject*, CallData&, const Identifier&, const String& errorMessage);

    bool canPerformFastPutInline(VM&, PropertyName);
    bool canPerformFastPutInlineExcludingProto(VM&);

    bool mayBePrototype() const;
    void didBecomePrototype();

    Optional<Structure::PropertyHashEntry> findPropertyHashEntry(VM&, PropertyName) const;

    DECLARE_EXPORT_INFO;

protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(jsDynamicCast<JSObject*>(vm, this));
        ASSERT(structure(vm)->hasPolyProto() || getPrototypeDirect(vm).isNull() || Heap::heap(this) == Heap::heap(getPrototypeDirect(vm)));
        ASSERT(structure(vm)->isObject());
        ASSERT(classInfo(vm));
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    // To instantiate objects you likely want JSFinalObject, below.
    // To create derived types you likely want JSNonFinalObject, below.
    JSObject(VM&, Structure*, Butterfly* = nullptr);
    
    // Visits the butterfly unless there is a race. Returns the structure if there was no race.
    Structure* visitButterfly(SlotVisitor&);
    
    Structure* visitButterflyImpl(SlotVisitor&);
    
    void markAuxiliaryAndVisitOutOfLineProperties(SlotVisitor&, Butterfly*, Structure*, PropertyOffset maxOffset);

    // Call this if you know that the object is in a mode where it has array
    // storage. This will assert otherwise.
    ArrayStorage* arrayStorage()
    {
        ASSERT(hasAnyArrayStorage(indexingType()));
        return m_butterfly->arrayStorage();
    }
        
    // Call this if you want to predicate some actions on whether or not the
    // object is in a mode where it has array storage.
    ArrayStorage* arrayStorageOrNull()
    {
        switch (indexingType()) {
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return m_butterfly->arrayStorage();
                
        default:
            return nullptr;
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
    void convertDoubleForValue(VM&, JSValue);
    void convertFromCopyOnWrite(VM&);

    static Butterfly* createArrayStorageButterfly(VM&, JSObject* intendedOwner, Structure*, unsigned length, unsigned vectorLength, Butterfly* oldButterfly = nullptr);
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
        
    bool defineOwnNonIndexProperty(JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool throwException);

    template<IndexingType indexingShape>
    bool putByIndexBeyondVectorLengthWithoutAttributes(JSGlobalObject*, unsigned propertyName, JSValue);
    bool putByIndexBeyondVectorLengthWithArrayStorage(JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow, ArrayStorage*);

    bool increaseVectorLength(VM&, unsigned newLength);
    void deallocateSparseIndexMap();
    bool defineOwnIndexedProperty(JSGlobalObject*, unsigned, const PropertyDescriptor&, bool throwException);
    SparseArrayValueMap* allocateSparseIndexMap(VM&);
        
    void notifyPresenceOfIndexedAccessors(VM&);
        
    bool attemptToInterceptPutByIndexOnHole(JSGlobalObject*, unsigned index, JSValue, bool shouldThrow, bool& putResult);
        
    // Call this if you want setIndexQuickly to succeed and you're sure that
    // the array is contiguous.
    bool WARN_UNUSED_RETURN ensureLength(VM& vm, unsigned length)
    {
        RELEASE_ASSERT(length <= MAX_STORAGE_VECTOR_LENGTH);
        ASSERT(hasContiguous(indexingType()) || hasInt32(indexingType()) || hasDouble(indexingType()) || hasUndecided(indexingType()));

        if (m_butterfly->vectorLength() < length || isCopyOnWrite(indexingMode())) {
            if (!ensureLengthSlow(vm, length))
                return false;
        }
            
        if (m_butterfly->publicLength() < length)
            m_butterfly->setPublicLength(length);
        return true;
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
    friend class VMInspector;

    // Nobody should ever ask any of these questions on something already known to be a JSObject.
    using JSCell::isAPIValueWrapper;
    using JSCell::isGetterSetter;
    void getObject();
    void getString(JSGlobalObject* globalObject);
    void isObject();
    void isString();
        
    Butterfly* createInitialIndexedStorage(VM&, unsigned length);
        
    ArrayStorage* enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(VM&, ArrayStorage*);
        
    template<PutMode>
    bool putDirectInternal(VM&, PropertyName, JSValue, unsigned attr, PutPropertySlot&);

    JS_EXPORT_PRIVATE NEVER_INLINE bool putInlineSlow(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

    bool getNonIndexPropertySlot(JSGlobalObject*, PropertyName, PropertySlot&);
    bool getOwnNonIndexPropertySlot(VM&, Structure*, PropertyName, PropertySlot&);
    JS_EXPORT_PRIVATE void fillGetterPropertySlot(VM&, PropertySlot&, JSCell*, unsigned, PropertyOffset);
    void fillCustomGetterPropertySlot(VM&, PropertySlot&, CustomGetterSetter*, unsigned, Structure*);

    JS_EXPORT_PRIVATE bool getOwnStaticPropertySlot(VM&, PropertyName, PropertySlot&);
        
    bool putByIndexBeyondVectorLength(JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow);
    bool putDirectIndexBeyondVectorLengthWithArrayStorage(JSGlobalObject*, unsigned propertyName, JSValue, unsigned attributes, PutDirectIndexMode, ArrayStorage*);
    JS_EXPORT_PRIVATE bool putDirectIndexSlowOrBeyondVectorLength(JSGlobalObject*, unsigned propertyName, JSValue, unsigned attributes, PutDirectIndexMode);
        
    unsigned getNewVectorLength(VM&, unsigned indexBias, unsigned currentVectorLength, unsigned currentLength, unsigned desiredLength);
    unsigned getNewVectorLength(VM&, unsigned desiredLength);

    ArrayStorage* constructConvertedArrayStorageWithoutCopyingElements(VM&, unsigned neededLength);
        
    JS_EXPORT_PRIVATE void setIndexQuicklyToUndecided(VM&, unsigned index, JSValue);
    JS_EXPORT_PRIVATE void convertInt32ToDoubleOrContiguousWhilePerformingSetIndex(VM&, unsigned index, JSValue);
    JS_EXPORT_PRIVATE void convertDoubleToContiguousWhilePerformingSetIndex(VM&, unsigned index, JSValue);
        
    bool ensureLengthSlow(VM&, unsigned length);
        
    ContiguousJSValues tryMakeWritableInt32Slow(VM&);
    ContiguousDoubles tryMakeWritableDoubleSlow(VM&);
    ContiguousJSValues tryMakeWritableContiguousSlow(VM&);
    JS_EXPORT_PRIVATE ArrayStorage* ensureArrayStorageSlow(VM&);

    PropertyOffset prepareToPutDirectWithoutTransition(VM&, PropertyName, unsigned attributes, StructureID, Structure*);

    AuxiliaryBarrier<Butterfly*> m_butterfly;
#if CPU(ADDRESS32)
    unsigned m_32BitPadding;
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
    explicit JSNonFinalObject(VM& vm, Structure* structure, Butterfly* butterfly = nullptr)
        : JSObject(vm, structure, butterfly)
    {
    }

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(!this->structure(vm)->hasInlineStorage());
        ASSERT(classInfo(vm));
    }
};

class JSFinalObject;

// JSFinalObject is a type of JSObject that contains sufficient internal
// storage to fully make use of the collector cell containing it.
class JSFinalObject final : public JSObject {
    friend class JSObject;
public:
    using Base = JSObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM&);

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        return (sizeof(JSObject) + inlineCapacity * sizeof(WriteBarrierBase<Unknown>)).unsafeGet();
    }

    static inline const TypeInfo typeInfo() { return TypeInfo(FinalObjectType, StructureFlags); }
    static constexpr IndexingType defaultIndexingType = NonArray;
        
    static constexpr unsigned defaultSize = 64;
    static inline unsigned defaultInlineCapacity()
    {
        return (defaultSize - allocationSize(0)) / sizeof(WriteBarrier<Unknown>);
    }

    static constexpr unsigned maxSize = 512;
    static inline unsigned maxInlineCapacity()
    {
        return (maxSize - allocationSize(0)) / sizeof(WriteBarrier<Unknown>);
    }

    static JSFinalObject* create(VM&, Structure*, Butterfly* = nullptr);
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, unsigned inlineCapacity)
    {
        return Structure::create(vm, globalObject, prototype, typeInfo(), info(), defaultIndexingType, inlineCapacity);
    }

    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);

    DECLARE_EXPORT_INFO;

private:
    friend class LLIntOffsetsExtractor;

    void visitChildrenCommon(SlotVisitor&);

    explicit JSFinalObject(VM& vm, Structure* structure, Butterfly* butterfly = nullptr)
        : JSObject(vm, structure, butterfly)
    {
        gcSafeZeroMemory(inlineStorageUnsafe(), structure->inlineCapacity() * sizeof(EncodedJSValue));
    }

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(structure(vm)->totalStorageCapacity() == structure(vm)->inlineCapacity());
        ASSERT(classInfo(vm));
    }
};

JS_EXPORT_PRIVATE EncodedJSValue JSC_HOST_CALL objectPrivateFuncInstanceOf(JSGlobalObject*, CallFrame*);

inline JSObject* JSObject::createRawObject(VM& vm, Structure* structure, Butterfly* butterfly)
{
    JSObject* finalObject = new (
        NotNull, 
        allocateCell<JSFinalObject>(
            vm.heap,
            JSFinalObject::allocationSize(structure->inlineCapacity())
        )
    ) JSObject(vm, structure, butterfly);
    finalObject->finishCreation(vm);
    return finalObject;
}

inline JSFinalObject* JSFinalObject::create(VM& vm, Structure* structure, Butterfly* butterfly)
{
    JSFinalObject* finalObject = new (
        NotNull, 
        allocateCell<JSFinalObject>(
            vm.heap,
            allocationSize(structure->inlineCapacity())
        )
    ) JSFinalObject(vm, structure, butterfly);
    finalObject->finishCreation(vm);
    return finalObject;
}

inline size_t JSObject::offsetOfInlineStorage()
{
    return sizeof(JSObject);
}

inline bool JSObject::isGlobalObject() const
{
    return type() == GlobalObjectType;
}

inline bool JSObject::isJSLexicalEnvironment() const
{
    return type() == LexicalEnvironmentType || type() == ModuleEnvironmentType;
}

inline bool JSObject::isGlobalLexicalEnvironment() const
{
    return type() == GlobalLexicalEnvironmentType;
}

inline bool JSObject::isStrictEvalActivation() const
{
    return type() == StrictEvalActivationType;
}

inline bool JSObject::isEnvironment() const
{
    bool result = GlobalObjectType <= type() && type() <= StrictEvalActivationType;
    ASSERT((isGlobalObject() || isJSLexicalEnvironment() || isGlobalLexicalEnvironment() || isStrictEvalActivation()) == result);
    return result;
}

inline bool JSObject::isErrorInstance() const
{
    return type() == ErrorInstanceType;
}

inline bool JSObject::isWithScope() const
{
    return type() == WithScopeType;
}

inline void JSObject::setStructure(VM& vm, Structure* structure)
{
    ASSERT(structure);
    ASSERT(!m_butterfly == !(structure->outOfLineCapacity() || structure->hasIndexingHeader(this)));
    JSCell::setStructure(vm, structure);
}

inline void JSObject::setButterfly(VM& vm, Butterfly* butterfly)
{
    if (isX86() || vm.heap.mutatorShouldBeFenced()) {
        WTF::storeStoreFence();
        m_butterfly.set(vm, this, butterfly);
        WTF::storeStoreFence();
        return;
    }

    m_butterfly.set(vm, this, butterfly);
}

inline void JSObject::nukeStructureAndSetButterfly(VM& vm, StructureID oldStructureID, Butterfly* butterfly)
{
    if (isX86() || vm.heap.mutatorShouldBeFenced()) {
        setStructureIDDirectly(nuke(oldStructureID));
        WTF::storeStoreFence();
        m_butterfly.set(vm, this, butterfly);
        WTF::storeStoreFence();
        return;
    }

    m_butterfly.set(vm, this, butterfly);
}

inline JSObject* asObject(JSCell* cell)
{
    ASSERT(cell);
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
}

inline JSValue JSObject::getPrototypeDirect(VM& vm) const
{
    return structure(vm)->storedPrototype(this);
}

inline JSValue JSObject::getPrototype(VM& vm, JSGlobalObject* globalObject)
{
    auto getPrototypeMethod = methodTable(vm)->getPrototype;
    MethodTable::GetPrototypeFunctionPtr defaultGetPrototype = JSObject::getPrototype;
    if (LIKELY(getPrototypeMethod == defaultGetPrototype))
        return getPrototypeDirect(vm);
    return getPrototypeMethod(this, globalObject);
}

// Normally, we never shrink the butterfly so if we know an offset is valid for some
// past structure then it should be valid for any new structure. However, we may sometimes
// shrink the butterfly when we are holding the Structure's ConcurrentJSLock, such as when we
// flatten an object.
inline JSValue JSObject::getDirectConcurrently(Structure* structure, PropertyOffset offset) const
{
    ConcurrentJSLocker locker(structure->lock());
    if (!structure->isValidOffset(offset))
        return { };
    return getDirect(offset);
}

// It is safe to call this method with a PropertyName that is actually an index,
// but if so will always return false (doesn't search index storage).
ALWAYS_INLINE bool JSObject::getOwnNonIndexPropertySlot(VM& vm, Structure* structure, PropertyName propertyName, PropertySlot& slot)
{
    unsigned attributes;
    PropertyOffset offset = structure->get(vm, propertyName, attributes);
    if (!isValidOffset(offset)) {
        if (!TypeInfo::hasStaticPropertyTable(inlineTypeFlags()))
            return false;
        return getOwnStaticPropertySlot(vm, propertyName, slot);
    }
    
    // getPropertySlot relies on this method never returning index properties!
    ASSERT(!parseIndex(propertyName));

    JSValue value = getDirect(offset);
    if (value.isCell()) {
        ASSERT(value);
        JSCell* cell = value.asCell();
        JSType type = cell->type();
        switch (type) {
        case GetterSetterType:
            fillGetterPropertySlot(vm, slot, cell, attributes, offset);
            return true;
        case CustomGetterSetterType:
            fillCustomGetterPropertySlot(vm, slot, jsCast<CustomGetterSetter*>(cell), attributes, structure);
            return true;
        default:
            break;
        }
    }
    
    slot.setValue(this, attributes, value, offset);
    return true;
}

ALWAYS_INLINE void JSObject::fillCustomGetterPropertySlot(VM& vm, PropertySlot& slot, CustomGetterSetter* customGetterSetter, unsigned attributes, Structure* structure)
{
    ASSERT(attributes & PropertyAttribute::CustomAccessorOrValue);
    if (customGetterSetter->inherits<DOMAttributeGetterSetter>(vm)) {
        auto* domAttribute = jsCast<DOMAttributeGetterSetter*>(customGetterSetter);
        if (structure->isUncacheableDictionary())
            slot.setCustom(this, attributes, domAttribute->getter(), domAttribute->domAttribute());
        else
            slot.setCacheableCustom(this, attributes, domAttribute->getter(), domAttribute->domAttribute());
        return;
    }

    if (structure->isUncacheableDictionary())
        slot.setCustom(this, attributes, customGetterSetter->getter());
    else
        slot.setCacheableCustom(this, attributes, customGetterSetter->getter());
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    Structure* structure = object->structure(vm);
    if (object->getOwnNonIndexPropertySlot(vm, structure, propertyName, slot))
        return true;
    if (Optional<uint32_t> index = parseIndex(propertyName))
        return getOwnPropertySlotByIndex(object, globalObject, index.value(), slot);
    return false;
}

ALWAYS_INLINE void JSObject::doPutPropertySecurityCheck(JSObject*, JSGlobalObject*, PropertyName, PutPropertySlot&)
{
}

// It may seem crazy to inline a function this large but it makes a big difference
// since this is function very hot in variable lookup
template<bool checkNullStructure>
ALWAYS_INLINE bool JSObject::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto& structureIDTable = vm.heap.structureIDTable();
    JSObject* object = this;
    while (true) {
        if (UNLIKELY(TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags()))) {
            // If propertyName is an index then we may have missed it (as this loop is using
            // getOwnNonIndexPropertySlot), so we cannot safely call the overridden getOwnPropertySlot
            // (lest we return a property from a prototype that is shadowed). Check now for an index,
            // if so we need to start afresh from this object.
            if (Optional<uint32_t> index = parseIndex(propertyName))
                return getPropertySlot(globalObject, index.value(), slot);
            // Safe to continue searching from current position; call getNonIndexPropertySlot to avoid
            // parsing the int again.
            return object->getNonIndexPropertySlot(globalObject, propertyName, slot);
        }
        ASSERT(object->type() != ProxyObjectType);
        Structure* structure = structureIDTable.get(object->structureID());
#if USE(JSVALUE64)
        if (checkNullStructure && UNLIKELY(!structure))
            CRASH_WITH_INFO(object->type(), object->structureID(), structureIDTable.size());
#endif
        if (object->getOwnNonIndexPropertySlot(vm, structure, propertyName, slot))
            return true;
        // FIXME: This doesn't look like it's following the specification:
        // https://bugs.webkit.org/show_bug.cgi?id=172572
        JSValue prototype = structure->storedPrototype(object);
        if (!prototype.isObject())
            break;
        object = asObject(prototype);
    }

    if (Optional<uint32_t> index = parseIndex(propertyName))
        return getPropertySlot(globalObject, index.value(), slot);
    return false;
}

inline JSValue JSObject::get(JSGlobalObject* globalObject, PropertyName propertyName) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
    bool hasProperty = const_cast<JSObject*>(this)->getPropertySlot(globalObject, propertyName, slot);

    EXCEPTION_ASSERT(!scope.exception() || isTerminatedExecutionException(vm, scope.exception()) || !hasProperty);
    RETURN_IF_EXCEPTION(scope, jsUndefined());

    if (hasProperty)
        RELEASE_AND_RETURN(scope, slot.getValue(globalObject, propertyName));

    return jsUndefined();
}

inline JSValue JSObject::get(JSGlobalObject* globalObject, unsigned propertyName) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
    bool hasProperty = const_cast<JSObject*>(this)->getPropertySlot(globalObject, propertyName, slot);

    EXCEPTION_ASSERT(!scope.exception() || isTerminatedExecutionException(vm, scope.exception()) || !hasProperty);
    RETURN_IF_EXCEPTION(scope, jsUndefined());

    if (hasProperty)
        RELEASE_AND_RETURN(scope, slot.getValue(globalObject, propertyName));

    return jsUndefined();
}

inline bool JSObject::putDirect(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!value.isGetterSetter() && !(attributes & PropertyAttribute::Accessor));
    ASSERT(!value.isCustomGetterSetter() && !(attributes & PropertyAttribute::CustomAccessorOrValue));
    PutPropertySlot slot(this);
    return putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, value, attributes, slot);
}

inline bool JSObject::putDirect(VM& vm, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    ASSERT(!value.isGetterSetter());
    ASSERT(!value.isCustomGetterSetter());
    return putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, value, 0, slot);
}

inline size_t offsetInButterfly(PropertyOffset offset)
{
    return offsetInOutOfLineStorage(offset) + Butterfly::indexOfPropertyStorage();
}

inline size_t JSObject::butterflyPreCapacity()
{
    VM& vm = this->vm();
    if (UNLIKELY(hasIndexingHeader(vm)))
        return butterfly()->indexingHeader()->preCapacity(structure(vm));
    return 0;
}

inline size_t JSObject::butterflyTotalSize()
{
    VM& vm = this->vm();
    Structure* structure = this->structure(vm);
    Butterfly* butterfly = this->butterfly();
    size_t preCapacity;
    size_t indexingPayloadSizeInBytes;
    bool hasIndexingHeader = this->hasIndexingHeader(vm);

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

template<unsigned charactersCount>
ALWAYS_INLINE Identifier makeIdentifier(VM& vm, const char (&characters)[charactersCount])
{
    return Identifier::fromString(vm, characters);
}

ALWAYS_INLINE Identifier makeIdentifier(VM& vm, const char* name)
{
    return Identifier::fromString(vm, name);
}

ALWAYS_INLINE Identifier makeIdentifier(VM&, const Identifier& name)
{
    return name;
}

bool validateAndApplyPropertyDescriptor(JSGlobalObject*, JSObject*, PropertyName, bool isExtensible,
    const PropertyDescriptor& descriptor, bool isCurrentDefined, const PropertyDescriptor& current, bool throwException);

JS_EXPORT_PRIVATE NEVER_INLINE bool ordinarySetSlow(JSGlobalObject*, JSObject*, PropertyName, JSValue, JSValue receiver, bool shouldThrow);

// Helper for defining native functions, if you're not using a static hash table.
// Use this macro from within finishCreation() methods in prototypes. This assumes
// you've defined variables called globalObject, globalObject, and vm, and they
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
    JSC_NATIVE_INTRINSIC_FUNCTION(jsName, cppName, (attributes), (length), JSC::NoIntrinsic)

#define JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(jsName, cppName, attributes, length) \
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(jsName, cppName, (attributes), (length), JSC::NoIntrinsic)

// Identical helpers but for builtins. Note that currently, we don't support builtins that are
// also intrinsics, but we probably will do that eventually.
#define JSC_BUILTIN_FUNCTION(jsName, generatorName, attributes) \
    putDirectBuiltinFunction(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (generatorName)(vm), (attributes))

#define JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(jsName, generatorName, attributes) \
    putDirectBuiltinFunctionWithoutTransition(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (generatorName)(vm), (attributes))

#define JSC_TO_STRING_TAG_WITHOUT_TRANSITION() \
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, \
        jsNontrivialString(vm, info()->className), JSC::PropertyAttribute::DontEnum | JSC::PropertyAttribute::ReadOnly)

// Helper for defining native getters on properties.
#define JSC_NATIVE_INTRINSIC_GETTER(jsName, cppName, attributes, intrinsic)  \
    putDirectNativeIntrinsicGetter(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (cppName), \
        (intrinsic), ((attributes) | JSC::PropertyAttribute::Accessor))

#define JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(jsName, cppName, attributes, intrinsic)  \
    putDirectNativeIntrinsicGetterWithoutTransition(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (cppName), \
        (intrinsic), ((attributes) | JSC::PropertyAttribute::Accessor))

#define JSC_NATIVE_GETTER(jsName, cppName, attributes) \
    JSC_NATIVE_INTRINSIC_GETTER((jsName), (cppName), (attributes), JSC::NoIntrinsic)

#define JSC_NATIVE_GETTER_WITHOUT_TRANSITION(jsName, cppName, attributes) \
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION((jsName), (cppName), (attributes), JSC::NoIntrinsic)


#define STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(DerivedClass, BaseClass) \
    static_assert(sizeof(DerivedClass) == sizeof(BaseClass)); \
    static_assert(DerivedClass::destroy == BaseClass::destroy);

} // namespace JSC
