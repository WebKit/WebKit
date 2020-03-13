/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "ClassInfo.h"
#include "ConcurrentJSLock.h"
#include "DeletePropertySlot.h"
#include "IndexingType.h"
#include "JSCJSValue.h"
#include "JSCast.h"
#include "JSType.h"
#include "JSTypeInfo.h"
#include "PropertyName.h"
#include "PropertyNameArray.h"
#include "PropertyOffset.h"
#include "PutPropertySlot.h"
#include "StructureIDBlob.h"
#include "StructureRareData.h"
#include "StructureTransitionTable.h"
#include "TinyBloomFilter.h"
#include "Watchpoint.h"
#include "WriteBarrierInlines.h"
#include <wtf/CompactRefPtrTuple.h>
#include <wtf/PrintStream.h>

namespace WTF {

class UniquedStringImpl;

} // namespace WTF

namespace JSC {

class DeferGC;
class LLIntOffsetsExtractor;
class PropertyNameArray;
class PropertyNameArrayData;
class PropertyTable;
class StructureChain;
class StructureShape;
class SlotVisitor;
class JSString;
struct DumpContext;
struct HashTable;
struct HashTableValue;

// The out-of-line property storage capacity to use when first allocating out-of-line
// storage. Note that all objects start out without having any out-of-line storage;
// this comes into play only on the first property store that exhausts inline storage.
static constexpr unsigned initialOutOfLineCapacity = 4;

// The factor by which to grow out-of-line storage when it is exhausted, after the
// initial allocation.
static constexpr unsigned outOfLineGrowthFactor = 2;

struct PropertyMapEntry {
    UniquedStringImpl* key;
    PropertyOffset offset;
    uint8_t attributes;

    PropertyMapEntry()
        : key(nullptr)
        , offset(invalidOffset)
        , attributes(0)
    {
    }
    
    PropertyMapEntry(UniquedStringImpl* key, PropertyOffset offset, unsigned attributes)
        : key(key)
        , offset(offset)
        , attributes(attributes)
    {
        ASSERT(this->attributes == attributes);
    }
};

class StructureFireDetail : public FireDetail {
public:
    StructureFireDetail(const Structure* structure)
        : m_structure(structure)
    {
    }
    
    void dump(PrintStream& out) const override;

private:
    const Structure* m_structure;
};

class DeferredStructureTransitionWatchpointFire : public DeferredWatchpointFire {
    WTF_MAKE_NONCOPYABLE(DeferredStructureTransitionWatchpointFire);
public:
    JS_EXPORT_PRIVATE DeferredStructureTransitionWatchpointFire(VM&, Structure*);
    JS_EXPORT_PRIVATE ~DeferredStructureTransitionWatchpointFire();
    
    void dump(PrintStream& out) const override;

    const Structure* structure() const { return m_structure; }

private:
    const Structure* m_structure;
};

class Structure final : public JSCell {
    static constexpr uint16_t shortInvalidOffset = std::numeric_limits<uint16_t>::max() - 1;
    static constexpr uint16_t useRareDataFlag = std::numeric_limits<uint16_t>::max();
public:
    friend class StructureTransitionTable;

    typedef JSCell Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;
    
    enum PolyProtoTag { PolyProto };
    static Structure* create(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, unsigned inlineCapacity = 0);
    static Structure* create(PolyProtoTag, VM&, JSGlobalObject*, JSObject* prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, unsigned inlineCapacity = 0);

    ~Structure();
    
    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.structureSpace;
    }

    JS_EXPORT_PRIVATE static bool isValidPrototype(JSValue);

protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(m_prototype.get().isEmpty() || isValidPrototype(m_prototype.get()));
    }

    void finishCreation(VM& vm, const Structure* previous)
    {
        this->finishCreation(vm);
        if (previous->hasRareData()) {
            const StructureRareData* previousRareData = previous->rareData();
            if (previousRareData->hasSharedPolyProtoWatchpoint()) {
                ensureRareData(vm);
                rareData()->setSharedPolyProtoWatchpoint(previousRareData->copySharedPolyProtoWatchpoint());
            }
        }
    }

    void finishCreation(VM& vm, CreatingEarlyCellTag)
    {
        Base::finishCreation(vm, this, CreatingEarlyCell);
        ASSERT(m_prototype);
        ASSERT(m_prototype.isNull());
        ASSERT(!vm.structureStructure);
    }

public:
    StructureID id() const { return m_blob.structureID(); }
    int32_t objectInitializationBlob() const { return m_blob.blobExcludingStructureID(); }
    int64_t idBlob() const { return m_blob.blob(); }

    bool isProxy() const
    {
        JSType type = m_blob.type();
        return type == ImpureProxyType || type == PureForwardingProxyType || type == ProxyObjectType;
    }

    static void dumpStatistics();

    JS_EXPORT_PRIVATE static Structure* addPropertyTransition(VM&, Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* addNewPropertyTransition(VM&, Structure*, PropertyName, unsigned attributes, PropertyOffset&, PutPropertySlot::Context = PutPropertySlot::UnknownContext, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* addPropertyTransitionToExistingStructureConcurrently(Structure*, UniquedStringImpl* uid, unsigned attributes, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* addPropertyTransitionToExistingStructure(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    static Structure* removeNewPropertyTransition(VM&, Structure*, PropertyName, PropertyOffset&, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* removePropertyTransition(VM&, Structure*, PropertyName, PropertyOffset&, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* removePropertyTransitionFromExistingStructure(Structure*, PropertyName, PropertyOffset&);
    static Structure* removePropertyTransitionFromExistingStructureConcurrently(Structure*, PropertyName, PropertyOffset&);
    static Structure* changePrototypeTransition(VM&, Structure*, JSValue prototype, DeferredStructureTransitionWatchpointFire&);
    JS_EXPORT_PRIVATE static Structure* attributeChangeTransition(VM&, Structure*, PropertyName, unsigned attributes);
    JS_EXPORT_PRIVATE static Structure* toCacheableDictionaryTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    static Structure* toUncacheableDictionaryTransition(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    JS_EXPORT_PRIVATE static Structure* sealTransition(VM&, Structure*);
    JS_EXPORT_PRIVATE static Structure* freezeTransition(VM&, Structure*);
    static Structure* preventExtensionsTransition(VM&, Structure*);
    static Structure* nonPropertyTransition(VM&, Structure*, NonPropertyTransition);
    JS_EXPORT_PRIVATE static Structure* nonPropertyTransitionSlow(VM&, Structure*, NonPropertyTransition);

    JS_EXPORT_PRIVATE bool isSealed(VM&);
    JS_EXPORT_PRIVATE bool isFrozen(VM&);
    bool isStructureExtensible() const { return !didPreventExtensions(); }

    JS_EXPORT_PRIVATE Structure* flattenDictionaryStructure(VM&, JSObject*);

    static constexpr bool needsDestruction = true;
    static void destroy(JSCell*);

    // Versions that take a func will call it after making the change but while still holding
    // the lock. The callback is not called if there is no change being made, like if you call
    // removePropertyWithoutTransition() and the property is not found.
    template<typename Func>
    PropertyOffset addPropertyWithoutTransition(VM&, PropertyName, unsigned attributes, const Func&);
    template<typename Func>
    PropertyOffset removePropertyWithoutTransition(VM&, PropertyName, const Func&);
    void setPrototypeWithoutTransition(VM&, JSValue prototype);
        
    bool isDictionary() const { return dictionaryKind() != NoneDictionaryKind; }
    bool isUncacheableDictionary() const { return dictionaryKind() == UncachedDictionaryKind; }
  
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

    TypeInfo::OutOfLineTypeFlags outOfLineTypeFlags() const
    {
#if CPU(ADDRESS64)
        return m_outOfLineTypeFlagsAndPropertyTableUnsafe.type();
#else
        return m_outOfLineTypeFlags;
#endif
    }
    
    // Type accessors.
    TypeInfo typeInfo() const { return m_blob.typeInfo(outOfLineTypeFlags()); }
    bool isObject() const { return typeInfo().isObject(); }

    IndexingType indexingType() const { return m_blob.indexingModeIncludingHistory() & AllWritableArrayTypes; }
    IndexingType indexingMode() const  { return m_blob.indexingModeIncludingHistory() & AllArrayTypes; }
    IndexingType indexingModeIncludingHistory() const { return m_blob.indexingModeIncludingHistory(); }
        
    inline bool mayInterceptIndexedAccesses() const;
    
    bool holesMustForwardToPrototype(VM&, JSObject*) const;
        
    JSGlobalObject* globalObject() const { return m_globalObject.get(); }

    // NOTE: This method should only be called during the creation of structures, since the global
    // object of a structure is presumed to be immutable in a bunch of places.
    void setGlobalObject(VM&, JSGlobalObject*);

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
    StructureChain* prototypeChain(JSGlobalObject*, JSObject* base) const;
    static void visitChildren(JSCell*, SlotVisitor&);
    
    // A Structure is cheap to mark during GC if doing so would only add a small and bounded amount
    // to our heap footprint. For example, if the structure refers to a global object that is not
    // yet marked, then as far as we know, the decision to mark this Structure would lead to a large
    // increase in footprint because no other object refers to that global object. This method
    // returns true if all user-controlled (and hence unbounded in size) objects referenced from the
    // Structure are already marked.
    bool isCheapDuringGC(VM&);
    
    // Returns true if this structure is now marked.
    bool markIfCheap(SlotVisitor&);
    
    bool hasRareData() const
    {
        return isRareData(cachedPrototypeChainOrRareData());
    }

    StructureRareData* rareData()
    {
        ASSERT(hasRareData());
        return static_cast<StructureRareData*>(cachedPrototypeChainOrRareData());
    }

    const StructureRareData* rareData() const
    {
        ASSERT(hasRareData());
        return static_cast<const StructureRareData*>(cachedPrototypeChainOrRareData());
    }

    const StructureRareData* rareDataConcurrently() const
    {
        JSCell* cell = cachedPrototypeChainOrRareData();
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
    
    Structure* previousID(VM& vm) const
    {
        if (!m_previousID)
            return nullptr;
        return vm.getStructure(m_previousID);
    }
    bool transitivelyTransitionedFrom(Structure* structureToFind);

    PropertyOffset maxOffset() const
    {
#if CPU(ADDRESS64)
        uint16_t maxOffset = m_maxOffsetAndTransitionPropertyName.type();
#else
        uint16_t maxOffset = m_maxOffset;
#endif
        if (maxOffset == shortInvalidOffset)
            return invalidOffset;
        if (maxOffset == useRareDataFlag)
            return rareData()->m_maxOffset;
        return maxOffset;
    }

    void setMaxOffset(VM& vm, PropertyOffset offset)
    {
        ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
        auto commit = [&](uint16_t value) {
#if CPU(ADDRESS64)
            m_maxOffsetAndTransitionPropertyName.setType(value);
#else
            m_maxOffset = value;
#endif
        };

        if (offset == invalidOffset) {
            commit(shortInvalidOffset);
            return;
        }
        if (offset < useRareDataFlag && offset < shortInvalidOffset) {
            commit(offset);
            return;
        }
#if CPU(ADDRESS64)
        uint16_t maxOffset = m_maxOffsetAndTransitionPropertyName.type();
#else
        uint16_t maxOffset = m_maxOffset;
#endif
        if (maxOffset == useRareDataFlag) {
            rareData()->m_maxOffset = offset;
            return;
        }

        ensureRareData(vm)->m_maxOffset = offset;
        WTF::storeStoreFence();
        commit(useRareDataFlag);
    }

    PropertyOffset transitionOffset() const
    {
#if CPU(ADDRESS64)
        uint16_t transitionOffset = m_transitionOffsetAndClassInfo.type();
#else
        uint16_t transitionOffset = m_transitionOffset;
#endif
        if (transitionOffset == shortInvalidOffset)
            return invalidOffset;
        if (transitionOffset == useRareDataFlag)
            return rareData()->m_transitionOffset;
        return transitionOffset;
    }

    void setTransitionOffset(VM& vm, PropertyOffset offset)
    {
        ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
        auto commit = [&](uint16_t value) {
#if CPU(ADDRESS64)
            m_transitionOffsetAndClassInfo.setType(value);
#else
            m_transitionOffset = value;
#endif
        };

        if (offset == invalidOffset) {
            commit(shortInvalidOffset);
            return;
        }
        if (offset < useRareDataFlag && offset < shortInvalidOffset) {
            commit(offset);
            return;
        }
#if CPU(ADDRESS64)
        uint16_t transitionOffset = m_transitionOffsetAndClassInfo.type();
#else
        uint16_t transitionOffset = m_transitionOffset;
#endif
        if (transitionOffset == useRareDataFlag) {
            rareData()->m_transitionOffset = offset;
            return;
        }

        ensureRareData(vm)->m_transitionOffset = offset;
        WTF::storeStoreFence();
        commit(useRareDataFlag);
    }

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
        COMPILE_ASSERT(outOfLineGrowthFactor == 2, outOfLineGrowthFactor_is_two);
        return WTF::roundUpToPowerOfTwo(outOfLineSize);
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
    bool hasInlineStorage() const { return !!inlineCapacity(); }
    unsigned inlineCapacity() const
    {
#if CPU(ADDRESS64)
        return static_cast<uint8_t>(m_inlineCapacityAndCachedPrototypeChainOrRareData.type());
#else
        return m_inlineCapacity;
#endif
    }
    unsigned inlineSize() const
    {
        return std::min<unsigned>(maxOffset() + 1, inlineCapacity());
    }
    unsigned totalStorageCapacity() const
    {
        ASSERT(structure()->classInfo() == info());
        return outOfLineCapacity() + inlineCapacity();
    }

    bool isValidOffset(PropertyOffset offset) const
    {
        return JSC::isValidOffset(offset)
            && offset <= maxOffset()
            && (offset < static_cast<int>(inlineCapacity()) || offset >= firstOutOfLineOffset);
    }

    bool hijacksIndexingHeader() const
    {
        return isTypedView(classInfo()->typedArrayStorageType);
    }
    
    bool couldHaveIndexingHeader() const
    {
        return hasIndexedProperties(indexingType())
            || hijacksIndexingHeader();
    }
    
    bool hasIndexingHeader(const JSCell*) const;
    bool mayHaveIndexingHeader() const;
    bool canCacheDeleteIC() const;
    
    bool masqueradesAsUndefined(JSGlobalObject* lexicalGlobalObject);

    PropertyOffset get(VM&, PropertyName);
    PropertyOffset get(VM&, PropertyName, unsigned& attributes);

    // This is a somewhat internalish method. It will call your functor while possibly holding the
    // Structure's lock. There is no guarantee whether the lock is held or not in any particular
    // call. So, you have to assume the worst. Also, the functor returns true if it wishes for you
    // to continue or false if it's done.
    template<typename Functor>
    void forEachPropertyConcurrently(const Functor&);

    template<typename Functor>
    void forEachProperty(VM&, const Functor&);
    
    PropertyOffset getConcurrently(UniquedStringImpl* uid);
    PropertyOffset getConcurrently(UniquedStringImpl* uid, unsigned& attributes);
    
    Vector<PropertyMapEntry> getPropertiesConcurrently();
    
    void setHasGetterSetterPropertiesWithProtoCheck(bool is__proto__)
    {
        setHasGetterSetterProperties(true);
        if (!is__proto__)
            setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true);
    }
    
    void setContainsReadOnlyProperties() { setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true); }
    
    void setHasCustomGetterSetterPropertiesWithProtoCheck(bool is__proto__)
    {
        setHasCustomGetterSetterProperties(true);
        if (!is__proto__)
            setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true);
    }

    void setCachedPropertyNameEnumerator(VM&, JSPropertyNameEnumerator*);
    JSPropertyNameEnumerator* cachedPropertyNameEnumerator() const;
    bool canCachePropertyNameEnumerator(VM&) const;
    bool canAccessPropertiesQuicklyForEnumeration() const;

    void setCachedOwnKeys(VM&, JSImmutableButterfly*);
    JSImmutableButterfly* cachedOwnKeys() const;
    JSImmutableButterfly* cachedOwnKeysIgnoringSentinel() const;
    bool canCacheOwnKeys() const;

    void getPropertyNamesFromStructure(VM&, PropertyNameArray&, EnumerationMode);

    JSString* objectToStringValue()
    {
        if (!hasRareData())
            return 0;
        return rareData()->objectToStringValue();
    }

    void setObjectToStringValue(JSGlobalObject*, VM&, JSString* value, PropertySlot toStringTagSymbolSlot);

    const ClassInfo* classInfo() const
    {
#if CPU(ADDRESS64)
        return m_transitionOffsetAndClassInfo.pointer();
#else
        return m_classInfo;
#endif
    }

    static ptrdiff_t structureIDOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_blob) + StructureIDBlob::structureIDOffset();
    }

    static ptrdiff_t prototypeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_prototype);
    }

    static ptrdiff_t globalObjectOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_globalObject);
    }

    static ptrdiff_t offsetOfClassInfo()
    {
#if CPU(ADDRESS64)
        return OBJECT_OFFSETOF(Structure, m_transitionOffsetAndClassInfo);
#else
        return OBJECT_OFFSETOF(Structure, m_classInfo);
#endif
    }

    static ptrdiff_t indexingModeIncludingHistoryOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_blob) + StructureIDBlob::indexingModeIncludingHistoryOffset();
    }

#if CPU(LITTLE_ENDIAN)
    static ptrdiff_t offsetOfInlineCapacity()
    {
#if CPU(ADDRESS64)
        return OBJECT_OFFSETOF(Structure, m_inlineCapacityAndCachedPrototypeChainOrRareData) + CompactPointerTuple<JSCell*, uint16_t>::offsetOfType();
#else
        return OBJECT_OFFSETOF(Structure, m_inlineCapacity);
#endif

    }
#endif

    static ptrdiff_t offsetOfCachedPrototypeChainOrRareData()
    {
#if CPU(ADDRESS64)
        return OBJECT_OFFSETOF(Structure, m_inlineCapacityAndCachedPrototypeChainOrRareData);
#else
        return OBJECT_OFFSETOF(Structure, m_cachedPrototypeChainOrRareData);
#endif
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
    
    bool dfgShouldWatchIfPossible() const
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
    
    bool dfgShouldWatch() const
    {
        return dfgShouldWatchIfPossible() && transitionWatchpointSetIsStillValid();
    }
        
    void addTransitionWatchpoint(Watchpoint* watchpoint) const
    {
        ASSERT(transitionWatchpointSetIsStillValid());
        m_transitionWatchpointSet.add(watchpoint);
    }
    
    void didTransitionFromThisStructure(DeferredStructureTransitionWatchpointFire* = nullptr) const;
    
    InlineWatchpointSet& transitionWatchpointSet() const
    {
        return m_transitionWatchpointSet;
    }
    
    WatchpointSet* ensurePropertyReplacementWatchpointSet(VM&, PropertyOffset);
    void startWatchingPropertyForReplacements(VM& vm, PropertyOffset offset)
    {
        ensurePropertyReplacementWatchpointSet(vm, offset);
    }
    void startWatchingPropertyForReplacements(VM&, PropertyName);
    WatchpointSet* propertyReplacementWatchpointSet(PropertyOffset);
    void didReplaceProperty(PropertyOffset);
    void didCachePropertyReplacement(VM&, PropertyOffset);
    
    void startWatchingInternalPropertiesIfNecessary(VM& vm)
    {
        if (LIKELY(didWatchInternalProperties()))
            return;
        startWatchingInternalProperties(vm);
    }
    
    Ref<StructureShape> toStructureShape(JSValue, bool& sawPolyProtoStructure);
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dumpBrief(PrintStream&, const CString&) const;
    
    static void dumpContextHeader(PrintStream&);
    
    static bool shouldConvertToPolyProto(const Structure* a, const Structure* b);

    UniquedStringImpl* transitionPropertyName() const
    {
#if CPU(ADDRESS64)
        return m_maxOffsetAndTransitionPropertyName.pointer();
#else
        return m_transitionPropertyName.get();
#endif
    }

    struct PropertyHashEntry {
        const HashTable* table;
        const HashTableValue* value;
    };
    Optional<PropertyHashEntry> findPropertyHashEntry(PropertyName) const;
    
    DECLARE_EXPORT_INFO;

private:
    bool ruleOutUnseenProperty(UniquedStringImpl*) const;
#if CPU(ADDRESS64)
    // As a propertyHash, 64bit environment uses 16bit property-hash + seenProperties set.
    uintptr_t propertyHash() const { return m_propertyHashAndSeenProperties.data(); }
#else
    uint32_t propertyHash() const { return m_propertyHash; }
#endif
    TinyBloomFilter seenProperties() const;
    void addPropertyHashAndSeenProperty(unsigned, UniquedStringImpl*);

    void setTransitionPropertyName(const AbstractLocker&, UniquedStringImpl* transitionPropertyName)
    {
#if CPU(ADDRESS64)
        m_maxOffsetAndTransitionPropertyName.setPointer(transitionPropertyName);
#else
        m_transitionPropertyName = transitionPropertyName;
#endif
    }

    typedef enum { 
        NoneDictionaryKind = 0,
        CachedDictionaryKind = 1,
        UncachedDictionaryKind = 2
    } DictionaryKind;

public:
#define DEFINE_BITFIELD(type, lowerName, upperName, width, offset) \
    static constexpr uint32_t s_##lowerName##Shift = offset;\
    static constexpr uint32_t s_##lowerName##Mask = ((1 << (width - 1)) | ((1 << (width - 1)) - 1));\
    static constexpr uint32_t s_bitWidthOf##upperName = width;\
    type lowerName() const { return static_cast<type>((m_bitField >> offset) & s_##lowerName##Mask); }\
    void set##upperName(type newValue) \
    {\
        m_bitField &= ~(s_##lowerName##Mask << offset);\
        m_bitField |= (newValue & s_##lowerName##Mask) << offset;\
        ASSERT(newValue == lowerName());\
    }

    DEFINE_BITFIELD(DictionaryKind, dictionaryKind, DictionaryKind, 2, 0);
    DEFINE_BITFIELD(bool, isPinnedPropertyTable, IsPinnedPropertyTable, 1, 2);
    DEFINE_BITFIELD(bool, hasGetterSetterProperties, HasGetterSetterProperties, 1, 3);
    DEFINE_BITFIELD(bool, hasReadOnlyOrGetterSetterPropertiesExcludingProto, HasReadOnlyOrGetterSetterPropertiesExcludingProto, 1, 4);
    DEFINE_BITFIELD(bool, isQuickPropertyAccessAllowedForEnumeration, IsQuickPropertyAccessAllowedForEnumeration, 1, 5);
    DEFINE_BITFIELD(unsigned, transitionPropertyAttributes, TransitionPropertyAttributes, 8, 6);
    DEFINE_BITFIELD(bool, didPreventExtensions, DidPreventExtensions, 1, 14);
    DEFINE_BITFIELD(bool, didTransition, DidTransition, 1, 15);
    DEFINE_BITFIELD(bool, staticPropertiesReified, StaticPropertiesReified, 1, 16);
    DEFINE_BITFIELD(bool, hasBeenFlattenedBefore, HasBeenFlattenedBefore, 1, 17);
    DEFINE_BITFIELD(bool, hasCustomGetterSetterProperties, HasCustomGetterSetterProperties, 1, 18);
    DEFINE_BITFIELD(bool, didWatchInternalProperties, DidWatchInternalProperties, 1, 19);
    DEFINE_BITFIELD(bool, transitionWatchpointIsLikelyToBeFired, TransitionWatchpointIsLikelyToBeFired, 1, 20);
    DEFINE_BITFIELD(bool, hasBeenDictionary, HasBeenDictionary, 1, 21);
    DEFINE_BITFIELD(bool, protectPropertyTableWhileTransitioning, ProtectPropertyTableWhileTransitioning, 1, 22);
    DEFINE_BITFIELD(bool, hasUnderscoreProtoPropertyExcludingOriginalProto, HasUnderscoreProtoPropertyExcludingOriginalProto, 1, 23);
    DEFINE_BITFIELD(bool, isPropertyDeletionTransition, IsPropertyDeletionTransition, 1, 24);

    static_assert(s_bitWidthOfTransitionPropertyAttributes <= sizeof(TransitionPropertyAttributes) * 8);

private:
    friend class LLIntOffsetsExtractor;

    JS_EXPORT_PRIVATE Structure(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType, unsigned inlineCapacity);
    Structure(VM&);
    Structure(VM&, Structure*, DeferredStructureTransitionWatchpointFire*);

    static Structure* create(VM&, Structure*, DeferredStructureTransitionWatchpointFire* = nullptr);
    
    static Structure* addPropertyTransitionToExistingStructureImpl(Structure*, UniquedStringImpl* uid, unsigned attributes, PropertyOffset&);
    static Structure* removePropertyTransitionFromExistingStructureImpl(Structure*, PropertyName, unsigned attributes, PropertyOffset&);

    // This will return the structure that has a usable property table, that property table,
    // and the list of structures that we visited before we got to it. If it returns a
    // non-null structure, it will also lock the structure that it returns; it is your job
    // to unlock it.
    void findStructuresAndMapForMaterialization(VM&, Vector<Structure*, 8>& structures, Structure*&, PropertyTable*&);
    
    static Structure* toDictionaryTransition(VM&, Structure*, DictionaryKind, DeferredStructureTransitionWatchpointFire* = nullptr);

    enum class ShouldPin { No, Yes };
    template<ShouldPin, typename Func>
    PropertyOffset add(VM&, PropertyName, unsigned attributes, const Func&);
    PropertyOffset add(VM&, PropertyName, unsigned attributes);
    template<ShouldPin, typename Func>
    PropertyOffset remove(VM&, PropertyName, const Func&);
    PropertyOffset remove(VM&, PropertyName);

    void checkConsistency();

    // This may grab the lock, or not. Do not call when holding the Structure's lock.
    PropertyTable* ensurePropertyTableIfNotEmpty(VM& vm)
    {
        if (PropertyTable* result = propertyTableUnsafeOrNull())
            return result;
        if (!previousID(vm))
            return nullptr;
        return materializePropertyTable(vm);
    }
    
    // This may grab the lock, or not. Do not call when holding the Structure's lock.
    PropertyTable* ensurePropertyTable(VM& vm)
    {
        if (PropertyTable* result = propertyTableUnsafeOrNull())
            return result;
        return materializePropertyTable(vm);
    }
    
    PropertyTable* propertyTableUnsafeOrNull() const
    {
#if CPU(ADDRESS64)
        return m_outOfLineTypeFlagsAndPropertyTableUnsafe.pointer();
#else
        return m_propertyTableUnsafe.get();
#endif
    }
    
    // This will grab the lock. Do not call when holding the Structure's lock.
    JS_EXPORT_PRIVATE PropertyTable* materializePropertyTable(VM&, bool setPropertyTable = true);
    
    void setPropertyTable(VM& vm, PropertyTable* table);
    void clearPropertyTable();
    
    PropertyTable* takePropertyTableOrCloneIfPinned(VM&);
    PropertyTable* copyPropertyTableForPinning(VM&);

    void setPreviousID(VM&, Structure*);
    void clearPreviousID();

    int transitionCountEstimate() const
    {
        // Since the number of transitions is often the same as the last offset (except if there are deletes)
        // we keep the size of Structure down by not storing both.
        return numberOfSlotsForMaxOffset(maxOffset(), inlineCapacity());
    }

    bool isValid(JSGlobalObject*, StructureChain* cachedPrototypeChain, JSObject* base) const;

    // You have to hold the structure lock to do these.
    JS_EXPORT_PRIVATE void pin(const AbstractLocker&, VM&, PropertyTable*);
    void pinForCaching(const AbstractLocker&, VM&, PropertyTable*);
    
    bool isRareData(JSCell* cell) const
    {
        return cell && cell->type() == StructureRareDataType;
    }

    template<typename DetailsFunc>
    bool checkOffsetConsistency(PropertyTable*, const DetailsFunc&) const;
    bool checkOffsetConsistency() const;

    JS_EXPORT_PRIVATE void allocateRareData(VM&);
    
    void startWatchingInternalProperties(VM&);

    StructureChain* cachedPrototypeChain() const;
    void setCachedPrototypeChain(VM&, StructureChain*);

    void setOutOfLineTypeFlags(TypeInfo::OutOfLineTypeFlags);
    void setClassInfo(const ClassInfo*);
    void setInlineCapacity(uint8_t);

    JSCell* cachedPrototypeChainOrRareData() const
    {
#if CPU(ADDRESS64)
        return m_inlineCapacityAndCachedPrototypeChainOrRareData.pointer();
#else
        return m_cachedPrototypeChainOrRareData.get();
#endif
    }

    static constexpr int s_maxTransitionLength = 64;
    static constexpr int s_maxTransitionLengthForNonEvalPutById = 512;

    // These need to be properly aligned at the beginning of the 'Structure'
    // part of the object.
    StructureIDBlob m_blob;

    // The property table pointer should be accessed through ensurePropertyTable(). During GC, m_propertyTableUnsafe field part may be set to 0 by another thread.
    // During a Heap Snapshot GC we avoid clearing the table so it is safe to use.
#if CPU(ADDRESS64)
public:
    static constexpr uintptr_t classInfoMask = CompactPointerTuple<const ClassInfo*, uint16_t>::pointerMask;
    static constexpr uintptr_t cachedPrototypeChainOrRareDataMask = CompactPointerTuple<JSCell*, uint16_t>::pointerMask;
private:
    // Structure is one of the most frequently allocated data structure. Moreover, Structure tends to be alive a long time!
    // This motivates extra complicated hack which optimizes sizeof(Structure).
    //
    // We combine 16bit data and 64bit pointer into one pointer-size field to (1) save memory while (2) not losing atomic load/store.
    // The key here is analyzing data access patterns carefully. They are categoriezed into three types.
    //     1. ImmutableAfterConstruction
    //     2. MutableFromAnyThread
    //     3. MutableFromMainThread
    //  We assume that loading happens from any threads. Under this assumption, MutableFromAnyThread + (MutableFromMainThread / MutableFromAnyThread) is the pair which is racy.
    //  Other pairs works well. We carefully put assertions to setters, analyze access patterns and pick appropriate pairs in Structure fields.
    CompactPointerTuple<PropertyTable*, TypeInfo::OutOfLineTypeFlags> m_outOfLineTypeFlagsAndPropertyTableUnsafe; // ImmutableAfterConstruction(m_outOfLineTypeFlags) and MutableFromAnyThread(m_propertyTableUnsafe).
    CompactRefPtrTuple<UniquedStringImpl, uint16_t> m_maxOffsetAndTransitionPropertyName; // MutableFromMainThread(m_maxOffset) and MutableFromMainThread(m_transitionPropertyName).
    CompactPointerTuple<const ClassInfo*, uint16_t> m_transitionOffsetAndClassInfo; // MutableFromMainThread(m_transitionOffset) and ImmutableAfterConstruction(m_classInfo).
    CompactPointerTuple<JSCell*, uint16_t> m_inlineCapacityAndCachedPrototypeChainOrRareData; // ImmutableAfterConstruction(m_inlineCapacity) and MutableFromMainThread(m_cachedPrototypeChainOrRareData).
    CompactPointerTuple<UniquedStringImpl*, uint16_t> m_propertyHashAndSeenProperties; // MutableFromMainThread(m_propertyHash) and MutableFromMainThread(m_seenProperties).
#else
    TypeInfo::OutOfLineTypeFlags m_outOfLineTypeFlags { 0 };
    uint8_t m_inlineCapacity { 0 };
    uint32_t m_propertyHash { 0 };
    uint16_t m_transitionOffset { 0 };
    uint16_t m_maxOffset { 0 };
    WriteBarrier<PropertyTable> m_propertyTableUnsafe;
    const ClassInfo* m_classInfo { nullptr };
    WriteBarrier<JSCell> m_cachedPrototypeChainOrRareData;
    uintptr_t m_seenProperties { 0 };
    RefPtr<UniquedStringImpl> m_transitionPropertyName;
#endif
    StructureID m_previousID { 0 };
    uint32_t m_bitField { 0 };

    StructureTransitionTable m_transitionTable;
    WriteBarrier<JSGlobalObject> m_globalObject;
    WriteBarrier<Unknown> m_prototype;

    mutable InlineWatchpointSet m_transitionWatchpointSet;

    COMPILE_ASSERT(firstOutOfLineOffset < 256, firstOutOfLineOffset_fits);

    friend class VMInspector;
    friend class JSDollarVMHelper;
};
#if CPU(ADDRESS64)
static_assert(sizeof(Structure) <= 96, "Do not increase sizeof(Structure), it immediately causes memory regression");
#endif

} // namespace JSC
