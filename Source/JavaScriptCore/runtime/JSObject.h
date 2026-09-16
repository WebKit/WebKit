/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2024 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ArrayConventions.h>
#include <JavaScriptCore/ArrayStorage.h>
#include <JavaScriptCore/Butterfly.h>
#include <JavaScriptCore/CagedBarrierPtr.h>
#include <JavaScriptCore/ClassInfo.h>
#include <JavaScriptCore/CustomGetterSetter.h>
#include <JavaScriptCore/DOMAttributeGetterSetter.h>
#include <JavaScriptCore/DeletePropertySlot.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/IndexingHeaderInlines.h>
#include <JavaScriptCore/Intrinsic.h>
#include <JavaScriptCore/JSCJSValueCell.h>
#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/MathCommon.h>
#include <JavaScriptCore/PropertySlot.h>
#include <JavaScriptCore/PropertyStorage.h>
#include <JavaScriptCore/PutDirectIndexMode.h>
#include <JavaScriptCore/PutPropertySlot.h>
#include <JavaScriptCore/Structure.h>
#include <JavaScriptCore/StructureTransitionTable.h>
#include <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace DOMJIT {
class Signature;
}

inline JSCell* getJSFunction(JSValue); // Defined in JSObjectInlines.h

class ArrayProfile;
class Exception;
class GetterSetter;
class InternalFunction;
class JSFunction;
class JSString;
class LLIntOffsetsExtractor;
class MarkedBlock;
class ObjectInitializationScope;
class PropertyDescriptor;
class PropertyNameArrayBuilder;
class Structure;
class ThrowScope;
class VM;
struct HashTable;
struct HashTableValue;

JS_EXPORT_PRIVATE Exception* throwTypeError(JSGlobalObject*, ThrowScope&, const String&);
extern JS_EXPORT_PRIVATE const ASCIILiteral NonExtensibleObjectPropertyDefineError;
extern JS_EXPORT_PRIVATE const ASCIILiteral ReadonlyPropertyWriteError;
extern JS_EXPORT_PRIVATE const ASCIILiteral ReadonlyPropertyChangeError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnableToDeletePropertyError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnconfigurablePropertyChangeAccessMechanismError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnconfigurablePropertyChangeConfigurabilityError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnconfigurablePropertyChangeEnumerabilityError;
extern JS_EXPORT_PRIVATE const ASCIILiteral UnconfigurablePropertyChangeWritabilityError;
extern JS_EXPORT_PRIVATE const ASCIILiteral PrototypeValueCanOnlyBeAnObjectOrNullTypeError;

class JSFinalObject;

#if ASSERT_ENABLED
#define JS_EXPORT_PRIVATE_IF_ASSERT_ENABLED JS_EXPORT_PRIVATE
#else
#define JS_EXPORT_PRIVATE_IF_ASSERT_ENABLED
#endif

// Debug-only information handed to getOwnNonIndexPropertySlot on the debugLLIntGetById=true path (rdar://157153895)
struct PrototypeChainDebugData {
    // The original base value where getPropertySlot began the prototype walk
    class JSObject* bottomOfChain;
    // The prototype-chain child of the object being examined (null if it's the base)
    class JSObject* previousInChain;
};

class JSObject : public JSCell {
    friend class BatchedTransitionOptimizer;
    friend class JIT;
    friend class JSCell;
    friend class JSFinalObject;
    friend class JSObjectWithButterfly;
    friend class MarkedBlock;

    enum PutMode : uint8_t {
        PutModePut,
        PutModeDefineOwnProperty,
    };

public:
    using Base = JSCell;

    DECLARE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE);

    JS_EXPORT_PRIVATE static size_t estimatedSize(JSCell*, VM&);
    JS_EXPORT_PRIVATE static void analyzeHeap(JSCell*, HeapAnalyzer&);

    JS_EXPORT_PRIVATE static String calculatedClassName(JSObject*);

    // This is the fully virtual [[GetPrototypeOf]] internal function defined
    // in the ECMAScript 6 specification. Use this when doing a [[GetPrototypeOf]] 
    // operation as dictated in the specification.
    JSValue getPrototype(JSGlobalObject*); // defined in JSObjectInlines.h
    JS_EXPORT_PRIVATE static JSValue getPrototype(JSObject*, JSGlobalObject*);
    // This gets the prototype directly off of the structure. This does not do
    // dynamic dispatch on the getPrototype method table method. It is not valid 
    // to use this when performing a [[GetPrototypeOf]] operation in the specification.
    // It is valid to use though when you know that you want to directly get it
    // without consulting the method table. This is akin to getting the [[Prototype]]
    // internal field directly as described in the specification.
    JSValue getPrototypeDirect() const; // defined in JSObjectInlines.h

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
        
    inline bool mayInterceptIndexedAccesses();

    inline JSValue get(JSGlobalObject*, PropertyName) const; // Defined in JSObjectInlines.h
    inline JSValue get(JSGlobalObject*, unsigned propertyName) const; // Defined in JSObjectInlines.h
    JSValue get(JSGlobalObject*, uint64_t propertyName) const;

    template<typename T, typename PropertyNameType>
    inline T getAs(JSGlobalObject*, PropertyNameType) const; // Defined in JSObjectInlines.h

    template<bool checkNullStructure = false, bool debugLLIntGetById = false>
    bool getPropertySlot(JSGlobalObject*, PropertyName, PropertySlot&);
    bool getPropertySlot(JSGlobalObject*, unsigned propertyName, PropertySlot&);
    bool getPropertySlot(JSGlobalObject*, uint64_t propertyName, PropertySlot&);
    template<typename CallbackWhenNoException> typename std::invoke_result<CallbackWhenNoException, bool, PropertySlot&>::type getPropertySlot(JSGlobalObject*, PropertyName, CallbackWhenNoException) const;
    template<typename CallbackWhenNoException> typename std::invoke_result<CallbackWhenNoException, bool, PropertySlot&>::type getPropertySlot(JSGlobalObject*, PropertyName, PropertySlot&, CallbackWhenNoException) const;

    template<typename PropertyNameType> JSValue getIfPropertyExists(JSGlobalObject*, const PropertyNameType&);
    bool noSideEffectMayHaveNonIndexProperty(VM&, PropertyName);

    enum class SortMode { Default, Ascending };
    template<SortMode mode = SortMode::Default, typename Functor>
    void forEachOwnIndexedProperty(JSGlobalObject*, const Functor&);

private:
    static bool getOwnPropertySlotImpl(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
public:
    JS_EXPORT_PRIVATE_IF_ASSERT_ENABLED static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);

    JS_EXPORT_PRIVATE static bool getOwnPropertySlotByIndex(JSObject*, JSGlobalObject*, unsigned propertyName, PropertySlot&);
    bool getOwnPropertySlotInline(JSGlobalObject*, PropertyName, PropertySlot&);

    // The key difference between this and getOwnPropertySlot is that getOwnPropertySlot
    // currently returns incorrect results for the DOM window (with non-own properties)
    // being returned. Once this is fixed we should migrate code & remove this method.
    JS_EXPORT_PRIVATE bool getOwnPropertyDescriptor(JSGlobalObject*, PropertyName, PropertyDescriptor&);

    static bool getPrivateFieldSlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    inline bool hasPrivateField(JSGlobalObject*, PropertyName);
    inline bool getPrivateField(JSGlobalObject*, PropertyName, PropertySlot&);
    inline void setPrivateField(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    inline void definePrivateField(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    inline bool hasPrivateBrand(JSGlobalObject*, JSValue brand);
    inline void checkPrivateBrand(JSGlobalObject*, JSValue brand);
    inline void setPrivateBrand(JSGlobalObject*, JSValue brand);

    unsigned getArrayLength() const
    {
        if (!hasIndexedProperties(indexingType()))
            return 0;
        return butterfly()->publicLength();
    }

    unsigned getVectorLength()
    {
        if (!hasIndexedProperties(indexingType()))
            return 0;
        return butterfly()->vectorLength();
    }
    
    inline bool canHaveExistingOwnIndexedGetterSetterProperties(); // Defined in RenderObjectInlines.h

    // This is only valid after using canPerformFastPropertyEnumerationCommon().
    // This code is not checking getOwnPropertySlot override etc.
    inline unsigned canHaveExistingOwnIndexedProperties() const; // Defined in RenderObjectInlines.h

    static bool putInlineForJSObject(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    
    JS_EXPORT_PRIVATE static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    static bool NODELETE mightBeSpecialProperty(VM&, JSType, UniquedStringImpl*);
    JS_EXPORT_PRIVATE NEVER_INLINE static bool definePropertyOnReceiver(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    // putByIndex assumes that the receiver is this JSCell object.
    JS_EXPORT_PRIVATE static bool putByIndex(JSCell*, JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow);
        
    // This performs the ECMAScript Set() operation.
    ALWAYS_INLINE bool putByIndexInline(JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow); // Defined in JSObjectInlines.h
    ALWAYS_INLINE bool putByIndexInline(JSGlobalObject* globalObject, uint64_t propertyName, JSValue value, bool shouldThrow); // Defined in JSObjectInlines.h

    // This is similar to the putDirect* methods:
    //  - the prototype chain is not consulted
    //  - accessors are not called.
    //  - it will ignore extensibility and read-only properties if PutDirectIndexLikePutDirect is passed as the mode (the default).
    // This method creates a property with attributes writable, enumerable and configurable all set to true if attributes is zero,
    // otherwise, it creates a property with the provided attributes. Semantically, this is performing defineOwnProperty.
    bool putDirectIndex(JSGlobalObject* globalObject, unsigned propertyName, JSValue value, unsigned attributes, PutDirectIndexMode mode); // Defined in JSObjectInlines.h
    // This is semantically equivalent to performing defineOwnProperty(propertyName, {configurable:true, writable:true, enumerable:true, value:value}).
    bool putDirectIndex(JSGlobalObject* globalObject, unsigned propertyName, JSValue value); // Defined in JSObjectInlines.h

    ALWAYS_INLINE bool putDirectIndex(JSGlobalObject* globalObject, uint64_t propertyName, JSValue value, unsigned attributes, PutDirectIndexMode mode); // Defined in JSObjectInlines.h

    // A generally non-throwing version of putDirect and putDirectIndex.
    // However, it's only guaranteed to not throw based on what the receiver is.
    // For example, if the receiver is a ProxyObject, this is not guaranteed, since
    // it may call into arbitrary JS code. It's the responsibility of the user of
    // this API to ensure that the receiver object is a well known type if they
    // want to ensure that this won't throw an exception.
    JS_EXPORT_PRIVATE bool putDirectMayBeIndex(JSGlobalObject*, PropertyName, JSValue);
        
    bool hasIndexingHeader() const
    {
        return structure()->hasIndexingHeader(this);
    }

    bool canGetIndexQuicklyForTypedArray(unsigned) const;
    JSValue getIndexQuicklyForTypedArray(unsigned, ArrayProfile* = nullptr) const;
    
    bool canGetIndexQuickly(unsigned i) const; // Defined in JSObjectInlines.h

    bool canGetIndexQuickly(uint64_t i) const; // Defined in JSObjectInlines.h

    JSValue getIndexQuickly(unsigned i) const; // Defined in JSObjectInlines.h

    // Uses the (optional) array profile to set the m_mayBeLargeTypedArray bit when relevant
    JSValue tryGetIndexQuickly(unsigned i, ArrayProfile* arrayProfile = nullptr) const; // Defined in JSObjectInlines.h

    JSValue tryGetIndexQuickly(uint64_t i) const; // Defined in JSObjectInlines.h

    JSValue getDirectIndex(JSGlobalObject* globalObject, unsigned i); // Defined in JSObjectInlines.h

    JSValue getIndex(JSGlobalObject* globalObject, uint64_t i) const; // Defined in JSObjectInlines.h

    void setIndexQuicklyForTypedArray(unsigned, JSValue);
    void setIndexQuicklyForArrayStorageIndexingType(VM&, unsigned, JSValue);

    // Return true to indicate success
    // Use the (optional) array profile to set the m_mayBeLargeTypedArray bit when relevant
    bool trySetIndexQuicklyForTypedArray(unsigned, JSValue, ArrayProfile*);
    bool trySetIndexQuickly(VM& vm, unsigned i, JSValue v, ArrayProfile* arrayProfile = nullptr); // Defined in JSObjectInlines.h

    void setIndexQuickly(VM& vm, unsigned i, JSValue v); // Defined in JSObjectInlines.h

    inline void initializeIndex(ObjectInitializationScope&, unsigned, JSValue); // Defined in JSObjectInlines.h

    // NOTE: Clients of this method may call it more than once for any index, and this is supposed
    // to work.
    ALWAYS_INLINE void initializeIndex(ObjectInitializationScope&, unsigned, JSValue, IndexingType); // Defined in JSObjectInlines.h

    inline void initializeIndexWithoutBarrier(ObjectInitializationScope&, unsigned, JSValue); // Defined in JSObjectInlines.h

    // This version of initializeIndex is for cases where you know that you will not need any
    // barriers. This implies not having any data format conversions.
    ALWAYS_INLINE void initializeIndexWithoutBarrier(ObjectInitializationScope&, unsigned, JSValue, IndexingType); // Defined in JSObjectInlines.h
        
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
            return !!butterfly()->arrayStorage()->m_sparseMap;
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
            return butterfly()->arrayStorage()->inSparseMode();
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
    bool putDirect(VM&, PropertyName, JSValue, unsigned attributes, PutPropertySlot&);
    bool putDirect(VM&, PropertyName, JSValue, PutPropertySlot&);
    void putDirectWithoutTransition(VM&, PropertyName, JSValue, unsigned attributes = 0);
    bool putDirectNonIndexAccessor(VM&, PropertyName, GetterSetter*, unsigned attributes);
    void putDirectNonIndexAccessorWithoutTransition(VM&, PropertyName, GetterSetter*, unsigned attributes);
    bool putDirectAccessor(JSGlobalObject*, PropertyName, GetterSetter*, unsigned attributes);
    JS_EXPORT_PRIVATE bool putDirectCustomAccessor(VM&, PropertyName, JSValue, unsigned attributes);
    void putDirectCustomGetterSetterWithoutTransition(VM&, PropertyName, JSValue, unsigned attributes);

    bool putGetter(JSGlobalObject*, PropertyName, JSValue, unsigned attributes);
    bool putSetter(JSGlobalObject*, PropertyName, JSValue, unsigned attributes);

    JS_EXPORT_PRIVATE bool hasProperty(JSGlobalObject*, PropertyName) const;
    JS_EXPORT_PRIVATE bool hasProperty(JSGlobalObject*, unsigned propertyName) const;
    bool hasProperty(JSGlobalObject*, uint64_t propertyName) const;
    bool hasEnumerableProperty(JSGlobalObject*, PropertyName) const;
    bool hasEnumerableProperty(JSGlobalObject*, unsigned propertyName) const;
    bool hasOwnProperty(JSGlobalObject*, PropertyName, PropertySlot&) const;
    bool hasOwnProperty(JSGlobalObject*, PropertyName) const;
    bool hasOwnProperty(JSGlobalObject*, unsigned) const;

    JS_EXPORT_PRIVATE static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    JS_EXPORT_PRIVATE static bool deletePropertyByIndex(JSCell*, JSGlobalObject*, unsigned propertyName);
    bool deleteProperty(JSGlobalObject*, PropertyName);
    bool deleteProperty(JSGlobalObject*, uint32_t propertyName);
    bool deleteProperty(JSGlobalObject*, uint64_t propertyName);

    JSValue ordinaryToPrimitive(JSGlobalObject*, PreferredPrimitiveType) const;

    JS_EXPORT_PRIVATE bool hasInstance(JSGlobalObject*, JSValue value, JSValue hasInstanceValue);
    JS_EXPORT_PRIVATE bool hasInstance(JSGlobalObject*, JSValue);
    static bool defaultHasInstance(JSGlobalObject*, JSValue, JSValue prototypeProperty);

    static constexpr unsigned maximumPrototypeChainDepth = 40000;
    JS_EXPORT_PRIVATE void getPropertyNames(JSGlobalObject*, PropertyNameArrayBuilder&, DontEnumPropertiesMode);
    JS_EXPORT_PRIVATE static void getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArrayBuilder&, DontEnumPropertiesMode);
    JS_EXPORT_PRIVATE static void NODELETE getOwnSpecialPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArrayBuilder&, DontEnumPropertiesMode);
    JS_EXPORT_PRIVATE void getOwnIndexedPropertyNames(JSGlobalObject*, PropertyNameArrayBuilder&, DontEnumPropertiesMode);
    JS_EXPORT_PRIVATE void getOwnNonIndexPropertyNames(JSGlobalObject*, PropertyNameArrayBuilder&, DontEnumPropertiesMode);
    void getNonReifiedStaticPropertyNames(VM&, PropertyNameArrayBuilder&, DontEnumPropertiesMode);

    JS_EXPORT_PRIVATE uint32_t getEnumerableLength();

    JS_EXPORT_PRIVATE JSValue toPrimitive(JSGlobalObject*, PreferredPrimitiveType = NoPreference) const;
    JS_EXPORT_PRIVATE double toNumber(JSGlobalObject*) const;
    JS_EXPORT_PRIVATE JSString* toString(JSGlobalObject*) const;

    // This get function only looks at the property map.
    //
    // RAW-DOUBLE AWARE. These two overloads already hold the Structure, so they take the checked path -- there is no
    // reason for them to use the unchecked getDirect(PropertyOffset) and 40 callers benefit at once. One of those
    // callers was the 8th unhooked reader found in this project:
    // ObjectAllocationProfileBase::possibleDefaultPropertyCount (ObjectAllocationProfileInlines.h:160), reached from
    // slow_path_create_this via JSFunction::allocateAndInitializeRareData, which reads a property off a PROTOTYPE
    // while sizing an allocation profile. Located by lldb backtrace on the ASSERT_ENABLED detector while running
    // v8-raytrace-strict. See 07-PLAN 5ah.
    JSValue getDirect(VM& vm, PropertyName propertyName) const
    {
        Structure* structure = this->structure();
        PropertyOffset offset = structure->get(vm, propertyName);
        checkOffset(offset, structure->inlineCapacity());
        return offset != invalidOffset ? getDirect(*structure, offset) : JSValue();
    }
    
    JSValue getDirect(VM& vm, PropertyName propertyName, unsigned& attributes) const
    {
        Structure* structure = this->structure();
        PropertyOffset offset = structure->get(vm, propertyName, attributes);
        checkOffset(offset, structure->inlineCapacity());
        return offset != invalidOffset ? getDirect(*structure, offset) : JSValue();
    }

    PropertyOffset getDirectOffset(VM& vm, PropertyName propertyName)
    {
        Structure* structure = this->structure();
        PropertyOffset offset = structure->get(vm, propertyName);
        checkOffset(offset, structure->inlineCapacity());
        return offset;
    }

    PropertyOffset getDirectOffset(VM& vm, PropertyName propertyName, unsigned& attributes)
    {
        Structure* structure = this->structure();
        PropertyOffset offset = structure->get(vm, propertyName, attributes);
        checkOffset(offset, structure->inlineCapacity());
        return offset;
    }

    bool hasInlineStorage() const { return structure()->hasInlineStorage(); }
    ConstPropertyStorage inlineStorageUnsafe() const
    {
        return std::bit_cast<ConstPropertyStorage>(std::bit_cast<const char*>(this) + offsetOfInlineStorage());
    }
    PropertyStorage inlineStorageUnsafe()
    {
        return std::bit_cast<PropertyStorage>(std::bit_cast<char*>(this) + offsetOfInlineStorage());
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

    const Butterfly* butterfly() const LIFETIME_BOUND
    {
        return const_cast<JSObject*>(this)->butterfly();
    }

    Butterfly* butterfly() LIFETIME_BOUND
    {
        // Access m_butterfly field of JSObjectWithButterfly regardless of whether this object is a derived class of JSObjectWithButterfly.
        // This is safe as atom of GC heap allocation is 16 bytes, thus the butterfly field, offset from 8 byte, is always accessible.
        // We intentionally load it regardless to make this function branchless. This is critical to keep this fast while we have butterfly-less objects.
        auto* b = *std::bit_cast<Butterfly**>(std::bit_cast<char*>(this) + butterflyOffset());
        if (type() == WebAssemblyGCObjectType) [[unlikely]]
            b = nullptr;
        return b;
    }

    ConstPropertyStorage outOfLineStorage() const { return butterfly()->propertyStorage(); }
    PropertyStorage outOfLineStorage() { return butterfly()->propertyStorage(); }

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

    bool hasCustomProperties() { return structure()->didTransition(); }

    bool hasNonReifiedStaticProperties()
    {
        return TypeInfo::hasStaticPropertyTable(inlineTypeFlags()) && !structure()->staticPropertiesReified();
    }

    // putOwnDataProperty has 'put' like semantics, however this method:
    //  - assumes the object contains no own getter/setter properties.
    //  - provides no special handling for __proto__
    //  - does not walk the prototype chain (to check for accessors or non-writable properties).
    // This is used by JSLexicalEnvironment.
    bool putOwnDataProperty(VM&, PropertyName, JSValue, PutPropertySlot&);
    bool putOwnDataPropertyMayBeIndex(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

    void putOwnDataPropertyBatching(VM&, UniquedStringImpl**, const EncodedJSValue*, unsigned size);
private:
    void validatePutOwnDataProperty(VM&, PropertyName, JSValue);
public:

    // Fast access to known property offsets.
    // The UNCHECKED read. It cannot know the field's representation, so once useRawDoubleFieldStorage is on it will
    // return bits(d)+2^49 reinterpreted as a double for any Double-represented field -- 1.5 reads back as 1.625.
    // ~140 call sites reach this; rather than audit them by hand, the assertion below makes a Debug run of the test
    // suite report exactly which are reachable for a double-carrying object. Prefer getDirect(Structure&, offset).
    // See analysis/prompt/box2d/07-PLAN-double-field.md section 5f.
    ALWAYS_INLINE JSValue getDirect(PropertyOffset offset) const
    {
#if ASSERT_ENABLED
        assertNotRawDoubleFieldRead(offset);
#endif
        return locationForOffset(offset)->get();
    }
#if ASSERT_ENABLED
    JS_EXPORT_PRIVATE void assertNotRawDoubleFieldRead(PropertyOffset) const;
#endif

    // RAW-DOUBLE-AWARE READ. Correct even when the field is stored as a raw double rather than a NaN-boxed JSValue.
    // Prefer this wherever a Structure is already in hand; the bare getDirect(PropertyOffset) above CANNOT check and
    // is the remaining hazard tracked in analysis/prompt/box2d/07-PLAN-double-field.md section 5d.
    //
    // Cost: one bit test on the Structure in the overwhelmingly common case. Only structures that actually carry a
    // Double-represented property pay the property-table walk, and C++ reads are not a hot path -- the JIT knows the
    // representation statically from its CheckStructure and emits no check at all (section 5e).
    // Structure-aware overload, retained for its ~70 call sites but now ADDING NO CODE: guarantee-only storage keeps
    // every slot a NaN-boxed JSValue, claimed or not. The branch that used to be here is what pushed
    // getOwnNonIndexPropertySlot out of line in the patched build; see
    // repro/bugs/open/25-ROOT-CAUSE-class-c-is-code-size.md.
    ALWAYS_INLINE JSValue getDirect(Structure&, PropertyOffset offset) const
    {
        return getDirect(offset);
    }
    JS_EXPORT_PRIVATE JSValue getDirectRawDoubleAware(Structure&, PropertyOffset) const;
    JSValue getDirect(Locker<JSCellLock>&, Concurrency, Structure* expectedStructure, PropertyOffset) const;
    JSValue getDirectConcurrently(Locker<JSCellLock>&, Structure* expectedStructure, PropertyOffset) const;
    // PHASE 0 census hook for the double-field representation project. Out-of-line and option-gated inside, so
    // this stays a one-liner in the common case. See analysis/prompt/box2d/01-DESIGN-double-field-representation.md.
    JS_EXPORT_PRIVATE void noteDoubleFieldSplitCensus(PropertyOffset, JSValue);

    // RAW-DOUBLE-AWARE WRITE. The mirror of getDirect(Structure&, PropertyOffset), and it must stay a mirror: a slot
    // whose Structure marks it Double is stored as a bare IEEE-754 double, with neither the bias add nor purifyNaN.
    // Getting the two out of step in EITHER direction is the 1.625/1.375 bug class documented in OptionsList.h.
    //
    // This overload takes the DESTINATION structure explicitly because `this->structure()` is NOT always the right
    // authority. Two callers in JSObjectInlines.h add a brand-new property and store to it BEFORE setStructure()
    // installs the structure that owns it, so at store time the object still points at the old structure -- which has
    // neither the property nor its mask bit. That is exactly the object-construction path, i.e. where every double
    // field is first written, so consulting `this->structure()` there would silently store boxed into a slot every
    // reader then treats as raw. Verified by reading JSObjectInlines.h; see 07-PLAN section 5t.
    // NOTE ON TUNING THIS FUNCTION: don't, without a full-sweep measurement. Removing the census gate below and
    // marking this ALWAYS_INLINE was tried and is NET NEGATIVE: it helped json-stringify (-2.56% -> -1.75%),
    // postcss and prismjs, and cost babylonjs-scene-es6 (-0.97% -> -2.33%), FlightPlanner (+0.20% -> -1.85%),
    // threejs, jsdom-d3-startup and json-parse -- summing to -15.6 against -11.8. See
    // repro/bugs/open/25-ROOT-CAUSE-class-c-is-code-size.md: at this scale these gates move cost between
    // benchmarks rather than removing it. The one change that genuinely removed cost was deleting the large
    // dataLogLn diagnostics from ALWAYS_INLINE functions (-260 KB of __text).
    void putDirectOffset(VM& vm, Structure& destinationStructure, PropertyOffset offset, JSValue value)
    {
        // ONE BIT TEST PLUS TWO COMPARES, and nothing else. The claim gate below is required for correctness; the
        // census gate that used to sit in front of it -- two Options loads and two branches on EVERY C++ property
        // store -- was pure diagnostics, and its job is done (it produced doc 19's Gap 1 sizing and the
        // offset-contiguity data that justifies mightHaveClaimAt's range test). The census now runs inside the
        // out-of-line writer, so it only observes stores to claimed structures: a deliberate loss of coverage,
        // recorded in repro/bugs/open/26-ROOT-CAUSES-of-the-14-regressions.md.
        //
        // Verified motivation, not a guess: 3 profiles per side on async-fs put Structure::hasRawDoubleFields() at
        // 0,0,0 samples in base against 27,26,21 patched with a spread of 6 -- i.e. this line is measurably hot on
        // the createIteratorResultObject store path that every `await` allocates through.
        //
        // A DOUBLE NEEDS NO WRITER AT ALL, claimed or not, and that is what the second test buys. Under guarantee-only
        // every slot holds a NaN-boxed JSValue, so for a value that is ALREADY a double the out-of-line writer does
        // nothing but re-derive the claim and perform this same store. The writer is only genuinely needed for the two
        // exceptional kinds: an Int32, which must be re-encoded as a double so a claimed reader does not de-bias a
        // tagged integer, and a non-number, which must give the claim up. Both are rare.
        //
        // This matters because mightHaveClaimAt() is deliberately COARSE -- a summary bit plus a [first,last] range
        // compare, with no dependent load -- so it fires for every offset inside the claimed range, including offsets
        // that carry no claim at all. On json-parse-inspector that is ~1.9M calls into the out-of-line writer per run
        // (see the comment in putDirectOffsetRawDoubleAware), against 475,924 stores the census attributes to claimed
        // offsets, and json-parse-inspector is the largest durable regression in the suite at -2.34%.
        //
        // COST: the census no longer observes pure-double stores to claimed offsets, since those never reach the
        // writer. That is a further deliberate narrowing of an already-narrowed diagnostic (26-ROOT-CAUSES). Engagement
        // is still checkable without it via $vm.isRawDoubleField, which queries the Structure directly.
        if (destinationStructure.mightHaveClaimAt(offset) && !value.isDouble()) [[unlikely]] {
            putDirectOffsetRawDoubleAware(vm, destinationStructure, offset, value);
            return;
        }
        locationForOffset(offset)->set(vm, this, value);
    }

    // FOR CALLERS THAT ALREADY KNOW THE CLAIM STATE. Prefer this wherever the attributes, or the owning Structure's
    // claim bit, have just been computed for another reason -- passing the answer in is strictly cheaper than having
    // the store path re-derive it.
    //
    // WHY IT EXISTS, measured. `locationForOffset(offset)->set(...)` needs only the offset and the value; the claim
    // gate is the ONLY reason a property store touches the Structure at all. mightHaveClaimAt loads Structure's
    // summary bit plus its two range bytes, and on a workload that builds many distinct shapes those lines are cold,
    // so the gate buys a likely cache miss per store. Bisected by build against the unpatched baseline:
    //
    //   claims never created, gate still running : json-parse -1.14%,  regexp-octane -1.26%
    //   claims never created, gate compiled out  : json-parse +0.12% (p=0.52), regexp-octane -0.31% (p=0.55)
    //
    // i.e. a ~1.2% tax paid whether or not any claim exists anywhere, which is why it hit claim-FREE regexp-octane
    // exactly as hard as claim-heavy json-parse.
    //
    // destinationStructure is dereferenced only on the slow path, so an unused reference costs a register, not a load.
    ALWAYS_INLINE void putDirectOffset(VM& vm, Structure& destinationStructure, PropertyOffset offset, JSValue value, bool slotIsClaimed)
    {
        if (slotIsClaimed && !value.isDouble()) [[unlikely]] {
            putDirectOffsetRawDoubleAware(vm, destinationStructure, offset, value);
            return;
        }
        locationForOffset(offset)->set(vm, this, value);
    }

    // Convenience overload for the ~70 callers that store into a property the object ALREADY has, or that added it in
    // place without a transition. For those `this->structure()` is the destination structure. The one bit test is
    // paid only by objects that actually carry a Double-represented field.
    void putDirectOffset(VM& vm, PropertyOffset offset, JSValue value)
    {
        putDirectOffset(vm, *this->structure(), offset, value);
    }
    JS_EXPORT_PRIVATE void putDirectOffsetRawDoubleAware(VM&, Structure&, PropertyOffset, JSValue);

    // PHASE 3 WIDENING. If `propertyName` is currently Double-represented on this object, move this object to a
    // structure that no longer claims it and re-box the value already in the slot. Called from putDirectInternal
    // before a store that cannot be represented as a raw double. No-op unless the object actually carries a
    // Double-represented field, so the common path is one bit test.
    //
    // Returns TRUE if it transitioned. The caller must then disable caching on the PutPropertySlot: this is a
    // structure transition on a store to an EXISTING property, which the Replace IC cannot model.
    JS_EXPORT_PRIVATE bool widenDoubleRepresentation(VM&, PropertyName);
    void putDirectWithoutBarrier(PropertyOffset offset, JSValue value) { locationForOffset(offset)->setWithoutWriteBarrier(value); }

    JS_EXPORT_PRIVATE bool putDirectNativeIntrinsicGetter(VM&, JSGlobalObject*, Identifier, NativeFunction, Intrinsic, unsigned attributes);
    JS_EXPORT_PRIVATE void putDirectNativeIntrinsicGetterWithoutTransition(VM&, JSGlobalObject*, Identifier, NativeFunction, Intrinsic, unsigned attributes);
    JS_EXPORT_PRIVATE bool putDirectNativeFunction(VM&, JSGlobalObject*, const PropertyName&, unsigned functionLength, NativeFunction, ImplementationVisibility, Intrinsic, unsigned attributes);
    JS_EXPORT_PRIVATE bool putDirectNativeFunction(VM&, JSGlobalObject*, const PropertyName&, unsigned functionLength, NativeFunction, ImplementationVisibility, Intrinsic, const DOMJIT::Signature*, unsigned attributes);
    JS_EXPORT_PRIVATE void putDirectNativeFunctionWithoutTransition(VM&, JSGlobalObject*, const PropertyName&, unsigned functionLength, NativeFunction, ImplementationVisibility, Intrinsic, unsigned attributes);

    JS_EXPORT_PRIVATE JSFunction* putDirectBuiltinFunction(VM&, JSGlobalObject*, const PropertyName&, FunctionExecutable*, unsigned attributes);
    JSFunction* putDirectBuiltinFunctionWithoutTransition(VM&, JSGlobalObject*, const PropertyName&, FunctionExecutable*, unsigned attributes);

    JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    bool createDataProperty(JSGlobalObject*, PropertyName, JSValue, bool shouldThrow);

    bool isEnvironment() const;
    bool isGlobalObject() const;
    bool isJSLexicalEnvironment() const;
    bool isGlobalLexicalEnvironment() const;
    bool isStrictEvalActivation() const;
    bool isWithScope() const;

    bool isErrorInstance() const;

    JS_EXPORT_PRIVATE void seal(VM&);
    JS_EXPORT_PRIVATE void freeze(VM&);
    void materializeLazyOwnProperties(VM&);
    JS_EXPORT_PRIVATE static bool preventExtensions(JSObject*, JSGlobalObject*);
    JS_EXPORT_PRIVATE static bool NODELETE isExtensible(JSObject*, JSGlobalObject*);
    bool isSealed(VM& vm) { return structure()->isSealed(vm); }
    bool isFrozen(VM& vm) { return structure()->isFrozen(vm); }

    JS_EXPORT_PRIVATE bool NODELETE anyObjectInChainMayInterceptIndexedAccesses() const;
    bool NODELETE needsSlowPutIndexing() const;

private:
    TransitionKind NODELETE suggestedArrayStorageTransition() const;
public:
    // You should only call isStructureExtensible() when:
    // - Performing this check in a way that isn't described in the specification 
    //   as calling the virtual [[IsExtensible]] trap.
    // - When you're guaranteed that object->methodTable()->isExtensible isn't
    //   overridden.
    ALWAYS_INLINE bool isStructureExtensible() { return structure()->isStructureExtensible(); }
    // You should call this when performing [[IsExtensible]] trap in a place
    // that is described in the specification. This performs the fully virtual
    // [[IsExtensible]] trap.
    bool isExtensible(JSGlobalObject*);
    bool indexingShouldBeSparse()
    {
        return !isStructureExtensible()
            || structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero();
    }

    bool staticPropertiesReified() { return structure()->staticPropertiesReified(); }
    void reifyAllStaticProperties(JSGlobalObject*);

    JS_EXPORT_PRIVATE Butterfly* allocateMoreOutOfLineStorage(VM&, size_t oldSize, size_t newSize);

    // Call this when you do not need to change the structure.
    inline void setButterfly(VM&, Butterfly*); // Defined in JSObjectInlines.h

    // Call this if you do need to change the structure, or if you changed something about a structure
    // in-place.
    inline void nukeStructureAndSetButterfly(VM&, StructureID oldStructureID, Butterfly*); // Defined in JSObjectInlines.h

    void setStructure(VM&, Structure*);

    JS_EXPORT_PRIVATE void convertToDictionary(VM&);
    JS_EXPORT_PRIVATE void convertToUncacheableDictionary(VM&);

    void flattenDictionaryObject(VM& vm)
    {
        structure()->flattenDictionaryStructure(vm, this);
    }
    void shiftButterflyAfterFlattening(const ConcurrentJSLocker&, VM&, Structure*, size_t outOfLineCapacityAfter);

    JSGlobalObject* realmMayBeNull() const
    {
        return structure()->realm();
    }

    JSGlobalObject* realm() const
    {
        SUPPRESS_FORWARD_DECL_ARG auto* result = realmMayBeNull();
        RELEASE_ASSERT(result, "Do not call JSObject::realm() on objects with realmless structures (e.g., WebAssembly GC objects)");
        ASSERT(!isGlobalObject() || ((JSObject*)result) == this);
        return result;
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
    ContiguousJSValues tryMakeWritableInt32(VM& vm); // Defined in JSObjectInlines.h

    // Returns 0 if double storage cannot be created - either because
    // indexing should be sparse, we're having a bad time, or because
    // we already have a more general form of storage (contiguous,
    // or array storage).
    ContiguousDoubles tryMakeWritableDouble(VM& vm); // Defined in JSObjectInlines.h

    // Returns 0 if contiguous storage cannot be created - either because
    // indexing should be sparse or because we're having a bad time.
    ContiguousJSValues tryMakeWritableContiguous(VM& vm); // Defined in JSObjectInlines.h

    // Ensure that the object is in a mode where it has array storage. Use
    // this if you're about to perform actions that would have required the
    // object to be converted to have array storage, if it didn't have it
    // already.
    ArrayStorage* ensureArrayStorage(VM& vm)
    {
        if (hasAnyArrayStorage(indexingType())) [[likely]]
            return butterfly()->arrayStorage();

        return ensureArrayStorageSlow(vm);
    }

    void ensureWritable(VM& vm); // Defined in JSObjectInlines.h

    static constexpr size_t offsetOfInlineStorage();

    static constexpr ptrdiff_t butterflyOffset()
    {
        return sizeof(JSObject);
    }

    void* butterflyAddress()
    {
        return std::bit_cast<char*>(this) + butterflyOffset();
    }

    JS_EXPORT_PRIVATE JSValue getMethod(JSGlobalObject*, CallData&, const Identifier&, const String& errorMessage);

    bool canPerformFastPutInline(VM&, PropertyName);
    bool canPerformFastPutInlineExcludingProto();

    bool mayBePrototype() const;
    void didBecomePrototype(VM&);

    std::optional<Structure::PropertyHashEntry> findPropertyHashEntry(PropertyName) const;

    DECLARE_EXPORT_INFO;

    template <bool debugLLIntGetById = false>
    bool getOwnNonIndexPropertySlot(VM&, Structure*, PropertyName, PropertySlot&, const PrototypeChainDebugData* = nullptr);
    bool getNonIndexPropertySlot(JSGlobalObject*, PropertyName, PropertySlot&);

    JS_EXPORT_PRIVATE NEVER_INLINE bool putInlineSlow(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    JS_EXPORT_PRIVATE NEVER_INLINE bool putInlineFastReplacingStaticPropertyIfNeeded(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    bool putInlineFast(JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

protected:
#if ASSERT_ENABLED
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(is<JSObject>(this));
        ASSERT(structure()->hasPolyProto() || structure()->storedPrototype().isNull() || Heap::heap(this) == Heap::heap(structure()->storedPrototype()));
        ASSERT(structure()->isObject());
        ASSERT(classInfo());
    }
#endif

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    // To instantiate objects you likely want JSFinalObject, below.
    // To create derived types you likely want JSNonFinalObject, below.
    JSObject(VM&, Structure*);

    // Returns reference to butterfly field storage. Only valid for objects that have butterfly storage
    // (JSObjectWithButterfly subclasses). Used by JSObject methods that manipulate butterfly storage.
    ALWAYS_INLINE AuxiliaryBarrier<Butterfly*>& butterflyRef()
    {
        ASSERT(type() != WebAssemblyGCObjectType);
        return *std::bit_cast<AuxiliaryBarrier<Butterfly*>*>(std::bit_cast<char*>(this) + butterflyOffset());
    }

    JSObject(CreatingWellDefinedBuiltinCellTag, StructureID structureID, int32_t blob)
        : JSCell(CreatingWellDefinedBuiltinCell, structureID, blob)
    {
    }

    // Call this if you know that the object is in a mode where it has array
    // storage. This will assert otherwise.
    ArrayStorage* arrayStorage()
    {
        ASSERT(hasAnyArrayStorage(indexingType()));
        return butterfly()->arrayStorage();
    }
        
    // Call this if you want to predicate some actions on whether or not the
    // object is in a mode where it has array storage.
    ArrayStorage* arrayStorageOrNull()
    {
        switch (indexingType()) {
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return butterfly()->arrayStorage();
                
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
    static Butterfly* tryCreateArrayStorageButterfly(VM&, JSObject* intendedOwner, Structure*, unsigned length, unsigned vectorLength, Butterfly* oldButterfly = nullptr);

    ArrayStorage* createArrayStorage(VM&, unsigned length, unsigned vectorLength);
    ArrayStorage* createInitialArrayStorage(VM&);
        
    ContiguousJSValues convertUndecidedToInt32(VM&);
    ContiguousDoubles convertUndecidedToDouble(VM&);
    ContiguousJSValues convertUndecidedToContiguous(VM&);
    ArrayStorage* convertUndecidedToArrayStorage(VM&, TransitionKind);
    ArrayStorage* convertUndecidedToArrayStorage(VM&);
        
    ContiguousDoubles convertInt32ToDouble(VM&);
    ContiguousJSValues convertInt32ToContiguous(VM&);
    ArrayStorage* convertInt32ToArrayStorage(VM&, TransitionKind);
    ArrayStorage* convertInt32ToArrayStorage(VM&);

    ContiguousJSValues convertDoubleToContiguous(VM&);
    ArrayStorage* convertDoubleToArrayStorage(VM&, TransitionKind);
    ArrayStorage* convertDoubleToArrayStorage(VM&);
        
    ArrayStorage* convertContiguousToArrayStorage(VM&, TransitionKind);
    ArrayStorage* convertContiguousToArrayStorage(VM&);

    void convertToIndexingTypeIfNeeded(VM&, IndexingType);
        
    ArrayStorage* ensureArrayStorageExistsAndEnterDictionaryIndexingMode(VM&);
        
    bool defineOwnNonIndexProperty(JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool throwException);

    template<IndexingType indexingShape>
    bool putByIndexBeyondVectorLengthWithoutAttributes(JSGlobalObject*, unsigned propertyName, JSValue);
    bool putByIndexBeyondVectorLengthWithArrayStorage(JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow, ArrayStorage*);

    bool increaseVectorLength(VM&, unsigned newLength);
    void NODELETE deallocateSparseIndexMap();
    bool defineOwnIndexedProperty(JSGlobalObject*, unsigned, const PropertyDescriptor&, bool throwException);
    SparseArrayValueMap* allocateSparseIndexMap(VM&);
        
    void notifyPresenceOfIndexedAccessors(VM&);
        
    bool attemptToInterceptPutByIndexOnHole(JSGlobalObject*, unsigned index, JSValue, bool shouldThrow, bool& putResult);
        
    // Call this if you want setIndexQuickly to succeed and you're sure that
    // the array is contiguous.
    [[nodiscard]] bool ensureLength(VM& vm, unsigned length); // Defined in JSObjectInlines.h
        
    // Call this if you want to shrink the butterfly backing store, and you're
    // sure that the array is contiguous.
    void reallocateAndShrinkButterfly(VM&, unsigned length);
    
    template<IndexingType indexingShape>
    unsigned NODELETE countElements(Butterfly*);
        
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
    ASCIILiteral putDirectInternal(VM&, PropertyName, JSValue, unsigned attr, PutPropertySlot&);

    JS_EXPORT_PRIVATE NEVER_INLINE ASCIILiteral putDirectToDictionaryWithoutExtensibility(VM&, PropertyName, JSValue, PutPropertySlot&);
    JS_EXPORT_PRIVATE void fillGetterPropertySlot(VM&, PropertySlot&, JSCell*, unsigned, PropertyOffset);
    void fillCustomGetterPropertySlot(PropertySlot&, CustomGetterSetter*, unsigned, Structure*, PropertyOffset);

    JS_EXPORT_PRIVATE bool getOwnStaticPropertySlot(VM&, PropertyName, PropertySlot&);
        
    bool putByIndexBeyondVectorLength(JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow);
    bool putDirectIndexBeyondVectorLengthWithArrayStorage(JSGlobalObject*, unsigned propertyName, JSValue, unsigned attributes, PutDirectIndexMode, ArrayStorage*);
    JS_EXPORT_PRIVATE bool putDirectIndexSlowOrBeyondVectorLength(JSGlobalObject*, unsigned propertyName, JSValue, unsigned attributes, PutDirectIndexMode);
        
    unsigned getNewVectorLength(unsigned indexBias, unsigned currentVectorLength, unsigned currentLength, unsigned desiredLength);
    unsigned getNewVectorLength(unsigned desiredLength);

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

    NO_RETURN_DUE_TO_CRASH NEVER_INLINE void crashDueToEmptyValueAtValidOffset(Structure*, PropertyName, PropertyOffset, JSObject* bottomOfChain, JSObject* previousInChain, unsigned attributes, int line, const char* filename, const char* function_name);
};

// JSObjectWithButterfly is a JSObject that has out-of-line property storage (butterfly).
// All normal JS objects go through this class. Wasm GC objects inherit JSObject directly
// without butterfly to save 8 bytes per allocation.
class JSObjectWithButterfly : public JSObject {
    friend class JSObject;
    friend class JSFinalObject;
    friend class LLIntOffsetsExtractor;

public:
    using Base = JSObject;

    DECLARE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE);

    DECLARE_EXPORT_INFO;

    const Butterfly* butterfly() const LIFETIME_BOUND { return m_butterfly.get(); }
    Butterfly* butterfly() LIFETIME_BOUND { return m_butterfly.get(); }
    Dependency fencedButterfly(Butterfly*& butterfly)
    {
        return Dependency::loadAndFence(static_cast<Butterfly**>(butterflyAddress()), butterfly);
    }

    ConstPropertyStorage outOfLineStorage() const { return m_butterfly->propertyStorage(); }
    PropertyStorage outOfLineStorage() { return m_butterfly->propertyStorage(); }

    void* butterflyAddress()
    {
        return &m_butterfly;
    }

    // Visits the butterfly unless there is a race. Returns the structure if there was no race.
    template<typename Visitor> Structure* visitButterfly(Visitor&);
    template<typename Visitor> Structure* visitButterflyImpl(Visitor&);
    template<typename Visitor> void markAuxiliaryAndVisitOutOfLineProperties(Visitor&, Butterfly*, Structure*, PropertyOffset maxOffset);

protected:
    JSObjectWithButterfly(VM& vm, Structure* structure, Butterfly* butterfly = nullptr)
        : JSObject(vm, structure)
        , m_butterfly(butterfly, WriteBarrierEarlyInit)
    {
    }

    JSObjectWithButterfly(CreatingWellDefinedBuiltinCellTag, StructureID structureID, int32_t blob)
        : JSObject(CreatingWellDefinedBuiltinCell, structureID, blob)
        , m_butterfly(nullptr, WriteBarrierEarlyInit)
    {
    }

private:
    AuxiliaryBarrier<Butterfly*> m_butterfly;
#if CPU(ADDRESS32)
    unsigned m_32BitPadding;
#endif
};

constexpr size_t JSObject::offsetOfInlineStorage()
{
    return sizeof(JSObjectWithButterfly);
}

// JSNonFinalObject is a type of JSObject that has some internal storage,
// but also preserves some space in the collector cell for additional
// data members in derived types.
class JSNonFinalObject : public JSObjectWithButterfly {
    friend class JSObject;

public:
    typedef JSObjectWithButterfly Base;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

protected:
    explicit JSNonFinalObject(VM& vm, Structure* structure, Butterfly* butterfly = nullptr)
        : JSObjectWithButterfly(vm, structure, butterfly)
    {
    }

#if ASSERT_ENABLED
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(!this->structure()->hasInlineStorage());
        ASSERT(classInfo());
    }
#endif
};

// JSFinalObject is a type of JSObject that contains sufficient internal
// storage to fully make use of the collector cell containing it.
class JSFinalObject final : public JSObjectWithButterfly {
    friend class JSObject;
public:
    using Base = JSObjectWithButterfly;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM&);

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        return sizeof(JSObjectWithButterfly) + inlineCapacity * sizeof(WriteBarrierBase<Unknown>);
    }

    static inline constexpr TypeInfo typeInfo() { return TypeInfo(FinalObjectType, StructureFlags); }
    static constexpr IndexingType defaultIndexingType = NonArray;
    static constexpr uint32_t defaultTypeInfoBlob()
    {
        return TypeInfoBlob::typeInfoBlob(defaultIndexingType, typeInfo().type(), typeInfo().inlineTypeFlags());
    }

    static constexpr unsigned defaultSizeInBytes = 64;
    static constexpr unsigned defaultInlineCapacity = (defaultSizeInBytes - sizeof(JSObjectWithButterfly)) / sizeof(WriteBarrier<Unknown>);
    static_assert(defaultInlineCapacity < firstOutOfLineOffset);

    static constexpr unsigned maxSizeInBytes = 512;
    static constexpr unsigned maxInlineCapacity = (maxSizeInBytes - sizeof(JSObjectWithButterfly)) / sizeof(WriteBarrier<Unknown>);
    static_assert(maxInlineCapacity < firstOutOfLineOffset);

    static JSFinalObject* create(VM&, Structure*);
    static JSFinalObject* createWithButterfly(VM&, Structure*, Butterfly*);
    static JSFinalObject* createWithButterflyCopyingInlineStorage(VM&, Structure*, Butterfly*, const WriteBarrierBase<Unknown>* inlineStorageSource);
    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue, unsigned);

    static JSFinalObject* createDefaultEmptyObject(JSGlobalObject*);

    DECLARE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE);

    DECLARE_EXPORT_INFO;

private:
    friend class LLIntOffsetsExtractor;

    explicit JSFinalObject(VM& vm, Structure* structure, Butterfly* butterfly, size_t inlineCapacity)
        : JSObjectWithButterfly(vm, structure, butterfly)
    {
        // We do not need to use gcSafeMemcpy since this object is not exposed yet.
        memset(inlineStorageUnsafe(), 0, inlineCapacity * sizeof(EncodedJSValue));
    }

    // Initializes the inline storage by copying from |inlineStorageSource| instead of zero-filling.
    explicit JSFinalObject(VM& vm, Structure* structure, Butterfly* butterfly, size_t inlineCapacity, const WriteBarrierBase<Unknown>* inlineStorageSource)
        : JSObjectWithButterfly(vm, structure, butterfly)
    {
        auto* destination = reinterpret_cast<EncodedJSValue*>(inlineStorageUnsafe());
        auto* source = reinterpret_cast<const EncodedJSValue*>(inlineStorageSource);
        for (size_t i = 0; i < inlineCapacity; ++i)
            destination[i] = source[i];
    }

    explicit JSFinalObject(CreatingWellDefinedBuiltinCellTag, StructureID structureID)
        : JSObjectWithButterfly(CreatingWellDefinedBuiltinCell, structureID, defaultTypeInfoBlob())
    {
        // We do not need to use gcSafeMemcpy since this object is not exposed yet.
        memset(inlineStorageUnsafe(), 0, defaultInlineCapacity * sizeof(EncodedJSValue));
     }

#if ASSERT_ENABLED
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(butterfly() || structure()->totalStorageCapacity() == structure()->inlineCapacity());
        ASSERT(classInfo());
    }
#endif
};

JS_EXPORT_PRIVATE JSC_DECLARE_HOST_FUNCTION(objectPrivateFuncInstanceOf);

inline JSFinalObject* JSFinalObject::createWithButterfly(VM& vm, Structure* structure, Butterfly* butterfly)
{
    size_t inlineCapacity = structure->inlineCapacity();
    JSFinalObject* finalObject = new (
        NotNull,
        allocateCell<JSFinalObject>(vm, allocationSize(inlineCapacity))
    ) JSFinalObject(vm, structure, butterfly, inlineCapacity);
    finalObject->finishCreation(vm);
    return finalObject;
}

inline JSFinalObject* JSFinalObject::createWithButterflyCopyingInlineStorage(VM& vm, Structure* structure, Butterfly* butterfly, const WriteBarrierBase<Unknown>* inlineStorageSource)
{
    size_t inlineCapacity = structure->inlineCapacity();
    JSFinalObject* finalObject = new (NotNull, allocateCell<JSFinalObject>(vm, allocationSize(inlineCapacity))) JSFinalObject(vm, structure, butterfly, inlineCapacity, inlineStorageSource);
    finalObject->finishCreation(vm);
    return finalObject;
}

inline JSFinalObject* JSFinalObject::create(VM& vm, Structure* structure)
{
    return createWithButterfly(vm, structure, nullptr);
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
    ASSERT(!butterfly() == !(structure->outOfLineCapacity() || structure->hasIndexingHeader(this)));
    JSCell::setStructure(vm, structure);
}

inline JSObject* asObject(JSCell* cell)
{
    ASSERT(cell);
    ASSERT(cell->isObjectSlow());
    return uncheckedDowncast<JSObject>(cell);
}

inline JSObject* asObject(JSValue value)
{
    return asObject(value.asCell());
}

inline JSObject::JSObject(VM& vm, Structure* structure)
    : JSCell(vm, structure)
{
}

// Normally, we never shrink the butterfly so if we know an offset is valid for some
// past structure then it should be valid for any new structure. However, we may sometimes
// shrink the butterfly when we are holding the Structure's ConcurrentJSLock, such as when we
// flatten an object.
IGNORE_RETURN_TYPE_WARNINGS_BEGIN
ALWAYS_INLINE JSValue JSObject::getDirect(Locker<JSCellLock>& cellLock, Concurrency concurrency, Structure* expectedStructure, PropertyOffset offset) const
{
    switch (concurrency) {
    case Concurrency::MainThread:
        ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
        // Raw-double aware, for the same reason as the ConcurrentThread branch below: both are reached from
        // PropertyCondition (isStillValidAssumingImpurePropertyWatchpoint here, attemptToMakeEquivalence* there),
        // whose result gets baked into compiled code as a watchpoint or a folded constant. Fixing only one branch
        // leaves the bug alive on whichever concurrency the caller happens to use -- which is exactly what
        // happened: the concurrent branch was fixed first and the detector immediately fired on this one.
        return getDirect(*expectedStructure, offset);
    case Concurrency::ConcurrentThread:
        return getDirectConcurrently(cellLock, expectedStructure, offset);
    }
}
IGNORE_RETURN_TYPE_WARNINGS_END

inline JSValue JSObject::getDirectConcurrently(Locker<JSCellLock>&, Structure* expectedStructure, PropertyOffset offset) const
{
    // We always take the cell lock before the structure lock.
    // We must take the cell lock to prevent places like JSArray::unshiftCountWithArrayStorage
    // from changing the butterfly out from under us.
    ConcurrentJSLocker locker { expectedStructure->lock() };
    if (!expectedStructure->isValidOffset(offset))
        return { };
    // RAW-DOUBLE AWARE, and it must be. This is the 6th unhooked reader in the project and by far the most
    // damaging, because it does not merely return a wrong value to a caller -- it runs on the DFG compiler thread
    // from PropertyCondition::attemptToMakeEquivalenceWithoutBarrier, which CONSTANT-FOLDS the value it reads into
    // compiled code. Reading a raw 1.5 as a JSValue yields 1.375, so the prototype load folds to 1.375 and every
    // execution of that compiled code is wrong. Found by lldb backtrace on the ASSERT_ENABLED detector, on thread
    // "JIT Worklist Helper Thread" inside ByteCodeParser::planLoad; the earlier 5666-test sweep missed it because
    // nothing in that corpus put a Double-represented field on a prototype.
    //
    // isRawDoubleOffset is lock-free and allocation-free by construction (Structure.h), which is what makes it
    // legal on a compiler thread holding these two locks.
    return getDirect(*expectedStructure, offset);
}

// It is safe to call this method with a PropertyName that is actually an index,
// but if so will always return false (doesn't search index storage).
template<bool debugLLIntGetById>
ALWAYS_INLINE bool JSObject::getOwnNonIndexPropertySlot(VM& vm, Structure* structure, PropertyName propertyName, PropertySlot& slot, const PrototypeChainDebugData* debugData)
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

    // Structure-aware: this is the generic C++ read path for every get_by_id that misses its IC, so it is the site
    // a Double-represented field is overwhelmingly most likely to be read through. `structure` is already in hand,
    // so the raw-double check costs one bit test. Empirically the top offender: 91 of the 96 stress tests that
    // tripped the ASSERT_ENABLED detector reached it here (both template instantiations).
    JSValue value = getDirect(*structure, offset);

    if constexpr (debugLLIntGetById) {
        if (!value)
            crashDueToEmptyValueAtValidOffset(structure, propertyName, offset, debugData->bottomOfChain, debugData->previousInChain, attributes, __LINE__, __FILE__, WTF_PRETTY_FUNCTION);
    }

    // NOTE: two large dataLogLn diagnostics used to sit here, hunting "a raw-double slot read as a JSValue".
    // They are obsolete under guarantee-only storage -- every slot IS a JSValue, claimed or not -- and they were
    // enormously expensive in CODE SIZE: this function is ALWAYS_INLINE and inlined into dozens of hot callers, and
    // an Options-gated branch still emits its whole body at every one of them. Measured: they accounted for the bulk
    // of the patch's +326 KB of __text, which showed up as ~1% regressions across every startup/library benchmark.
    // See repro/bugs/open/25-ROOT-CAUSE-class-c-is-code-size.md. Do not reintroduce a dataLogLn in this function;
    // put diagnostics behind an out-of-line JS_EXPORT_PRIVATE call.

    if (value.isCell()) {
        ASSERT(value);
        JSCell* cell = value.asCell();
        JSType type = cell->type();
        switch (type) {
        case GetterSetterType:
            ASSERT(attributes & PropertyAttribute::Accessor);
            fillGetterPropertySlot(vm, slot, cell, attributes, offset);
            return true;
        case CustomGetterSetterType:
            ASSERT(attributes & PropertyAttribute::CustomAccessorOrValue);
            fillCustomGetterPropertySlot(slot, uncheckedDowncast<CustomGetterSetter>(cell), attributes, structure, offset);
            return true;
        default:
            break;
        }
    }
    
    slot.setValue(this, attributes, value, offset);
    return true;
}

ALWAYS_INLINE void JSObject::fillCustomGetterPropertySlot(PropertySlot& slot, CustomGetterSetter* customGetterSetter, unsigned attributes, Structure* structure, PropertyOffset offset)
{
    ASSERT(attributes & PropertyAttribute::CustomAccessorOrValue);
    if (customGetterSetter->inherits<DOMAttributeGetterSetter>()) {
        auto* domAttribute = uncheckedDowncast<DOMAttributeGetterSetter>(customGetterSetter);
        if (structure->isUncacheableDictionary())
            slot.setCustom(this, attributes, domAttribute->getter(), domAttribute->setter(), domAttribute->domAttribute());
        else
            slot.setCacheableCustom(this, attributes, domAttribute->getter(), domAttribute->setter(), domAttribute->domAttribute(), offset);
        return;
    }

    if (structure->isUncacheableDictionary())
        slot.setCustom(this, attributes, customGetterSetter->getter(), customGetterSetter->setter());
    else
        slot.setCacheableCustom(this, attributes, customGetterSetter->getter(), customGetterSetter->setter(), offset);
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlotImpl(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    Structure* structure = object->structure();
    if (object->getOwnNonIndexPropertySlot(vm, structure, propertyName, slot))
        return true;
    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return getOwnPropertySlotByIndex(object, globalObject, index.value(), slot);
    return false;
}

#if !ASSERT_ENABLED
ALWAYS_INLINE bool JSObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    return getOwnPropertySlotImpl(object, globalObject, propertyName, slot);
}
#endif

// It may seem crazy to inline a function this large but it makes a big difference
// since this is function very hot in variable lookup
template<bool checkNullStructure, bool debugLLIntGetById>
ALWAYS_INLINE bool JSObject::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    JSObject* object = this;
    JSObject* previous = nullptr;
    while (true) {
        if (TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags())) [[unlikely]] {
            // If propertyName is an index then we may have missed it (as this loop is using
            // getOwnNonIndexPropertySlot), so we cannot safely call the overridden getOwnPropertySlot
            // (lest we return a property from a prototype that is shadowed). Check now for an index,
            // if so we need to start afresh from this object.
            if (std::optional<uint32_t> index = parseIndex(propertyName))
                return getPropertySlot(globalObject, index.value(), slot);
            // Safe to continue searching from current position; call getNonIndexPropertySlot to avoid
            // parsing the int again.
            return object->getNonIndexPropertySlot(globalObject, propertyName, slot);
        }
        ASSERT(object->type() != ProxyObjectType);
        Structure* structure = object->structureID().decode();
        if (checkNullStructure) {
            if (!structure) [[unlikely]]
                CRASH_WITH_INFO(object->type(), object->structureID().bits());
        }
        if constexpr (debugLLIntGetById) {
            PrototypeChainDebugData debugData { .bottomOfChain = this, .previousInChain = previous };
            if (object->getOwnNonIndexPropertySlot<debugLLIntGetById>(vm, structure, propertyName, slot, &debugData))
                return true;
        } else {
            if (object->getOwnNonIndexPropertySlot<debugLLIntGetById>(vm, structure, propertyName, slot))
                return true;
        }
        // FIXME: This doesn't look like it's following the specification:
        // https://bugs.webkit.org/show_bug.cgi?id=172572
        JSValue prototype = structure->storedPrototype(object);
        if (!prototype.isObject())
            break;
        if constexpr (debugLLIntGetById)
            previous = object;
        object = asObject(prototype);
    }

    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return getPropertySlot(globalObject, index.value(), slot);
    return false;
}

inline bool JSObject::putDirect(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!value.isGetterSetterSlow() && !(attributes & PropertyAttribute::Accessor));
    ASSERT(!value.isCustomGetterSetterSlow() && !(attributes & PropertyAttribute::CustomAccessorOrValue));
    PutPropertySlot slot(this);
    return putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, value, attributes, slot).isNull();
}

inline bool JSObject::putDirect(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes, PutPropertySlot& slot)
{
    ASSERT(!value.isGetterSetterSlow());
    ASSERT(!value.isCustomGetterSetterSlow());
    return putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, value, attributes, slot).isNull();
}

inline bool JSObject::putDirect(VM& vm, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    ASSERT(!value.isGetterSetterSlow());
    ASSERT(!value.isCustomGetterSetterSlow());
    return putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, value, 0, slot).isNull();
}

inline constexpr intptr_t offsetInButterfly(PropertyOffset offset)
{
    return offsetInOutOfLineStorage(offset) + Butterfly::indexOfPropertyStorage();
}

inline size_t JSObject::butterflyPreCapacity()
{
    if (hasIndexingHeader()) [[unlikely]]
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

    if (hasIndexingHeader) [[unlikely]] {
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
    static_assert(!(JSObject::offsetOfInlineStorage() % sizeof(EncodedJSValue)));
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
    return static_cast<size_t>(addressOffset);
}

static_assert(!(sizeof(JSObjectWithButterfly) % sizeof(WriteBarrierBase<Unknown>)), "JSObject inline storage has correct alignment");
static_assert(sizeof(JSObject) == sizeof(JSCell), "JSObject should be the same size as JSCell (no butterfly)");
static_assert(JSObject::butterflyOffset() == sizeof(JSObject), "butterfly offset must be right after JSObject");

ALWAYS_INLINE Identifier makeIdentifier(VM& vm, ASCIILiteral literal)
{
    return Identifier::fromString(vm, literal);
}

ALWAYS_INLINE Identifier makeIdentifier(VM&, const Identifier& name)
{
    return name;
}

bool validateAndApplyPropertyDescriptor(JSGlobalObject*, JSObject*, PropertyName, bool isExtensible,
    const PropertyDescriptor& descriptor, bool isCurrentDefined, const PropertyDescriptor& current, bool throwException);

JS_EXPORT_PRIVATE NEVER_INLINE bool ordinarySetSlow(JSGlobalObject*, JSObject*, PropertyName, JSValue, JSValue receiver, bool shouldThrow);
JS_EXPORT_PRIVATE NEVER_INLINE bool ordinarySetWithOwnDescriptor(JSGlobalObject*, JSObject*, PropertyName, JSValue, JSValue receiver, PropertyDescriptor&& ownDescriptor, bool shouldThrow);

bool setterThatIgnoresPrototypeProperties(JSGlobalObject*, JSValue thisValue, JSObject* homeObject, PropertyName, JSValue, bool shouldThrow);

// Helper for defining native functions, if you're not using a static hash table.
// Use this macro from within finishCreation() methods in prototypes. This assumes
// you've defined variables called globalObject, globalObject, and vm, and they
// have the expected meanings.
#define JSC_NATIVE_INTRINSIC_FUNCTION(jsName, cppName, attributes, length, implementationVisibility, intrinsic) \
    putDirectNativeFunction(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (length), cppName, \
        (implementationVisibility), (intrinsic), (attributes))

#define JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(jsName, cppName, attributes, length, implementationVisibility, intrinsic) \
    putDirectNativeFunctionWithoutTransition(\
        vm, globalObject, makeIdentifier(vm, (jsName)), (length), cppName, \
        (implementationVisibility), (intrinsic), (attributes))

// As above, but this assumes that the function you're defining doesn't have an
// intrinsic.
#define JSC_NATIVE_FUNCTION(jsName, cppName, attributes, length, implementationVisibility) \
    JSC_NATIVE_INTRINSIC_FUNCTION(jsName, cppName, (attributes), (length), (implementationVisibility), JSC::NoIntrinsic)

#define JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(jsName, cppName, attributes, length, implementationVisibility) \
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(jsName, cppName, (attributes), (length), (implementationVisibility), JSC::NoIntrinsic)

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

// JSValue::put, JSValue::putByIndex, and JSValue::getPrototype are defined in JSObjectInlines.h.

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
