/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/ClassInfo.h>
#include <JavaScriptCore/Concurrency.h>
#include <JavaScriptCore/ConcurrentJSLock.h>
#include <JavaScriptCore/IndexingType.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/JSTypeInfo.h>
#include <JavaScriptCore/PropertyName.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/PropertyOffset.h>
#include <JavaScriptCore/PutPropertySlot.h>
#include <JavaScriptCore/StructureRareData.h>
#include <JavaScriptCore/StructureTransitionTable.h>
#include <JavaScriptCore/TypeInfoBlob.h>
#include <JavaScriptCore/Watchpoint.h>
#include <wtf/Atomics.h>
#include <wtf/CompactPointerTuple.h>
#include <wtf/CompactPtr.h>
#include <wtf/CompactRefPtr.h>

namespace WTF {

class UniquedStringImpl;

} // namespace WTF

namespace JSC {

class DeferGC;
class DeferredStructureTransitionWatchpointFire;
class LLIntOffsetsExtractor;
class PropertyNameArrayBuilder;
class PropertyNameArray;
class PropertyTable;
class StructureChain;
class StructureShape;
class JSString;
struct DumpContext;
struct HashTable;
struct HashTableValue;

namespace Integrity {
class Analyzer;
}

class DeferredStructureTransitionWatchpointFire final : public DeferredWatchpointFire {
    WTF_MAKE_NONCOPYABLE(DeferredStructureTransitionWatchpointFire);
public:
    DeferredStructureTransitionWatchpointFire(VM& vm, Structure* structure)
        : DeferredWatchpointFire()
        , m_vm(vm)
        , m_structure(structure)
    {
    }

    ~DeferredStructureTransitionWatchpointFire()
    {
        if (watchpointsToFire().state() == IsWatched)
            fireAllSlow();
    }

    const Structure* structure() const { return m_structure; }


private:
    JS_EXPORT_PRIVATE void fireAllSlow();

    VM& m_vm;
    const Structure* m_structure;
};

// The out-of-line property storage capacity to use when first allocating out-of-line
// storage. Note that all objects start out without having any out-of-line storage;
// this comes into play only on the first property store that exhausts inline storage.
static constexpr unsigned initialOutOfLineCapacity = 4;

// The factor by which to grow out-of-line storage when it is exhausted, after the
// initial allocation.
static constexpr unsigned outOfLineGrowthFactor = 2;

class PropertyTableEntry;
class CompactPropertyTableEntry {
public:
    CompactPropertyTableEntry()
        : m_data(nullptr, 0)
    {
    }

    CompactPropertyTableEntry(UniquedStringImpl* key, PropertyOffset offset, unsigned attributes)
        : m_data(key, ((offset << 8) | attributes))
    {
        ASSERT(this->attributes() == attributes);
        ASSERT(this->offset() == offset);
    }

    CompactPropertyTableEntry(const PropertyTableEntry&);

    UniquedStringImpl* key() const { return m_data.pointer(); }
    void setKey(UniquedStringImpl* key) { m_data.setPointer(key); }
    PropertyOffset offset() const { return m_data.type() >> 8; }
    void setOffset(PropertyOffset offset)
    {
        m_data.setType((m_data.type() & 0x00ffU) | (offset << 8));
        ASSERT(this->offset() == offset);
    }
    uint8_t attributes() const { return m_data.type(); }
    void setAttributes(uint8_t attributes)
    {
        m_data.setType((m_data.type() & 0xff00U) | attributes);
        ASSERT(this->attributes() == attributes);
    }

private:
    CompactPointerTuple<UniquedStringImpl*, uint16_t> m_data;
};

class PropertyTableEntry {
public:
    PropertyTableEntry() = default;

    PropertyTableEntry(UniquedStringImpl* key, PropertyOffset offset, unsigned attributes)
        : m_key(key)
        , m_offset(offset)
        , m_attributes(attributes)
    {
        ASSERT(this->attributes() == attributes);
    }

    PropertyTableEntry(const CompactPropertyTableEntry& entry)
        : m_key(entry.key())
        , m_offset(entry.offset())
        , m_attributes(entry.attributes())
    {
    }

    UniquedStringImpl* key() const { return m_key; }
    void setKey(UniquedStringImpl* key) { m_key = key; }
    PropertyOffset offset() const { return m_offset; }
    void setOffset(PropertyOffset offset) { m_offset = offset; }
    uint8_t attributes() const { return m_attributes; }
    void setAttributes(uint8_t attributes) { m_attributes = attributes; }

private:
    UniquedStringImpl* m_key { nullptr };
    PropertyOffset m_offset { 0 };
    uint8_t m_attributes { 0 };
};


inline CompactPropertyTableEntry::CompactPropertyTableEntry(const PropertyTableEntry& entry)
    : m_data(entry.key(), ((entry.offset() << 8) | entry.attributes()))
{
}

class StructureFireDetail final : public FireDetail {
public:
    inline StructureFireDetail(const Structure*); // Defined in StructureInlines.h.

    void dump(PrintStream& out) const final;

private:
    explicit StructureFireDetail(ClangVTableWorkaroundTag);

    const Structure* m_structure;
};

class Structure : public JSCell {
    static constexpr uint16_t shortInvalidOffset = std::numeric_limits<uint16_t>::max() - 1;
    static constexpr uint16_t useRareDataFlag = std::numeric_limits<uint16_t>::max();
public:
    friend class StructureTransitionTable;

    typedef JSCell Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;
    static constexpr uint8_t numberOfLowerTierPreciseCells = 0;

    static_assert(JSCell::atomSize >= MarkedBlock::atomSize);

    static constexpr int s_maxTransitionLength = 128;
    static constexpr int s_maxTransitionLengthForNonEvalPutById = 512;
    static constexpr int s_maxTransitionLengthForRemove = 4096; // Picked from benchmarking measurement.

    using SeenProperties = TinyBloomFilter<CompactPtr<UniquedStringImpl>::StorageType>;

    enum PolyProtoTag { PolyProto };
    inline static Structure* create(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, unsigned inlineCapacity = 0); // Defined in StructureInlines.h
    static Structure* create(PolyProtoTag, VM&, JSGlobalObject*, JSObject* prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, unsigned inlineCapacity = 0);

    ~Structure();
    
    template<typename CellType, SubspaceAccess>
    inline static GCClient::IsoSubspace* subspaceFor(VM&); // Defined in StructureInlines.h

    JS_EXPORT_PRIVATE static bool isValidPrototype(JSValue);

protected:
    inline void finishCreation(VM& vm, const Structure* previous, DeferredStructureTransitionWatchpointFire* deferred); // Defined in StructureInlines.h

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(m_prototype.get().isEmpty() || isValidPrototype(m_prototype.get()));
    }

private:
    inline void finishCreation(VM&, CreatingEarlyCellTag); // Defined in StructureInlines.h

    void NODELETE validateFlags();

public:
    StructureID id() const { return StructureID::encode(this); }

    uint32_t typeInfoBlob() const { return m_blob.blob(); }

    bool isProxy() const
    {
        JSType type = m_blob.type();
        return type == GlobalProxyType || type == ProxyObjectType;
    }

    static void dumpStatistics();

    inline bool shouldDoCacheableDictionaryTransitionForAdd(PutPropertySlot::Context context)
    {
        int maxTransitionLength;
        if (context == PutPropertySlot::PutById)
            maxTransitionLength = s_maxTransitionLengthForNonEvalPutById;
        else
            maxTransitionLength = s_maxTransitionLength;
        return transitionCountEstimate() > maxTransitionLength;
    }

    inline bool shouldDoCacheableDictionaryTransitionForRemoveAndAttributeChange()
    {
        return transitionCountEstimate() > s_maxTransitionLengthForRemove || transitionCountHasOverflowed();
    }

    ALWAYS_INLINE bool transitionCountHasOverflowed() const
    {
        int transitionCount = 0;
        for (auto* structure = this; structure; structure = structure->previousID()) {
            if (++transitionCount > s_maxTransitionLength)
                return true;
        }

        return false;
    }

    Structure* trySingleTransition() { return m_transitionTable.trySingleTransition(); }

    JS_EXPORT_PRIVATE static Structure* addPropertyTransition(VM&, Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    enum class MayReplaceExistingTransition : bool { No = false, Yes = true };

    JS_EXPORT_PRIVATE static Structure* addNewPropertyTransition(VM&, Structure*, PropertyName, unsigned attributes, PropertyOffset&, PutPropertySlot::Context = PutPropertySlot::UnknownContext, DeferredStructureTransitionWatchpointFire* = nullptr, MayReplaceExistingTransition = MayReplaceExistingTransition::No);
    static Structure* addPropertyTransitionToExistingStructureConcurrently(Structure*, UniquedStringImpl* uid, unsigned attributes, PropertyOffset&);
    static Structure* addPropertyTransitionToExistingStructure(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    static Structure* removeNewPropertyTransition(VM&, Structure*, PropertyName, PropertyOffset&, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* removePropertyTransition(VM&, Structure*, PropertyName, PropertyOffset&, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* removePropertyTransitionFromExistingStructure(Structure*, PropertyName, PropertyOffset&);
    static Structure* removePropertyTransitionFromExistingStructureConcurrently(Structure*, PropertyName, PropertyOffset&);
    static Structure* changePrototypeTransition(VM&, Structure*, JSValue prototype, DeferredStructureTransitionWatchpointFire&);
    static Structure* changeGlobalProxyTargetTransition(VM&, Structure*, JSGlobalObject*, DeferredStructureTransitionWatchpointFire&);
    JS_EXPORT_PRIVATE static Structure* attributeChangeTransition(VM&, Structure*, PropertyName, unsigned attributes, DeferredStructureTransitionWatchpointFire* = nullptr);

    // Returns a Structure identical to `structure` except that `offset` is guaranteed NOT to be Double-represented,
    // so an arbitrary JSValue may be stored there. Returns `structure` unchanged in the overwhelmingly common case:
    // one bit test (hasRawDoubleFields(), inside isRawDoubleOffset).
    //
    // WHY THIS EXISTS. StructureTransitionTable::Hash::Key masks PropertyAttribute::RepresentationDouble out of the
    // key for every transition kind EXCEPT PropertyAttributeChange, which makes "give me the transition for
    // attributes A" ambiguous for the rest: the cached answer may claim the slot raw even though the caller passed
    // the bit clear, because some earlier double store created it. Callers that can absorb either answer (a numeric
    // store) need nothing. Callers that CANNOT -- the replace arm of putDirectInternal, and engine-internal builders
    // that lay out a fixed shape and then store cells into it -- must say so, and this is how.
    JS_EXPORT_PRIVATE static Structure* ensureBoxedRepresentation(VM&, Structure*, PropertyName, PropertyOffset,
        DeferredStructureTransitionWatchpointFire* = nullptr);

    // addPropertyTransition that additionally guarantees the new property's slot is BOXED. For engine-internal
    // structure builders, which lay out a fixed shape once and then store arbitrary JSValues into it by offset. They
    // pass the representation bit clear, but the ADD transition key masks it, so they can be served a raw-claiming
    // sibling created by an unrelated script store of a double under the same property name -- after which storing a
    // boolean into the slot trips putDirectOffsetRawDoubleAware's RELEASE_ASSERT, which is NOT compiled out in
    // release. repro/bugs/07-repro-descriptor-structure-adopts-raw-transition.js.
    // Builds a SECOND PropertyAddition child whose attributes lack RepresentationDouble, and lets the transition table
    // overwrite the entry with it, so a raw claim a script has disproved stops being handed out to future adds. Never
    // touches a live slot. See the long comment on the definition in Structure.cpp.
    JS_EXPORT_PRIVATE static Structure* replaceRawPropertyAdditionWithBoxed(VM&, Structure*, PropertyName,
        unsigned attributes, PropertyOffset&, DeferredStructureTransitionWatchpointFire* = nullptr);

    JS_EXPORT_PRIVATE static Structure* addPropertyTransitionForBoxedSlot(VM&, Structure*, PropertyName,
        unsigned attributes, PropertyOffset&);
    static Structure* attributeChangeTransitionToExistingStructureConcurrently(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* attributeChangeTransitionToExistingStructure(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* toCacheableDictionaryTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* toUncacheableDictionaryTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    JS_EXPORT_PRIVATE static Structure* sealTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    JS_EXPORT_PRIVATE static Structure* freezeTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* preventExtensionsTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* nonPropertyTransition(VM&, Structure*, TransitionKind, DeferredStructureTransitionWatchpointFire*);
    static Structure* setBrandTransitionFromExistingStructureConcurrently(Structure*, UniquedStringImpl*);
    static Structure* setBrandTransition(VM&, Structure*, Symbol* brand, DeferredStructureTransitionWatchpointFire* = nullptr);
    JS_EXPORT_PRIVATE static Structure* becomePrototypeTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);

    JS_EXPORT_PRIVATE bool isSealed(VM&);
    JS_EXPORT_PRIVATE bool isFrozen(VM&);
    bool isStructureExtensible() const { return !didPreventExtensions(); }

    JS_EXPORT_PRIVATE Structure* flattenDictionaryStructure(VM&, JSObject*);

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    // Versions that take a func will call it after making the change but while still holding
    // the lock. The callback is not called if there is no change being made, like if you call
    // removePropertyWithoutTransition() and the property is not found.
    template<typename Func>
    PropertyOffset addPropertyWithoutTransition(VM&, PropertyName, unsigned attributes, const Func&);
    template<typename Func>
    PropertyOffset removePropertyWithoutTransition(VM&, PropertyName, const Func&);
    template<typename Func>
    PropertyOffset attributeChangeWithoutTransition(VM&, PropertyName, unsigned attributes, const Func&);
    template<typename Func>
    auto addOrReplacePropertyWithoutTransition(VM&, PropertyName, unsigned attributes, const Func&) -> decltype(auto);
    void setPrototypeWithoutTransition(VM&, JSValue prototype);
        
    bool isDictionary() const { return dictionaryKind() != NoneDictionaryKind; }
    bool isUncacheableDictionary() const { return dictionaryKind() == UncachedDictionaryKind; }
    bool isCacheableDictionary() const { return dictionaryKind() == CachedDictionaryKind; }
  
    bool prototypeQueriesAreCacheable()
    {
        return !typeInfo().prohibitsPropertyCaching();
    }
    
    bool propertyAccessesAreCacheable()
    {
        return dictionaryKind() != UncachedDictionaryKind
            && prototypeQueriesAreCacheable()
            && !(typeInfo().getOwnPropertySlotIsImpure() && !typeInfo().newImpurePropertyFiresWatchpoints());
    }

    bool propertyAccessesAreCacheableForAbsence()
    {
        // FIXME: dictionaries cannot be cached for absence, so check for dictionaries here instead
        // of at all call sites.
        return !typeInfo().getOwnPropertySlotIsImpureForPropertyAbsence();
    }

    bool needImpurePropertyWatchpoint()
    {
        return propertyAccessesAreCacheable()
            && typeInfo().getOwnPropertySlotIsImpure()
            && typeInfo().newImpurePropertyFiresWatchpoints();
    }

    bool isImmutablePrototypeExoticObject()
    {
        return typeInfo().isImmutablePrototypeExoticObject();
    }

    // We use SlowPath in GetByStatus for structures that may get new impure properties later to prevent
    // DFG from inlining property accesses since structures don't transition when a new impure property appears.
    bool takesSlowPathInDFGForImpureProperty()
    {
        return typeInfo().getOwnPropertySlotIsImpure();
    }

    bool hasNonReifiedStaticProperties() const
    {
        return typeInfo().hasStaticPropertyTable() && !staticPropertiesReified();
    }

    bool isNonExtensibleOrHasNonConfigurableProperties() const
    {
        return didPreventExtensions() || hasNonConfigurableProperties();
    }

    bool hasAnyOfBitFieldFlags(unsigned flags) const
    {
        return m_bitField & flags;
    }

    // Type accessors.
    TypeInfo typeInfo() const { return m_blob.typeInfo(m_outOfLineTypeFlags); }
    bool isObject() const { return typeInfo().isObject(); }
    const ClassInfo* classInfoForCells() const { return m_classInfo; }
    CellState typeInfoDefaultCellState() const { return m_blob.defaultCellState(); }
protected:
    // You probably want typeInfo().type()
    JSType type() { return JSCell::type(); }
    // You probably want classInfoForCell()
    const ClassInfo* classInfo() const = delete;
public:

    IndexingType indexingType() const { return m_blob.indexingModeIncludingHistory() & AllWritableArrayTypes; }
    IndexingType indexingMode() const  { return m_blob.indexingModeIncludingHistory() & AllArrayTypes; }
    Dependency fencedIndexingMode(IndexingType& indexingType)
    {
        Dependency dependency = m_blob.fencedIndexingModeIncludingHistory(indexingType);
        indexingType &= AllArrayTypes;
        return dependency;
    }
    IndexingType indexingModeIncludingHistory() const { return m_blob.indexingModeIncludingHistory(); }
        
    inline bool mayInterceptIndexedAccesses() const;
    
    inline bool holesMustForwardToPrototype(JSObject*) const;
        
    JSGlobalObject* realm() const LIFETIME_BOUND { return m_realm.get(); }

    // NOTE: This method should only be called during the creation of structures, since the realm
    // of a structure is presumed to be immutable in a bunch of places.
    void setRealm(VM&, JSGlobalObject*);

    ALWAYS_INLINE bool hasMonoProto() const
    {
        return !m_prototype.get().isEmpty();
    }
    ALWAYS_INLINE bool hasPolyProto() const
    {
        return !hasMonoProto();
    }
    ALWAYS_INLINE JSValue storedPrototype() const
    {
        ASSERT(hasMonoProto());
        return m_prototype.get();
    }
    JSValue storedPrototype(const JSObject*) const;
    JSObject* storedPrototypeObject(const JSObject*) const;
    Structure* storedPrototypeStructure(const JSObject*) const;

    JSObject* storedPrototypeObject() const;
    Structure* storedPrototypeStructure() const;
    JSValue prototypeForLookup(JSGlobalObject*) const;
    JSValue prototypeForLookup(JSGlobalObject*, JSCell* base) const;
    StructureChain* prototypeChain(VM&, JSGlobalObject*, JSObject* base) const;
    DECLARE_VISIT_CHILDREN;
    
    // A Structure is cheap to mark during GC if doing so would only add a small and bounded amount
    // to our heap footprint. For example, if the structure refers to a global object that is not
    // yet marked, then as far as we know, the decision to mark this Structure would lead to a large
    // increase in footprint because no other object refers to that global object. This method
    // returns true if all user-controlled (and hence unbounded in size) objects referenced from the
    // Structure are already marked.
    template<typename Visitor> bool isCheapDuringGC(Visitor&);
    
    // Returns true if this structure is now marked.
    template<typename Visitor> bool markIfCheap(Visitor&);
    
    bool hasRareData() const
    {
        return isRareData(m_previousOrRareData.get());
    }

    StructureRareData* rareData()
    {
        ASSERT(hasRareData());
        return static_cast<StructureRareData*>(m_previousOrRareData.get());
    }

    StructureRareData* tryRareData()
    {
        JSCell* value = m_previousOrRareData.get();
        WTF::dependentLoadLoadFence();
        if (isRareData(value))
            return static_cast<StructureRareData*>(value);
        return nullptr;
    }

    const StructureRareData* rareData() const
    {
        ASSERT(hasRareData());
        return static_cast<const StructureRareData*>(m_previousOrRareData.get());
    }

    const StructureRareData* rareDataConcurrently() const
    {
        JSCell* cell = m_previousOrRareData.get();
        if (isRareData(cell))
            return static_cast<StructureRareData*>(cell);
        return nullptr;
    }

    StructureRareData* ensureRareData(VM& vm)
    {
        if (!hasRareData())
            allocateRareData(vm);
        return rareData();
    }
    
    inline Structure* previousID() const; // Defined below
    inline bool transitivelyTransitionedFrom(Structure* structureToFind); // Defined below

    inline PropertyOffset maxOffset() const; // Defined below

    inline void setMaxOffset(VM&, PropertyOffset); // Defined below

    inline PropertyOffset transitionOffset() const; // Defined below

    inline void setTransitionOffset(VM&, PropertyOffset); // Defined below

    static unsigned outOfLineCapacity(PropertyOffset maxOffset)
    {
        unsigned outOfLineSize = Structure::outOfLineSize(maxOffset);

        // This algorithm completely determines the out-of-line property storage growth algorithm.
        // The JSObject code will only trigger a resize if the value returned by this algorithm
        // changed between the new and old structure. So, it's important to keep this simple because
        // it's on a fast path.
        
        if (!outOfLineSize)
            return 0;

        if (outOfLineSize <= initialOutOfLineCapacity)
            return initialOutOfLineCapacity;

        ASSERT(outOfLineSize > initialOutOfLineCapacity);
        static_assert(outOfLineGrowthFactor == 2);
        return roundUpToPowerOfTwo(outOfLineSize);
    }
    
    static unsigned outOfLineSize(PropertyOffset maxOffset)
    {
        return numberOfOutOfLineSlotsForMaxOffset(maxOffset);
    }

    unsigned outOfLineCapacity() const
    {
        return outOfLineCapacity(maxOffset());
    }
    unsigned outOfLineSize() const
    {
        return outOfLineSize(maxOffset());
    }
    bool hasInlineStorage() const
    {
        return !!m_inlineCapacity;
    }
    unsigned inlineCapacity() const
    {
        return m_inlineCapacity;
    }
    unsigned inlineSize() const
    {
        return std::min<unsigned>(maxOffset() + 1, m_inlineCapacity);
    }
    unsigned totalStorageCapacity() const
    {
        ASSERT(structure()->classInfoForCells() == info());
        return outOfLineCapacity() + inlineCapacity();
    }

    bool isValidOffset(PropertyOffset offset) const
    {
        return JSC::isValidOffset(offset)
            && offset <= maxOffset()
            && (offset < m_inlineCapacity || offset >= firstOutOfLineOffset);
    }

    bool hijacksIndexingHeader() const
    {
        return isTypedView(m_blob.type());
    }
    
    bool couldHaveIndexingHeader() const
    {
        return hasIndexedProperties(indexingType())
            || hijacksIndexingHeader();
    }
    
    bool hasIndexingHeader(const JSCell*) const;    
    bool masqueradesAsUndefined(JSGlobalObject* lexicalGlobalObject)
    {
        return typeInfo().masqueradesAsUndefined() && realm() == lexicalGlobalObject;
    }

    PropertyOffset get(VM&, PropertyName);
    PropertyOffset get(VM&, PropertyName, unsigned& attributes);

    inline bool canPerformFastPropertyEnumerationCommon() const; // Defined below

    inline bool canPerformFastPropertyEnumeration() const; // Defined below

    // This is a somewhat internalish method. It will call your functor while possibly holding the
    // Structure's lock. There is no guarantee whether the lock is held or not in any particular
    // call. So, you have to assume the worst. Also, the functor returns true if it wishes for you
    // to continue or false if it's done.
    template<typename Functor>
    void forEachPropertyConcurrently(const Functor&);

    template<typename Functor>
    void forEachProperty(VM&, const Functor&);

    IGNORE_RETURN_TYPE_WARNINGS_BEGIN
    inline PropertyOffset get(VM&, Concurrency, UniquedStringImpl* uid, unsigned& attributes); // Defined in StructureInlines.h
    IGNORE_RETURN_TYPE_WARNINGS_END

    IGNORE_RETURN_TYPE_WARNINGS_BEGIN
    inline PropertyOffset get(VM&, Concurrency, UniquedStringImpl* uid); // Defined in StructureInlines.h
    IGNORE_RETURN_TYPE_WARNINGS_END

    inline PropertyOffset getConcurrently(UniquedStringImpl* uid); // Defined in StructureInlines.h
    PropertyOffset getConcurrently(UniquedStringImpl* uid, unsigned& attributes);
    
    Vector<PropertyTableEntry> getPropertiesConcurrently();
    
    void setHasAnyKindOfGetterSetterPropertiesWithProtoCheck(bool is__proto__)
    {
        setHasAnyKindOfGetterSetterProperties(true);
        if (!is__proto__)
            setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true);
    }
    
    void setContainsReadOnlyProperties() { setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true); }
    
    void setCachedPropertyNameEnumerator(VM&, JSPropertyNameEnumerator*, StructureChain*);
    JSPropertyNameEnumerator* NODELETE cachedPropertyNameEnumerator() const;
    uintptr_t NODELETE cachedPropertyNameEnumeratorAndFlag() const;
    bool NODELETE canCachePropertyNameEnumerator(VM&) const;
    bool NODELETE canAccessPropertiesQuicklyForEnumeration() const;

    inline JSCellButterfly* cachedPropertyNames(CachedPropertyNamesKind kind) const; // Defined in StructureInlines.h
    inline JSCellButterfly* cachedPropertyNamesIgnoringSentinel(CachedPropertyNamesKind kind) const; // Defined in StructureInlines.h
    void setCachedPropertyNames(VM&, CachedPropertyNamesKind, JSCellButterfly*);
    bool canCacheOwnPropertyNames() const
    {
        if (isDictionary())
            return false;
        if (hasIndexedProperties(indexingType()))
            return false;
        if (typeInfo().overridesAnyFormOfGetOwnPropertyNames())
            return false;
        return true;
    }

    void getPropertyNamesFromStructure(VM&, PropertyNameArrayBuilder&, DontEnumPropertiesMode);

    inline JSValue cachedSpecialProperty(CachedSpecialPropertyKey key); // Defined in StructureInlines.h
    void cacheSpecialProperty(JSGlobalObject*, VM&, JSValue, CachedSpecialPropertyKey, const PropertySlot&);

    inline JSString* defaultToPrimitiveFastAndNonObservable(VM&);

    static constexpr ptrdiff_t prototypeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_prototype);
    }

    static constexpr ptrdiff_t realmOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_realm);
    }

    static constexpr ptrdiff_t classInfoOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_classInfo);
    }

    static constexpr ptrdiff_t outOfLineTypeFlagsOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_outOfLineTypeFlags);
    }

    static constexpr ptrdiff_t indexingModeIncludingHistoryOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_blob) + TypeInfoBlob::indexingModeIncludingHistoryOffset();
    }
    
    static constexpr ptrdiff_t propertyTableUnsafeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_propertyTableUnsafe);
    }

    static constexpr ptrdiff_t inlineCapacityOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_inlineCapacity);
    }

    static constexpr ptrdiff_t previousOrRareDataOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_previousOrRareData);
    }

    static constexpr ptrdiff_t bitFieldOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_bitField);
    }

    static constexpr ptrdiff_t propertyHashOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_propertyHash);
    }

    static constexpr ptrdiff_t seenPropertiesOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_seenProperties) + SeenProperties::offsetOfBits();
    }

    static Structure* createStructure(VM&);
        
    bool transitionWatchpointSetHasBeenInvalidated() const
    {
        return m_transitionWatchpointSet.hasBeenInvalidated();
    }
        
    bool transitionWatchpointSetIsStillValid() const
    {
        return m_transitionWatchpointSet.isStillValid();
    }
    
    bool dfgMayWatchIfPossible() const
    {
        // FIXME: We would like to not watch things that are unprofitable to watch, like
        // dictionaries. Unfortunately, we can't do such things: a dictionary could get flattened,
        // in which case it will start to appear watchable and so the DFG will think that it is
        // watching it. We should come up with a comprehensive story for not watching things that
        // aren't profitable to watch.
        // https://bugs.webkit.org/show_bug.cgi?id=133625
        
        // - We don't watch Structures that either decided not to be watched, or whose predecessors
        //   decided not to be watched. This happens when a transition is fired while being watched.
        if (transitionWatchpointIsLikelyToBeFired())
            return false;

        // - Don't watch Structures that had been dictionaries.
        if (hasBeenDictionary())
            return false;
        
        return true;
    }
    
    bool dfgMayWatch() const
    {
        return dfgMayWatchIfPossible() && transitionWatchpointSetIsStillValid();
    }

    bool propertyNameEnumeratorMayWatch() const
    {
        return dfgMayWatch() && !hasPolyProto();
    }
        
    inline void addTransitionWatchpoint(Watchpoint* watchpoint) const; // Defined in StructureInlinesLight.h
    
    void NODELETE didTransitionFromThisStructureWithoutFiringWatchpoint() const;
    void fireStructureTransitionWatchpoint(DeferredStructureTransitionWatchpointFire*) const;

    InlineWatchpointSet& transitionWatchpointSet() const
    {
        return m_transitionWatchpointSet;
    }
    
    WatchpointSet* ensurePropertyReplacementWatchpointSet(VM&, PropertyOffset);
    inline void startWatchingPropertyForReplacements(VM& vm, PropertyOffset offset); // Defined in StructureInlines.h
    void startWatchingPropertyForReplacements(VM&, PropertyName);
    WatchpointSet* propertyReplacementWatchpointSet(PropertyOffset);
    WatchpointSet* firePropertyReplacementWatchpointSet(VM&, PropertyOffset, const char* reason);

    void didReplaceProperty(PropertyOffset offset)
    {
        if (!isWatchingReplacement()) [[likely]]
            return;
        didReplacePropertySlow(offset);
    }
    void didCachePropertyReplacement(VM&, PropertyOffset);
    
    inline void startWatchingInternalPropertiesIfNecessary(VM& vm); // Defined in StructureInlines.h
    
    Ref<StructureShape> toStructureShape(JSValue, bool& sawPolyProtoStructure);
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dumpBrief(PrintStream&, const ASCIICString&) const;
    
    static void dumpContextHeader(PrintStream&);
    
    ConcurrentJSLock& lock() LIFETIME_BOUND { return m_lock; }

    unsigned propertyHash() const { return m_propertyHash; }
    SeenProperties seenProperties() const { return m_seenProperties; }

    static bool shouldConvertToPolyProto(const Structure* a, const Structure* b);

    UniquedStringImpl* transitionPropertyName() const { return m_transitionPropertyName.get(); }

    struct PropertyHashEntry {
        const HashTable* table;
        const HashTableValue* value;
    };
    std::optional<PropertyHashEntry> findPropertyHashEntry(PropertyName) const;
    
    DECLARE_EXPORT_INFO;

private:
    JS_EXPORT_PRIVATE void didReplacePropertySlow(PropertyOffset);

    typedef enum {
        NoneDictionaryKind = 0,
        CachedDictionaryKind = 1,
        UncachedDictionaryKind = 2
    } DictionaryKind;

public:
    enum class DefinitelyNonThenableState : uint8_t {
        NotComputed = 0,
        NonThenable = 1, // Cached `true`. Sound only while the realm's promiseThenWatchpointSet is intact.
        MaybeThenable = 2, // Cached `false`. Always safe (a stale `false` only loses the optimization).
        Uncacheable = 3, // Prototype chain isn't covered by the watchpoint; always recompute.
    };

#define DEFINE_BITFIELD(type, lowerName, upperName, width, offset) \
    static constexpr uint32_t s_##lowerName##Shift = offset;\
    static constexpr uint32_t s_##lowerName##Mask = ((1 << (width - 1)) | ((1 << (width - 1)) - 1));\
    static constexpr uint32_t s_##lowerName##Bits = s_##lowerName##Mask << s_##lowerName##Shift;\
    static constexpr uint32_t s_bitWidthOf##upperName = width;\
    type lowerName() const { return static_cast<type>((m_bitField >> offset) & s_##lowerName##Mask); }\
    void set##upperName(type newValue) \
    {\
        m_bitField &= ~(s_##lowerName##Mask << offset);\
        m_bitField |= (static_cast<uint32_t>(newValue) & s_##lowerName##Mask) << offset;\
    }

    DEFINE_BITFIELD(DictionaryKind, dictionaryKind, DictionaryKind, 2, 0);
    DEFINE_BITFIELD(bool, isPinnedPropertyTable, IsPinnedPropertyTable, 1, 2);
    DEFINE_BITFIELD(bool, hasAnyKindOfGetterSetterProperties, HasAnyKindOfGetterSetterProperties, 1, 3);
    DEFINE_BITFIELD(bool, hasReadOnlyOrGetterSetterPropertiesExcludingProto, HasReadOnlyOrGetterSetterPropertiesExcludingProto, 1, 4);
    DEFINE_BITFIELD(bool, isQuickPropertyAccessAllowedForEnumeration, IsQuickPropertyAccessAllowedForEnumeration, 1, 5);
    DEFINE_BITFIELD(bool, hasNonEnumerableProperties, HasNonEnumerableProperties, 1, 6);
    DEFINE_BITFIELD(bool, hasSpecialProperties, HasSpecialProperties, 1, 7);
    DEFINE_BITFIELD(DefinitelyNonThenableState, definitelyNonThenableState, DefinitelyNonThenableState, 2, 8); // This flag can be flipped on the main thread at any timing.
    DEFINE_BITFIELD(TransitionKind, transitionKind, TransitionKind, 5, 13);
    DEFINE_BITFIELD(bool, isWatchingReplacement, IsWatchingReplacement, 1, 18); // This flag can be fliped on the main thread at any timing.
    DEFINE_BITFIELD(bool, mayBePrototype, MayBePrototype, 1, 19);
    DEFINE_BITFIELD(bool, didPreventExtensions, DidPreventExtensions, 1, 20);
    DEFINE_BITFIELD(bool, didTransition, DidTransition, 1, 21);
    DEFINE_BITFIELD(bool, staticPropertiesReified, StaticPropertiesReified, 1, 22);
    DEFINE_BITFIELD(bool, hasBeenFlattenedBefore, HasBeenFlattenedBefore, 1, 23);
    DEFINE_BITFIELD(bool, didWatchInternalProperties, DidWatchInternalProperties, 1, 24);
    DEFINE_BITFIELD(bool, transitionWatchpointIsLikelyToBeFired, TransitionWatchpointIsLikelyToBeFired, 1, 25);
    DEFINE_BITFIELD(bool, hasBeenDictionary, HasBeenDictionary, 1, 26);
    DEFINE_BITFIELD(bool, protectPropertyTableWhileTransitioning, ProtectPropertyTableWhileTransitioning, 1, 27);
    DEFINE_BITFIELD(bool, hasUnderscoreProtoPropertyExcludingOriginalProto, HasUnderscoreProtoPropertyExcludingOriginalProto, 1, 28);
    DEFINE_BITFIELD(bool, hasNonConfigurableProperties, HasNonConfigurableProperties, 1, 29);
    DEFINE_BITFIELD(bool, hasNonConfigurableReadOnlyOrGetterSetterProperties, HasNonConfigurableReadOnlyOrGetterSetterProperties, 1, 30);
    // Summary bit for the double-field representation project: true if ANY property of this Structure carries
    // PropertyAttribute::RepresentationDouble. Lets a reader that must ask "is offset N raw-double?" exit on a single
    // bit test in the overwhelmingly common case; only structures where this is true pay a property-table walk.
    // See analysis/prompt/box2d/07-PLAN-double-field.md section 5e. Bit 31 was the last free bit in m_bitField.
    DEFINE_BITFIELD(bool, hasRawDoubleFields, HasRawDoubleFields, 1, 31);

    // Per-offset companion to the summary bit above. The mask itself lives in StructureRareData, NOT here: Structures
    // are among the most numerous cells in the heap and sizeof(Structure) sits exactly on a 16-byte size class, so an
    // inline mask would cost +16 bytes on EVERY Structure to serve the small minority that carry a Double-represented
    // field. Every caller gates on hasRawDoubleFields() first, so the rare-data indirection is only taken on
    // Structures that have already answered yes, and rare data is allocated lazily.
    //
    // Safe to call from ANY thread, including a GC marking thread: it reads only words written before the Structure
    // is published, takes no lock and allocates nothing. Answers false for offsets it cannot describe, which is the
    // safe direction of the one-directional invariant (see StructureRareData::m_rawDoubleMask).
    static constexpr unsigned s_rawDoubleMaskWords = 2;
    static constexpr unsigned s_rawDoubleMaskBits = s_rawDoubleMaskWords * 64;

    // isRawDoubleOffset SPLIT IN TWO, so a caller that walks many offsets on ONE structure pays the
    // structure-level part once instead of per offset. isRawDoubleOffset is exactly their composition, so there is
    // still one implementation of each half and no second predicate that could drift from the first -- drift between
    // two predicates answering the same question is what produced the use-after-free in repro/bugs/05.
    //
    // The structure-level half: null means no offset on this structure can be raw. Folds in the summary bit, the
    // option, and the two dependent loads into StructureRareData.
    inline const RawDoubleMask* rawDoubleMaskIfAny() const;

    // The per-offset half: one bound check and one bit test. Static because it needs nothing from the Structure once
    // the mask is in hand.
    static ALWAYS_INLINE bool maskSaysRawDouble(const RawDoubleMask*, PropertyOffset);

    // Two compares and no loads. False means DEFINITELY not claimed; true means "ask the mask". See
    // m_rawDoubleFirstOffset.
    // Recompute [first, last] from the mask bits. Called after a claim is cleared: widening-only ranges are correct
    // but leave a cleared offset inside the range forever, so every later store to it takes the out-of-line writer
    // for nothing. json-parse-inspector has 13 DISQUALIFIED keys carrying 1,720,166 stores -- i.e. violated claims --
    // which is exactly that case at scale. Cheap: one 128-bit scan, once per violation.
    inline void renarrowRawDoubleRange();

    // Two compares plus one bit test. False means DEFINITELY not claimed; true means "ask the mask".
    //
    // DO NOT "OPTIMISE" THE hasRawDoubleFields() TEST AWAY. It looks redundant -- an unclaimed Structure carries
    // first=UINT8_MAX/last=0, so the two compares alone already answer false -- and removing it does cut ~6 KB of
    // __text across the 82 inlined putDirectOffset call sites, with NO change in behaviour (the census confirms
    // zero extra stores reach the out-of-line writer, and renarrowRawDoubleRange does restore the sentinel).
    // It was measured anyway and it is 1.25 points WORSE: json-parse-inspector -1.48% -> -2.73% at n=120.
    //
    // That is the finding, not a fluke: this regression is driven by code LAYOUT, and layout is not monotonic in
    // code size. Doc 26 established the same thing from the other direction (421 KB of never-executed code netted
    // +0.3 overall, random-signed, +-2.3 per test). So shrinking the patch locally is not a fix strategy -- it just
    // reshuffles which benchmark pays. See repro/bugs/open/28-POST-REBASE-JS3-SWEEP.md.
    ALWAYS_INLINE bool mightHaveClaimAt(PropertyOffset offset) const
    {
        if (!hasRawDoubleFields()) [[likely]]
            return false;
        unsigned o = static_cast<unsigned>(offset);
        return o >= m_rawDoubleFirstOffset && o <= m_rawDoubleLastOffset;
    }

    inline bool isRawDoubleOffset(PropertyOffset) const;


    // PHASE B1: the claim invalidation channel, shared with every descendant Structure. Null unless this lineage
    // actually claims a field and --useDoubleFieldClaimWatchpoint is on. See RawDoubleMask::claimRecord.
    inline InlineWatchpointSet* claimWatchpointIfAny() const;
    inline void ensureClaimWatchpoint(VM&);

    // PHASE B2: has this lineage's claim been given up? Folded into isRawDoubleOffset, so no consumer can miss it.
    inline bool claimGivenUp() const;
    // Give the claim up: clear the bit and fire, so every reader and every future compile agrees at once. Main
    // thread only -- fireAll jettisons dependent code.
    bool giveUpClaim(VM&, PropertyOffset, const char* reason);

    // Can an INLINE CACHE -- an LLInt metadata cache or a Repatch stub -- read and write this Structure's property
    // slots directly, as raw JSValues?
    //
    // DELIBERATELY NOT FOLDED INTO propertyAccessesAreCacheable(). That predicate has 42 users, including DFG
    // abstract interpretation, ObjectPropertyConditionSet and PropertyCondition. Turning it off for a Structure
    // carrying a Double-represented field would stop DFG and FTL resolving these accesses to GetByOffset/PutByOffset
    // -- and that is precisely the path that handles raw doubles for FREE, because it knows the offset and the
    // structure statically and can prove the representation at compile time. Folding them together would defeat the
    // optimisation it is meant to protect.
    //
    // The inline caches genuinely cannot: their offset arrives in a register at runtime, so honouring the per-offset
    // mask would mean walking it on every property access in asm. They decline instead, and the access funnels to the
    // C++ slow path, which is raw-aware. Costs those objects the interpreter/baseline fast path, which is acceptable
    // because DFG and FTL -- where the time actually goes -- keep theirs.
    //
    // THE OPTION CHECK IS LOAD-BEARING, and it now lives inside isRawDoubleOffset() rather than being applied by each
    // caller. It used to be applied here because hasRawDoubleFields() was driven by useDoubleFieldRepresentation
    // alone, which defaults true, so testing the summary bit by itself would have disabled inline caching for every
    // double-field object in a default build. Marking is now gated on useRawDoubleFieldStorage too
    // (JSObjectInlines.h), so hasRawDoubleFields() IMPLIES storage is enabled and the two can no longer disagree --
    // the divergence between a gated writer and an ungated collector is what freed 20000/20000 live objects in
    // repro/bugs/05-repro-gc-option-gating-uaf.js.
    inline bool inlineCachesCanAccessPropertySlotsDirectly() const; // Defined in StructureInlines.h.

    // PER-OFFSET form of the same question, for the callers that already hold the offset and so can decline only the
    // one access instead of every access on the Structure. Same load-bearing option check.
    inline bool inlineCachesCanAccessSlotDirectly(PropertyOffset) const; // Defined in StructureInlines.h.


    // Returns false if the offset cannot be represented, so a caller that MUST know the field is raw (a writer about
    // to store raw bits) can decline rather than create a slot no reader will recognise.
    inline bool setRawDoubleOffset(VM&, PropertyOffset);

    // WRITE-SIDE VALIDATOR for the invariant `per-offset mask <=> RepresentationDouble attribute`. The mask is DERIVED
    // from the attributes, so the two disagreeing means the derivation was skipped or undone. Checking it where the
    // structure is BUILT rather than where a slot is read is the point: every read-side validator in this project has
    // been either perturbing (the census hides the crash it was meant to catch) or silently broken
    // (getPropertiesConcurrently returns empty without a materialised table). This one walks the real table and
    // RELEASE_ASSERTs, so lldb breaks with the creating path still on the stack. See 07-PLAN 5aw.
    JS_EXPORT_PRIVATE void validateRawDoubleMaskAgreement(VM&, const char* site);
    inline void clearRawDoubleOffset(PropertyOffset);
    inline void copyRawDoubleMaskFrom(VM&, const Structure* other);

    // Replace the WHOLE per-offset map at once. flattenDictionaryStructure is the only operation that MOVES existing
    // properties between offsets rather than adding or removing one, and setRawDoubleOffset/clearRawDoubleOffset
    // cannot express a permutation: applied bit by bit they would transiently claim a slot they have not moved yet,
    // and the concurrent collector reads this map without a lock.
    inline void renumberRawDoubleMask(const std::array<uint64_t, s_rawDoubleMaskWords>&);

    // WHY did isRawDoubleOffset() answer false? JSObject.h cannot include StructureInlines.h, so the read-side
    // detector there cannot ask the mask directly and has been reduced to printing the ATTRIBUTES as a stand-in.
    // That is what made a lost mask indistinguishable from a mask that was never set. Out-of-line and diagnostic
    // only. Returns: 0 no summary bit, 1 no rare data, 2 null mask, 3 offset not representable, 4 bit clear,
    // 5 bit set.
    JS_EXPORT_PRIVATE unsigned rawDoubleMaskDebugState(PropertyOffset) const;

    enum class StructureVariant : uint8_t {
        Normal,
        Branded,
        WebAssemblyGC,
    };

    StructureVariant variant() const { return m_structureVariant; }
    bool isBrandedStructure() { return variant() == StructureVariant::Branded; }

    static_assert(s_bitWidthOfTransitionKind <= sizeof(TransitionKind) * 8);

    static bool bitFieldFlagsCantBeChangedWithoutTransition(unsigned flags)
    {
        return flags == (flags & (
            s_didPreventExtensionsBits
            | s_isQuickPropertyAccessAllowedForEnumerationBits
            | s_hasNonEnumerablePropertiesBits
            | s_hasSpecialPropertiesBits
            | s_hasAnyKindOfGetterSetterPropertiesBits
            | s_hasReadOnlyOrGetterSetterPropertiesExcludingProtoBits
            | s_hasUnderscoreProtoPropertyExcludingOriginalProtoBits
            | s_hasNonConfigurablePropertiesBits
            | s_hasNonConfigurableReadOnlyOrGetterSetterPropertiesBits
        ));
    }

    TransitionPropertyAttributes transitionPropertyAttributes() const { return m_transitionPropertyAttributes; }
    void setTransitionPropertyAttributes(TransitionPropertyAttributes transitionPropertyAttributes) { m_transitionPropertyAttributes = transitionPropertyAttributes; }

    int transitionCountEstimate() const
    {
        // Since the number of transitions is often the same as the last offset (except if there are deletes)
        // we keep the size of Structure down by not storing both.
        return numberOfSlotsForMaxOffset(maxOffset(), m_inlineCapacity);
    }

    void reconcileWeakReferencesAtGCEnd(VM&, CollectionScope);

protected:
    Structure(VM&, StructureVariant, Structure* previous); // Branded/Normal only
    Structure(VM&, StructureVariant, JSGlobalObject*, const TypeInfo&, const ClassInfo*); // WebAssemblyGC only

private:
    friend class LLIntOffsetsExtractor;

    JS_EXPORT_PRIVATE Structure(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType, unsigned inlineCapacity);
    Structure(VM&, CreatingEarlyCellTag);

    static Structure* create(VM&, Structure*, DeferredStructureTransitionWatchpointFire*);

    static Structure* addPropertyTransitionToExistingStructureImpl(Structure*, UniquedStringImpl* uid, unsigned attributes, PropertyOffset&);
    ALWAYS_INLINE static Structure* attributeChangeTransitionToExistingStructureImpl(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    static Structure* removePropertyTransitionFromExistingStructureImpl(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    static Structure* setBrandTransitionFromExistingStructureImpl(Structure*, UniquedStringImpl*);

    JS_EXPORT_PRIVATE static Structure* nonPropertyTransitionSlow(VM&, Structure*, TransitionKind, DeferredStructureTransitionWatchpointFire*);

    // This function does the both didTransitionFromThisStructureWithoutFiringWatchpoint and fireStructureTransitionWatchpoint.
    void didTransitionFromThisStructure(DeferredStructureTransitionWatchpointFire*) const;

    // This will return the structure that has a usable property table, that property table,
    // and the list of structures that we visited before we got to it. If it returns a
    // non-null structure, it will also lock the structure that it returns; it is your job
    // to unlock it.
    bool findStructuresAndMapForMaterialization(Vector<Structure*, 8>& structures, Structure*& structure, PropertyTable*&) WTF_ACQUIRES_LOCK_IF(true, structure->m_lock);
    
    static Structure* toDictionaryTransition(VM&, Structure*, DictionaryKind, DeferredStructureTransitionWatchpointFire* = nullptr);

    enum class ShouldPin : bool { No, Yes };
    template<ShouldPin, typename Func>
    PropertyOffset add(VM&, PropertyName, unsigned attributes, const Func&);
    PropertyOffset add(VM&, PropertyName, unsigned attributes);
    template<ShouldPin, typename Func>
    PropertyOffset remove(VM&, PropertyName, const Func&);
    PropertyOffset remove(VM&, PropertyName);
    template<ShouldPin, typename Func>
    PropertyOffset attributeChange(VM&, PropertyName, unsigned attributes, const Func&);
    PropertyOffset attributeChange(VM&, PropertyName, unsigned attributes);

#if ASSERT_ENABLED
    JS_EXPORT_PRIVATE void checkConsistency();
#else
    ALWAYS_INLINE void checkConsistency() { }
#endif

    // This may grab the lock, or not. Do not call when holding the Structure's lock.
    PropertyTable* ensurePropertyTableIfNotEmpty(VM& vm)
    {
        if (PropertyTable* result = m_propertyTableUnsafe.get())
            return result;
        if (!previousID())
            return nullptr;
        return materializePropertyTable(vm);
    }
    
    // This may grab the lock, or not. Do not call when holding the Structure's lock.
    PropertyTable* ensurePropertyTable(VM& vm)
    {
        if (PropertyTable* result = m_propertyTableUnsafe.get())
            return result;
        return materializePropertyTable(vm);
    }
    
    PropertyTable* propertyTableOrNull() const
    {
        return m_propertyTableUnsafe.get();
    }
    
    // This will grab the lock. Do not call when holding the Structure's lock.
    JS_EXPORT_PRIVATE PropertyTable* materializePropertyTable(VM&, bool setPropertyTable = true);
    
    void setPropertyTable(VM& vm, PropertyTable* table);
    
    PropertyTable* takePropertyTableOrCloneIfPinned(VM&);
    PropertyTable* copyPropertyTableForPinning(VM&);

    void setPreviousID(VM&, Structure*);

    void clearPreviousID()
    {
        if (hasRareData())
            rareData()->clearPreviousID();
        else
            m_previousOrRareData.clear();
    }

    bool isValid(JSGlobalObject*, StructureChain* cachedPrototypeChain, JSObject* base) const;

    // You have to hold the structure lock to do these.
    // Keep them inlined function since they are used in the critical path of Dictionary JSObject modification.
    void pin(const AbstractLocker&, VM&, PropertyTable*);
    void pinForCaching(const AbstractLocker&, VM&, PropertyTable*);
    
    static bool isRareData(JSCell* cell)
    {
        return cell && cell->type() != StructureType;
    }

    JS_EXPORT_PRIVATE void allocateRareData(VM&);

    template<typename DetailsFunc>
    void checkOffsetConsistency(PropertyTable*, const DetailsFunc&) const;
    void checkOffsetConsistency() const;

    void startWatchingInternalProperties(VM&);

    inline void clearCachedPrototypeChain(); // Defined in StructureInlines.h

    bool NODELETE holesMustForwardToPrototypeSlow(JSObject*) const;

    // These need to be properly aligned at the beginning of the 'Structure'
    // part of the object.
    TypeInfoBlob m_blob;
    TypeInfo::OutOfLineTypeFlags m_outOfLineTypeFlags;

    uint8_t m_inlineCapacity;

    ConcurrentJSLock m_lock;

    uint32_t m_bitField;
    TransitionPropertyAttributes m_transitionPropertyAttributes { 0 };

    // FIXME: We should probably have a brandedStructureStructure/webAssemblyGCStructureStructure instead of this.
    StructureVariant m_structureVariant { StructureVariant::Normal };

    uint16_t m_transitionOffset;
    uint16_t m_maxOffset;

    // CLAIMED-OFFSET RANGE: a CONSERVATIVE SUPERSET of the offsets this Structure claims as double-only. An offset
    // outside [first, last] is definitely not claimed, so the caller can take its fast path with no dependent loads
    // at all -- where asking the real question means Structure -> StructureRareData -> unique_ptr -> mask bits, three
    // dependent loads. Inside the range, the mask is still consulted for the exact answer.
    //
    // WHY A RANGE IS ENOUGH, measured: the census reports claimed offsets are CONTIGUOUS on 40 of 41 structures
    // (`[dbl-contig] contiguous=40 gappy=1`), and a range-only test would forfeit 0.00031% of double stores -- 6 of
    // 1.9M on json-parse-inspector. It is only used to skip work, never to claim, so the one gappy structure costs a
    // mask walk it would have done anyway.
    //
    // FREE IN SPACE: these two bytes sit in the padding between m_maxOffset and the 4-byte-aligned m_propertyHash,
    // so sizeof(Structure) is unchanged -- which matters, because Structures are among the most numerous cells in the
    // heap and the type sits exactly on a 16-byte size class (see StructureRareData.h's note on the same problem).
    // Empty range: first > last, so every test fails.
    uint8_t m_rawDoubleFirstOffset { UINT8_MAX };
    uint8_t m_rawDoubleLastOffset { 0 };

    uint32_t m_propertyHash;
    SeenProperties m_seenProperties;


    WriteBarrier<JSGlobalObject> m_realm;
    WriteBarrier<Unknown> m_prototype;
    mutable WriteBarrier<StructureChain> m_cachedPrototypeChain;

    WriteBarrier<JSCell> m_previousOrRareData;

    CompactRefPtr<UniquedStringImpl> m_transitionPropertyName;

    const ClassInfo* m_classInfo;

    StructureTransitionTable m_transitionTable;

    // Should be accessed through ensurePropertyTable(). During GC, it may be set to 0 by another thread.
    // During a Heap Snapshot GC we avoid clearing the table so it is safe to use.
    WriteBarrier<PropertyTable> m_propertyTableUnsafe;

    mutable InlineWatchpointSet m_transitionWatchpointSet;

    static_assert(firstOutOfLineOffset < 256);

    friend class VMInspector;
    friend class JSDollarVMHelper;
    friend class Integrity::Analyzer;
};

JS_EXPORT_PRIVATE void dumpTransitionKind(PrintStream&, TransitionKind);
MAKE_PRINT_ADAPTOR(TransitionKindDump, TransitionKind, dumpTransitionKind);

// Defined here rather than in JSCell.h because it needs Structure to be complete.
inline const ClassInfo* JSCell::classInfo() const
{
    // If the mutator is currently sweeping, then accessing the structure is not safe since the
    // structure may have been swept already (and we're probably being called from this object's
    // destructor). This can only be verified for the mutator thread since other threads might be
    // querying JSCells that are not being swept by the mutator.
    // validateIsNotSweeping() is out-of-line to avoid pulling vm() into this header.
    ASSERT(validateIsNotSweeping());
    return structure()->classInfoForCells();
}

inline bool JSCell::inherits(const ClassInfo* info) const
{
    return classInfo()->isSubClassOf(info);
}

template<typename Target>
inline bool JSCell::inherits() const
{
    return JSCastingHelpers::inherits<Target>(this);
}

inline Structure* Structure::previousID() const
{
    ASSERT(structure()->classInfoForCells() == info());
    // This is so written because it's used concurrently. We only load from m_previousOrRareData
    // once, and this load is guaranteed atomic.
    JSCell* cell = m_previousOrRareData.get();
    if (isRareData(cell))
        return static_cast<StructureRareData*>(cell)->previousID();
    return static_cast<Structure*>(cell);
}

inline bool Structure::transitivelyTransitionedFrom(Structure* structureToFind)
{
    for (Structure* current = this; current; current = current->previousID()) {
        if (current == structureToFind)
            return true;
    }
    return false;
}

inline PropertyOffset Structure::maxOffset() const
{
    uint16_t maxOffset = m_maxOffset;
    if (maxOffset == shortInvalidOffset)
        return invalidOffset;
    if (maxOffset == useRareDataFlag)
        return rareData()->m_maxOffset;
    return maxOffset;
}

inline void Structure::setMaxOffset(VM& vm, PropertyOffset offset)
{
    if (offset == invalidOffset)
        m_maxOffset = shortInvalidOffset;
    else if (offset < useRareDataFlag && offset < shortInvalidOffset)
        m_maxOffset = offset;
    else if (m_maxOffset == useRareDataFlag)
        rareData()->m_maxOffset = offset;
    else {
        ensureRareData(vm)->m_maxOffset = offset;
        WTF::storeStoreFence();
        m_maxOffset = useRareDataFlag;
    }
}

inline PropertyOffset Structure::transitionOffset() const
{
    uint16_t transitionOffset = m_transitionOffset;
    if (transitionOffset == shortInvalidOffset)
        return invalidOffset;
    if (transitionOffset == useRareDataFlag)
        return rareData()->m_transitionOffset;
    return transitionOffset;
}

inline void Structure::setTransitionOffset(VM& vm, PropertyOffset offset)
{
    if (offset == invalidOffset)
        m_transitionOffset = shortInvalidOffset;
    else if (offset < useRareDataFlag && offset < shortInvalidOffset)
        m_transitionOffset = offset;
    else if (m_transitionOffset == useRareDataFlag)
        rareData()->m_transitionOffset = offset;
    else {
        ensureRareData(vm)->m_transitionOffset = offset;
        WTF::storeStoreFence();
        m_transitionOffset = useRareDataFlag;
    }
}

inline bool Structure::canPerformFastPropertyEnumerationCommon() const
{
    if (typeInfo().overridesGetOwnPropertySlot())
        return false;
    if (typeInfo().overridesAnyFormOfGetOwnPropertyNames())
        return false;
    if (hasAnyKindOfGetterSetterProperties())
        return false;
    if (isUncacheableDictionary())
        return false;
    // Cannot perform fast [[Put]] to |target| if the property names of the |source| contain "__proto__".
    if (hasUnderscoreProtoPropertyExcludingOriginalProto())
        return false;
    return true;
}

inline bool Structure::canPerformFastPropertyEnumeration() const
{
    if (!canPerformFastPropertyEnumerationCommon())
        return false;
    // FIXME: Indexed properties can be handled.
    // https://bugs.webkit.org/show_bug.cgi?id=185358
    if (hasIndexedProperties(indexingType()))
        return false;
    return true;
}

} // namespace JSC
