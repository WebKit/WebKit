/*
 * Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGAbstractValue_h
#define DFGAbstractValue_h

#if ENABLE(DFG_JIT)

#include "ArrayProfile.h"
#include "DFGFiltrationResult.h"
#include "DFGFrozenValue.h"
#include "DFGNodeFlags.h"
#include "DFGStructureAbstractValue.h"
#include "DFGStructureClobberState.h"
#include "JSCell.h"
#include "SpeculatedType.h"
#include "DumpContext.h"
#include "StructureSet.h"

namespace JSC { namespace DFG {

class Graph;
struct Node;

struct AbstractValue {
    AbstractValue()
        : m_type(SpecNone)
        , m_arrayModes(0)
    {
    }
    
    void clear()
    {
        m_type = SpecNone;
        m_arrayModes = 0;
        m_structure.clear();
        m_value = JSValue();
        checkConsistency();
    }
    
    bool isClear() const { return m_type == SpecNone; }
    bool operator!() const { return isClear(); }
    
    void makeHeapTop()
    {
        makeTop(SpecHeapTop);
    }
    
    void makeBytecodeTop()
    {
        makeTop(SpecBytecodeTop);
    }
    
    void makeFullTop()
    {
        makeTop(SpecFullTop);
    }
    
    void clobberStructures()
    {
        if (m_type & SpecCell) {
            m_structure.clobber();
            clobberArrayModes();
        } else {
            ASSERT(m_structure.isClear());
            ASSERT(!m_arrayModes);
        }
        checkConsistency();
    }
    
    static void clobberStructuresFor(AbstractValue& value)
    {
        value.clobberStructures();
    }
    
    void observeInvalidationPoint()
    {
        m_structure.observeInvalidationPoint();
        checkConsistency();
    }
    
    static void observeInvalidationPointFor(AbstractValue& value)
    {
        value.observeInvalidationPoint();
    }
    
    void observeTransition(Structure* from, Structure* to)
    {
        if (m_type & SpecCell) {
            m_structure.observeTransition(from, to);
            observeIndexingTypeTransition(from->indexingType(), to->indexingType());
        }
        checkConsistency();
    }
    
    void observeTransitions(const TransitionVector& vector);
    
    class TransitionObserver {
    public:
        TransitionObserver(Structure* from, Structure* to)
            : m_from(from)
            , m_to(to)
        {
        }
        
        void operator()(AbstractValue& value)
        {
            value.observeTransition(m_from, m_to);
        }
    private:
        Structure* m_from;
        Structure* m_to;
    };
    
    class TransitionsObserver {
    public:
        TransitionsObserver(const TransitionVector& vector)
            : m_vector(vector)
        {
        }
        
        void operator()(AbstractValue& value)
        {
            value.observeTransitions(m_vector);
        }
    private:
        const TransitionVector& m_vector;
    };
    
    void clobberValue()
    {
        m_value = JSValue();
    }
    
    bool isHeapTop() const
    {
        return (m_type | SpecHeapTop) == m_type
            && m_structure.isTop()
            && m_arrayModes == ALL_ARRAY_MODES
            && !m_value;
    }
    
    bool valueIsTop() const
    {
        return !m_value && m_type;
    }
    
    JSValue value() const
    {
        return m_value;
    }
    
    static AbstractValue heapTop()
    {
        AbstractValue result;
        result.makeHeapTop();
        return result;
    }
    
    static AbstractValue bytecodeTop()
    {
        AbstractValue result;
        result.makeBytecodeTop();
        return result;
    }
    
    static AbstractValue fullTop()
    {
        AbstractValue result;
        result.makeFullTop();
        return result;
    }
    
    void setOSREntryValue(Graph&, const FrozenValue&);
    
    void set(Graph&, const FrozenValue&, StructureClobberState);
    void set(Graph&, Structure*);
    void set(Graph&, const StructureSet&);
    
    void setType(SpeculatedType type)
    {
        if (type & SpecCell) {
            m_structure.makeTop();
            m_arrayModes = ALL_ARRAY_MODES;
        } else {
            m_structure.clear();
            m_arrayModes = 0;
        }
        m_type = type;
        m_value = JSValue();
        checkConsistency();
    }
    
    void fixTypeForRepresentation(NodeFlags representation);
    void fixTypeForRepresentation(Node*);
    
    bool operator==(const AbstractValue& other) const
    {
        return m_type == other.m_type
            && m_arrayModes == other.m_arrayModes
            && m_structure == other.m_structure
            && m_value == other.m_value;
    }
    bool operator!=(const AbstractValue& other) const
    {
        return !(*this == other);
    }
    
    bool merge(const AbstractValue& other)
    {
        if (other.isClear())
            return false;
        
#if !ASSERT_DISABLED
        AbstractValue oldMe = *this;
#endif
        bool result = false;
        if (isClear()) {
            *this = other;
            result = !other.isClear();
        } else {
            result |= mergeSpeculation(m_type, other.m_type);
            result |= mergeArrayModes(m_arrayModes, other.m_arrayModes);
            result |= m_structure.merge(other.m_structure);
            if (m_value != other.m_value) {
                result |= !!m_value;
                m_value = JSValue();
            }
        }
        checkConsistency();
        ASSERT(result == (*this != oldMe));
        return result;
    }
    
    void merge(SpeculatedType type)
    {
        mergeSpeculation(m_type, type);
        
        if (type & SpecCell) {
            m_structure.makeTop();
            m_arrayModes = ALL_ARRAY_MODES;
        }
        m_value = JSValue();

        checkConsistency();
    }
    
    bool couldBeType(SpeculatedType desiredType) const
    {
        return !!(m_type & desiredType);
    }
    
    bool isType(SpeculatedType desiredType) const
    {
        return !(m_type & ~desiredType);
    }
    
    FiltrationResult filter(Graph&, const StructureSet&);
    FiltrationResult filterArrayModes(ArrayModes);
    FiltrationResult filter(SpeculatedType);
    FiltrationResult filterByValue(const FrozenValue& value);
    FiltrationResult filter(const AbstractValue&);
    
    FiltrationResult changeStructure(Graph&, const StructureSet&);
    
    bool contains(Structure*) const;
    
    bool validate(JSValue value) const
    {
        if (isHeapTop())
            return true;
        
        if (!!m_value && m_value != value)
            return false;
        
        if (mergeSpeculations(m_type, speculationFromValue(value)) != m_type)
            return false;
        
        if (value.isEmpty()) {
            ASSERT(m_type & SpecEmpty);
            return true;
        }
        
        if (!!value && value.isCell()) {
            ASSERT(m_type & SpecCell);
            Structure* structure = value.asCell()->structure();
            return m_structure.contains(structure)
                && (m_arrayModes & asArrayModes(structure->indexingType()));
        }
        
        return true;
    }
    
    bool hasClobberableState() const
    {
        return m_structure.isNeitherClearNorTop()
            || !arrayModesAreClearOrTop(m_arrayModes);
    }
    
#if ASSERT_DISABLED
    void checkConsistency() const { }
    void assertIsRegistered(Graph&) const { }
#else
    void checkConsistency() const;
    void assertIsRegistered(Graph&) const;
#endif
    
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dump(PrintStream&) const;
    
    // This is a proven constraint on the structures that this value can have right
    // now. The structure of the current value must belong to this set. The set may
    // be TOP, indicating that it is the set of all possible structures, in which
    // case the current value can have any structure. The set may be BOTTOM (empty)
    // in which case this value cannot be a cell. This is all subject to change
    // anytime a new value is assigned to this one, anytime there is a control flow
    // merge, or most crucially, anytime a side-effect or structure check happens.
    // In case of a side-effect, we must assume that any value with a structure that
    // isn't being watched may have had its structure changed, hence contravening
    // our proof. In such a case we make the proof valid again by switching this to
    // TOP (i.e. claiming that we have proved that this value may have any
    // structure).
    StructureAbstractValue m_structure;
    
    // This is a proven constraint on the possible types that this value can have
    // now or any time in the future, unless it is reassigned. This field is
    // impervious to side-effects unless the side-effect can reassign the value
    // (for example if we're talking about a captured variable). The relationship
    // between this field, and the structure fields above, is as follows. The
    // fields above constraint the structures that a cell may have, but they say
    // nothing about whether or not the value is known to be a cell. More formally,
    // the m_structure is itself an abstract value that consists of the
    // union of the set of all non-cell values and the set of cell values that have
    // the given structure. This abstract value is then the intersection of the
    // m_structure and the set of values whose type is m_type. So, for
    // example if m_type is SpecFinal|SpecInt32 and m_structure is
    // [0x12345] then this abstract value corresponds to the set of all integers
    // unified with the set of all objects with structure 0x12345.
    SpeculatedType m_type;
    
    // This is a proven constraint on the possible indexing types that this value
    // can have right now. It also implicitly constraints the set of structures
    // that the value may have right now, since a structure has an immutable
    // indexing type. This is subject to change upon reassignment, or any side
    // effect that makes non-obvious changes to the heap.
    ArrayModes m_arrayModes;
    
    // This is a proven constraint on the possible values that this value can
    // have now or any time in the future, unless it is reassigned. Note that this
    // implies nothing about the structure. Oddly, JSValue() (i.e. the empty value)
    // means either BOTTOM or TOP depending on the state of m_type: if m_type is
    // BOTTOM then JSValue() means BOTTOM; if m_type is not BOTTOM then JSValue()
    // means TOP. Also note that this value isn't necessarily known to the GC
    // (strongly or even weakly - it may be an "fragile" value, see
    // DFGValueStrength.h). If you perform any optimization based on a cell m_value
    // that requires that the value be kept alive, you must call freeze() on that
    // value, which will turn it into a weak value.
    JSValue m_value;

private:
    void clobberArrayModes()
    {
        // FIXME: We could make this try to predict the set of array modes that this object
        // could have in the future. For now, just do the simple thing.
        m_arrayModes = ALL_ARRAY_MODES;
    }
    
    void observeIndexingTypeTransition(IndexingType from, IndexingType to)
    {
        if (m_arrayModes & asArrayModes(from))
            m_arrayModes |= asArrayModes(to);
    }
    
    bool validateType(JSValue value) const
    {
        if (isHeapTop())
            return true;
        
        // Constant folding always represents Int52's in a double (i.e. Int52AsDouble).
        // So speculationFromValue(value) for an Int52 value will return Int52AsDouble,
        // and that's fine - the type validates just fine.
        SpeculatedType type = m_type;
        if (type & SpecInt52)
            type |= SpecInt52AsDouble;
        
        if (mergeSpeculations(type, speculationFromValue(value)) != type)
            return false;
        
        if (value.isEmpty()) {
            ASSERT(m_type & SpecEmpty);
            return true;
        }
        
        return true;
    }
    
    void makeTop(SpeculatedType top)
    {
        m_type |= top;
        m_arrayModes = ALL_ARRAY_MODES;
        m_structure.makeTop();
        m_value = JSValue();
        checkConsistency();
    }
    
    void filterValueByType();
    void filterArrayModesByType();
    
    bool shouldBeClear() const;
    FiltrationResult normalizeClarity();
    FiltrationResult normalizeClarity(Graph&);
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAbstractValue_h


