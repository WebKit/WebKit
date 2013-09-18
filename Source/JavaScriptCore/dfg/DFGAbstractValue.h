/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "ArrayProfile.h"
#include "DFGFiltrationResult.h"
#include "DFGStructureAbstractValue.h"
#include "JSCell.h"
#include "SpeculatedType.h"
#include "DumpContext.h"
#include "StructureSet.h"

namespace JSC { namespace DFG {

class Graph;

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
        m_currentKnownStructure.clear();
        m_futurePossibleStructure.clear();
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
    
    void clobberStructures()
    {
        if (m_type & SpecCell) {
            m_currentKnownStructure.makeTop();
            clobberArrayModes();
        } else {
            ASSERT(m_currentKnownStructure.isClear());
            ASSERT(!m_arrayModes);
        }
        checkConsistency();
    }
        
    void clobberValue()
    {
        m_value = JSValue();
    }
    
    bool isHeapTop() const
    {
        return (m_type | SpecHeapTop) == m_type && m_currentKnownStructure.isTop() && m_futurePossibleStructure.isTop();
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
    
    void setMostSpecific(Graph&, JSValue);
    void set(Graph&, JSValue);
    void set(Graph&, Structure*);
    
    void setType(SpeculatedType type)
    {
        if (type & SpecCell) {
            m_currentKnownStructure.makeTop();
            m_futurePossibleStructure.makeTop();
            m_arrayModes = ALL_ARRAY_MODES;
        } else {
            m_currentKnownStructure.clear();
            m_futurePossibleStructure.clear();
            m_arrayModes = 0;
        }
        m_type = type;
        m_value = JSValue();
        checkConsistency();
    }
    
    bool operator==(const AbstractValue& other) const
    {
        return m_type == other.m_type
            && m_arrayModes == other.m_arrayModes
            && m_currentKnownStructure == other.m_currentKnownStructure
            && m_futurePossibleStructure == other.m_futurePossibleStructure
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
            result |= m_currentKnownStructure.addAll(other.m_currentKnownStructure);
            result |= m_futurePossibleStructure.addAll(other.m_futurePossibleStructure);
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
            m_currentKnownStructure.makeTop();
            m_futurePossibleStructure.makeTop();
            m_arrayModes = ALL_ARRAY_MODES;
        }
        m_value = JSValue();

        checkConsistency();
    }
    
    bool couldBeType(SpeculatedType desiredType)
    {
        return !!(m_type & desiredType);
    }
    
    bool isType(SpeculatedType desiredType)
    {
        return !(m_type & ~desiredType);
    }
    
    FiltrationResult filter(Graph&, const StructureSet&);
    
    FiltrationResult filterArrayModes(ArrayModes arrayModes);
    
    FiltrationResult filter(SpeculatedType type);
    
    FiltrationResult filterByValue(JSValue value);
    
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
            return m_currentKnownStructure.contains(structure)
                && m_futurePossibleStructure.contains(structure)
                && (m_arrayModes & asArrayModes(structure->indexingType()));
        }
        
        return true;
    }
    
    Structure* bestProvenStructure() const
    {
        if (m_currentKnownStructure.hasSingleton())
            return m_currentKnownStructure.singleton();
        if (m_futurePossibleStructure.hasSingleton())
            return m_futurePossibleStructure.singleton();
        return 0;
    }
    
    bool hasClobberableState() const
    {
        return m_currentKnownStructure.isNeitherClearNorTop()
            || !arrayModesAreClearOrTop(m_arrayModes);
    }
    
#if ASSERT_DISABLED
    void checkConsistency() const { }
#else
    void checkConsistency() const;
#endif
    
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dump(PrintStream&) const;
    
    // A great way to think about the difference between m_currentKnownStructure and
    // m_futurePossibleStructure is to consider these four examples:
    //
    // 1) x = foo();
    //
    //    In this case x's m_currentKnownStructure and m_futurePossibleStructure will
    //    both be TOP, since we don't know anything about x for sure, yet.
    //
    // 2) x = foo();
    //    y = x.f;
    //
    //    Where x will later have a new property added to it, 'g'. Because of the
    //    known but not-yet-executed property addition, x's current structure will
    //    not be watchpointable; hence we have no way of statically bounding the set
    //    of possible structures that x may have if a clobbering event happens. So,
    //    x's m_currentKnownStructure will be whatever structure we check to get
    //    property 'f', and m_futurePossibleStructure will be TOP.
    //
    // 3) x = foo();
    //    y = x.f;
    //
    //    Where x has a terminal structure that is still watchpointable. In this case,
    //    x's m_currentKnownStructure and m_futurePossibleStructure will both be
    //    whatever structure we checked for when getting 'f'.
    //
    // 4) x = foo();
    //    y = x.f;
    //    bar();
    //
    //    Where x has a terminal structure that is still watchpointable. In this
    //    case, m_currentKnownStructure will be TOP because bar() may potentially
    //    change x's structure and we have no way of proving otherwise, but
    //    x's m_futurePossibleStructure will be whatever structure we had checked
    //    when getting property 'f'.
    
    // NB. All fields in this struct must have trivial destructors.

    // This is a proven constraint on the structures that this value can have right
    // now. The structure of the current value must belong to this set. The set may
    // be TOP, indicating that it is the set of all possible structures, in which
    // case the current value can have any structure. The set may be BOTTOM (empty)
    // in which case this value cannot be a cell. This is all subject to change
    // anytime a new value is assigned to this one, anytime there is a control flow
    // merge, or most crucially, anytime a side-effect or structure check happens.
    // In case of a side-effect, we typically must assume that any value may have
    // had its structure changed, hence contravening our proof. We make the proof
    // valid again by switching this to TOP (i.e. claiming that we have proved that
    // this value may have any structure). Of note is that the proof represented by
    // this field is not subject to structure transition watchpoints - even if one
    // fires, we can be sure that this proof is still valid.
    StructureAbstractValue m_currentKnownStructure;
    
    // This is a proven constraint on the structures that this value can have now
    // or any time in the future subject to the structure transition watchpoints of
    // all members of this set not having fired. This set is impervious to side-
    // effects; even if one happens the side-effect can only cause the value to
    // change to at worst another structure that is also a member of this set. But,
    // the theorem being proved by this field is predicated upon there not being
    // any new structure transitions introduced into any members of this set. In
    // cases where there is no way for us to guard this happening, the set must be
    // TOP. But in cases where we can guard new structure transitions (all members
    // of the set have still-valid structure transition watchpoints) then this set
    // will be finite. Anytime that we make use of the finite nature of this set,
    // we must first issue a structure transition watchpoint, which will effectively
    // result in m_currentKnownStructure being filtered according to
    // m_futurePossibleStructure.
    StructureAbstractValue m_futurePossibleStructure;
    
    // This is a proven constraint on the possible types that this value can have
    // now or any time in the future, unless it is reassigned. This field is
    // impervious to side-effects unless the side-effect can reassign the value
    // (for example if we're talking about a captured variable). The relationship
    // between this field, and the structure fields above, is as follows. The
    // fields above constraint the structures that a cell may have, but they say
    // nothing about whether or not the value is known to be a cell. More formally,
    // the m_currentKnownStructure is itself an abstract value that consists of the
    // union of the set of all non-cell values and the set of cell values that have
    // the given structure. This abstract value is then the intersection of the
    // m_currentKnownStructure and the set of values whose type is m_type. So, for
    // example if m_type is SpecFinal|SpecInt32 and m_currentKnownStructure is
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
    // means TOP.
    JSValue m_value;

private:
    void clobberArrayModes()
    {
        // FIXME: We could make this try to predict the set of array modes that this object
        // could have in the future. For now, just do the simple thing.
        m_arrayModes = ALL_ARRAY_MODES;
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
        m_currentKnownStructure.makeTop();
        m_futurePossibleStructure.makeTop();
        m_value = JSValue();
        checkConsistency();
    }
    
    void setFuturePossibleStructure(Graph&, Structure* structure);

    void filterValueByType();
    void filterArrayModesByType();
    
    bool shouldBeClear() const;
    FiltrationResult normalizeClarity();
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAbstractValue_h


