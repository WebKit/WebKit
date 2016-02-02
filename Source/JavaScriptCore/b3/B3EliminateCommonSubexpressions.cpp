/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "B3EliminateCommonSubexpressions.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3BlockWorklist.h"
#include "B3Dominators.h"
#include "B3HeapRange.h"
#include "B3InsertionSetInlines.h"
#include "B3MemoryValue.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3PureCSE.h"
#include "B3SlotBaseValue.h"
#include "B3StackSlot.h"
#include "B3ValueKey.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "DFGGraph.h"
#include <wtf/CommaPrinter.h>
#include <wtf/HashMap.h>
#include <wtf/ListDump.h>
#include <wtf/RangeSet.h>

namespace JSC { namespace B3 {

namespace {

const bool verbose = false;

// FIXME: We could treat Patchpoints with a non-empty set of reads as a "memory value" and somehow
// eliminate redundant ones. We would need some way of determining if two patchpoints are replacable.
// It doesn't seem right to use the reads set for this. We could use the generator, but that feels
// lame because the FTL will pretty much use a unique generator for each patchpoint even when two
// patchpoints have the same semantics as far as CSE would be concerned. We could invent something
// like a "value ID" for patchpoints. By default, each one gets a unique value ID, but FTL could force
// some patchpoints to share the same one as a signal that they will return the same value if executed
// in the same heap with the same inputs.

typedef Vector<MemoryValue*, 1> MemoryMatches;

struct ImpureBlockData {
    void dump(PrintStream& out) const
    {
        out.print("{writes = ", writes, ", memoryValues = {");

        CommaPrinter comma;
        for (auto& entry : memoryValues)
            out.print(comma, pointerDump(entry.key), "=>", pointerListDump(entry.value));

        out.print("}}");
    }
    
    RangeSet<HeapRange> writes;
    
    // Maps an address base to all of the MemoryValues that do things to it. After we're done
    // processing a map, this tells us the values at tail. Note that we use a Matches array
    // because those MemoryValues could be turned into Identity's, and we need to check for this
    // as we go.
    HashMap<Value*, Matches> memoryValues;
};

class CSE {
public:
    CSE(Procedure& proc)
        : m_proc(proc)
        , m_dominators(proc.dominators())
        , m_impureBlockData(proc.size())
        , m_insertionSet(proc)
    {
    }

    bool run()
    {
        if (verbose)
            dataLog("B3 before CSE:\n", m_proc);
        
        m_proc.resetValueOwners();

        for (BasicBlock* block : m_proc) {
            ImpureBlockData& data = m_impureBlockData[block];
            for (Value* value : *block) {
                if (HeapRange writes = value->effects().writes)
                    clobber(data, writes);

                if (MemoryValue* memory = value->as<MemoryValue>())
                    addMemoryValue(data, memory);
            }

            if (verbose)
                dataLog("Block ", *block, ": ", data, "\n");
        }

        Vector<BasicBlock*> postOrder = m_proc.blocksInPostOrder();
        for (unsigned i = postOrder.size(); i--;) {
            m_block = postOrder[i];
            if (verbose)
                dataLog("Looking at ", *m_block, ":\n");

            m_data = ImpureBlockData();
            for (m_index = 0; m_index < m_block->size(); ++m_index) {
                m_value = m_block->at(m_index);
                process();
            }
            m_insertionSet.execute(m_block);
            m_impureBlockData[m_block] = m_data;
        }

        for (BasicBlock* block : m_proc) {
            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                auto iter = m_sets.find(block->at(valueIndex));
                if (iter == m_sets.end())
                    continue;

                for (Value* value : iter->value)
                    m_insertionSet.insertValue(valueIndex + 1, value);
            }
            m_insertionSet.execute(block);
        }

        if (verbose)
            dataLog("B3 after CSE:\n", m_proc);

        return m_changed;
    }
    
private:
    void process()
    {
        m_value->performSubstitution();

        if (m_pureCSE.process(m_value, m_dominators)) {
            ASSERT(!m_value->effects().writes);
            m_changed = true;
            return;
        }

        if (HeapRange writes = m_value->effects().writes)
            clobber(m_data, writes);
        
        if (MemoryValue* memory = m_value->as<MemoryValue>())
            processMemory(memory);
    }

    void clobber(ImpureBlockData& data, HeapRange writes)
    {
        data.writes.add(writes);
        
        data.memoryValues.removeIf(
            [&] (HashMap<Value*, Matches>::KeyValuePairType& entry) -> bool {
                entry.value.removeAllMatching(
                    [&] (Value* value) -> bool {
                        if (MemoryValue* memory = value->as<MemoryValue>())
                            return memory->range().overlaps(writes);
                        return true;
                    });
                return entry.value.isEmpty();
            });
    }

    void processMemory(MemoryValue* memory)
    {
        Value* ptr = memory->lastChild();
        HeapRange range = memory->range();
        int32_t offset = memory->offset();
        Type type = memory->type();

        // FIXME: Empower this to insert more casts and shifts. For example, a Load8 could match a
        // Store and mask the result. You could even have:
        //
        // Store(@value, @ptr, offset = 0)
        // Load8Z(@ptr, offset = 2)
        //
        // Which could be turned into something like this:
        //
        // Store(@value, @ptr, offset = 0)
        // ZShr(@value, 16)
        
        switch (memory->opcode()) {
        case Load8Z: {
            handleMemoryValue(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && (candidate->opcode() == Load8Z || candidate->opcode() == Store8);
                },
                [&] (MemoryValue* match, Vector<Value*>& fixups) -> Value* {
                    if (match->opcode() == Store8) {
                        Value* mask = m_proc.add<Const32Value>(m_value->origin(), 0xff);
                        fixups.append(mask);
                        Value* zext = m_proc.add<Value>(
                            BitAnd, m_value->origin(), match->child(0), mask);
                        fixups.append(zext);
                        return zext;
                    }
                    return nullptr;
                });
            break;
        }

        case Load8S: {
            handleMemoryValue(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && (candidate->opcode() == Load8S || candidate->opcode() == Store8);
                },
                [&] (MemoryValue* match, Vector<Value*>& fixups) -> Value* {
                    if (match->opcode() == Store8) {
                        Value* sext = m_proc.add<Value>(
                            SExt8, m_value->origin(), match->child(0));
                        fixups.append(sext);
                        return sext;
                    }
                    return nullptr;
                });
            break;
        }

        case Load16Z: {
            handleMemoryValue(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && (candidate->opcode() == Load16Z || candidate->opcode() == Store16);
                },
                [&] (MemoryValue* match, Vector<Value*>& fixups) -> Value* {
                    if (match->opcode() == Store16) {
                        Value* mask = m_proc.add<Const32Value>(m_value->origin(), 0xffff);
                        fixups.append(mask);
                        Value* zext = m_proc.add<Value>(
                            BitAnd, m_value->origin(), match->child(0), mask);
                        fixups.append(zext);
                        return zext;
                    }
                    return nullptr;
                });
            break;
        }

        case Load16S: {
            handleMemoryValue(
                ptr, range, [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && (candidate->opcode() == Load16S || candidate->opcode() == Store16);
                },
                [&] (MemoryValue* match, Vector<Value*>& fixups) -> Value* {
                    if (match->opcode() == Store16) {
                        Value* sext = m_proc.add<Value>(
                            SExt16, m_value->origin(), match->child(0));
                        fixups.append(sext);
                        return sext;
                    }
                    return nullptr;
                });
            break;
        }

        case Load: {
            handleMemoryValue(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    if (candidate->offset() != offset)
                        return false;

                    if (candidate->opcode() == Load && candidate->type() == type)
                        return true;

                    if (candidate->opcode() == Store && candidate->child(0)->type() == type)
                        return true;

                    return false;
                });
            break;
        }

        case Store8:
        case Store16:
        case Store: {
            addMemoryValue(m_data, memory);
            break;
        }

        default:
            dataLog("Bad memory value: ", deepDump(m_proc, m_value), "\n");
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    template<typename Filter>
    void handleMemoryValue(Value* ptr, HeapRange range, const Filter& filter)
    {
        handleMemoryValue(
            ptr, range, filter,
            [] (MemoryValue*, Vector<Value*>&) -> Value* {
                return nullptr;
            });
    }

    template<typename Filter, typename Replace>
    void handleMemoryValue(
        Value* ptr, HeapRange range, const Filter& filter, const Replace& replace)
    {
        MemoryMatches matches = findMemoryValue(ptr, range, filter);
        if (replaceMemoryValue(matches, replace))
            return;
        addMemoryValue(m_data, m_value->as<MemoryValue>());
    }

    template<typename Replace>
    bool replaceMemoryValue(const MemoryMatches& matches, const Replace& replace)
    {
        if (matches.isEmpty())
            return false;

        if (verbose)
            dataLog("Eliminating ", *m_value, " due to ", pointerListDump(matches), "\n");
        
        m_changed = true;

        if (matches.size() == 1) {
            MemoryValue* dominatingMatch = matches[0];
            RELEASE_ASSERT(m_dominators.dominates(dominatingMatch->owner, m_block));
            
            if (verbose)
                dataLog("    Eliminating using ", *dominatingMatch, "\n");
            Vector<Value*> extraValues;
            if (Value* value = replace(dominatingMatch, extraValues)) {
                for (Value* extraValue : extraValues)
                    m_insertionSet.insertValue(m_index, extraValue);
                m_value->replaceWithIdentity(value);
            } else {
                if (dominatingMatch->isStore())
                    m_value->replaceWithIdentity(dominatingMatch->child(0));
                else
                    m_value->replaceWithIdentity(dominatingMatch);
            }
            return true;
        }

        // FIXME: It would be way better if this phase just did SSA calculation directly.
        // Right now we're relying on the fact that CSE's position in the phase order is
        // almost right before SSA fixup.

        Variable* variable = m_proc.addVariable(m_value->type());

        VariableValue* get = m_insertionSet.insert<VariableValue>(
            m_index, Get, m_value->origin(), variable);
        if (verbose)
            dataLog("    Inserting get of value: ", *get, "\n");
        m_value->replaceWithIdentity(get);
            
        for (MemoryValue* match : matches) {
            Vector<Value*>& sets = m_sets.add(match, Vector<Value*>()).iterator->value;

            Value* value = replace(match, sets);
            if (!value) {
                if (match->isStore())
                    value = match->child(0);
                else
                    value = match;
            }
                
            Value* set = m_proc.add<VariableValue>(Set, m_value->origin(), variable, value);
            sets.append(set);
        }

        return true;
    }

    void addMemoryValue(ImpureBlockData& data, MemoryValue* memory)
    {
        if (verbose)
            dataLog("Adding memory: ", *memory, "\n");
        
        Matches& matches = data.memoryValues.add(memory->lastChild(), Matches()).iterator->value;

        if (matches.contains(memory))
            return;

        matches.append(memory);
    }

    template<typename Filter>
    MemoryMatches findMemoryValue(Value* ptr, HeapRange range, const Filter& filter)
    {
        if (verbose)
            dataLog(*m_value, ": looking for ", *ptr, "...\n");
        
        auto find = [&] (ImpureBlockData& data) -> MemoryValue* {
            auto iter = data.memoryValues.find(ptr);
            if (iter != data.memoryValues.end()) {
                for (Value* candidateValue : iter->value) {
                    if (MemoryValue* candidateMemory = candidateValue->as<MemoryValue>()) {
                        if (filter(candidateMemory))
                            return candidateMemory;
                    }
                }
            }
            return nullptr;
        };

        if (MemoryValue* match = find(m_data)) {
            if (verbose)
                dataLog("    Found ", *match, " locally.\n");
            return { match };
        }

        if (m_data.writes.overlaps(range)) {
            if (verbose)
                dataLog("    Giving up because of writes.\n");
            return { };
        }

        BlockWorklist worklist;
        Vector<BasicBlock*, 8> seenList;

        worklist.pushAll(m_block->predecessors());

        MemoryMatches matches;

        while (BasicBlock* block = worklist.pop()) {
            if (verbose)
                dataLog("    Looking at ", *block, "\n");
            seenList.append(block);

            ImpureBlockData& data = m_impureBlockData[block];

            MemoryValue* match = find(data);
            if (match && match != m_value) {
                if (verbose)
                    dataLog("    Found match: ", *match, "\n");
                matches.append(match);
                continue;
            }

            if (data.writes.overlaps(range)) {
                if (verbose)
                    dataLog("    Giving up because of writes.\n");
                return { };
            }

            if (!block->numPredecessors()) {
                if (verbose)
                    dataLog("    Giving up because it's live at root.\n");
                // This essentially proves that this is live at the prologue. That means that we
                // cannot reliably optimize this case.
                return { };
            }
            
            worklist.pushAll(block->predecessors());
        }

        if (verbose)
            dataLog("    Got matches: ", pointerListDump(matches), "\n");
        return matches;
    }

    Procedure& m_proc;

    Dominators& m_dominators;
    PureCSE m_pureCSE;
    
    IndexMap<BasicBlock, ImpureBlockData> m_impureBlockData;

    ImpureBlockData m_data;

    BasicBlock* m_block;
    unsigned m_index;
    Value* m_value;

    HashMap<Value*, Vector<Value*>> m_sets;

    InsertionSet m_insertionSet;

    bool m_changed { false };
};

} // anonymous namespace

bool eliminateCommonSubexpressions(Procedure& proc)
{
    PhaseScope phaseScope(proc, "eliminateCommonSubexpressions");

    CSE cse(proc);
    return cse.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

