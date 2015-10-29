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
#include "B3ReduceStrength.h"

#if ENABLE(B3_JIT)

#include "B3InsertionSetInlines.h"
#include "B3MemoryValue.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

namespace {

class ReduceStrength {
public:
    ReduceStrength(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
    {
    }

    bool run()
    {
        bool result = false;
        do {
            m_changed = false;

            for (BasicBlock* block : m_proc.blocksInPreOrder()) {
                m_block = block;
                
                for (m_index = 0; m_index < block->size(); ++m_index)
                    process();
                m_insertionSet.execute(m_block);

                // FIXME: This should also do DCE.
                block->removeNops();
            }
            
            result |= m_changed;
        } while (m_changed);
        return result;
    }
    
private:
    void process()
    {
        m_value = m_block->at(m_index);
        m_value->performSubstitution();
        
        switch (m_value->opcode()) {
        case Add:
            handleCommutativity();

            // Turn this: Add(Add(value, constant1), constant2)
            // Into this: Add(value, constant1 + constant2)
            if (m_value->child(0)->opcode() == Add && isInt(m_value->type())) {
                Value* newSum = m_value->child(1)->addConstant(m_proc, m_value->child(0)->child(1));
                if (newSum) {
                    m_insertionSet.insertValue(m_index, newSum);
                    m_value->child(0) = m_value->child(0)->child(0);
                    m_value->child(1) = newSum;
                    m_changed = true;
                }
            }

            // Turn this: Add(constant1, constant2)
            // Into this: constant1 + constant2
            replaceWithNewValue(m_value->child(0)->addConstant(m_proc, m_value->child(1)));
            break;

        case Sub:
            // Turn this: Sub(value, constant)
            // Into this: Add(value, -constant)
            if (isInt(m_value->type())) {
                if (Value* negatedConstant = m_value->child(1)->negConstant(m_proc)) {
                    m_insertionSet.insertValue(m_index, negatedConstant);
                    Value* add = m_insertionSet.insert<Value>(
                        m_index, Add, m_value->origin(), m_value->child(0), negatedConstant);
                    m_value->replaceWithIdentity(add);
                    m_changed = true;
                }
            }

            // Turn this: Sub(constant1, constant2)
            // Into this: constant1 - constant2
            replaceWithNewValue(m_value->child(0)->subConstant(m_proc, m_value->child(1)));
            break;

        case Load8Z:
        case Load8S:
        case Load16Z:
        case Load16S:
        case LoadFloat:
        case Load:
        case Store8:
        case Store16:
        case StoreFloat:
        case Store: {
            Value* address = m_value->children().last();
            MemoryValue* memory = m_value->as<MemoryValue>();

            // Turn this: Load(Add(address, offset1), offset = offset2)
            // Into this: Load(address, offset = offset1 + offset2)
            //
            // Also turns this: Store(value, Add(address, offset1), offset = offset2)
            // Into this: Store(value, address, offset = offset1 + offset2)
            if (address->opcode() == Add && address->child(1)->hasIntPtr()) {
                intptr_t offset = address->child(1)->asIntPtr();
                if (!sumOverflows<intptr_t>(offset, memory->offset())) {
                    offset += memory->offset();
                    int32_t smallOffset = static_cast<int32_t>(offset);
                    if (smallOffset == offset) {
                        address = address->child(0);
                        memory->children().last() = address;
                        memory->setOffset(smallOffset);
                        m_changed = true;
                    }
                }
            }

            // Turn this: Load(constant1, offset = constant2)
            // Into this: Laod(constant1 + constant2)
            //
            // This is a fun canonicalization. It purely regresses naively generated code. We rely
            // on constant materialization to be smart enough to materialize this constant the smart
            // way. We want this canonicalization because we want to know if two memory accesses see
            // the same address.
            if (memory->offset()) {
                if (Value* newAddress = address->addConstant(m_proc, memory->offset())) {
                    m_insertionSet.insertValue(m_index, newAddress);
                    address = newAddress;
                    memory->children().last() = newAddress;
                    memory->setOffset(0);
                    m_changed = true;
                }
            }
            
            break;
        }
            
        default:
            break;
        }
    }

    // Turn this: Add(constant, value)
    // Into this: Add(value, constant)
    //
    // Also:
    // Turn this: Add(value1, value2)
    // Into this: Add(value2, value1)
    // If we decide that value2 coming first is the canonical ordering.
    void handleCommutativity()
    {
        // Leave it alone if the right child is a constant.
        if (m_value->child(1)->isConstant())
            return;
        
        if (m_value->child(0)->isConstant()) {
            std::swap(m_value->child(0), m_value->child(1));
            m_changed = true;
            return;
        }

        // Sort the operands. This is an important canonicalization. We use the index instead of
        // the address to make this at least slightly deterministic.
        if (m_value->child(0)->index() > m_value->child(1)->index()) {
            std::swap(m_value->child(0), m_value->child(1));
            m_changed = true;
            return;
        }
    }

    void replaceWithNewValue(Value* newValue)
    {
        if (!newValue)
            return;
        m_insertionSet.insertValue(m_index, newValue);
        m_value->replaceWithIdentity(newValue);
        m_changed = true;
    }

    Procedure& m_proc;
    InsertionSet m_insertionSet;
    BasicBlock* m_block;
    unsigned m_index;
    Value* m_value;
    bool m_changed;
};

} // anonymous namespace

bool reduceStrength(Procedure& proc)
{
    PhaseScope phaseScope(proc, "reduceStrength");
    ReduceStrength reduceStrength(proc);
    return reduceStrength.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

