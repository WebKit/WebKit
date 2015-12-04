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
#include "B3InsertionSetInlines.h"
#include "B3MemoryValue.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
#include "B3ValueKeyInlines.h"

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
        // Eventually this phase will do smart things. For now, it uses a super simple heuristic: it
        // places constants in the block that uses them, and makes sure that each block has only one
        // materialization for each constant. Note that this mostly only matters for large constants, since
        // small constants get fused into the instructions that use them. But it might matter for small
        // constants if they are used in instructions that don't do immediates, like conditional moves.

        // FIXME: Implement a better story for constants. At a minimum this should allow the B3
        // client to specify important constants that always get hoisted. Also, the table used to
        // hold double constants should have a pointer to it that is hoisted. If we wanted to be more
        // aggressive, we could make constant materialization be a feature of Air: we could label
        // some Tmps as being unmaterialized constants and have a late Air phase - post register
        // allocation - that creates materializations of those constant Tmps by scavenging leftover
        // registers.

        // First we need to figure out which constants go into the data section. These are non-zero
        // double constants.
        for (Value* value : m_proc.values()) {
            if (!needsMotion(value))
                continue;
            m_toRemove.append(value);
            ValueKey key = value->key();
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
                    if (!needsMotion(child))
                        continue;

                    ValueKey key = child->key();
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

private:
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
        return key.opcode() == ConstDouble && key != doubleZero();
    }

    bool needsMotion(const Value* value)
    {
        return value->isConstant();
    }

    static ValueKey doubleZero()
    {
        return ValueKey(ConstDouble, Double, 0.0);
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

