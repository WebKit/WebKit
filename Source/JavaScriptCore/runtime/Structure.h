/*
 * Copyright (C) 2008, 2009, 2012 Apple Inc. All rights reserved.
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
#include "JSCell.h"
#include "JSType.h"
#include "JSValue.h"
#include "PropertyMapHashTable.h"
#include "PropertyName.h"
#include "PropertyNameArray.h"
#include "Protect.h"
#include "StructureTransitionTable.h"
#include "JSTypeInfo.h"
#include "Watchpoint.h"
#include "Weak.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/StringImpl.h>


namespace JSC {

    class LLIntOffsetsExtractor;
    class PropertyNameArray;
    class PropertyNameArrayData;
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

        static Structure* create(JSGlobalData&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType = NonArray, PropertyOffset inlineCapacity = 0);

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
        bool putWillGrowOutOfLineStorage()
        {
            ASSERT(outOfLineCapacity() >= outOfLineSize());
            
            if (!m_propertyTable) {
                unsigned currentSize = numberOfOutOfLineSlotsForLastOffset(m_offset);
                ASSERT(outOfLineCapacity() >= currentSize);
                return currentSize == outOfLineCapacity();
            }
            
            ASSERT(totalStorageCapacity() >= m_propertyTable->propertyStorageSize());
            if (m_propertyTable->hasDeletedOffset())
                return false;
            
            ASSERT(totalStorageCapacity() >= m_propertyTable->size());
            return m_propertyTable->size() == totalStorageCapacity();
        }
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
            return m_previous.get();
        }
        bool transitivelyTransitionedFrom(Structure* structureToFind);

        void growOutOfLineCapacity();
        unsigned outOfLineCapacity() const
        {
            ASSERT(structure()->classInfo() == &s_info);
            return m_outOfLineCapacity;
        }
        unsigned outOfLineSize() const
        {
            ASSERT(structure()->classInfo() == &s_info);
            if (m_propertyTable) {
                unsigned totalSize = m_propertyTable->propertyStorageSize();
                unsigned inlineCapacity = this->inlineCapacity();
                if (totalSize < inlineCapacity)
                    return 0;
                return totalSize - inlineCapacity;
            }
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
            unsigned result;
            if (m_propertyTable)
                result = m_propertyTable->propertyStorageSize();
            else
                result = m_offset + 1;
            return std::min<unsigned>(result, m_inlineCapacity);
        }
        unsigned totalStorageSize() const
        {
            if (m_propertyTable)
                return m_propertyTable->propertyStorageSize();
            return numberOfSlotsForLastOffset(m_offset, m_typeInfo.type());
        }
        unsigned totalStorageCapacity() const
        {
            ASSERT(structure()->classInfo() == &s_info);
            return m_outOfLineCapacity + inlineCapacity();
        }

        PropertyOffset firstValidOffset() const
        {
            if (hasInlineStorage())
                return 0;
            return firstOutOfLineOffset;
        }
        PropertyOffset lastValidOffset() const
        {
            if (m_propertyTable)
                return offsetForPropertyNumber(m_propertyTable->propertyStorageSize() - 1, m_inlineCapacity);
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
            if (m_propertyTable)
                return m_propertyTable->isEmpty();
            return !JSC::isValidOffset(m_offset);
        }

        JS_EXPORT_PRIVATE void despecifyDictionaryFunction(JSGlobalData&, PropertyName);
        void disableSpecificFunctionTracking() { m_specificFunctionThrashCount = maxSpecificFunctionThrashCount; }

        void setEnumerationCache(JSGlobalData&, JSPropertyNameIterator* enumerationCache); // Defined in JSPropertyNameIterator.h.
        JSPropertyNameIterator* enumerationCache(); // Defined in JSPropertyNameIterator.h.
        void getPropertyNamesFromStructure(JSGlobalData&, PropertyNameArray&, EnumerationMode);

        JSString* objectToStringValue() { return m_objectToStringValue.get(); }

        void setObjectToStringValue(JSGlobalData& globalData, const JSCell* owner, JSString* value)
        {
            m_objectToStringValue.set(globalData, owner, value);
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

        JS_EXPORT_PRIVATE Structure(JSGlobalData&, JSGlobalObject*, JSValue prototype, const TypeInfo&, const ClassInfo*, IndexingType, PropertyOffset inlineCapacity);
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

        void createPropertyMap(unsigned keyCount = 0);
        void checkConsistency();

        bool despecifyFunction(JSGlobalData&, PropertyName);
        void despecifyAllFunctions(JSGlobalData&);

        PassOwnPtr<PropertyTable> copyPropertyTable(JSGlobalData&, Structure* owner);
        PassOwnPtr<PropertyTable> copyPropertyTableForPinning(JSGlobalData&, Structure* owner);
        JS_EXPORT_PRIVATE void materializePropertyMap(JSGlobalData&);
        void materializePropertyMapIfNecessary(JSGlobalData& globalData)
        {
            ASSERT(structure()->classInfo() == &s_info);
            if (!m_propertyTable && m_previous)
                materializePropertyMap(globalData);
        }
        void materializePropertyMapIfNecessaryForPinning(JSGlobalData& globalData)
        {
            ASSERT(structure()->classInfo() == &s_info);
            if (!m_propertyTable)
                materializePropertyMap(globalData);
        }

        int transitionCount() const
        {
            // Since the number of transitions is always the same as m_offset, we keep the size of Structure down by not storing both.
            return numberOfSlotsForLastOffset(m_offset, m_typeInfo.type());
        }

        bool isValid(JSGlobalObject*, StructureChain* cachedPrototypeChain) const;
        bool isValid(ExecState*, StructureChain* cachedPrototypeChain) const;
        
        void pin();

        static const int s_maxTransitionLength = 64;

        static const unsigned maxSpecificFunctionThrashCount = 3;

        TypeInfo m_typeInfo;
        IndexingType m_indexingType;
        
        WriteBarrier<JSGlobalObject> m_globalObject;
        WriteBarrier<Unknown> m_prototype;
        mutable WriteBarrier<StructureChain> m_cachedPrototypeChain;

        WriteBarrier<Structure> m_previous;
        RefPtr<StringImpl> m_nameInPrevious;
        WriteBarrier<JSCell> m_specificValueInPrevious;

        const ClassInfo* m_classInfo;

        StructureTransitionTable m_transitionTable;

        WriteBarrier<JSPropertyNameIterator> m_enumerationCache;

        OwnPtr<PropertyTable> m_propertyTable;

        WriteBarrier<JSString> m_objectToStringValue;
        
        mutable InlineWatchpointSet m_transitionWatchpointSet;

        uint32_t m_outOfLineCapacity;
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

    inline Structure* Structure::create(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, PropertyOffset inlineCapacity)
    {
        ASSERT(globalData.structureStructure);
        ASSERT(classInfo);
        Structure* structure = new (NotNull, allocateCell<Structure>(globalData.heap)) Structure(globalData, globalObject, prototype, typeInfo, classInfo, indexingType, inlineCapacity);
        structure->finishCreation(globalData);
        return structure;
    }
        
    inline Structure* Structure::createStructure(JSGlobalData& globalData)
    {
        ASSERT(!globalData.structureStructure);
        Structure* structure = new (NotNull, allocateCell<Structure>(globalData.heap)) Structure(globalData);
        structure->finishCreation(globalData, CreatingEarlyCell);
        return structure;
    }

    inline Structure* Structure::create(JSGlobalData& globalData, const Structure* structure)
    {
        ASSERT(globalData.structureStructure);
        Structure* newStructure = new (NotNull, allocateCell<Structure>(globalData.heap)) Structure(globalData, structure);
        newStructure->finishCreation(globalData);
        return newStructure;
    }
        
    inline PropertyOffset Structure::get(JSGlobalData& globalData, PropertyName propertyName)
    {
        ASSERT(structure()->classInfo() == &s_info);
        materializePropertyMapIfNecessary(globalData);
        if (!m_propertyTable)
            return invalidOffset;

        PropertyMapEntry* entry = m_propertyTable->find(propertyName.uid()).first;
        return entry ? entry->offset : invalidOffset;
    }

    inline PropertyOffset Structure::get(JSGlobalData& globalData, const WTF::String& name)
    {
        ASSERT(structure()->classInfo() == &s_info);
        materializePropertyMapIfNecessary(globalData);
        if (!m_propertyTable)
            return invalidOffset;

        PropertyMapEntry* entry = m_propertyTable->findWithString(name.impl()).first;
        return entry ? entry->offset : invalidOffset;
    }
    
    inline bool Structure::masqueradesAsUndefined(JSGlobalObject* lexicalGlobalObject)
    {
        return typeInfo().masqueradesAsUndefined() && globalObject() == lexicalGlobalObject;
    }

    ALWAYS_INLINE void SlotVisitor::internalAppend(JSCell* cell)
    {
        ASSERT(!m_isCheckingForDefaultMarkViolation);
        if (!cell)
            return;
#if ENABLE(GC_VALIDATION)
        validate(cell);
#endif
        if (Heap::testAndSetMarked(cell) || !cell->structure())
            return;

        m_visitCount++;
        
        MARK_LOG_CHILD(*this, cell);

        // Should never attempt to mark something that is zapped.
        ASSERT(!cell->isZapped());
        
        m_stack.append(cell);
    }

    inline StructureTransitionTable::Hash::Key StructureTransitionTable::keyForWeakGCMapFinalizer(void*, Structure* structure)
    {
        // Newer versions of the STL have an std::make_pair function that takes rvalue references.
        // When either of the parameters are bitfields, the C++ compiler will try to bind them as lvalues, which is invalid. To work around this, use unary "+" to make the parameter an rvalue.
        // See https://bugs.webkit.org/show_bug.cgi?id=59261 for more details.
        return Hash::Key(structure->m_nameInPrevious.get(), +structure->m_attributesInPrevious);
    }

    inline bool Structure::transitivelyTransitionedFrom(Structure* structureToFind)
    {
        for (Structure* current = this; current; current = current->previousID()) {
            if (current == structureToFind)
                return true;
        }
        return false;
    }

} // namespace JSC

#endif // Structure_h
