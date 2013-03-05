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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "IndexingType.h"
#include "JSCJSValue.h"
#include "JSCell.h"
#include "JSType.h"
#include "PropertyName.h"
#include "PropertyNameArray.h"
#include "PropertyOffset.h"
#include "Protect.h"
#include "StructureRareData.h"
#include "StructureTransitionTable.h"
#include "JSTypeInfo.h"
#include "Watchpoint.h"
#include "Weak.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/StringImpl.h>


namespace JSC {

class LLIntOffsetsExtractor;
class PropertyNameArray;
class PropertyNameArrayData;
class PropertyTable;
class StructureChain;
class SlotVisitor;
class JSString;

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

    static Structure* create(JSGlobalData&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, unsigned inlineCapacity = 0);

protected:
    void finishCreation(JSGlobalData& globalData)
    {
        Base::finishCreation(globalData);
        ASSERT(m_prototype);
        ASSERT(m_prototype.isObject() || m_prototype.isNull());
    }

    void finishCreation(JSGlobalData& globalData, CreatingEarlyCellTag)
    {
        Base::finishCreation(globalData, this, CreatingEarlyCell);
        ASSERT(m_prototype);
        ASSERT(m_prototype.isNull());
        ASSERT(!globalData.structureStructure);
    }

public:
    static void dumpStatistics();

    JS_EXPORT_PRIVATE static Structure* addPropertyTransition(JSGlobalData&, Structure*, PropertyName, unsigned attributes, JSCell* specificValue, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* addPropertyTransitionToExistingStructure(Structure*, PropertyName, unsigned attributes, JSCell* specificValue, PropertyOffset&);
    static Structure* removePropertyTransition(JSGlobalData&, Structure*, PropertyName, PropertyOffset&);
    JS_EXPORT_PRIVATE static Structure* changePrototypeTransition(JSGlobalData&, Structure*, JSValue prototype);
    JS_EXPORT_PRIVATE static Structure* despecifyFunctionTransition(JSGlobalData&, Structure*, PropertyName);
    static Structure* attributeChangeTransition(JSGlobalData&, Structure*, PropertyName, unsigned attributes);
    static Structure* toCacheableDictionaryTransition(JSGlobalData&, Structure*);
    static Structure* toUncacheableDictionaryTransition(JSGlobalData&, Structure*);
    static Structure* sealTransition(JSGlobalData&, Structure*);
    static Structure* freezeTransition(JSGlobalData&, Structure*);
    static Structure* preventExtensionsTransition(JSGlobalData&, Structure*);
    static Structure* nonPropertyTransition(JSGlobalData&, Structure*, NonPropertyTransition);

    bool isSealed(JSGlobalData&);
    bool isFrozen(JSGlobalData&);
    bool isExtensible() const { return !m_preventExtensions; }
    bool didTransition() const { return m_didTransition; }
    bool putWillGrowOutOfLineStorage();
    JS_EXPORT_PRIVATE size_t suggestedNewOutOfLineStorageCapacity(); 

    Structure* flattenDictionaryStructure(JSGlobalData&, JSObject*);

    static const bool needsDestruction = true;
    static const bool hasImmortalStructure = true;
    static void destroy(JSCell*);

    // These should be used with caution.  
    JS_EXPORT_PRIVATE PropertyOffset addPropertyWithoutTransition(JSGlobalData&, PropertyName, unsigned attributes, JSCell* specificValue);
    PropertyOffset removePropertyWithoutTransition(JSGlobalData&, PropertyName);
    void setPrototypeWithoutTransition(JSGlobalData& globalData, JSValue prototype) { m_prototype.set(globalData, this, prototype); }
        
    bool isDictionary() const { return m_dictionaryKind != NoneDictionaryKind; }
    bool isUncacheableDictionary() const { return m_dictionaryKind == UncachedDictionaryKind; }

    bool propertyAccessesAreCacheable() { return m_dictionaryKind != UncachedDictionaryKind && !typeInfo().prohibitsPropertyCaching(); }

    // Type accessors.
    const TypeInfo& typeInfo() const { ASSERT(structure()->classInfo() == &s_info); return m_typeInfo; }
    bool isObject() const { return typeInfo().isObject(); }

    IndexingType indexingType() const { return m_indexingType & AllArrayTypes; }
    IndexingType indexingTypeIncludingHistory() const { return m_indexingType; }
        
    bool mayInterceptIndexedAccesses() const
    {
        return !!(indexingTypeIncludingHistory() & MayHaveIndexedAccessors);
    }
        
    bool anyObjectInChainMayInterceptIndexedAccesses() const;
        
    bool needsSlowPutIndexing() const;
    NonPropertyTransition suggestedArrayStorageTransition() const;
        
    JSGlobalObject* globalObject() const { return m_globalObject.get(); }
    void setGlobalObject(JSGlobalData& globalData, JSGlobalObject* globalObject) { m_globalObject.set(globalData, this, globalObject); }
        
    JSValue storedPrototype() const { return m_prototype.get(); }
    JSValue prototypeForLookup(ExecState*) const;
    JSValue prototypeForLookup(JSGlobalObject*) const;
    JSValue prototypeForLookup(CodeBlock*) const;
    StructureChain* prototypeChain(JSGlobalData&, JSGlobalObject*) const;
    StructureChain* prototypeChain(ExecState*) const;
    static void visitChildren(JSCell*, SlotVisitor&);
        
    // Will just the prototype chain intercept this property access?
    bool prototypeChainMayInterceptStoreTo(JSGlobalData&, PropertyName);
        
    bool transitionDidInvolveSpecificValue() const { return !!m_specificValueInPrevious; }
        
    Structure* previousID() const
    {
        ASSERT(structure()->classInfo() == &s_info);
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
        ASSERT(structure()->classInfo() == &s_info);
            
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
        ASSERT(structure()->classInfo() == &s_info);
        return outOfLineCapacity() + inlineCapacity();
    }

    PropertyOffset firstValidOffset() const
    {
        if (hasInlineStorage())
            return 0;
        return firstOutOfLineOffset;
    }
    PropertyOffset lastValidOffset() const
    {
        return m_offset;
    }
    bool isValidOffset(PropertyOffset offset) const
    {
        return offset >= firstValidOffset()
            && offset <= lastValidOffset();
    }

    bool masqueradesAsUndefined(JSGlobalObject* lexicalGlobalObject);

    PropertyOffset get(JSGlobalData&, PropertyName);
    PropertyOffset get(JSGlobalData&, const WTF::String& name);
    JS_EXPORT_PRIVATE PropertyOffset get(JSGlobalData&, PropertyName, unsigned& attributes, JSCell*& specificValue);

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

    JS_EXPORT_PRIVATE void despecifyDictionaryFunction(JSGlobalData&, PropertyName);
    void disableSpecificFunctionTracking() { m_specificFunctionThrashCount = maxSpecificFunctionThrashCount; }

    void setEnumerationCache(JSGlobalData&, JSPropertyNameIterator* enumerationCache); // Defined in JSPropertyNameIterator.h.
    JSPropertyNameIterator* enumerationCache(); // Defined in JSPropertyNameIterator.h.
    void getPropertyNamesFromStructure(JSGlobalData&, PropertyNameArray&, EnumerationMode);

    JSString* objectToStringValue()
    {
        if (!typeInfo().structureHasRareData())
            return 0;
        return rareData()->objectToStringValue();
    }

    void setObjectToStringValue(JSGlobalData& globalData, const JSCell* owner, JSString* value)
    {
        if (!typeInfo().structureHasRareData())
            allocateRareData(globalData);
        rareData()->setObjectToStringValue(globalData, owner, value);
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

    static ptrdiff_t prototypeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_prototype);
    }

    static ptrdiff_t globalObjectOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_globalObject);
    }

    static ptrdiff_t typeInfoFlagsOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_typeInfo) + TypeInfo::flagsOffset();
    }

    static ptrdiff_t typeInfoTypeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_typeInfo) + TypeInfo::typeOffset();
    }
        
    static ptrdiff_t classInfoOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_classInfo);
    }
        
    static ptrdiff_t indexingTypeOffset()
    {
        return OBJECT_OFFSETOF(Structure, m_indexingType);
    }

    static Structure* createStructure(JSGlobalData&);
        
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
        m_transitionWatchpointSet.notifyWrite();
    }
        
    static JS_EXPORTDATA const ClassInfo s_info;

private:
    friend class LLIntOffsetsExtractor;

    JS_EXPORT_PRIVATE Structure(JSGlobalData&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType, unsigned inlineCapacity);
    Structure(JSGlobalData&);
    Structure(JSGlobalData&, const Structure*);

    static Structure* create(JSGlobalData&, const Structure*);
        
    typedef enum { 
        NoneDictionaryKind = 0,
        CachedDictionaryKind = 1,
        UncachedDictionaryKind = 2
    } DictionaryKind;
    static Structure* toDictionaryTransition(JSGlobalData&, Structure*, DictionaryKind);

    PropertyOffset putSpecificValue(JSGlobalData&, PropertyName, unsigned attributes, JSCell* specificValue);
    PropertyOffset remove(PropertyName);

    void createPropertyMap(JSGlobalData&, unsigned keyCount = 0);
    void checkConsistency();

    bool despecifyFunction(JSGlobalData&, PropertyName);
    void despecifyAllFunctions(JSGlobalData&);

    WriteBarrier<PropertyTable>& propertyTable();

    PropertyTable* copyPropertyTable(JSGlobalData&, Structure* owner);
    PropertyTable* copyPropertyTableForPinning(JSGlobalData&, Structure* owner);
    JS_EXPORT_PRIVATE void materializePropertyMap(JSGlobalData&);
    void materializePropertyMapIfNecessary(JSGlobalData& globalData)
    {
        ASSERT(structure()->classInfo() == &s_info);
        ASSERT(checkOffsetConsistency());
        if (!propertyTable() && previousID())
            materializePropertyMap(globalData);
    }
    void materializePropertyMapIfNecessaryForPinning(JSGlobalData& globalData)
    {
        ASSERT(structure()->classInfo() == &s_info);
        checkOffsetConsistency();
        if (!propertyTable())
            materializePropertyMap(globalData);
    }

    void setPreviousID(JSGlobalData& globalData, Structure* transition, Structure* structure)
    {
        if (typeInfo().structureHasRareData())
            rareData()->setPreviousID(globalData, transition, structure);
        else
            m_previousOrRareData.set(globalData, transition, structure);
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

    void allocateRareData(JSGlobalData&);
    void cloneRareDataFrom(JSGlobalData&, const Structure*);

    static const int s_maxTransitionLength = 64;

    static const unsigned maxSpecificFunctionThrashCount = 3;

    TypeInfo m_typeInfo;
    IndexingType m_indexingType;
        
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

    uint8_t m_inlineCapacity;
    COMPILE_ASSERT(firstOutOfLineOffset < 256, firstOutOfLineOffset_fits);

    // m_offset does not account for anonymous slots
    PropertyOffset m_offset;

    unsigned m_dictionaryKind : 2;
    bool m_isPinnedPropertyTable : 1;
    bool m_hasGetterSetterProperties : 1;
    bool m_hasReadOnlyOrGetterSetterPropertiesExcludingProto : 1;
    bool m_hasNonEnumerableProperties : 1;
    unsigned m_attributesInPrevious : 22;
    unsigned m_specificFunctionThrashCount : 2;
    unsigned m_preventExtensions : 1;
    unsigned m_didTransition : 1;
    unsigned m_staticFunctionReified;
};

} // namespace JSC

#endif // Structure_h
