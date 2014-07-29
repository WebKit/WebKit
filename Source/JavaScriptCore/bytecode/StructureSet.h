/*
 * Copyright (C) 2011, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef StructureSet_h
#define StructureSet_h

#include "ArrayProfile.h"
#include "SpeculatedType.h"
#include "Structure.h"
#include "DumpContext.h"

namespace JSC {

namespace DFG {
class StructureAbstractValue;
struct AbstractValue;
}

class StructureSet {
public:
    StructureSet()
        : m_pointer(0)
    {
        setEmpty();
    }
    
    StructureSet(Structure* structure)
        : m_pointer(0)
    {
        set(structure);
    }
    
    ALWAYS_INLINE StructureSet(const StructureSet& other)
        : m_pointer(0)
    {
        copyFrom(other);
    }
    
    ALWAYS_INLINE StructureSet& operator=(const StructureSet& other)
    {
        if (this == &other)
            return *this;
        deleteStructureListIfNecessary();
        copyFrom(other);
        return *this;
    }
    
    ~StructureSet()
    {
        deleteStructureListIfNecessary();
    }
    
    void clear();
    
    Structure* onlyStructure() const
    {
        if (isThin())
            return singleStructure();
        OutOfLineList* list = structureList();
        if (list->m_length != 1)
            return nullptr;
        return list->list()[0];
    }
    
    bool isEmpty() const
    {
        bool result = isThin() && !singleStructure();
        if (result)
            ASSERT(m_pointer != reservedValue);
        return result;
    }
    
    bool add(Structure*);
    bool remove(Structure*);
    bool contains(Structure*) const;
    
    bool merge(const StructureSet&);
    void filter(const StructureSet&);
    void exclude(const StructureSet&);
    
#if ENABLE(DFG_JIT)
    void filter(const DFG::StructureAbstractValue&);
    void filter(SpeculatedType);
    void filterArrayModes(ArrayModes);
    void filter(const DFG::AbstractValue&);
#endif // ENABLE(DFG_JIT)
    
    template<typename Functor>
    void genericFilter(Functor& functor)
    {
        if (isThin()) {
            if (!singleStructure())
                return;
            if (functor(singleStructure()))
                return;
            clear();
            return;
        }
        
        OutOfLineList* list = structureList();
        for (unsigned i = 0; i < list->m_length; ++i) {
            if (functor(list->list()[i]))
                continue;
            list->list()[i--] = list->list()[--list->m_length];
        }
        if (!list->m_length)
            clear();
    }
    
    bool isSubsetOf(const StructureSet&) const;
    bool isSupersetOf(const StructureSet& other) const
    {
        return other.isSubsetOf(*this);
    }
    
    bool overlaps(const StructureSet&) const;
    
    size_t size() const
    {
        if (isThin())
            return !!singleStructure();
        return structureList()->m_length;
    }
    
    Structure* at(size_t i) const
    {
        if (isThin()) {
            ASSERT(!i);
            ASSERT(singleStructure());
            return singleStructure();
        }
        ASSERT(i < structureList()->m_length);
        return structureList()->list()[i];
    }
    
    Structure* operator[](size_t i) const { return at(i); }
    
    Structure* last() const
    {
        if (isThin()) {
            ASSERT(singleStructure());
            return singleStructure();
        }
        return structureList()->list()[structureList()->m_length - 1];
    }
    
    bool operator==(const StructureSet& other) const;
    
    SpeculatedType speculationFromStructures() const;
    ArrayModes arrayModesFromStructures() const;
    
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dump(PrintStream&) const;
    
private:
    friend class DFG::StructureAbstractValue;
    
    static const uintptr_t thinFlag = 1;
    static const uintptr_t reservedFlag = 2;
    static const uintptr_t flags = thinFlag | reservedFlag;
    static const uintptr_t reservedValue = 4;

    static const unsigned defaultStartingSize = 4;
    
    bool addOutOfLine(Structure*);
    bool containsOutOfLine(Structure*) const;
    
    class ContainsOutOfLine {
    public:
        ContainsOutOfLine(const StructureSet& set)
            : m_set(set)
        {
        }
        
        bool operator()(Structure* structure)
        {
            return m_set.containsOutOfLine(structure);
        }
    private:
        const StructureSet& m_set;
    };

    ALWAYS_INLINE void copyFrom(const StructureSet& other)
    {
        if (other.isThin() || other.m_pointer == reservedValue) {
            bool value = getReservedFlag();
            m_pointer = other.m_pointer;
            setReservedFlag(value);
            return;
        }
        copyFromOutOfLine(other);
    }
    void copyFromOutOfLine(const StructureSet& other);
    
    class OutOfLineList {
    public:
        static OutOfLineList* create(unsigned capacity);
        static void destroy(OutOfLineList*);
        
        Structure** list() { return bitwise_cast<Structure**>(this + 1); }
        
        OutOfLineList(unsigned length, unsigned capacity)
            : m_length(length)
            , m_capacity(capacity)
        {
        }

        unsigned m_length;
        unsigned m_capacity;
    };
    
    ALWAYS_INLINE void deleteStructureListIfNecessary()
    {
        if (!isThin() && m_pointer != reservedValue)
            OutOfLineList::destroy(structureList());
    }
    
    bool isThin() const { return m_pointer & thinFlag; }
    
    void* pointer() const
    {
        return bitwise_cast<void*>(m_pointer & ~flags);
    }
    
    Structure* singleStructure() const
    {
        ASSERT(isThin());
        return static_cast<Structure*>(pointer());
    }
    
    OutOfLineList* structureList() const
    {
        ASSERT(!isThin());
        return static_cast<OutOfLineList*>(pointer());
    }
    
    void set(Structure* structure)
    {
        set(bitwise_cast<uintptr_t>(structure), true);
    }
    void set(OutOfLineList* structures)
    {
        set(bitwise_cast<uintptr_t>(structures), false);
    }
    void setEmpty()
    {
        set(0, true);
    }
    void set(uintptr_t pointer, bool singleStructure)
    {
        m_pointer = pointer | (singleStructure ? thinFlag : 0) | (m_pointer & reservedFlag);
    }
    bool getReservedFlag() const { return m_pointer & reservedFlag; }
    void setReservedFlag(bool value)
    {
        if (value)
            m_pointer |= reservedFlag;
        else
            m_pointer &= ~reservedFlag;
    }

    uintptr_t m_pointer;
};

} // namespace JSC

#endif // StructureSet_h
