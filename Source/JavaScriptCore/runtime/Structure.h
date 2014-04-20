/*
 * Copyright (C) 2008, 2009, 2012, 2013 Apple Inc. All rights reserved.
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

    static void dumpStatistics();

    JS_EXPORT_PRIVATE static Structure* addPropertyTransition(VM&, Structure*, PropertyName, unsigned attributes, JSCell* specificValue, PropertyOffset&, PutPropertySlot::Context = PutPropertySlot::UnknownContext);
    static Structure* addPropertyTransitionToExistingStructureConcurrently(Structure*, StringImpl* uid, unsigned attributes, JSCell* specificValue, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* addPropertyTransitionToExistingStructure(Structure*, PropertyName, unsigned attributes, JSCell* specificValue, PropertyOffset&);
    static Structure* removePropertyTransition(VM&, Structure*, PropertyName, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* changePrototypeTransition(VM&, Structure*, JSValue prototype);
    JS_EXPORT_PRIVATE static Structure* despecifyFunctionTransition(VM&, Structure*, PropertyName);
    static Structure* attributeChangeTransition(VM&, Structure*, PropertyName, unsigned attributes);
    static Structure* toCacheableDictionaryTransition(VM&, Structure*);
    static Structure* toUncacheableDictionaryTransition(VM&, Structure*);
    static Structure* sealTransition(VM&, Structure*);
    static Structure* freezeTransition(VM&, Structure*);
    static Structure* preventExtensionsTransition(VM&, Structure*);
    static Structure* nonPropertyTransition(VM&, Structure*, NonPropertyTransition);

    bool isSealed(VM&);
    bool isFrozen(VM&);
    bool isExtensible() const { return !m_preventExtensions; }
    bool didTransition() const { return m_didTransition; }
    bool putWillGrowOutOfLineStorage();
    JS_EXPORT_PRIVATE size_t suggestedNewOutOfLineStorageCapacity(); 

    Structure* flattenDictionaryStructure(VM&, JSObject*);

    static const bool needsDestruction = true;
    static const bool hasImmortalStructure = true;
    static void destroy(JSCell*);

    // These should be used with caution.  
    JS_EXPORT_PRIVATE PropertyOffset addPropertyWithoutTransition(VM&, PropertyName, unsigned attributes, JSCell* specificValue);
    PropertyOffset removePropertyWithoutTransition(VM&, PropertyName);
    void setPrototypeWithoutTransition(VM& vm, JSValue prototype) { m_prototype.set(vm, this, prototype); }
        
    bool isDictionary() const { return m_dictionaryKind != NoneDictionaryKind; }
    bool isUncacheableDictionary() const { return m_dictionaryKind == UncachedDictionaryKind; }

    bool propertyAccessesAreCacheable() { return m_dictionaryKind != UncachedDictionaryKind && !typeInfo().prohibitsPropertyCaching(); }

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
        
    bool transitionDidInvolveSpecificValue() const { return !!m_specificValueInPrevious; }
        
    Structure* previousID() const
    {
        ASSERT(structure()->classInfo() == info());
        if (typeInfo().structureHasRareData())
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
    PropertyOffset get(VM&, const WTF::String& name);
    JS_EXPORT_PRIVATE PropertyOffset get(VM&, PropertyName, unsigned& attributes, JSCell*& specificValue);

    PropertyOffset getConcurrently(VM&, StringImpl* uid);
    PropertyOffset getConcurrently(VM&, StringImpl* uid, unsigned& attributes, JSCell*& specificValue);

    bool hasGetterSetterProperties() const { return m_hasGetterSetterProperties; }
    bool hasReadOnlyOrGetterSetterPropertiesExcludingProto() const { return m_hasReadOnlyOrGetterSetterPropertiesExcludingProto; }
    void setHasGetterSetterProperties(bool is__proto__)
    {
        m_hasGetterSetterProperties = true;
        if (!is__proto__)
            m_hasReadOnlyOrGetterSetterPropertiesExcludingProto = true;
    }
    void setContainsReadOnlyProperties()
    {
        m_hasReadOnlyOrGetterSetterPropertiesExcludingProto = true;
    }

    bool hasNonEnumerableProperties() const { return m_hasNonEnumerableProperties; }
        
    bool isEmpty() const
    {
        ASSERT(checkOffsetConsistency());
        return !JSC::isValidOffset(m_offset);
    }

    JS_EXPORT_PRIVATE void despecifyDictionaryFunction(VM&, PropertyName);
    void disableSpecificFunctionTracking() { m_specificFunctionThrashCount = maxSpecificFunctionThrashCount; }

    void setEnumerationCache(VM&, JSPropertyNameIterator* enumerationCache); // Defined in JSPropertyNameIterator.h.
    JSPropertyNameIterator* enumerationCache(); // Defined in JSPropertyNameIterator.h.
    void getPropertyNamesFromStructure(VM&, PropertyNameArray&, EnumerationMode);

    JSString* objectToStringValue()
    {
        if (!typeInfo().structureHasRareData())
            return 0;
        return rareData()->objectToStringValue();
    }

    void setObjectToStringValue(VM& vm, const JSCell* owner, JSString* value)
    {
        if (!typeInfo().structureHasRareData())
            allocateRareData(vm);
        rareData()->setObjectToStringValue(vm, owner, value);
    }

    bool staticFunctionsReified()
    {
        return m_staticFunctionReified;
    }

    void setStaticFunctionsReified()
    {
        m_staticFunctionReified = true;
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
        
    void addTransitionWatchpoint(Watchpoint* watchpoint) const
    {
        ASSERT(transitionWatchpointSetIsStillValid());
        m_transitionWatchpointSet.add(watchpoint);
    }
        
    void notifyTransitionFromThisStructure() const
    {
        m_transitionWatchpointSet.fireAll();
    }
    
    InlineWatchpointSet& transitionWatchpointSet() const
    {
        return m_transitionWatchpointSet;
    }
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dumpBrief(PrintStream&, const CString&) const;
    
    static void dumpContextHeader(PrintStream&);
    
    DECLARE_EXPORT_INFO;

private:
    friend class LLIntOffsetsExtractor;

    JS_EXPORT_PRIVATE Structure(VM&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType, unsigned inlineCapacity);
    Structure(VM&);
    Structure(VM&, const Structure*);

    static Structure* create(VM&, const Structure*);
    
    static Structure* addPropertyTransitionToExistingStructureImpl(Structure*, StringImpl* uid, unsigned attributes, JSCell* specificValue, PropertyOffset&);

    // This will return the structure that has a usable property table, that property table,
    // and the list of structures that we visited before we got to it. If it returns a
    // non-null structure, it will also lock the structure that it returns; it is your job
    // to unlock it.
    void findStructuresAndMapForMaterialization(Vector<Structure*, 8>& structures, Structure*&, PropertyTable*&);
    
    typedef enum { 
        NoneDictionaryKind = 0,
        CachedDictionaryKind = 1,
        UncachedDictionaryKind = 2
    } DictionaryKind;
    static Structure* toDictionaryTransition(VM&, Structure*, DictionaryKind);

    PropertyOffset putSpecificValue(VM&, PropertyName, unsigned attributes, JSCell* specificValue);
    PropertyOffset remove(PropertyName);

    void createPropertyMap(const GCSafeConcurrentJITLocker&, VM&, unsigned keyCount = 0);
    void checkConsistency();

    bool despecifyFunction(VM&, PropertyName);
    void despecifyAllFunctions(VM&);

    WriteBarrier<PropertyTable>& propertyTable();
    PropertyTable* takePropertyTableOrCloneIfPinned(VM&, Structure* owner);
    PropertyTable* copyPropertyTable(VM&, Structure* owner);
    PropertyTable* copyPropertyTableForPinning(VM&, Structure* owner);
    JS_EXPORT_PRIVATE void materializePropertyMap(VM&);
    void materializePropertyMapIfNecessary(VM& vm, DeferGC&)
    {
        ASSERT(!isCompilationThread());
        ASSERT(structure()->classInfo() == info());
        ASSERT(checkOffsetConsistency());
        if (!propertyTable() && previousID())
            materializePropertyMap(vm);
    }
    void materializePropertyMapIfNecessaryForPinning(VM& vm, DeferGC&)
    {
        ASSERT(structure()->classInfo() == info());
        checkOffsetConsistency();
        if (!propertyTable())
            materializePropertyMap(vm);
    }

    void setPreviousID(VM& vm, Structure* transition, Structure* structure)
    {
        if (typeInfo().structureHasRareData())
            rareData()->setPreviousID(vm, transition, structure);
        else
            m_previousOrRareData.set(vm, transition, structure);
    }

    void clearPreviousID()
    {
        if (typeInfo().structureHasRareData())
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
        ASSERT(!typeInfo().structureHasRareData());
        return static_cast<Structure*>(m_previousOrRareData.get());
    }

    StructureRareData* rareData() const
    {
        ASSERT(typeInfo().structureHasRareData());
        return static_cast<StructureRareData*>(m_previousOrRareData.get());
    }
        
    bool checkOffsetConsistency() const;

    void allocateRareData(VM&);
    void cloneRareDataFrom(VM&, const Structure*);

    static const int s_maxTransitionLength = 64;
    static const int s_maxTransitionLengthForNonEvalPutById = 512;

    static const unsigned maxSpecificFunctionThrashCount = 3;
    
    // These need to be properly aligned at the beginning of the 'Structure'
    // part of the object.
    StructureIDBlob m_blob;
    TypeInfo::OutOfLineTypeFlags m_outOfLineTypeFlags;

    WriteBarrier<JSGlobalObject> m_globalObject;
    WriteBarrier<Unknown> m_prototype;
    mutable WriteBarrier<StructureChain> m_cachedPrototypeChain;

    WriteBarrier<JSCell> m_previousOrRareData;

    RefPtr<StringImpl> m_nameInPrevious;
    WriteBarrier<JSCell> m_specificValueInPrevious;

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
    
    unsigned m_dictionaryKind : 2;
    bool m_isPinnedPropertyTable : 1;
    bool m_hasGetterSetterProperties : 1;
    bool m_hasReadOnlyOrGetterSetterPropertiesExcludingProto : 1;
    bool m_hasNonEnumerableProperties : 1;
    unsigned m_attributesInPrevious : 14;
    unsigned m_specificFunctionThrashCount : 2;
    unsigned m_preventExtensions : 1;
    unsigned m_didTransition : 1;
    unsigned m_staticFunctionReified : 1;
};

} // namespace JSC

#endif // Structure_h
