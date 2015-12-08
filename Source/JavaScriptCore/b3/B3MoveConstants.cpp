/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "B3MoveConstants.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3Dominators.h"
#include "B3InsertionSetInlines.h"
#include "B3MemoryValue.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
#include "B3ValueKeyInlines.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

namespace {

class MoveConstants {
public:
    MoveConstants(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
    {
    }

    void run()
    {
        // This phase uses super simple heuristics to ensure that constants are in profitable places
        // and also lowers constant materialization code. Constants marked "fast" by the client are
        // hoisted to the lowest common dominator. A table is created for constants that need to be
        // loaded to be materialized, and all of their Values are turned into loads from that table.
        // Non-fast constants get materialized in the block that uses them. Constants that are
        // materialized by loading get special treatment when they get used in some kind of Any in a
        // StackmapValue. In that case, the Constants are sunk to the point of use, since that allows
        // the instruction selector to sink the constants into an Arg::imm64.

        // FIXME: Implement a better story for constants. For example, the table used to hold double
        // constants should have a pointer to it that is hoisted. If we wanted to be more aggressive,
        // we could make constant materialization be a feature of Air: we could label some Tmps as
        // being unmaterialized constants and have a late Air phase - post register allocation - that
        // creates materializations of those constant Tmps by scavenging leftover registers.

        hoistFastConstants();
        sinkSlowConstants();
    }

private:
    void hoistFastConstants()
    {
        Dominators& dominators = m_proc.dominators();
        HashMap<ValueKey, Value*> valueForConstant;
        IndexMap<BasicBlock, Vector<Value*>> materializations(m_proc.size());

        // We determine where things get materialized based on where they are used.
        for (BasicBlock* block : m_proc) {
            for (Value* value : *block) {
                for (Value*& child : value->children()) {
                    ValueKey key = child->key();
                    if (!m_proc.isFastConstant(key))
                        continue;

                    auto result = valueForConstant.add(key, child);
                    if (result.isNewEntry) {
                        // Assume that this block is where we want to materialize the value.
                        child->owner = block;
                        continue;
                    }

                    // Make 'value' use the canonical constant rather than the one it was using.
                    child = result.iterator->value;

                    // Determine the least common dominator. That's the lowest place in the CFG where
                    // we could materialize the constant while still having only one materialization
                    // in the resulting code.
                    while (!dominators.dominates(child->owner, block))
                        child->owner = dominators.idom(child->owner);
                }
            }
        }

        // Make sure that each basic block knows what to materialize. This also refines the
        // materialization block based on execution frequency. It finds the minimum block frequency
        // of all of its dominators, and selects the closest block amongst those that are tied for
        // lowest frequency.
        for (auto& entry : valueForConstant) {
            Value* value = entry.value;
            for (BasicBlock* block = value->owner; block; block = dominators.idom(block)) {
                if (block->frequency() < value->owner->frequency())
                    value->owner = block;
            }
            materializations[entry.value->owner].append(entry.value);
        }

        // Get rid of Value's that are fast constants but aren't canonical. Also remove the canonical
        // ones from the CFG, since we're going to reinsert them elsewhere.
        for (BasicBlock* block : m_proc) {
            for (Value*& value : *block) {
                ValueKey key = value->key();
                if (!m_proc.isFastConstant(key))
                    continue;

                if (valueForConstant.get(key) == value)
                    value = m_proc.add<Value>(Nop, value->origin());
                else
                    value->replaceWithNop();
            }
        }

        // Now make sure that we move constants to where they are supposed to go. Again, we do this
        // based on uses.
        for (BasicBlock* block : m_proc) {
            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                Value* value = block->at(valueIndex);
                for (Value* child : value->children()) {
                    ValueKey key = child->key();
                    if (!m_proc.isFastConstant(key))
                        continue;

                    // If we encounter a fast constant, then it must be canonical, since we already
                    // got rid of the non-canonical ones.
                    ASSERT(valueForConstant.get(key) == child);

                    if (child->owner != block) {
                        // This constant isn't our problem. It's going to be materialized in another
                        // block.
                        continue;
                    }
                    
                    // We're supposed to materialize this constant in this block, and we haven't
                    // done it yet.
                    m_insertionSet.insertValue(valueIndex, child);
                    child->owner = nullptr;
                }
            }

            // We may have some constants that need to be materialized right at the end of this
            // block.
            for (Value* value : materializations[block]) {
                if (!value->owner) {
                    // It's already materialized in this block.
                    continue;
                }

                m_insertionSet.insertValue(block->size() - 1, value);
            }
            m_insertionSet.execute(block);
        }
    }
    
    void sinkSlowConstants()
    {
        // First we need to figure out which constants go into the data section. These are non-zero
        // double constants.
        for (Value* value : m_proc.values()) {
            ValueKey key = value->key();
            if (!needsMotion(key))
                continue;
            m_toRemove.append(value);
            if (goesInTable(key))
                m_constTable.add(key, m_constTable.size());
        }
        
        m_dataSection = static_cast<int64_t*>(m_proc.addDataSection(m_constTable.size() * sizeof(int64_t)));
        for (auto& entry : m_constTable)
            m_dataSection[entry.value] = entry.key.value();
        
        for (BasicBlock* block : m_proc) {
            m_constants.clear();
            
            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                Value* value = block->at(valueIndex);
                StackmapValue* stackmap = value->as<StackmapValue>();
                for (unsigned childIndex = 0; childIndex < value->numChildren(); ++childIndex) {
                    Value*& child = value->child(childIndex);
                    ValueKey key = child->key();
                    if (!needsMotion(key))
                        continue;

                    if (stackmap
                        && goesInTable(key)
                        && stackmap->constrainedChild(childIndex).rep().isAny()) {
                        // This is a weird special case. When we constant-fold an argument to a
                        // stackmap, and that argument has the Any constraint, we want to just
                        // tell the stackmap's generator that the argument is a constant rather
                        // than materializing it in a register. For this to work, we need
                        // lowerToAir to see this argument as a constant rather than as a load
                        // from a table.
                        child = m_insertionSet.insertValue(
                            valueIndex, key.materialize(m_proc, value->origin()));
                        continue;
                    }
                    
                    child = materialize(valueIndex, key, value->origin());
                }
            }
            
            m_insertionSet.execute(block);
        }

        for (Value* toRemove : m_toRemove)
            toRemove->replaceWithNop();
    }
    
    Value* materialize(unsigned valueIndex, const ValueKey& key, const Origin& origin)
    {
        if (Value* result = m_constants.get(key))
            return result;

        // Note that we deliberately don't do this in one add() because this is a recursive function
        // that may rehash the map.

        Value* result;
        if (goesInTable(key)) {
            Value* tableBase = materialize(
                valueIndex,
                ValueKey(
                    constPtrOpcode(), pointerType(),
                    static_cast<int64_t>(bitwise_cast<intptr_t>(m_dataSection))),
                origin);
            result = m_insertionSet.insert<MemoryValue>(
                valueIndex, Load, key.type(), origin, tableBase,
                sizeof(intptr_t) * m_constTable.get(key));
        } else
            result = m_insertionSet.insertValue(valueIndex, key.materialize(m_proc, origin));
        m_constants.add(key, result);
        return result;
    }

    bool goesInTable(const ValueKey& key)
    {
        return (key.opcode() == ConstDouble && key != doubleZero())
        || (key.opcode() == ConstFloat && key != floatZero());
    }

    bool needsMotion(const ValueKey& key)
    {
        return key.isConstant() && !m_proc.isFastConstant(key);
    }

    static ValueKey doubleZero()
    {
        return ValueKey(ConstDouble, Double, 0.0);
    }

    static ValueKey floatZero()
    {
        return ValueKey(ConstFloat, Double, 0.0);
    }

    Procedure& m_proc;
    Vector<Value*> m_toRemove;
    HashMap<ValueKey, unsigned> m_constTable;
    int64_t* m_dataSection;
    HashMap<ValueKey, Value*> m_constants;
    InsertionSet m_insertionSet;
};

} // anonymous namespace

void moveConstants(Procedure& proc)
{
    PhaseScope phaseScope(proc, "moveConstants");
    MoveConstants moveConstants(proc);
    moveConstants.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

