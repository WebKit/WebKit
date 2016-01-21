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
#include "B3ValueKey.h"
#include "B3ValueInlines.h"
#include <wtf/HashMap.h>
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
    RangeSet<HeapRange> writes;
    
    // Maps an address base to all of the MemoryValues that do things to it. After we're done
    // processing a map, this tells us the values at tail.
    HashMap<Value*, MemoryMatches> memoryValues;
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
            m_data = &m_impureBlockData[block];
            for (Value* value : *block)
                m_data->writes.add(value->effects().writes);
        }
        
        for (BasicBlock* block : m_proc.blocksInPreOrder()) {
            m_block = block;
            m_data = &m_impureBlockData[block];
            m_writes.clear();
            for (m_index = 0; m_index < block->size(); ++m_index) {
                m_value = block->at(m_index);
                process();
            }
            m_insertionSet.execute(block);
        }

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
            clobber(writes);
        
        if (MemoryValue* memory = m_value->as<MemoryValue>())
            processMemory(memory);
    }

    void clobber(HeapRange writes)
    {
        m_writes.add(writes);
        
        m_data->memoryValues.removeIf(
            [&] (HashMap<Value*, MemoryMatches>::KeyValuePairType& entry) -> bool {
                entry.value.removeAllMatching(
                    [&] (MemoryValue* memory) -> bool {
                        return memory->range().overlaps(writes);
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
            MemoryValue* match = findMemoryValue(
                ptr, range, [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && (candidate->opcode() == Load8Z || candidate->opcode() == Store8);
                });
            if (replace(match))
                break;
            addMemoryValue(memory);
            break;
        }

        case Load8S: {
            MemoryValue* match = findMemoryValue(
                ptr, range, [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && (candidate->opcode() == Load8S || candidate->opcode() == Store8);
                });
            if (match) {
                if (match->opcode() == Store8) {
                    m_value->replaceWithIdentity(
                        m_insertionSet.insert<Value>(
                            m_index, SExt8, m_value->origin(), match->child(0)));
                    m_changed = true;
                    break;
                }
                replace(match);
                break;
            }
            addMemoryValue(memory);
            break;
        }

        case Load16Z: {
            MemoryValue* match = findMemoryValue(
                ptr, range, [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && (candidate->opcode() == Load16Z || candidate->opcode() == Store16);
                });
            if (replace(match))
                break;
            addMemoryValue(memory);
            break;
        }

        case Load16S: {
            MemoryValue* match = findMemoryValue(
                ptr, range, [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && (candidate->opcode() == Load16S || candidate->opcode() == Store16);
                });
            if (match) {
                if (match->opcode() == Store16) {
                    m_value->replaceWithIdentity(
                        m_insertionSet.insert<Value>(
                            m_index, SExt16, m_value->origin(), match->child(0)));
                    m_changed = true;
                    break;
                }
                replace(match);
                break;
            }
            addMemoryValue(memory);
            break;
        }

        case Load: {
            MemoryValue* match = findMemoryValue(
                ptr, range, [&] (MemoryValue* candidate) -> bool {
                    if (candidate->offset() != offset)
                        return false;

                    if (candidate->opcode() == Load && candidate->type() == type)
                        return true;

                    if (candidate->opcode() == Store && candidate->child(0)->type() == type)
                        return true;

                    return false;
                });
            if (replace(match))
                break;
            addMemoryValue(memory);
            break;
        }

        case Store8:
        case Store16:
        case Store: {
            addMemoryValue(memory);
            break;
        }

        default:
            dataLog("Bad memory value: ", deepDump(m_proc, m_value), "\n");
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    bool replace(MemoryValue* match)
    {
        if (!match)
            return false;

        if (verbose)
            dataLog("Eliminating ", *m_value, " due to ", *match, "\n");
        
        if (match->isStore())
            m_value->replaceWithIdentity(match->child(0));
        else
            m_value->replaceWithIdentity(match);
        m_changed = true;
        return true;
    }

    void addMemoryValue(MemoryValue* memory)
    {
        addMemoryValue(*m_data, memory);
    }

    void addMemoryValue(ImpureBlockData& data, MemoryValue* memory)
    {
        MemoryMatches& matches =
            data.memoryValues.add(memory->lastChild(), MemoryMatches()).iterator->value;

        if (matches.contains(memory))
            return;

        matches.append(memory);
    }

    template<typename Filter>
    MemoryValue* findMemoryValue(Value* ptr, HeapRange range, const Filter& filter)
    {
        auto find = [&] (ImpureBlockData& data) -> MemoryValue* {
            auto iter = data.memoryValues.find(ptr);
            if (iter != data.memoryValues.end()) {
                for (MemoryValue* candidate : iter->value) {
                    if (filter(candidate))
                        return candidate;
                }
            }
            return nullptr;
        };

        if (MemoryValue* match = find(*m_data))
            return match;

        if (m_writes.overlaps(range))
            return nullptr;

        BlockWorklist worklist;
        Vector<BasicBlock*, 8> seenList;

        worklist.pushAll(m_block->predecessors());

        MemoryValue* match = nullptr;

        while (BasicBlock* block = worklist.pop()) {
            seenList.append(block);

            ImpureBlockData& data = m_impureBlockData[block];

            if (m_dominators.strictlyDominates(block, m_block)) {
                match = find(data);
                if (match)
                    continue;
            }

            if (data.writes.overlaps(range))
                return nullptr;

            worklist.pushAll(block->predecessors());
        }

        if (!match)
            return nullptr;

        for (BasicBlock* block : seenList)
            addMemoryValue(m_impureBlockData[block], match);
        addMemoryValue(match);

        return match;
    }

    typedef Vector<Value*, 1> Matches;

    Procedure& m_proc;

    Dominators& m_dominators;
    PureCSE m_pureCSE;
    
    IndexMap<BasicBlock, ImpureBlockData> m_impureBlockData;

    ImpureBlockData* m_data;
    RangeSet<HeapRange> m_writes;

    BasicBlock* m_block;
    unsigned m_index;
    Value* m_value;

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

