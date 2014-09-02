/*
 * Copyright (C) 2008, 2009, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef Structure_h
#define Structure_h

#include "ClassInfo.h"
#include "ConcurrentJITLock.h"
#include "IndexingType.h"
#include "JSCJSValue.h"
#include "JSCell.h"
#include "JSType.h"
#include "PropertyName.h"
#include "PropertyNameArray.h"
#include "PropertyOffset.h"
#include "Protect.h"
#include "PutPropertySlot.h"
#include "StructureIDBlob.h"
#include "StructureRareData.h"
#include "StructureTransitionTable.h"
#include "JSTypeInfo.h"
#include "Watchpoint.h"
#include "Weak.h"
#include "WriteBarrierInlines.h"
#include <wtf/CompilationThread.h>
#include <wtf/PassRefPtr.h>
#include <wtf/PrintStream.h>
#include <wtf/RefCounted.h>
#include <wtf/text/StringImpl.h>


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

// The out-of-line property storage capacity to use when first allocating out-of-line
// storage. Note that all objects start out without having any out-of-line storage;
// this comes into play only on the first property store that exhausts inline storage.
static const unsigned initialOutOfLineCapacity = 4;

// The factor by which to grow out-of-line storage when it is exhausted, after the
// initial allocation.
static const unsigned outOfLineGrowthFactor = 2;

class Structure : public JSCell {
public:
    friend class StructureTransitionTable;

    typedef JSCell Base;
    
    static Structure* create(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, unsigned inlineCapacity = 0);

    ~Structure();

protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(m_prototype);
        ASSERT(m_prototype.isObject() || m_prototype.isNull());
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
        return type == ImpureProxyType || type == PureForwardingProxyType;
    }

    static void dumpStatistics();

    JS_EXPORT_PRIVATE static Structure* addPropertyTransition(VM&, Structure*, PropertyName, unsigned attributes, PropertyOffset&, PutPropertySlot::Context = PutPropertySlot::UnknownContext);
    static Structure* addPropertyTransitionToExistingStructureConcurrently(Structure*, StringImpl* uid, unsigned attributes, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* addPropertyTransitionToExistingStructure(Structure*, PropertyName, unsigned attributes, PropertyOffset&);
    static Structure* removePropertyTransition(VM&, Structure*, PropertyName, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* changePrototypeTransition(VM&, Structure*, JSValue prototype);
    static Structure* attributeChangeTransition(VM&, Structure*, PropertyName, unsigned attributes);
    JS_EXPORT_PRIVATE static Structure* toCacheableDictionaryTransition(VM&, Structure*);
    static Structure* toUncacheableDictionaryTransition(VM&, Structure*);
    JS_EXPORT_PRIVATE static Structure* sealTransition(VM&, Structure*);
    JS_EXPORT_PRIVATE static Structure* freezeTransition(VM&, Structure*);
    static Structure* preventExtensionsTransition(VM&, Structure*);
    JS_EXPORT_PRIVATE static Structure* nonPropertyTransition(VM&, Structure*, NonPropertyTransition);

    JS_EXPORT_PRIVATE bool isSealed(VM&);
    JS_EXPORT_PRIVATE bool isFrozen(VM&);
    bool isExtensible() const { return !preventExtensions(); }
    bool putWillGrowOutOfLineStorage();
    size_t suggestedNewOutOfLineStorageCapacity(); 

    JS_EXPORT_PRIVATE Structure* flattenDictionaryStructure(VM&, JSObject*);

    static const bool needsDestruction = true;
    static const bool hasImmortalStructure = true;
    static void destroy(JSCell*);

    // These should be used with caution.  
    JS_EXPORT_PRIVATE PropertyOffset addPropertyWithoutTransition(VM&, PropertyName, unsigned attributes);
    PropertyOffset removePropertyWithoutTransition(VM&, PropertyName);
    void setPrototypeWithoutTransition(VM& vm, JSValue prototype) { m_prototype.set(vm, this, prototype); }
        
    bool isDictionary() const { return dictionaryKind() != NoneDictionaryKind; }
    bool isUncacheableDictionary() const { return dictionaryKind() == UncachedDictionaryKind; }
  
    bool propertyAccessesAreCacheable() { return dictionaryKind() != UncachedDictionaryKind && !typeInfo().prohibitsPropertyCaching(); }

    // We use SlowPath in GetByIdStatus for structures that may get new impure properties later to prevent
    // DFG from inlining property accesses since structures don't transition when a new impure property appears.
    bool takesSlowPathInDFGForImpureProperty()
    {
        return typeInfo().hasImpureGetOwnPropertySlot();
    }

    // Type accessors.
    TypeInfo typeInfo() const { ASSERT(structure()->classInfo() == info()); return m_blob.typeInfo(m_outOfLineTypeFlags); }
    bool isObject() const { return typeInfo().isObject(); }

    IndexingType indexingType() const { return m_blob.indexingType() & AllArrayTypes; }
    IndexingType indexingTypeIncludingHistory() const { return m_blob.indexingType(); }
        
    bool mayInterceptIndexedAccesses() const
    {
        return !!(indexingTypeIncludingHistory() & MayHaveIndexedAccessors);
    }
        
    bool anyObjectInChainMayInterceptIndexedAccesses() const;
    bool holesMustForwardToPrototype(VM&) const;
        
    bool needsSlowPutIndexing() const;
    NonPropertyTransition suggestedArrayStorageTransition() const;
        
    JSGlobalObject* globalObject() const { return m_globalObject.get(); }
    void setGlobalObject(VM& vm, JSGlobalObject* globalObject) { m_globalObject.set(vm, this, globalObject); }
        
    JSValue storedPrototype() const { return m_prototype.get(); }
    JSObject* storedPrototypeObject() const;
    Structure* storedPrototypeStructure() const;
    JSValue prototypeForLookup(ExecState*) const;
    JSValue prototypeForLookup(JSGlobalObject*) const;
    JSValue prototypeForLookup(CodeBlock*) const;
    StructureChain* prototypeChain(VM&, JSGlobalObject*) const;
    StructureChain* prototypeChain(ExecState*) const;
    static void visitChildren(JSCell*, SlotVisitor&);
        
    // Will just the prototype chain intercept this property access?
    bool prototypeChainMayInterceptStoreTo(VM&, PropertyName);
        
    Structure* previousID() const
    {
        ASSERT(structure()->classInfo() == info());
        if (hasRareData())
            return rareData()->previousID();
        return previous();
    }
    bool transitivelyTransitionedFrom(Structure* structureToFind);

    unsigned outOfLineCapacity() const
    {
        ASSERT(checkOffsetConsistency());
            
        unsigned outOfLineSize = this->outOfLineSize();

        if (!outOfLineSize)
            return 0;

        if (outOfLineSize <= initialOutOfLineCapacity)
            return initialOutOfLineCapacity;

        ASSERT(outOfLineSize > initialOutOfLineCapacity);
        COMPILE_ASSERT(outOfLineGrowthFactor == 2, outOfLineGrowthFactor_is_two);
        return WTF::roundUpToPowerOfTwo(outOfLineSize);
    }
    unsigned outOfLineSize() const
    {
        ASSERT(checkOffsetConsistency());
        ASSERT(structure()->classInfo() == info());
            
        return numberOfOutOfLineSlotsForLastOffset(m_offset);
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
        return std::min<unsigned>(m_offset + 1, m_inlineCapacity);
    }
    unsigned totalStorageSize() const
    {
        return numberOfSlotsForLastOffset(m_offset, m_inlineCapacity);
    }
    unsigned totalStorageCapacity() const
    {
        ASSERT(structure()->classInfo() == info());
        return outOfLineCapacity() + inlineCapacity();
    }

    bool isValidOffset(PropertyOffset offset) const
    {
        return JSC::isValidOffset(offset)
            && offset <= m_offset
            && (offset < m_inlineCapacity || offset >= firstOutOfLineOffset);
    }
    
    bool couldHaveIndexingHeader() const
    {
        return hasIndexedProperties(indexingType())
            || isTypedView(m_classInfo->typedArrayStorageType);
    }
    
    bool hasIndexingHeader(const JSCell*) const;
    
    bool masqueradesAsUndefined(JSGlobalObject* lexicalGlobalObject);

    PropertyOffset get(VM&, PropertyName);
    PropertyOffset get(VM&, PropertyName, unsigned& attributes);

    PropertyOffset getConcurrently(VM&, StringImpl* uid);
    PropertyOffset getConcurrently(VM&, StringImpl* uid, unsigned& attributes);
    
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
    
    bool isEmpty() const
    {
        ASSERT(checkOffsetConsistency());
        return !JSC::isValidOffset(m_offset);
    }

    void setCachedStructurePropertyNameEnumerator(VM&, JSPropertyNameEnumerator*);
    void setCachedGenericPropertyNameEnumerator(VM&, JSPropertyNameEnumerator*);
    JSPropertyNameEnumerator* cachedStructurePropertyNameEnumerator() const;
    JSPropertyNameEnumerator* cachedGenericPropertyNameEnumerator() const;
    bool canCacheStructurePropertyNameEnumerator() const;
    bool canCacheGenericPropertyNameEnumerator() const;
    bool canAccessPropertiesQuickly() const;

    void getPropertyNamesFromStructure(VM&, PropertyNameArray&, EnumerationMode);

    JSString* objectToStringValue()
    {
        if (!hasRareData())
            return 0;
        return rareData()->objectToStringValue();
    }

    void setObjectToStringValue(VM& vm, JSString* value)
    {
        if (!hasRareData())
            allocateRareData(vm);
        rareData()->setObjectToStringValue(vm, value);
    }

    const ClassInfo* classInfo() const { return m_classInfo; }

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

    static ptrdiff_t classInfoOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_classInfo);
    }
        
    static ptrdiff_t indexingTypeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_blob) + StructureIDBlob::indexingTypeOffset();
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
    
    void didTransitionFromThisStructure() const;
    
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
    
    void startWatchingInternalPropertiesIfNecessaryForEntireChain(VM& vm)
    {
        for (Structure* structure = this; structure; structure = structure->storedPrototypeStructure())
            structure->startWatchingInternalPropertiesIfNecessary(vm);
    }

    PassRefPtr<StructureShape> toStructureShape(JSValue);
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dumpBrief(PrintStream&, const CString&) const;
    
    static void dumpContextHeader(PrintStream&);
    
    DECLARE_EXPORT_INFO;

private:
    typedef enum { 
        NoneDictionaryKind = 0,
        CachedDictionaryKind = 1,
        UncachedDictionaryKind = 2
    } DictionaryKind;

public:
#define DEFINE_BITFIELD(type, lowerName, upperName, width, offset) \
    static const uint32_t s_##lowerName##Shift = offset;\
    static const uint32_t s_##lowerName##Mask = ((1 << (width - 1)) | ((1 << (width - 1)) - 1));\
    type lowerName() const { return static_cast<type>((m_bitField >> offset) & s_##lowerName##Mask); }\
    void set##upperName(type newValue) \
    {\
        m_bitField &= ~(s_##lowerName##Mask << offset);\
        m_bitField |= (newValue & s_##lowerName##Mask) << offset;\
    }

    DEFINE_BITFIELD(DictionaryKind, dictionaryKind, DictionaryKind, 2, 0);
    DEFINE_BITFIELD(bool, isPinnedPropertyTable, IsPinnedPropertyTable, 1, 2);
    DEFINE_BITFIELD(bool, hasGetterSetterProperties, HasGetterSetterProperties, 1, 3);
    DEFINE_BITFIELD(bool, hasReadOnlyOrGetterSetterPropertiesExcludingProto, HasReadOnlyOrGetterSetterPropertiesExcludingProto, 1, 4);
    DEFINE_BITFIELD(bool, hasNonEnumerableProperties, HasNonEnumerableProperties, 1, 5);
    DEFINE_BITFIELD(unsigned, attributesInPrevious, AttributesInPrevious, 14, 6);
    DEFINE_BITFIELD(bool, preventExtensions, PreventExtensions, 1, 20);
    DEFINE_BITFIELD(bool, didTransition, DidTransition, 1, 21);
    DEFINE_BITFIELD(bool, staticFunctionsReified, StaticFunctionsReified, 1, 22);
    DEFINE_BITFIELD(bool, hasRareData, HasRareData, 1, 23);
    DEFINE_BITFIELD(bool, hasBeenFlattenedBefore, HasBeenFlattenedBefore, 1, 24);
    DEFINE_BITFIELD(bool, hasCustomGetterSetterProperties, HasCustomGetterSetterProperties, 1, 25);
    DEFINE_BITFIELD(bool, didWatchInternalProperties, DidWatchInternalProperties, 1, 26);

private:
    friend class LLIntOffsetsExtractor;

    JS_EXPORT_PRIVATE Structure(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType, unsigned inlineCapacity);
    Structure(VM&);
    Structure(VM&, Structure*);

    static Structure* create(VM&, Structure*);
    
    static Structure* addPropertyTransitionToExistingStructureImpl(Structure*, StringImpl* uid, unsigned attributes, PropertyOffset&);

    // This will return the structure that has a usable property table, that property table,
    // and the list of structures that we visited before we got to it. If it returns a
    // non-null structure, it will also lock the structure that it returns; it is your job
    // to unlock it.
    void findStructuresAndMapForMaterialization(Vector<Structure*, 8>& structures, Structure*&, PropertyTable*&);
    
    static Structure* toDictionaryTransition(VM&, Structure*, DictionaryKind);

    PropertyOffset add(VM&, PropertyName, unsigned attributes);
    PropertyOffset remove(PropertyName);

    void createPropertyMap(const GCSafeConcurrentJITLocker&, VM&, unsigned keyCount = 0);
    void checkConsistency();

    WriteBarrier<PropertyTable>& propertyTable();
    PropertyTable* takePropertyTableOrCloneIfPinned(VM&);
    PropertyTable* copyPropertyTable(VM&);
    PropertyTable* copyPropertyTableForPinning(VM&);
    JS_EXPORT_PRIVATE void materializePropertyMap(VM&);
    ALWAYS_INLINE void materializePropertyMapIfNecessary(VM& vm, DeferGC&)
    {
        ASSERT(!isCompilationThread());
        ASSERT(structure()->classInfo() == info());
        ASSERT(checkOffsetConsistency());
        if (!propertyTable() && previousID())
            materializePropertyMap(vm);
    }
    ALWAYS_INLINE void materializePropertyMapIfNecessary(VM& vm, PropertyTable*& table)
    {
        ASSERT(!isCompilationThread());
        ASSERT(structure()->classInfo() == info());
        ASSERT(checkOffsetConsistency());
        table = propertyTable().get();
        if (!table && previousID()) {
            DeferGC deferGC(vm.heap);
            materializePropertyMap(vm);
            table = propertyTable().get();
        }
    }
    void materializePropertyMapIfNecessaryForPinning(VM& vm, DeferGC&)
    {
        ASSERT(structure()->classInfo() == info());
        checkOffsetConsistency();
        if (!propertyTable())
            materializePropertyMap(vm);
    }

    void setPreviousID(VM& vm, Structure* structure)
    {
        if (hasRareData())
            rareData()->setPreviousID(vm, structure);
        else
            m_previousOrRareData.set(vm, this, structure);
    }

    void clearPreviousID()
    {
        if (hasRareData())
            rareData()->clearPreviousID();
        else
            m_previousOrRareData.clear();
    }

    int transitionCount() const
    {
        // Since the number of transitions is always the same as m_offset, we keep the size of Structure down by not storing both.
        return numberOfSlotsForLastOffset(m_offset, m_inlineCapacity);
    }

    bool isValid(JSGlobalObject*, StructureChain* cachedPrototypeChain) const;
    bool isValid(ExecState*, StructureChain* cachedPrototypeChain) const;
        
    void pin();

    Structure* previous() const
    {
        ASSERT(!hasRareData());
        return static_cast<Structure*>(m_previousOrRareData.get());
    }

    StructureRareData* rareData() const
    {
        ASSERT(hasRareData());
        return static_cast<StructureRareData*>(m_previousOrRareData.get());
    }
        
    bool checkOffsetConsistency() const;

    JS_EXPORT_PRIVATE void allocateRareData(VM&);
    
    void startWatchingInternalProperties(VM&);

    static const int s_maxTransitionLength = 64;
    static const int s_maxTransitionLengthForNonEvalPutById = 512;

    // These need to be properly aligned at the beginning of the 'Structure'
    // part of the object.
    StructureIDBlob m_blob;
    TypeInfo::OutOfLineTypeFlags m_outOfLineTypeFlags;

    WriteBarrier<JSGlobalObject> m_globalObject;
    WriteBarrier<Unknown> m_prototype;
    mutable WriteBarrier<StructureChain> m_cachedPrototypeChain;

    WriteBarrier<JSCell> m_previousOrRareData;

    RefPtr<StringImpl> m_nameInPrevious;

    const ClassInfo* m_classInfo;

    StructureTransitionTable m_transitionTable;

    // Should be accessed through propertyTable(). During GC, it may be set to 0 by another thread.
    WriteBarrier<PropertyTable> m_propertyTableUnsafe;

    mutable InlineWatchpointSet m_transitionWatchpointSet;

    COMPILE_ASSERT(firstOutOfLineOffset < 256, firstOutOfLineOffset_fits);

    // m_offset does not account for anonymous slots
    PropertyOffset m_offset;

    uint8_t m_inlineCapacity;
    
    ConcurrentJITLock m_lock;
    
    uint32_t m_bitField;
};

} // namespace JSC

#endif // Structure_h
