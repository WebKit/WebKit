/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "ArrayProfile.h"
#include "DFGAbstractValueClobberEpoch.h"
#include "DFGFiltrationResult.h"
#include "DFGFlushFormat.h"
#include "DFGFrozenValue.h"
#include "DFGNodeFlags.h"
#include "DFGStructureAbstractValue.h"
#include "DFGStructureClobberState.h"
#include "JSCast.h"
#include "ResultType.h"
#include "SpeculatedType.h"
#include "DumpContext.h"

namespace JSC {

class TrackedReferences;

namespace DFG {

class Graph;
struct Node;
class VariableAccessData;

struct AbstractValue {
    AbstractValue()
        : m_type(SpecNone)
        , m_arrayModes(0)
    {
#if USE(JSVALUE64) && !defined(NDEBUG)
        // The WTF Traits for AbstractValue allow the initialization of values with bzero().
        // We verify the correctness of this assumption here.
        static bool needsDefaultConstructorCheck = true;
        if (needsDefaultConstructorCheck) {
            needsDefaultConstructorCheck = false;
            ensureCanInitializeWithZeros();
        }
#endif
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
    
    ALWAYS_INLINE void fastForwardFromTo(AbstractValueClobberEpoch oldEpoch, AbstractValueClobberEpoch newEpoch)
    {
        if (newEpoch == oldEpoch)
            return;
        
        if (!(m_type & SpecCell))
            return;

        if (newEpoch.clobberEpoch() != oldEpoch.clobberEpoch())
            clobberStructures();
        if (newEpoch.structureClobberState() == StructuresAreWatched)
            m_structure.observeInvalidationPoint();

        checkConsistency();
    }
    
    ALWAYS_INLINE void fastForwardTo(AbstractValueClobberEpoch newEpoch)
    {
        if (newEpoch == m_effectEpoch)
            return;
        
        if (!(m_type & SpecCell)) {
            m_effectEpoch = newEpoch;
            return;
        }

        fastForwardToSlow(newEpoch);
    }
    
    void observeTransition(RegisteredStructure from, RegisteredStructure to)
    {
        if (m_type & SpecCell) {
            m_structure.observeTransition(from, to);
            observeIndexingTypeTransition(arrayModesFromStructure(from.get()), arrayModesFromStructure(to.get()));
        }
        checkConsistency();
    }
    
    void observeTransitions(const TransitionVector& vector);
    
    class TransitionObserver {
    public:
        TransitionObserver(RegisteredStructure from, RegisteredStructure to)
            : m_from(from)
            , m_to(to)
        {
        }
        
        void operator()(AbstractValue& value)
        {
            value.observeTransition(m_from, m_to);
        }
    private:
        RegisteredStructure m_from;
        RegisteredStructure m_to;
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

    bool isBytecodeTop() const
    {
        return (m_type | SpecBytecodeTop) == m_type
            && m_structure.isTop()
            && m_arrayModes == ALL_ARRAY_MODES
            && !m_value;
    }
    
    bool valueIsTop() const
    {
        return !m_value && m_type;
    }

    bool isInt52Any() const
    {
        return !(m_type & ~SpecInt52Any);
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
    
    void set(Graph&, const AbstractValue& other)
    {
        *this = other;
    }
    
    void set(Graph&, AbstractValue&& other)
    {
        *this = WTFMove(other);
    }
    
    void set(Graph&, const FrozenValue&, StructureClobberState);
    void set(Graph&, Structure*);
    void set(Graph&, RegisteredStructure);
    void set(Graph&, const RegisteredStructureSet&);
    
    // Set this value to represent the given set of types as precisely as possible.
    void setType(Graph&, SpeculatedType);
    
    // As above, but only valid for non-cell types.
    ALWAYS_INLINE void setNonCellType(SpeculatedType type)
    {
        RELEASE_ASSERT(!(type & SpecCell));
        m_structure.clear();
        m_arrayModes = 0;
        m_type = type;
        m_value = JSValue();
        checkConsistency();
    }

    void fixTypeForRepresentation(Graph&, NodeFlags representation, Node* = nullptr);
    void fixTypeForRepresentation(Graph&, Node*);
    
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
    
    ALWAYS_INLINE bool merge(const AbstractValue& other)
    {
        if (other.isClear())
            return false;
        
#if ASSERT_ENABLED
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
    
    bool mergeOSREntryValue(Graph&, JSValue, VariableAccessData*, Node*);
    
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

    // Filters the value using the given structure set. If the admittedTypes argument is not passed, this
    // implicitly filters by the types implied by the structure set, which are usually a subset of
    // SpecCell. Hence, after this call, the value will no longer have any non-cell members. But, you can
    // use admittedTypes to preserve some non-cell types. Note that it's wrong for admittedTypes to overlap
    // with SpecCell.
    FiltrationResult filter(Graph&, const RegisteredStructureSet&, SpeculatedType admittedTypes = SpecNone);
    
    FiltrationResult filterArrayModes(ArrayModes, SpeculatedType admittedTypes = SpecNone);

    ALWAYS_INLINE FiltrationResult filter(SpeculatedType type)
    {
        if ((m_type & type) == m_type)
            return FiltrationOK;
    
        // Fast path for the case that we don't even have a cell.
        if (!(m_type & SpecCell)) {
            m_type &= type;
            FiltrationResult result;
            if (m_type == SpecNone) {
                clear();
                result = Contradiction;
            } else
                result = FiltrationOK;
            checkConsistency();
            return result;
        }
        
        return filterSlow(type);
    }
    
    FiltrationResult filterByValue(const FrozenValue& value);
    FiltrationResult filter(const AbstractValue&);
    FiltrationResult filterClassInfo(Graph&, const ClassInfo*);

    ALWAYS_INLINE FiltrationResult fastForwardToAndFilterUnproven(AbstractValueClobberEpoch newEpoch, SpeculatedType type)
    {
        if (m_type & SpecCell)
            return fastForwardToAndFilterSlow(newEpoch, type);
        
        m_effectEpoch = newEpoch;
        m_type &= type;
        FiltrationResult result;
        if (m_type == SpecNone) {
            clear();
            result = Contradiction;
        } else
            result = FiltrationOK;
        checkConsistency();
        return result;
    }

    FiltrationResult changeStructure(Graph&, const RegisteredStructureSet&);
    
    bool contains(RegisteredStructure) const;

    bool validateOSREntryValue(JSValue value, FlushFormat format) const
    {
        if (isBytecodeTop())
            return true;
        
        if (format == FlushedInt52) {
            if (!isInt52Any())
                return false;

            if (!validateTypeAcceptingBoxedInt52(value))
                return false;

            if (!!m_value) {
                ASSERT(m_value.isAnyInt());
                ASSERT(value.isAnyInt());
                if (jsDoubleNumber(m_value.asAnyInt()) != jsDoubleNumber(value.asAnyInt()))
                    return false;
            }
        } else {
            if (!!m_value && m_value != value)
                return false;
        
            if (mergeSpeculations(m_type, speculationFromValue(value)) != m_type)
                return false;
            
            if (value.isEmpty()) {
                ASSERT(m_type & SpecEmpty);
                return true;
            }
        }
        
        if (!!value && value.isCell()) {
            ASSERT(m_type & SpecCell);
            Structure* structure = value.asCell()->structure();
            return m_structure.contains(structure)
                && (m_arrayModes & arrayModesFromStructure(structure));
        }
        
        return true;
    }
    
    bool hasClobberableState() const
    {
        return m_structure.isNeitherClearNorTop()
            || !arrayModesAreClearOrTop(m_arrayModes);
    }
    
#if ASSERT_ENABLED
    JS_EXPORT_PRIVATE void checkConsistency() const;
    void assertIsRegistered(Graph&) const;
#else
    void checkConsistency() const { }
    void assertIsRegistered(Graph&) const { }
#endif

    ResultType resultType() const;

    void dumpInContext(PrintStream&, DumpContext*) const;
    void dump(PrintStream&) const;
    
    void validateReferences(const TrackedReferences&);
    
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
    // impervious to side-effects. The relationship between this field, and the
    // structure fields above, is as follows. The fields above constraint the
    // structures that a cell may have, but they say nothing about whether or not
    // the value is known to be a cell. More formally, the m_structure is itself an
    // abstract value that consists of the union of the set of all non-cell values
    // and the set of cell values that have the given structure. This abstract
    // value is then the intersection of the m_structure and the set of values
    // whose type is m_type. So, for example if m_type is SpecFinal|SpecInt32Only and
    // m_structure is [0x12345] then this abstract value corresponds to the set of
    // all integers unified with the set of all objects with structure 0x12345.
    SpeculatedType m_type;
    
    // This is a proven constraint on the possible indexing types that this value
    // can have right now. It also implicitly constraints the set of structures
    // that the value may have right now, since a structure has an immutable
    // indexing type. This is subject to change upon reassignment, or any side
    // effect that makes non-obvious changes to the heap.
    ArrayModes m_arrayModes;
    
    // The effect epoch is usually ignored. This field is used by InPlaceAbstractState.
    //
    // InPlaceAbstractState needs to be able to clobberStructures() for all values it tracks. That
    // could be a lot of values. So, it makes this operation O(1) by bumping its effect epoch and
    // calling AbstractValue::fastForwardTo() anytime it vends someone an AbstractValue, which lazily
    // does clobberStructures(). The epoch type used here (AbstractValueClobberEpoch) is a bit more
    // complex than the normal Epoch, because it knows how to track clobberStructures() and
    // observeInvalidationPoint() precisely using integer math.
    //
    // One reason why it's here is to steal the 32-bit hole between m_arrayModes and m_value on
    // 64-bit systems.
    AbstractValueClobberEpoch m_effectEpoch;
    
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
    
    void observeIndexingTypeTransition(ArrayModes from, ArrayModes to)
    {
        if (m_arrayModes & from)
            m_arrayModes |= to;
    }
    
    bool validateTypeAcceptingBoxedInt52(JSValue value) const
    {
        if (isBytecodeTop())
            return true;
        
        if (m_type & SpecInt52Any) {
            if (mergeSpeculations(m_type, int52AwareSpeculationFromValue(value)) == m_type)
                return true;
        }

        if (mergeSpeculations(m_type, speculationFromValue(value)) != m_type)
            return false;
        
        return true;
    }
    
    void makeTop(SpeculatedType top)
    {
        m_type = top;
        m_arrayModes = ALL_ARRAY_MODES;
        m_structure.makeTop();
        m_value = JSValue();
        checkConsistency();
    }
    
    void fastForwardToSlow(AbstractValueClobberEpoch);
    FiltrationResult filterSlow(SpeculatedType);
    FiltrationResult fastForwardToAndFilterSlow(AbstractValueClobberEpoch, SpeculatedType);
    
    void filterValueByType();
    void filterArrayModesByType();

#if USE(JSVALUE64) && !defined(NDEBUG)
    JS_EXPORT_PRIVATE void ensureCanInitializeWithZeros();
#endif
    
    bool shouldBeClear() const;
    FiltrationResult normalizeClarity();
    FiltrationResult normalizeClarity(Graph&);
};

} } // namespace JSC::DFG

#if USE(JSVALUE64)
namespace WTF {
template <>
struct VectorTraits<JSC::DFG::AbstractValue> : VectorTraitsBase<false, JSC::DFG::AbstractValue> {
    static constexpr bool canInitializeWithMemset = true;
};

template <>
struct HashTraits<JSC::DFG::AbstractValue> : GenericHashTraits<JSC::DFG::AbstractValue> {
    static constexpr bool emptyValueIsZero = true;
};
};
#endif // USE(JSVALUE64)

#endif // ENABLE(DFG_JIT)
