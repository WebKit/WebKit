/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "B3SparseConditionalConstantPropagation.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3CaseCollectionInlines.h"
#include "B3Const32Value.h"
#include "B3Const64Value.h"
#include "B3ConstDoubleValue.h"
#include "B3ConstFloatValue.h"
#include "B3InsertionSet.h"
#include "B3Opcode.h"
#include "B3PhaseScope.h"
#include "B3PhiChildren.h"
#include "B3Procedure.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include <wtf/BitVector.h>
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/IndexMap.h>
#include <wtf/IndexSparseSet.h>
#include <bit>
#include <cmath>
#include <limits>

namespace JSC::B3 {
namespace B3SparseConditionalConstantPropagationInternal {
static constexpr bool verbose = false;
}

// AbstractValue represents the abstract state of a B3 value in the SCCP lattice.
// Lattice: Bottom < Constant < NonZero < Top
//
// Bottom: Value has not been computed yet or is unreachable
// Constant: Value is a known compile-time constant (most precise)
// NonZero: Value is known to be non-zero but exact value unknown
// Top: Value can be multiple values at runtime (least precise)
class AbstractValue {
public:
    enum class Kind : uint8_t {
        Bottom,   // Not yet computed / unreachable
        Constant, // Known constant value
        NonZero,  // Known non-zero but value unknown
        Top       // Unknown / multiple possible values
    };

    AbstractValue() = default;

    static AbstractValue bottom() { return AbstractValue(); }

    static AbstractValue top()
    {
        AbstractValue result;
        result.m_kind = Kind::Top;
        return result;
    }

    static AbstractValue nonZero(Type type)
    {
        AbstractValue result;
        result.m_kind = Kind::NonZero;
        result.m_type = type;
        return result;
    }

    static AbstractValue fromInt32(int32_t value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Int32;
        result.m_int32 = value;
        return result;
    }

    static AbstractValue fromInt64(int64_t value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Int64;
        result.m_int64 = value;
        return result;
    }

    static AbstractValue fromFloat(float value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Float;
        result.m_float = value;
        return result;
    }

    static AbstractValue fromDouble(double value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Double;
        result.m_double = value;
        return result;
    }

    bool isBottom() const { return m_kind == Kind::Bottom; }
    bool isConstant() const { return m_kind == Kind::Constant; }
    bool isTop() const { return m_kind == Kind::Top; }

    // Returns true if value is known to be non-zero
    // Only handles integer types - float/double zero tracking is too complex
    bool isNonZero() const
    {
        if (m_kind == Kind::NonZero)
            return true;
        if (isConstant()) {
            switch (m_type.kind()) {
            case Int32: return m_int32 != 0;
            case Int64: return m_int64 != 0;
            default: return false;
            }
        }
        return false;
    }

    // Returns true if value is known to be zero
    // Only handles integer types - float/double zero tracking is too complex
    bool isZero() const
    {
        if (!isConstant())
            return false;
        switch (m_type.kind()) {
        case Int32: return m_int32 == 0;
        case Int64: return m_int64 == 0;
        default: return false;
        }
    }

    // Returns a copy with isNonZero constraint applied
    // Follows lattice ordering: Bottom < Constant < NonZero < Top
    AbstractValue withNonZero() const
    {
        // If already more specific than NonZero (Bottom or non-zero Constant), return as-is
        if (isBottom())
            return *this;
        if (isConstant()) {
            // If constant is non-zero, it's already more specific than NonZero
            if (isNonZero())
                return *this;
            // If constant is zero, NonZero constraint creates contradiction -> Bottom
            return AbstractValue::bottom();
        }
        // If Top, narrow to NonZero
        if (isTop()) {
            AbstractValue result = *this;
            result.m_kind = Kind::NonZero;
            return result;
        }
        // Already NonZero
        return *this;
    }

    // Compare two constant values for equality
    bool equals(const AbstractValue& other) const
    {
        if (!isConstant() || !other.isConstant())
            return false;
        if (m_type != other.m_type)
            return false;
        switch (m_type.kind()) {
        case Int32: return m_int32 == other.m_int32;
        case Int64: return m_int64 == other.m_int64;
        case Float: return std::bit_cast<uint32_t>(m_float) == std::bit_cast<uint32_t>(other.m_float);
        case Double: return std::bit_cast<uint64_t>(m_double) == std::bit_cast<uint64_t>(other.m_double);
        default: return false;
        }
    }

    // Compute the intersection (meet) of two abstract values
    // Returns the most specific value that satisfies both constraints
    // Lattice: Bottom < Constant < NonZero < Top
    AbstractValue meet(const AbstractValue& other) const
    {
        // Bottom meets anything = Bottom
        if (isBottom() || other.isBottom())
            return AbstractValue::bottom();

        // Constant meets Constant = same constant or Bottom
        if (isConstant() && other.isConstant()) {
            if (equals(other))
                return *this;
            return AbstractValue::bottom(); // Contradiction
        }

        // Constant meets anything else = Constant if compatible
        if (isConstant()) {
            // Check if constant satisfies other's constraints
            if (other.m_kind == Kind::NonZero && !isNonZero())
                return AbstractValue::bottom(); // Contradiction: constant is zero but other requires nonZero
            return *this; // Constant is most specific
        }
        if (other.isConstant()) {
            if (m_kind == Kind::NonZero && !other.isNonZero())
                return AbstractValue::bottom();
            return other;
        }

        // NonZero meets NonZero = NonZero
        if (m_kind == Kind::NonZero && other.m_kind == Kind::NonZero)
            return *this;

        // NonZero meets Top = NonZero (NonZero is more specific)
        if (m_kind == Kind::NonZero && other.isTop())
            return *this;
        if (isTop() && other.m_kind == Kind::NonZero)
            return other;

        // Top meets Top = Top
        return AbstractValue::top();
    }

    Type type() const { return m_type; }

    int32_t int32Value() const
    {
        ASSERT(isConstant() && m_type == Int32);
        return m_int32;
    }

    int64_t int64Value() const
    {
        ASSERT(isConstant() && m_type == Int64);
        return m_int64;
    }

    float floatValue() const
    {
        ASSERT(isConstant() && m_type == Float);
        return m_float;
    }

    double doubleValue() const
    {
        ASSERT(isConstant() && m_type == Double);
        return m_double;
    }

    // Merge another abstract value into this one (for phi nodes / join points).
    // This is the JOIN operation in the lattice (least upper bound).
    // Lattice: Bottom < Constant < NonZero < Top
    // Returns true if this value changed.
    bool merge(const AbstractValue& other)
    {
        // Merging with Bottom doesn't change anything
        if (other.isBottom())
            return false;

        // If we're Bottom, take the other value
        if (isBottom()) {
            *this = other;
            return true;
        }

        // If we're already Top, can't change
        if (isTop())
            return false;

        // If other is Top, we become Top
        if (other.isTop()) {
            m_kind = Kind::Top;
            return true;
        }

        // Both are Constant
        if (isConstant() && other.isConstant()) {
            // If types differ, go to Top
            if (m_type != other.m_type) {
                m_kind = Kind::Top;
                return true;
            }

            // Same type, check if values are equal
            bool valuesEqual = false;
            switch (m_type.kind()) {
            case Int32:
                valuesEqual = int32Value() == other.int32Value();
                break;
            case Int64:
                valuesEqual = int64Value() == other.int64Value();
                break;
            case Float:
                valuesEqual = std::bit_cast<uint32_t>(floatValue()) == std::bit_cast<uint32_t>(other.floatValue());
                break;
            case Double:
                valuesEqual = std::bit_cast<uint64_t>(doubleValue()) == std::bit_cast<uint64_t>(other.doubleValue());
                break;
            default:
                valuesEqual = false;
                break;
            }

            if (valuesEqual)
                return false;

            // Different constant values
            // If both are non-zero integers, join to NonZero
            // Otherwise join to Top
            bool bothNonZero = isNonZero() && other.isNonZero();
            if (bothNonZero)
                m_kind = Kind::NonZero;
            else
                m_kind = Kind::Top;
            return true;
        }

        // One is Constant, other is NonZero
        if (isConstant() && other.m_kind == Kind::NonZero) {
            // If constant is non-zero, join is NonZero
            // If constant is zero, join is Top
            if (isNonZero())
                m_kind = Kind::NonZero;
            else
                m_kind = Kind::Top;
            return true;
        }

        if (m_kind == Kind::NonZero && other.isConstant()) {
            // If other constant is non-zero, stay NonZero
            // If other constant is zero, become Top
            if (!other.isNonZero()) {
                m_kind = Kind::Top;
                return true;
            }
            return false; // Stay NonZero
        }

        // Both are NonZero
        if (m_kind == Kind::NonZero && other.m_kind == Kind::NonZero)
            return false;

        // NonZero and Top = Top
        if (m_kind == Kind::NonZero && other.isTop()) {
            m_kind = Kind::Top;
            return true;
        }

        // Should not reach here
        RELEASE_ASSERT_NOT_REACHED();
    }

    explicit operator bool() const
    {
        return !isBottom();
    }

private:
    Kind m_kind { Kind::Bottom };
    Type m_type { Void };
    union {
        int32_t m_int32;
        int64_t m_int64;
        float m_float;
        double m_double;
    };
};

// ValueFlowProjection - encodes Value* and whether it's Shadow (for Phi) or Primary
// (matching DFG's NodeFlowProjection)
class ValueFlowProjection {
public:
    enum Kind {
        Primary,
        Shadow
    };

    ValueFlowProjection() = default;

    ValueFlowProjection(Value* value)
        : m_word(std::bit_cast<uintptr_t>(value))
    {
        ASSERT(kind() == Primary);
    }

    ValueFlowProjection(Value* value, Kind kind)
        : m_word(std::bit_cast<uintptr_t>(value) | (kind == Shadow ? shadowBit : 0))
    {
        ASSERT(this->kind() == kind);
    }

    ValueFlowProjection(WTF::HashTableDeletedValueType)
        : m_word(shadowBit)
    {
    }

    explicit operator bool() const { return !!m_word; }

    Kind kind() const { return (m_word & shadowBit) ? Shadow : Primary; }

    Value* value() const { return std::bit_cast<Value*>(m_word & ~shadowBit); }

    Value& operator*() const { return *value(); }
    Value* operator->() const { return value(); }

    unsigned hash() const
    {
        return m_word;
    }

    friend bool operator==(const ValueFlowProjection&, const ValueFlowProjection&) = default;

    bool operator<(ValueFlowProjection other) const
    {
        if (kind() != other.kind())
            return kind() < other.kind();
        return value() < other.value();
    }

    bool operator>(ValueFlowProjection other) const
    {
        return other < *this;
    }

    bool operator<=(ValueFlowProjection other) const
    {
        return !(*this > other);
    }

    bool operator>=(ValueFlowProjection other) const
    {
        return !(*this < other);
    }

    bool isHashTableDeletedValue() const
    {
        return *this == ValueFlowProjection(WTF::HashTableDeletedValue);
    }

    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    // Phi shadow projections can become invalid because the Phi might be folded to something else.
    bool isStillValid() const
    {
        return *this && (kind() == Primary || value()->opcode() == Phi);
    }

private:
    static constexpr uintptr_t shadowBit = 1;
    uintptr_t m_word { 0 };
};

} // namespace JSC::B3

// WTF hash support for ValueFlowProjection (matching DFG's NodeFlowProjection)
namespace WTF {

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::B3::ValueFlowProjection> : SimpleClassHashTraits<JSC::B3::ValueFlowProjection> { };

} // namespace WTF

namespace JSC::B3 {

// Dense index mapping for ValueFlowProjections (similar to DFG's FlowIndexing)
// Enables O(1) operations with IndexSparseSet
// Primary projection: index = value->index() * 2
// Shadow projection: index = value->index() * 2 + 1
inline unsigned toFlowIndex(ValueFlowProjection projection)
{
    return projection.value()->index() * 2 + (projection.kind() == ValueFlowProjection::Shadow ? 1 : 0);
}

inline ValueFlowProjection fromFlowIndex(Procedure& proc, unsigned index)
{
    Value* value = proc.values()[index / 2];
    return ValueFlowProjection(value, (index & 1) ? ValueFlowProjection::Shadow : ValueFlowProjection::Primary);
}

// Pair of ValueFlowProjection and its AbstractValue (matching DFG's NodeAbstractValuePair)
struct ValueAbstractValuePair {
    ValueFlowProjection value;
    AbstractValue abstractValue;

    ValueAbstractValuePair() = default;
    ValueAbstractValuePair(ValueFlowProjection v, const AbstractValue& av)
        : value(v)
        , abstractValue(av)
    {
    }
};

// FlowMap for B3 (matching DFG's FlowMap)
// Maps Value indices to AbstractValues, with separate storage for Phi shadows
template<typename T>
class FlowMap {
public:
    FlowMap(Procedure& proc)
        : m_proc(proc)
        , m_map(proc.values().size())
        , m_shadowMap(proc.values().size())
    {
    }

    void resize()
    {
        m_map.resize(m_proc.values().size());
        m_shadowMap.resize(m_proc.values().size());
    }

    // Access using ValueFlowProjection
    T& at(ValueFlowProjection projection)
    {
        if (projection.kind() == ValueFlowProjection::Shadow)
            return m_shadowMap[projection.value()];
        return m_map[projection.value()];
    }

private:
    Procedure& m_proc;
    IndexMap<Value*, T> m_map;
    IndexMap<Value*, T> m_shadowMap;
};

// Per-block state for SCCP (matching DFG's BasicBlock::SSAData)
struct BlockState {
    Vector<ValueAbstractValuePair> valuesAtHead;
    Vector<ValueAbstractValuePair> valuesAtTail;
    bool shouldRevisit { false };
    bool hasVisited { false };
};

// BranchDirection indicates which edges of a branch are executable
enum class BranchDirection : uint8_t {
    TakeBoth,   // Both edges are executable (default)
    TakeTrue,   // Only the true edge is executable
    TakeFalse   // Only the false edge is executable
};

// EdgeNarrowing represents a narrowed value for a specific edge
// When a branch condition provides information about a value, we can
// narrow that value's abstract state for the successor block
struct EdgeNarrowing {
    Value* value { nullptr };
    AbstractValue narrowedValue;

    EdgeNarrowing() = default;
    EdgeNarrowing(Value* v, const AbstractValue& av)
        : value(v)
        , narrowedValue(av)
    {
    }
};

// Collection of narrowings to apply when merging into a successor
using EdgeNarrowings = Vector<EdgeNarrowing, 4>;

// SCCP pass implementation following DFG's AbstractInterpreter pattern
class SCCP {
public:
    SCCP(Procedure& proc)
        : m_proc(proc)
        , m_abstractValues(proc)
        , m_blockStates(proc.size())
        , m_insertionSet(proc)
        , m_phiChildren(proc)
    {
        // Constant values are never changed. If you can access it, then this means,
        // 1. you are dominated by this constant value
        // 2. constant value never relies on path.
        // Let's initialize global state of constant values first. This simplifies our liveness computation
        // since we do not need to track constant values.
        for (auto* value : proc.values()) {
            if (value->isConstant())
                m_abstractValues.at(value) = computeAbstractValue(value);
        }

        // Compute liveness and initialize block states (matching DFG's initialize())
        computeLiveness();
    }

    void computeLiveness()
    {
        // Backward dataflow liveness analysis tracking ValueFlowProjections
        // (matching DFG's LivenessAnalysisPhase exactly)
        // Live = projections that are used in this block or live in successors

        // After packIndices(), proc.values().size() is the actual count
        // We need size * 2 because we have Primary and Shadow projections
        IndexSparseSet<unsigned, DefaultIndexSparseSetTraits<unsigned>, UnsafeVectorOverflow> workset(m_proc.values().size() * 2);

        // Index-based liveAtHead used during fixpoint iteration (matching DFG's m_liveAtHead)
        // Converted to ValueFlowProjection after convergence
        IndexMap<BasicBlock*, Vector<unsigned, 0, UnsafeVectorOverflow, 1>> liveAtHeadIndicesSet(m_proc.size());
        IndexMap<BasicBlock*, UncheckedKeyHashSet<unsigned, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>> liveAtTailSet(m_proc.size());

        // BitVector for tracking dirty blocks (matching DFG line 55, 172)
        BitVector dirtyBlocks;
        dirtyBlocks.ensureSize(m_proc.size());

        // Start with all valid blocks dirty (matching DFG lines 67-72)
        for (unsigned i = 0; i < m_proc.size(); ++i) {
            if (m_proc[i])
                dirtyBlocks.quickSet(i);
        }

        // Fixpoint iteration (matching DFG lines 75-85)
        bool changed;
        do {
            changed = false;

            // Process blocks in reverse order (backward analysis)
            for (unsigned i = m_proc.size(); i--;) {
                // Only process dirty blocks (matching DFG lines 79-81)
                if (!dirtyBlocks.quickClear(i))
                    continue;

                BasicBlock* block = m_proc[i];
                if (!block)
                    continue;

                // Initialize workset from liveAtTailSet (matching DFG lines 114-116)
                workset.clear();
                for (unsigned index : liveAtTailSet[block])
                    workset.add(index);

                // Process values in reverse order (matching DFG lines 118-145)
                for (unsigned j = block->size(); j--;) {
                    Value* value = block->at(j);

                    switch (value->opcode()) {
                    case Upsilon: {
                        UpsilonValue* upsilon = value->as<UpsilonValue>();
                        Value* phi = upsilon->phi();
                        if (phi) {
                            workset.remove(toFlowIndex(ValueFlowProjection(phi, ValueFlowProjection::Shadow)));
                            Value* child = upsilon->child(0);
                            if (!child->isConstant())
                                workset.add(toFlowIndex(ValueFlowProjection(child)));
                        }
                        break;
                    }

                    case Phi: {
                        workset.remove(toFlowIndex(ValueFlowProjection(value, ValueFlowProjection::Primary)));
                        workset.add(toFlowIndex(ValueFlowProjection(value, ValueFlowProjection::Shadow)));
                        break;
                    }

                    default:
                        if (!value->isConstant())
                            workset.remove(toFlowIndex(ValueFlowProjection(value, ValueFlowProjection::Primary)));
                        for (Value* child : value->children()) {
                            if (!child->isConstant())
                                workset.add(toFlowIndex(ValueFlowProjection(child, ValueFlowProjection::Primary)));
                        }
                        break;
                    }
                }

                // Size comparison early exit (matching DFG lines 149-150)
                // If workset size equals liveAtHead size, nothing new was added
                auto& liveAtHeadIndices = liveAtHeadIndicesSet[block];
                if (workset.size() == liveAtHeadIndices.size())
                    continue; // No change, skip this block

                // Incremental update (matching DFG lines 152-156)
                // Remove already-live indices from workset, leaving only NEW live values
                for (unsigned liveIndex : liveAtHeadIndices)
                    workset.remove(liveIndex);
                ASSERT(!workset.isEmpty());

                // Append only the NEW live values to liveAtHead
                liveAtHeadIndices.appendRange(workset.begin(), workset.end());

                // Propagate new live values to predecessors (matching DFG lines 158-168)
                bool changedPredecessor = false;
                for (BasicBlock* predecessor : block->predecessors()) {
                    auto& liveAtTail = liveAtTailSet[predecessor];
                    for (unsigned newIndex : workset) {
                        if (liveAtTail.add(newIndex)) {
                            if (!dirtyBlocks.quickSet(predecessor->index()))
                                changedPredecessor = true;
                        }
                    }
                }
                changed |= changedPredecessor;
            }
        } while (changed);

        // Convert index-based representation to ValueFlowProjection (matching DFG lines 88-103)
        for (BasicBlock* block : m_proc) {
            if (!block)
                continue;

            BlockState& state = m_blockStates[block];
            state.valuesAtHead = liveAtHeadIndicesSet[block].map([&](auto index) {
                return ValueAbstractValuePair(fromFlowIndex(m_proc, index), { });
            });
            state.valuesAtTail = WTF::map(liveAtTailSet[block], [&](auto index) {
                return ValueAbstractValuePair(fromFlowIndex(m_proc, index), { });
            });
        }
    }

    bool run()
    {
        dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "B3 SCCP starting");

        // Add quadratic complexity guard (matching DFG's approach)
        // SCCP is an optimization, not correctness-critical - better to skip than hang
        size_t largestBlockSize = 0;
        for (BasicBlock* block : m_proc) {
            if (block)
                largestBlockSize = std::max(largestBlockSize, block->size());
        }

        // Conservative threshold for B3 (B3 has more values than DFG nodes typically)
        // If any block is too large, skip SCCP optimization
        constexpr size_t maxB3ValuesInBasicBlockForPreciseAnalysis = 5000;
        if (largestBlockSize > maxB3ValuesInBasicBlockForPreciseAnalysis) {
            dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose,
                "B3 SCCP: Skipping due to large block (size=", largestBlockSize,
                " exceeds threshold=", maxB3ValuesInBasicBlockForPreciseAnalysis, ")");
            return false;
        }

        // Initialize worklist with entry block
        m_worklist.append(m_proc[0]);
        m_blockStates[m_proc[0]].shouldRevisit = true;

        // Fixed-point iteration
        while (!m_worklist.isEmpty()) {
            BasicBlock* block = m_worklist.takeFirst();
            BlockState& blockState = m_blockStates[block];
            blockState.shouldRevisit = false;

            dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "Processing block ", *block);

            beginBasicBlock(block);

            for (Value* value : *block)
                executeValue(value);

            // endBasicBlock merges into successors and adds them to worklist if they changed
            endBasicBlock(block);
        }

        // Apply constant folding transformations
        bool result = applyOptimizations();

        dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "B3 SCCP completed");

        return result;
    }

private:
    void beginBasicBlock(BasicBlock* block)
    {
        m_block = block;
        BlockState& state = m_blockStates[block];
        state.hasVisited = true;

        // Load valuesAtHead into global state (matching DFG lines 78-84)
        for (ValueAbstractValuePair& entry : state.valuesAtHead) {
            if (entry.value.isStillValid())
                m_abstractValues.at(entry.value) = entry.abstractValue;
        }
    }

    void endBasicBlock(BasicBlock* block)
    {
        BlockState& state = m_blockStates[block];

        // Save current global state to valuesAtTail (matching DFG lines 307-315)
        for (ValueAbstractValuePair& entry : state.valuesAtTail)
            entry.abstractValue = m_abstractValues.at(entry.value);

        // DON'T clear global m_abstractValues!
        // It persists and will be read by merge()
        mergeToSuccessors(block);

        // Reset block-local state only (matching DFG's reset() at line 328-334)
        m_block = nullptr;
    }

    bool mergeToSuccessors(BasicBlock* block)
    {
        Value* terminal = block->last();
        if (!terminal)
            return false;

        bool changed = false;

        switch (terminal->opcode()) {
        case Jump:
        case Oops:
        case Return:
            // Unconditional: merge to all successors (no narrowings)
            for (BasicBlock* successor : block->successorBlocks())
                changed |= mergeIntoSuccessor(successor);
            break;

        case Branch: {
            // During fixpoint iteration, always visit both edges (conservative)
            // We'll optimize constant branches in applyOptimizations() after convergence
            EdgeNarrowings takenNarrowings;
            computeBranchNarrowings(terminal, true, takenNarrowings);
            changed |= mergeIntoSuccessor(block->taken().block(), takenNarrowings);

            EdgeNarrowings notTakenNarrowings;
            computeBranchNarrowings(terminal, false, notTakenNarrowings);
            changed |= mergeIntoSuccessor(block->notTaken().block(), notTakenNarrowings);
            break;
        }

        case Switch: {
            // During fixpoint iteration, always visit all edges (conservative)
            // We'll optimize constant switches in applyOptimizations() after convergence
            SwitchValue* switchValue = terminal->as<SwitchValue>();

            // Visit all cases with per-case narrowings
            for (SwitchCase switchCase : switchValue->cases(block)) {
                EdgeNarrowings narrowings;
                computeSwitchNarrowings(terminal->child(0), switchCase.caseValue(), narrowings);
                changed |= mergeIntoSuccessor(switchCase.targetBlock(), narrowings);
            }

            // Visit fallthrough (no specific narrowing)
            changed |= mergeIntoSuccessor(switchValue->fallThrough(block));
            break;
        }

        default:
            // Other control flow: conservatively merge to all successors (no narrowings)
            for (BasicBlock* successor : block->successorBlocks())
                changed |= mergeIntoSuccessor(successor);
            break;
        }

        return changed;
    }

    bool mergeIntoSuccessor(BasicBlock* to, const EdgeNarrowings& narrowings = EdgeNarrowings())
    {
        BlockState& toState = m_blockStates[to];

        bool changed = false;

        // Merge from global m_abstractValues into successor's valuesAtHead
        // (matching DFG lines 370-391)
        for (ValueAbstractValuePair& entry : toState.valuesAtHead) {
            // Read from global FlowMap (which has predecessor's values)
            AbstractValue fromValue = m_abstractValues.at(entry.value);

            // Apply narrowing if available (for the specific value)
            if (entry.value.kind() == ValueFlowProjection::Primary) {
                auto index = narrowings.findIf([&](auto& edge) { return edge.value == entry.value.value(); });
                if (index != notFound)
                    fromValue = narrowings[index].narrowedValue;
            }

            // Merge into successor's valuesAtHead
            if (entry.abstractValue.merge(fromValue))
                changed = true;
        }

        // Always visit each block at least once, even if it has no live values
        // or if values didn't change. This ensures control flow propagation.
        if (!toState.hasVisited)
            changed = true;  // Force visit on first reach

        if (changed && !toState.shouldRevisit) {
            toState.shouldRevisit = true;
            m_worklist.append(to);
        }

        return changed;
    }

    void executeValue(Value* value)
    {
        dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "  Executing ", *value);

        AbstractValue result = computeAbstractValue(value);

        if (value->opcode() == Upsilon) {
            // Special case: Upsilon updates the phi's shadow projection
            UpsilonValue* upsilon = value->as<UpsilonValue>();
            Value* phi = upsilon->phi();
            if (phi) {
                ValueFlowProjection shadow(phi, ValueFlowProjection::Shadow);
                if (shadow.isStillValid())
                    forValue(shadow) = result;
            }
        } else {
            forValue(value) = result;
        }

        dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "    Result: ", result.isBottom() ? "Bottom" : result.isConstant() ? "Constant" : result.isNonZero() ? "NonZero" : "Top");
    }

    AbstractValue computeAbstractValue(Value* value)
    {
        switch (value->opcode()) {
        case Const32:
            return AbstractValue::fromInt32(value->as<Const32Value>()->value());

        case Const64:
            return AbstractValue::fromInt64(value->as<Const64Value>()->value());

        case ConstFloat:
            return AbstractValue::fromFloat(value->as<ConstFloatValue>()->value());

        case ConstDouble:
            return AbstractValue::fromDouble(value->as<ConstDoubleValue>()->value());

        case Phi: {
            // Phi reads from its shadow projection
            AbstractValue result = forValue(ValueFlowProjection(value, ValueFlowProjection::Shadow));
            dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "  Phi ", *value, " -> ",
                       result.isBottom() ? "Bottom" : result.isConstant() ? "Constant" : result.isNonZero() ? "NonZero" : "Top");
            return result;
        }

        case Upsilon: {
            // Upsilon: return the child's value
            return forValue(value->child(0));
        }

        case Identity:
            return forValue(value->child(0));

        case Opaque:
            // Opaque hides the value from optimizations - treat as unknown
            return AbstractValue::top();

        // Sign extension
        case SExt8: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int32)
                return AbstractValue::fromInt32(static_cast<int8_t>(child.int32Value()));
            return AbstractValue::top();
        }

        case SExt16: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int32)
                return AbstractValue::fromInt32(static_cast<int16_t>(child.int32Value()));
            return AbstractValue::top();
        }

        case SExt32: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int32)
                return AbstractValue::fromInt64(static_cast<int64_t>(child.int32Value()));
            return AbstractValue::top();
        }

        // Zero extension
        case ZExt32: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int32)
                return AbstractValue::fromInt64(static_cast<uint32_t>(child.int32Value()));
            return AbstractValue::top();
        }

        // Truncation
        case Trunc: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int64)
                return AbstractValue::fromInt32(static_cast<int32_t>(child.int64Value()));
            return AbstractValue::top();
        }

        // Arithmetic operations
        case Add:
        case Sub:
        case Mul:
        case Div:
        case Mod:
            return computeBinaryArithmetic(value);

        // Bitwise operations
        case BitAnd:
        case BitOr:
        case BitXor:
        case Shl:
        case SShr:
        case ZShr:
            return computeBitwise(value);

        // Comparison operations
        case Equal:
        case NotEqual:
        case LessThan:
        case GreaterThan:
        case LessEqual:
        case GreaterEqual:
        case Above:
        case Below:
        case AboveEqual:
        case BelowEqual:
        case EqualOrUnordered:
            return computeComparison(value);

        // Bitwise cast (reinterpret bits)
        case BitwiseCast: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            Type resultType = value->type();
            if (child.type() == Int32 && resultType == Float)
                return AbstractValue::fromFloat(std::bit_cast<float>(child.int32Value()));
            if (child.type() == Int64 && resultType == Double)
                return AbstractValue::fromDouble(std::bit_cast<double>(child.int64Value()));
            if (child.type() == Float && resultType == Int32)
                return AbstractValue::fromInt32(std::bit_cast<int32_t>(child.floatValue()));
            if (child.type() == Double && resultType == Int64)
                return AbstractValue::fromInt64(std::bit_cast<int64_t>(child.doubleValue()));
            return AbstractValue::top();
        }

        // Integer to floating point
        case IToD: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int32)
                return AbstractValue::fromDouble(static_cast<double>(child.int32Value()));
            if (child.type() == Int64)
                return AbstractValue::fromDouble(static_cast<double>(child.int64Value()));
            return AbstractValue::top();
        }

        case IToF: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int32)
                return AbstractValue::fromFloat(static_cast<float>(child.int32Value()));
            if (child.type() == Int64)
                return AbstractValue::fromFloat(static_cast<float>(child.int64Value()));
            return AbstractValue::top();
        }

        // Float/Double conversions
        case FloatToDouble: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Float)
                return AbstractValue::fromDouble(static_cast<double>(child.floatValue()));
            return AbstractValue::top();
        }

        case DoubleToFloat: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Double)
                return AbstractValue::fromFloat(static_cast<float>(child.doubleValue()));
            return AbstractValue::top();
        }

        // Count leading zeros
        case Clz: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int32) {
                uint32_t val = static_cast<uint32_t>(child.int32Value());
                return AbstractValue::fromInt32(val == 0 ? 32 : std::countl_zero(val));
            }
            if (child.type() == Int64) {
                uint64_t val = static_cast<uint64_t>(child.int64Value());
                return AbstractValue::fromInt64(val == 0 ? 64 : std::countl_zero(val));
            }
            return AbstractValue::top();
        }

        // Negate (use unsigned to avoid signed overflow UB)
        case Neg: {
            AbstractValue child = forValue(value->child(0));
            if (child.isBottom()) return AbstractValue::bottom();
            if (!child.isConstant()) return AbstractValue::top();
            if (child.type() == Int32)
                return AbstractValue::fromInt32(static_cast<int32_t>(-static_cast<uint32_t>(child.int32Value())));
            if (child.type() == Int64)
                return AbstractValue::fromInt64(static_cast<int64_t>(-static_cast<uint64_t>(child.int64Value())));
            if (child.type() == Float)
                return AbstractValue::fromFloat(-child.floatValue());
            if (child.type() == Double)
                return AbstractValue::fromDouble(-child.doubleValue());
            return AbstractValue::top();
        }

        default:
            // Conservative: unknown operations produce Top
            return AbstractValue::top();
        }
    }

    AbstractValue& forValue(ValueFlowProjection projection)
    {
        return m_abstractValues.at(projection);
    }

    AbstractValue& forValue(Value* value)
    {
        // Always constructs Primary projection, matching DFG (see DFGNodeFlowProjection.h line 44-48)
        return forValue(ValueFlowProjection(value));
    }

    AbstractValue computeBinaryArithmetic(Value* value)
    {
        AbstractValue left = forValue(value->child(0));
        AbstractValue right = forValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        // If either operand is not a constant, return Top
        if (!left.isConstant() || !right.isConstant())
            return AbstractValue::top();

        // Both are constants
        ASSERT(left.isConstant() && right.isConstant());

        // Type must match for arithmetic
        if (left.type() != right.type())
            return AbstractValue::top();

        switch (value->opcode()) {
        case Add:
            // Use unsigned to avoid signed overflow UB (B3 Add wraps on overflow)
            if (left.type() == Int32)
                return AbstractValue::fromInt32(static_cast<int32_t>(static_cast<uint32_t>(left.int32Value()) + static_cast<uint32_t>(right.int32Value())));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(static_cast<int64_t>(static_cast<uint64_t>(left.int64Value()) + static_cast<uint64_t>(right.int64Value())));
            if (left.type() == Float)
                return AbstractValue::fromFloat(left.floatValue() + right.floatValue());
            if (left.type() == Double)
                return AbstractValue::fromDouble(left.doubleValue() + right.doubleValue());
            break;

        case Sub:
            // Use unsigned to avoid signed overflow UB (B3 Sub wraps on overflow)
            if (left.type() == Int32)
                return AbstractValue::fromInt32(static_cast<int32_t>(static_cast<uint32_t>(left.int32Value()) - static_cast<uint32_t>(right.int32Value())));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(static_cast<int64_t>(static_cast<uint64_t>(left.int64Value()) - static_cast<uint64_t>(right.int64Value())));
            if (left.type() == Float)
                return AbstractValue::fromFloat(left.floatValue() - right.floatValue());
            if (left.type() == Double)
                return AbstractValue::fromDouble(left.doubleValue() - right.doubleValue());
            break;

        case Mul:
            // Use unsigned to avoid signed overflow UB (B3 Mul wraps on overflow)
            if (left.type() == Int32)
                return AbstractValue::fromInt32(static_cast<int32_t>(static_cast<uint32_t>(left.int32Value()) * static_cast<uint32_t>(right.int32Value())));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(static_cast<int64_t>(static_cast<uint64_t>(left.int64Value()) * static_cast<uint64_t>(right.int64Value())));
            if (left.type() == Float)
                return AbstractValue::fromFloat(left.floatValue() * right.floatValue());
            if (left.type() == Double)
                return AbstractValue::fromDouble(left.doubleValue() * right.doubleValue());
            break;

        case Div:
            // B3's Div has chill semantics: division by zero returns 0
            if (left.type() == Int32) {
                if (right.int32Value() == 0)
                    return AbstractValue::fromInt32(0);
                // Handle INT_MIN / -1 overflow (chill semantics returns INT_MIN)
                if (left.int32Value() == std::numeric_limits<int32_t>::min() && right.int32Value() == -1)
                    return AbstractValue::fromInt32(std::numeric_limits<int32_t>::min());
                return AbstractValue::fromInt32(left.int32Value() / right.int32Value());
            }
            if (left.type() == Int64) {
                if (right.int64Value() == 0)
                    return AbstractValue::fromInt64(0);
                // Handle INT64_MIN / -1 overflow
                if (left.int64Value() == std::numeric_limits<int64_t>::min() && right.int64Value() == -1)
                    return AbstractValue::fromInt64(std::numeric_limits<int64_t>::min());
                return AbstractValue::fromInt64(left.int64Value() / right.int64Value());
            }
            if (left.type() == Float)
                return AbstractValue::fromFloat(left.floatValue() / right.floatValue());
            if (left.type() == Double)
                return AbstractValue::fromDouble(left.doubleValue() / right.doubleValue());
            break;

        case Mod:
            // B3's Mod has chill semantics: modulo by zero returns 0
            if (left.type() == Int32) {
                if (right.int32Value() == 0)
                    return AbstractValue::fromInt32(0);
                // Handle INT_MIN % -1 (would overflow, returns 0)
                if (left.int32Value() == std::numeric_limits<int32_t>::min() && right.int32Value() == -1)
                    return AbstractValue::fromInt32(0);
                return AbstractValue::fromInt32(left.int32Value() % right.int32Value());
            }
            if (left.type() == Int64) {
                if (right.int64Value() == 0)
                    return AbstractValue::fromInt64(0);
                // Handle INT64_MIN % -1
                if (left.int64Value() == std::numeric_limits<int64_t>::min() && right.int64Value() == -1)
                    return AbstractValue::fromInt64(0);
                return AbstractValue::fromInt64(left.int64Value() % right.int64Value());
            }
            if (left.type() == Float)
                return AbstractValue::fromFloat(fmodf(left.floatValue(), right.floatValue()));
            if (left.type() == Double)
                return AbstractValue::fromDouble(fmod(left.doubleValue(), right.doubleValue()));
            break;

        default:
            break;
        }

        // Conservative fallback
        return AbstractValue::top();
    }

    AbstractValue computeBitwise(Value* value)
    {
        AbstractValue left = forValue(value->child(0));
        AbstractValue right = forValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        // If either operand is not a constant, return Top
        if (!left.isConstant() || !right.isConstant())
            return AbstractValue::top();

        // Both are constants
        ASSERT(left.isConstant() && right.isConstant());

        if (left.type() != right.type())
            return AbstractValue::top();

        switch (value->opcode()) {
        case BitAnd:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() & right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() & right.int64Value());
            if (left.type() == Float) {
                uint32_t leftBits = std::bit_cast<uint32_t>(left.floatValue());
                uint32_t rightBits = std::bit_cast<uint32_t>(right.floatValue());
                return AbstractValue::fromFloat(std::bit_cast<float>(leftBits & rightBits));
            }
            if (left.type() == Double) {
                uint64_t leftBits = std::bit_cast<uint64_t>(left.doubleValue());
                uint64_t rightBits = std::bit_cast<uint64_t>(right.doubleValue());
                return AbstractValue::fromDouble(std::bit_cast<double>(leftBits & rightBits));
            }
            break;

        case BitOr:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() | right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() | right.int64Value());
            if (left.type() == Float) {
                uint32_t leftBits = std::bit_cast<uint32_t>(left.floatValue());
                uint32_t rightBits = std::bit_cast<uint32_t>(right.floatValue());
                return AbstractValue::fromFloat(std::bit_cast<float>(leftBits | rightBits));
            }
            if (left.type() == Double) {
                uint64_t leftBits = std::bit_cast<uint64_t>(left.doubleValue());
                uint64_t rightBits = std::bit_cast<uint64_t>(right.doubleValue());
                return AbstractValue::fromDouble(std::bit_cast<double>(leftBits | rightBits));
            }
            break;

        case BitXor:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() ^ right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() ^ right.int64Value());
            if (left.type() == Float) {
                uint32_t leftBits = std::bit_cast<uint32_t>(left.floatValue());
                uint32_t rightBits = std::bit_cast<uint32_t>(right.floatValue());
                return AbstractValue::fromFloat(std::bit_cast<float>(leftBits ^ rightBits));
            }
            if (left.type() == Double) {
                uint64_t leftBits = std::bit_cast<uint64_t>(left.doubleValue());
                uint64_t rightBits = std::bit_cast<uint64_t>(right.doubleValue());
                return AbstractValue::fromDouble(std::bit_cast<double>(leftBits ^ rightBits));
            }
            break;

        case Shl:
            // Use unsigned to avoid signed overflow UB (shifting negative values)
            if (left.type() == Int32)
                return AbstractValue::fromInt32(static_cast<int32_t>(static_cast<uint32_t>(left.int32Value()) << (right.int32Value() & 31)));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(static_cast<int64_t>(static_cast<uint64_t>(left.int64Value()) << (right.int64Value() & 63)));
            break;

        case SShr:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() >> (right.int32Value() & 31));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() >> (right.int64Value() & 63));
            break;

        case ZShr:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(static_cast<uint32_t>(left.int32Value()) >> (right.int32Value() & 31));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(static_cast<uint64_t>(left.int64Value()) >> (right.int64Value() & 63));
            break;

        default:
            break;
        }

        return AbstractValue::top();
    }

    AbstractValue computeComparison(Value* value)
    {
        AbstractValue left = forValue(value->child(0));
        AbstractValue right = forValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        if (left.type() != right.type())
            return AbstractValue::top();

        // NonZero-based folding for Equal/NotEqual with zero
        // Equal(nonzero, 0) or Equal(0, nonzero) → false (0)
        // NotEqual(nonzero, 0) or NotEqual(0, nonzero) → true (1)
        if (left.type() == Int32 || left.type() == Int64) {
            if (value->opcode() == Equal || value->opcode() == NotEqual) {
                bool leftIsNonZero = left.isNonZero();
                bool rightIsNonZero = right.isNonZero();
                bool leftIsZero = left.isZero();
                bool rightIsZero = right.isZero();

                if ((leftIsNonZero && rightIsZero) || (leftIsZero && rightIsNonZero)) {
                    bool result = (value->opcode() == NotEqual);
                    return AbstractValue::fromInt32(result ? 1 : 0);
                }
            }
        }

        // Only fold when both operands are constants
        if (!left.isConstant() || !right.isConstant())
            return AbstractValue::top();

        bool result = false;
        switch (value->opcode()) {
        case Equal:
            if (left.type() == Int32)
                result = left.int32Value() == right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() == right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() == right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() == right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case NotEqual:
            if (left.type() == Int32)
                result = left.int32Value() != right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() != right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() != right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() != right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case LessThan:
            if (left.type() == Int32)
                result = left.int32Value() < right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() < right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() < right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() < right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case GreaterThan:
            if (left.type() == Int32)
                result = left.int32Value() > right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() > right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() > right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() > right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case LessEqual:
            if (left.type() == Int32)
                result = left.int32Value() <= right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() <= right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() <= right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() <= right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case GreaterEqual:
            if (left.type() == Int32)
                result = left.int32Value() >= right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() >= right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() >= right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() >= right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case Above:
            if (left.type() == Int32)
                result = static_cast<uint32_t>(left.int32Value()) > static_cast<uint32_t>(right.int32Value());
            else if (left.type() == Int64)
                result = static_cast<uint64_t>(left.int64Value()) > static_cast<uint64_t>(right.int64Value());
            else
                return AbstractValue::top();
            break;

        case Below:
            if (left.type() == Int32)
                result = static_cast<uint32_t>(left.int32Value()) < static_cast<uint32_t>(right.int32Value());
            else if (left.type() == Int64)
                result = static_cast<uint64_t>(left.int64Value()) < static_cast<uint64_t>(right.int64Value());
            else
                return AbstractValue::top();
            break;

        case AboveEqual:
            if (left.type() == Int32)
                result = static_cast<uint32_t>(left.int32Value()) >= static_cast<uint32_t>(right.int32Value());
            else if (left.type() == Int64)
                result = static_cast<uint64_t>(left.int64Value()) >= static_cast<uint64_t>(right.int64Value());
            else
                return AbstractValue::top();
            break;

        case BelowEqual:
            if (left.type() == Int32)
                result = static_cast<uint32_t>(left.int32Value()) <= static_cast<uint32_t>(right.int32Value());
            else if (left.type() == Int64)
                result = static_cast<uint64_t>(left.int64Value()) <= static_cast<uint64_t>(right.int64Value());
            else
                return AbstractValue::top();
            break;

        case EqualOrUnordered:
            if (left.type() == Float)
                result = (left.floatValue() == right.floatValue()) || std::isnan(left.floatValue()) || std::isnan(right.floatValue());
            else if (left.type() == Double)
                result = (left.doubleValue() == right.doubleValue()) || std::isnan(left.doubleValue()) || std::isnan(right.doubleValue());
            else
                return AbstractValue::top();
            break;

        default:
            return AbstractValue::top();
        }

        // Comparisons return Int32 (boolean)
        return AbstractValue::fromInt32(result ? 1 : 0);
    }

    bool applyOptimizations()
    {
        bool changed = false;

        // Replace values with constants (following DFG's ConstantFoldingPhase pattern)
        for (BasicBlock* block : m_proc) {
            // Skip unreachable blocks
            BlockState& blockState = m_blockStates[block];
            if (!blockState.hasVisited)
                continue;

            // Load block's valuesAtHead into global m_abstractValues
            beginBasicBlock(block);

            bool inserted = false;
            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                Value* value = block->at(valueIndex);

                // Re-execute to compute the abstract value at this point
                executeValue(value);

                // Skip terminals and phis
                if (value->opcode() == Phi || value->opcode() == Upsilon)
                    continue;

                // Don't replace terminal control flow instructions (handle separately below)
                if (value->effects().terminal)
                    continue;

                // Don't replace values that already are constants
                if (value->isConstant())
                    continue;

                AbstractValue abstractValue = forValue(value);

                if (!abstractValue.isConstant())
                    continue;

                // IMPORTANT: Type must match between abstract value and the B3 value
                if (abstractValue.type() != value->type())
                    continue;

                auto effects = value->effects();
                // Don't replace operations that must execute (have side effects).
                // mustExecute() includes: terminal, exitsSideways, writesLocalState, writes, writesPinned, fence
                if (effects.mustExecute())
                    continue;

                dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "Replacing ", *value, " with constant");

                Value* replacement = nullptr;
                switch (abstractValue.type().kind()) {
                case Int32:
                    replacement = m_insertionSet.insertIntConstant(valueIndex, value, abstractValue.int32Value());
                    break;
                case Int64:
                    replacement = m_insertionSet.insertIntConstant(valueIndex, value, abstractValue.int64Value());
                    break;
                case Float: {
                    float floatValue = abstractValue.floatValue();
                    replacement = m_insertionSet.insertValue(valueIndex, m_proc.addConstant(value->origin(), Float, std::bit_cast<uint32_t>(floatValue)));
                    break;
                }
                case Double: {
                    double doubleValue = abstractValue.doubleValue();
                    replacement = m_insertionSet.insertValue(valueIndex, m_proc.addConstant(value->origin(), Double, std::bit_cast<uint64_t>(doubleValue)));
                    break;
                }
                default:
                    continue;
                }

                if (replacement) {
                    dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "Replacing ", *value, " with constant");
                    value->replaceWithIdentity(replacement);
                    changed = true;
                    inserted = true;
                }
            }

            // Handle terminal control flow optimization (branches and switches)
            if (block->size() > 0) {
                Value* terminal = block->last();

                switch (terminal->opcode()) {
                case Branch: {
                    // Replace constant branches with jumps - but skip if block has side effects
                    // (already checked above, so this block is safe to optimize)
                    BranchDirection direction = computeBranchDirection(terminal->child(0));

                    if (direction == BranchDirection::TakeTrue) {
                        dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "Replacing Branch with Jump to taken block");
                        terminal->replaceWithJump(block, block->taken());
                        changed = true;
                    } else if (direction == BranchDirection::TakeFalse) {
                        dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "Replacing Branch with Jump to notTaken block");
                        terminal->replaceWithJump(block, block->notTaken());
                        changed = true;
                    }
                    break;
                }

                case Switch: {
                    // Replace constant switches with jumps - but skip if block has side effects
                    // (already checked above, so this block is safe to optimize)
                    SwitchValue* switchValue = terminal->as<SwitchValue>();
                    AbstractValue discriminant = forValue(terminal->child(0));

                    if (discriminant.isConstant() && discriminant.type() == Int64) {
                        int64_t switchVal = discriminant.int64Value();
                        BasicBlock* targetBlock = nullptr;

                        for (SwitchCase switchCase : switchValue->cases(block)) {
                            if (switchCase.caseValue() == switchVal) {
                                targetBlock = switchCase.targetBlock();
                                break;
                            }
                        }

                        if (!targetBlock)
                            targetBlock = switchValue->fallThrough(block);

                        dataLogLnIf(B3SparseConditionalConstantPropagationInternal::verbose, "Replacing Switch with Jump to constant case");
                        terminal->replaceWithJump(block, FrequentedBlock(targetBlock));
                        changed = true;
                    }
                    break;
                }

                default:
                    break;
                }
            }

            // Reset block-local state
            m_block = nullptr;
            if (inserted)
                m_insertionSet.execute(block);
        }

        if (changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }

        return changed;
    }

    // Compute which edges of a branch are executable based on condition's abstract value
    BranchDirection computeBranchDirection(Value* condition)
    {
        AbstractValue condValue = forValue(condition);

        if (condValue.isBottom())
            return BranchDirection::TakeBoth; // Conservative

        if (condValue.isConstant()) {
            bool isTrue = false;
            if (condValue.type() == Int32)
                isTrue = condValue.int32Value() != 0;
            else if (condValue.type() == Int64)
                isTrue = condValue.int64Value() != 0;
            else
                isTrue = true; // Conservative for other types

            return isTrue ? BranchDirection::TakeTrue : BranchDirection::TakeFalse;
        }

        // NonZero or Top: conservatively visit both edges
        return BranchDirection::TakeBoth;
    }

    // Compute narrowings for a branch edge
    void computeBranchNarrowings(Value* terminal, bool takenEdge, EdgeNarrowings& narrowings)
    {
        Value* condition = terminal->child(0);

        // Only do narrowing for integer types - float/double zero tracking is too complex
        if (condition->type() != Int32 && condition->type() != Int64)
            return;

        // For the taken edge, condition is nonZero
        // For the notTaken edge, condition is zero
        if (takenEdge) {
            AbstractValue condValue = forValue(condition);
            if (!condValue.isNonZero()) {
                // Narrow to nonZero by intersecting with NonZero
                // Use meet() to properly handle the case where condValue is Constant(0)
                AbstractValue nonZeroConstraint = AbstractValue::nonZero(condition->type());
                AbstractValue narrowed = condValue.meet(nonZeroConstraint);
                // Only add narrowing if it's not Bottom (which would indicate unreachable code)
                if (!narrowed.isBottom())
                    narrowings.append(EdgeNarrowing(condition, narrowed));
            }
        } else {
            // notTaken edge: condition is zero
            AbstractValue zeroValue = (condition->type() == Int64) ? AbstractValue::fromInt64(0) : AbstractValue::fromInt32(0);
            narrowings.append(EdgeNarrowing(condition, zeroValue));
        }
        computeComparisonNarrowings(condition, takenEdge, narrowings);
    }

    // Compute narrowings based on comparison operations
    void computeComparisonNarrowings(Value* cond, bool taken, EdgeNarrowings& narrowings)
    {
        if (cond->numChildren() != 2)
            return;

        Value* a = cond->child(0);
        Value* b = cond->child(1);
        AbstractValue aVal = forValue(a);
        AbstractValue bVal = forValue(b);

        // Only narrow integer types - float/double zero tracking is too complex
        if (a->type() != Int32 && a->type() != Int64)
            return;
        if (b->type() != Int32 && b->type() != Int64)
            return;

        switch (cond->opcode()) {
        case Equal:
            if (taken) {
                // a == b is true: narrow both to intersection
                AbstractValue intersection = aVal.meet(bVal);
                if (!intersection.isBottom()) {
                    if (!aVal.equals(intersection))
                        narrowings.append(EdgeNarrowing(a, intersection));
                    if (!bVal.equals(intersection))
                        narrowings.append(EdgeNarrowing(b, intersection));
                }
            } else {
                // a == b is false: a != b
                // If one operand is known zero, the other is nonZero
                if (bVal.isZero() && !aVal.isNonZero()) {
                    AbstractValue nonZeroConstraint = AbstractValue::nonZero(a->type());
                    AbstractValue narrowed = aVal.meet(nonZeroConstraint);
                    if (!narrowed.isBottom())
                        narrowings.append(EdgeNarrowing(a, narrowed));
                }
                if (aVal.isZero() && !bVal.isNonZero()) {
                    AbstractValue nonZeroConstraint = AbstractValue::nonZero(b->type());
                    AbstractValue narrowed = bVal.meet(nonZeroConstraint);
                    if (!narrowed.isBottom())
                        narrowings.append(EdgeNarrowing(b, narrowed));
                }
            }
            break;

        case NotEqual:
            if (taken) {
                // a != b is true
                // If one operand is known zero, the other is nonZero
                if (bVal.isZero() && !aVal.isNonZero()) {
                    AbstractValue nonZeroConstraint = AbstractValue::nonZero(a->type());
                    AbstractValue narrowed = aVal.meet(nonZeroConstraint);
                    if (!narrowed.isBottom())
                        narrowings.append(EdgeNarrowing(a, narrowed));
                }
                if (aVal.isZero() && !bVal.isNonZero()) {
                    AbstractValue nonZeroConstraint = AbstractValue::nonZero(b->type());
                    AbstractValue narrowed = bVal.meet(nonZeroConstraint);
                    if (!narrowed.isBottom())
                        narrowings.append(EdgeNarrowing(b, narrowed));
                }
            } else {
                // a != b is false: a == b, narrow both to intersection
                AbstractValue intersection = aVal.meet(bVal);
                if (!intersection.isBottom()) {
                    if (!aVal.equals(intersection))
                        narrowings.append(EdgeNarrowing(a, intersection));
                    if (!bVal.equals(intersection))
                        narrowings.append(EdgeNarrowing(b, intersection));
                }
            }
            break;

        default:
            // Other comparisons: limited narrowing without range tracking
            break;
        }
    }

    // Compute narrowings for a switch case edge
    void computeSwitchNarrowings(Value* discriminant, int64_t caseValue, EdgeNarrowings& narrowings)
    {
        // On a switch case edge, discriminant is known to equal the case value
        AbstractValue narrowed = AbstractValue::fromInt64(caseValue);
        narrowings.append(EdgeNarrowing(discriminant, narrowed));
    }

    Procedure& m_proc;
    BasicBlock* m_block { nullptr };

    // Global FlowMap (matching DFG's m_abstractValues)
    FlowMap<AbstractValue> m_abstractValues;

    // Per-block state (matching DFG's BasicBlock::SSAData)
    IndexMap<BasicBlock*, BlockState> m_blockStates;

    // Worklist
    Deque<BasicBlock*> m_worklist;

    InsertionSet m_insertionSet;
    PhiChildren m_phiChildren;
};

bool sparseConditionalConstantPropagation(Procedure& proc)
{
    PhaseScope phaseScope(proc, "SparseConditionalConstantPropagation");

    // Pack value indices to make them dense (matching DFG's packNodeIndices)
    proc.values().packIndices();
    SCCP sccp(proc);
    return sccp.run();
}

} // namespace JSC::B3

#endif // ENABLE(B3_JIT)
