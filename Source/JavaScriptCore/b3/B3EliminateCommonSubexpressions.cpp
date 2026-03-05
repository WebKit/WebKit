/*
 * Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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

#include "B3BlockWorklist.h"
#include "B3Dominators.h"
#include "B3HeapRange.h"
#include "B3InsertionSetInlines.h"
#include "B3MemoryValue.h"
#include "B3MemoryValueInlines.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3PureCSE.h"
#include "B3ValueInlines.h"
#include "B3VariableValue.h"
#include "B3WasmStructFieldValue.h"
#include "B3WasmStructGetValue.h"
#include "B3WasmStructSetValue.h"
#include <wtf/CommaPrinter.h>
#include <wtf/HashMap.h>
#include <wtf/ListDump.h>
#include <wtf/RangeSet.h>
#include <wtf/Scope.h>

namespace JSC { namespace B3 {

namespace {

namespace B3EliminateCommonSubexpressionsInternal {
static constexpr bool verbose = false;
}

// FIXME: We could treat Patchpoints with a non-empty set of reads as a "memory value" and somehow
// eliminate redundant ones. We would need some way of determining if two patchpoints are replacable.
// It doesn't seem right to use the reads set for this. We could use the generator, but that feels
// lame because the FTL will pretty much use a unique generator for each patchpoint even when two
// patchpoints have the same semantics as far as CSE would be concerned. We could invent something
// like a "value ID" for patchpoints. By default, each one gets a unique value ID, but FTL could force
// some patchpoints to share the same one as a signal that they will return the same value if executed
// in the same heap with the same inputs.

using MemoryMatches = Vector<MemoryValue*, 1>;
using WasmStructMatches = Vector<WasmStructFieldValue*, 1>;

class MemoryValueMap {
public:
    MemoryValueMap() { }

    void add(MemoryValue* memory)
    {
        Matches& matches = m_map.add(memory->lastChild(), Matches()).iterator->value;
        if (matches.contains(memory))
            return;
        matches.append(memory);
    }

    template<typename Functor>
    void removeIf(const Functor& functor)
    {
        m_map.removeIf(
            [&] (UncheckedKeyHashMap<Value*, Matches>::KeyValuePairType& entry) -> bool {
                entry.value.removeAllMatching(
                    [&] (Value* value) -> bool {
                        if (MemoryValue* memory = value->as<MemoryValue>())
                            return functor(memory);
                        return true;
                    });
                return entry.value.isEmpty();
            });
    }

    Matches* find(Value* ptr)
    {
        auto iter = m_map.find(ptr);
        if (iter == m_map.end())
            return nullptr;
        return &iter->value;
    }

    template<typename Functor>
    MemoryValue* find(Value* ptr, const Functor& functor)
    {
        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "        Looking for ", pointerDump(ptr), " in ", *this);
        if (Matches* matches = find(ptr)) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "        Matches: ", pointerListDump(*matches));
            for (Value* candidateValue : *matches) {
                dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "        Having candidate: ", pointerDump(candidateValue));
                if (MemoryValue* candidateMemory = candidateValue->as<MemoryValue>()) {
                    if (functor(candidateMemory))
                        return candidateMemory;
                }
            }
        }
        return nullptr;
    }

    void dump(PrintStream& out) const
    {
        out.print("{"_s);
        CommaPrinter comma;
        for (auto& entry : m_map)
            out.print(comma, pointerDump(entry.key), "=>"_s, pointerListDump(entry.value));
        out.print("}"_s);
    }
    
private:
    // This uses Matches for two reasons:
    // - It cannot be a MemoryValue* because the key is imprecise. Many MemoryValues could have the
    //   same key while being unaliased.
    // - It can't be a MemoryMatches array because the MemoryValue*'s could be turned into Identity's.
    UncheckedKeyHashMap<Value*, Matches> m_map;
};

using WasmStructFieldKey = std::tuple<Value*, uint64_t>;

class WasmStructValueMap {
public:
    WasmStructValueMap() = default;

    void add(WasmStructFieldValue* value)
    {
        WasmStructFieldKey key(value->child(0), value->fieldHeapKey());
        Matches& matches = m_map.add(key, Matches()).iterator->value;
        if (matches.contains(value))
            return;
        matches.append(value);
    }

    template<typename Functor>
    void removeIf(const Functor& functor)
    {
        m_map.removeIf(
            [&](UncheckedKeyHashMap<WasmStructFieldKey, Matches>::KeyValuePairType& entry) -> bool {
                entry.value.removeAllMatching(
                    [&](Value* value) -> bool {
                        if (auto* field = value->as<WasmStructFieldValue>())
                            return functor(field);
                        return true;
                    });
                return entry.value.isEmpty();
            });
    }

    template<typename Functor>
    WasmStructFieldValue* find(Value* structPtr, uint64_t fieldHeapKey, const Functor& functor)
    {
        auto iter = m_map.find(WasmStructFieldKey(structPtr, fieldHeapKey));
        if (iter == m_map.end())
            return nullptr;
        for (auto* candidate : iter->value) {
            if (auto* candidateStructField = candidate->as<WasmStructFieldValue>()) {
                if (functor(candidateStructField))
                    return candidateStructField;
            }
        }
        return nullptr;
    }

    void dump(PrintStream& out) const
    {
        out.print("{"_s);
        CommaPrinter comma;
        for (auto& entry : m_map)
            out.print(comma, "(", pointerDump(std::get<0>(entry.key)), ",", std::get<1>(entry.key), ")=>"_s, pointerListDump(entry.value));
        out.print("}"_s);
    }

private:
    UncheckedKeyHashMap<WasmStructFieldKey, Matches> m_map;
};

struct ImpureBlockData {
    void dump(PrintStream& out) const
    {
        out.print(
            "{reads = ", reads, ", writes = ", writes,
            ", memoryStoresAtHead = ", memoryStoresAtHead, ", memoryValuesAtTail = ", memoryValuesAtTail,
            ", wasmStructStoresAtHead = ", wasmStructStoresAtHead, ", wasmStructValuesAtTail = ", wasmStructValuesAtTail,
            "}");
    }

    RangeSet<HeapRange> reads; // This only gets used for forward store elimination.
    RangeSet<HeapRange> writes; // This gets used for both load and store elimination.
    bool fence { false };
    bool writesPinned { false };

    MemoryValueMap memoryStoresAtHead;
    MemoryValueMap memoryValuesAtTail;

    WasmStructValueMap wasmStructStoresAtHead;
    WasmStructValueMap wasmStructValuesAtTail;

    // This Maps x->y in "y = WasmAddress(@x)"
    UncheckedKeyHashMap<Value*, Value*> m_candidateWasmAddressesAtTail;
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
        dataLogIf(B3EliminateCommonSubexpressionsInternal::verbose, "B3 before CSE:\n", m_proc);

        m_proc.resetValueOwners();

        // Summarize the impure effects of each block, and the impure values available at the end of
        // each block. This doesn't edit code yet.
        for (BasicBlock* block : m_proc) {
            ImpureBlockData& data = m_impureBlockData[block];
            for (Value* value : *block) {
                Effects effects = value->effects();
                MemoryValue* memory = value->as<MemoryValue>();
                WasmStructFieldValue* wasmStructField = value->as<WasmStructFieldValue>();

                if (memory) {
                    if (memory->isStore()
                        && !data.reads.overlaps(memory->range())
                        && !data.writes.overlaps(memory->range())
                        && (!data.fence || !memory->hasFence()))
                        data.memoryStoresAtHead.add(memory);
                }
                if (wasmStructField) {
                    if (wasmStructField->opcode() == WasmStructSet
                        && !data.reads.overlaps(wasmStructField->range())
                        && !data.writes.overlaps(wasmStructField->range())
                        && !data.fence)
                        data.wasmStructStoresAtHead.add(wasmStructField);
                }

                data.reads.add(effects.reads);

                if (HeapRange writes = effects.writes)
                    clobber(data, writes);
                data.fence |= effects.fence;

                if (memory)
                    data.memoryValuesAtTail.add(memory);
                if (wasmStructField)
                    data.wasmStructValuesAtTail.add(wasmStructField);

                if (WasmAddressValue* wasmAddress = value->as<WasmAddressValue>())
                    data.m_candidateWasmAddressesAtTail.add(wasmAddress->child(0), wasmAddress);

                if (effects.writesPinned) {
                    data.writesPinned = true;
                    data.m_candidateWasmAddressesAtTail.clear();
                }
            }

            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "Block ", *block, ": ", data);
        }

        // Perform CSE. This edits code.
        Vector<BasicBlock*> postOrder = m_proc.blocksInPostOrder();
        for (unsigned i = postOrder.size(); i--;) {
            m_block = postOrder[i];
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "Looking at ", *m_block, ":");

            m_data = ImpureBlockData();
            for (m_index = 0; m_index < m_block->size(); ++m_index) {
                m_value = m_block->at(m_index);
                process();
            }
            m_insertionSet.execute(m_block);
            m_impureBlockData[m_block] = m_data;
        }

        // The previous pass might have requested that we insert code in some basic block other than
        // the one that it was looking at. This inserts them.
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

        dataLogIf(B3EliminateCommonSubexpressionsInternal::verbose, "B3 after CSE:\n", m_proc);

        return m_changed;
    }
    
private:
    void process()
    {
        m_value->performSubstitution();

        if (m_pureCSE.process(m_value, m_dominators)) {
            ASSERT(!m_value->effects().readsPinned || !m_data.writesPinned);
            ASSERT(!m_value->effects().writes);
            ASSERT(!m_value->effects().writesPinned);
            m_changed = true;
            return;
        }

        if (WasmAddressValue* wasmAddress = m_value->as<WasmAddressValue>()) {
            processWasmAddressValue(wasmAddress);
            return;
        }

        Effects effects = m_value->effects();

        if (effects.writesPinned) {
            m_data.writesPinned = true;
            m_data.m_candidateWasmAddressesAtTail.clear();
        }

        MemoryValue* memory = m_value->as<MemoryValue>();
        WasmStructGetValue* structGet = m_value->as<WasmStructGetValue>();
        WasmStructSetValue* structSet = m_value->as<WasmStructSetValue>();

        // Before clobber - try to eliminate redundant operations
        if (memory && processMemoryBeforeClobber(memory))
            return;

        if (structSet && processWasmStructSetBeforeClobber(structSet))
            return;

        // Clobber based on writes - this handles both MemoryValue and WasmStruct operations
        if (HeapRange writes = effects.writes)
            clobber(m_data, writes);

        // After clobber - CSE and tracking
        if (memory)
            processMemoryAfterClobber(memory);

        if (structGet)
            processWasmStructGetAfterClobber(structGet);

        if (structSet)
            processWasmStructSetAfterClobber(structSet);

        // The reads info should be updated even the block is processed
        // since the dominated store nodes may dependent on the data
        // read from the processed block. Note that there is no need to
        // update reads info if the node is deleted.
        m_data.reads.add(m_value->effects().reads);
    }

    // Return true if we got rid of the operation. If you changed IR in this function, you have to
    // set m_changed even if you also return true.
    bool processMemoryBeforeClobber(MemoryValue* memory)
    {
        Value* value = memory->child(0);
        Value* ptr = memory->lastChild();
        HeapRange range = memory->range();
        Value::OffsetType offset = memory->offset();

        switch (memory->opcode()) {
        case Store8:
            return handleStoreBeforeClobber(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && ((candidate->opcode() == Store8 && candidate->child(0) == value)
                            || ((candidate->opcode() == Load8Z || candidate->opcode() == Load8S)
                                && candidate == value));
                });
        case Store16:
            return handleStoreBeforeClobber(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && ((candidate->opcode() == Store16 && candidate->child(0) == value)
                            || ((candidate->opcode() == Load16Z || candidate->opcode() == Load16S)
                                && candidate == value));
                });
        case Store:
            return handleStoreBeforeClobber(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->offset() == offset
                        && ((candidate->opcode() == Store && candidate->child(0) == value)
                            || (candidate->opcode() == Load && candidate == value));
                });
        default:
            return false;
        }
    }

    void clobber(ImpureBlockData& data, HeapRange writes)
    {
        data.writes.add(writes);

        data.memoryValuesAtTail.removeIf(
            [&](MemoryValue* memory) {
                // If memory reads is immutable, clobbering never changes the result.
                if (memory->readsMutability() == Mutability::Immutable)
                    return false;
                return memory->range().overlaps(writes);
            });

        data.wasmStructValuesAtTail.removeIf(
            [&](WasmStructFieldValue* value) {
                // If field is immutable (only applies to Get), clobbering never changes the result
                if (auto* structGet = value->as<WasmStructGetValue>()) {
                    if (structGet->mutability() == Mutability::Immutable)
                        return false;
                }
                return value->range().overlaps(writes);
            });
    }

    void processMemoryAfterClobber(MemoryValue* memory)
    {
        Value* ptr = memory->lastChild();
        HeapRange range = memory->range();
        Value::OffsetType offset = memory->offset();
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
                    dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "        Consdering ", pointerDump(candidate));
                    if (candidate->offset() != offset)
                        return false;

                    dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "            offset ok.");
                    if (candidate->opcode() == Load) {
                        if (candidate->type() == type)
                            return true;
                        if (candidate->type() == Int64 && type == Int32)
                            return true;
                    }

                    dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "            not a load with ok type.");
                    if (candidate->opcode() == Store) {
                        if (candidate->child(0)->type() == type)
                            return true;
                        if (candidate->child(0)->type() == Int64 && type == Int32)
                            return true;
                    }

                    dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "            not a store with ok type.");
                    return false;
                },
                [&] (MemoryValue* match, Vector<Value*>& fixups) -> Value* {
                    if (match->opcode() == Load) {
                        if (match->type() == type)
                            return nullptr;

                        if (match->type() == Int64 && type == Int32) {
                            Value* trunc = m_proc.add<Value>(Trunc, m_value->origin(), match);
                            fixups.append(trunc);
                            return trunc;
                        }
                    }

                    if (match->opcode() == Store) {
                        if (match->child(0)->type() == type)
                            return nullptr;

                        if (match->child(0)->type() == Int64 && type == Int32) {
                            Value* trunc = m_proc.add<Value>(Trunc, m_value->origin(), match->child(0));
                            fixups.append(trunc);
                            return trunc;
                        }
                    }

                    return nullptr;
                });
            break;
        }

        case Store8: {
            handleStoreAfterClobber(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->opcode() == Store8
                        && candidate->offset() == offset;
                });
            break;
        }
            
        case Store16: {
            handleStoreAfterClobber(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->opcode() == Store16
                        && candidate->offset() == offset;
                });
            break;
        }
            
        case Store: {
            auto clobberWidth = memory->accessWidth();
            handleStoreAfterClobber(
                ptr, range,
                [&] (MemoryValue* candidate) -> bool {
                    return candidate->opcode() == Store
                        && candidate->offset() == offset
                        && candidate->accessWidth() >= clobberWidth;
                });
            break;
        }

        default:
            break;
        }
    }

    template<typename Filter>
    bool handleStoreBeforeClobber(Value* ptr, HeapRange range, const Filter& filter)
    {
        MemoryMatches matches = findMemoryValue(ptr, range, filter);
        if (matches.isEmpty())
            return false;

        m_value->replaceWithNop();
        m_changed = true;
        return true;
    }

    template<typename Filter>
    void handleStoreAfterClobber(Value* ptr, HeapRange range, const Filter& filter)
    {
        if (!m_value->traps() && findStoreAfterClobber(ptr, range, filter)) {
            m_value->replaceWithNop();
            m_changed = true;
            return;
        }

        m_data.memoryValuesAtTail.add(m_value->as<MemoryValue>());
    }

    template<typename Filter>
    bool findStoreAfterClobber(Value* ptr, HeapRange range, const Filter& filter)
    {
        if (m_value->as<MemoryValue>()->hasFence())
            return false;
        
        // We can eliminate a store if every forward path hits a store to the same location before
        // hitting any operation that observes the store. This search seems like it should be
        // expensive, but in the overwhelming majority of cases it will almost immediately hit an 
        // operation that interferes.

        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, *m_value, ": looking forward for stores to ", *ptr, "...");

        // First search forward in this basic block.
        // FIXME: It would be cool to get rid of this linear search. It's not super critical since
        // we will probably bail out very quickly, but it *is* annoying.
        for (unsigned index = m_index + 1; index < m_block->size(); ++index) {
            Value* value = m_block->at(index);

            if (MemoryValue* memoryValue = value->as<MemoryValue>()) {
                if (memoryValue->lastChild() == ptr && filter(memoryValue))
                    return true;
            }

            Effects effects = value->effects();
            if (effects.reads.overlaps(range) || effects.writes.overlaps(range))
                return false;
        }

        if (!m_block->numSuccessors())
            return false;

        BlockWorklist worklist;
        worklist.pushAll(m_block->successorBlocks());

        while (BasicBlock* block = worklist.pop()) {
            ImpureBlockData& data = m_impureBlockData[block];

            MemoryValue* match = data.memoryStoresAtHead.find(ptr, filter);
            if (match && match != m_value)
                continue;

            if (data.writes.overlaps(range) || data.reads.overlaps(range))
                return false;

            if (!block->numSuccessors())
                return false;

            worklist.pushAll(block->successorBlocks());
        }

        return true;
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
        m_data.memoryValuesAtTail.add(m_value->as<MemoryValue>());
    }

    template<typename Replace>
    bool replaceMemoryValue(const MemoryMatches& matches, const Replace& replace)
    {
        if (matches.isEmpty())
            return false;

        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "Eliminating ", *m_value, " due to ", pointerListDump(matches));
        
        m_changed = true;

        if (matches.size() == 1) {
            MemoryValue* dominatingMatch = matches[0];
            RELEASE_ASSERT(m_dominators.dominates(dominatingMatch->owner, m_block));
            
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Eliminating using ", *dominatingMatch);
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
        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Inserting get of value: ", *get);
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

    template<typename Filter>
    MemoryMatches findMemoryValue(Value* ptr, HeapRange range, const Filter& filter)
    {
        if constexpr (B3EliminateCommonSubexpressionsInternal::verbose) {
            dataLogLn(*m_value, ": looking backward for ", *ptr, "...");
            dataLogLn("    Full value: ", deepDump(m_value));
        }
        
        if (m_value->as<MemoryValue>()->hasFence()) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Giving up because fences.");
            return { };
        }
        
        if (MemoryValue* match = m_data.memoryValuesAtTail.find(ptr, filter)) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Found ", *match, " locally.");
            return { match };
        }

        if (m_data.writes.overlaps(range)) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Giving up because of writes.");
            return { };
        }

        BlockWorklist worklist;
        worklist.pushAll(m_block->predecessors());

        MemoryMatches matches;

        while (BasicBlock* block = worklist.pop()) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Looking at ", *block);

            ImpureBlockData& data = m_impureBlockData[block];

            MemoryValue* match = data.memoryValuesAtTail.find(ptr, filter);
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Consdering match: ", pointerDump(match));
            if (match && match != m_value) {
                dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Found match: ", *match);
                matches.append(match);
                continue;
            }

            if (data.writes.overlaps(range)) {
                dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Giving up because of writes.");
                return { };
            }

            if (!block->numPredecessors()) {
                dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Giving up because it's live at root.");
                // This essentially proves that this is live at the prologue. That means that we
                // cannot reliably optimize this case.
                return { };
            }
            
            worklist.pushAll(block->predecessors());
        }

        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Got matches: ", pointerListDump(matches));
        return matches;
    }

    void processWasmAddressValue(WasmAddressValue* wasmAddress)
    {
        Value* ptr = wasmAddress->child(0);

        if (Value* replacement = m_data.m_candidateWasmAddressesAtTail.get(ptr)) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Replacing WasmAddress: ", *wasmAddress, " with ", *replacement);
            wasmAddress->replaceWithIdentity(replacement);
            m_changed = true;
            return;
        }

        auto addPtrOnScopeExit = makeScopeExit([&] {
            m_data.m_candidateWasmAddressesAtTail.add(ptr, wasmAddress);
        });

        if (m_data.writesPinned) {
            // Someone before us in this block wrote to pinned. So we have no
            // hope of finding a match if the above search failed.
            return;
        }

        Value* candidateReplacement = nullptr;
        BasicBlock* dominator = nullptr;
        m_dominators.forAllStrictDominatorsOf(m_block, [&] (BasicBlock* block) {
            if (candidateReplacement)
                return;

            if (Value* replacement = m_impureBlockData[block].m_candidateWasmAddressesAtTail.get(ptr)) {
                candidateReplacement = replacement;
                dominator = block;
            }
        });

        if (!candidateReplacement)
            return;

        BlockWorklist worklist;
        worklist.pushAll(m_block->predecessors());
        while (BasicBlock* block = worklist.pop()) {
            if (block == dominator)
                continue;
            if (m_impureBlockData[block].writesPinned) {
                candidateReplacement = nullptr;
                break;
            }
            worklist.pushAll(block->predecessors());
        }

        if (candidateReplacement) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Replacing WasmAddress: ", *wasmAddress, " with ", *candidateReplacement);
            wasmAddress->replaceWithIdentity(candidateReplacement);
            m_changed = true;
        }
    }

    void processWasmStructGetAfterClobber(WasmStructGetValue* structGet)
    {
        Value* structPtr = structGet->child(0);
        HeapRange range = structGet->range();
        uint64_t fieldHeapKey = structGet->fieldHeapKey();

        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Processing WasmStructGet: ", *structGet, " fieldHeapKey=", fieldHeapKey);
        WasmStructMatches matches = findWasmStructValue(structPtr, range, fieldHeapKey, [&](WasmStructFieldValue*) { return true; });
        if (replaceWasmStructValue(matches, structGet))
            return;
        m_data.wasmStructValuesAtTail.add(structGet);
    }

    template<typename Filter>
    WasmStructMatches findWasmStructValue(Value* structPtr, HeapRange range, uint64_t fieldHeapKey, const Filter& filter)
    {
        if constexpr (B3EliminateCommonSubexpressionsInternal::verbose) {
            dataLogLn(*m_value, ": looking backward for WasmStruct structPtr=", *structPtr, " fieldHeapKey=", fieldHeapKey);
            dataLogLn("    Full value: ", deepDump(m_value));
        }

        // Check local block first
        if (auto* match = m_data.wasmStructValuesAtTail.find(structPtr, fieldHeapKey, filter)) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Found ", *match, " locally.");
            return { match };
        }

        // Check if current block has clobbering writes
        if (m_data.writes.overlaps(range)) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Giving up because of writes.");
            return { };
        }

        // Search backward through predecessors
        BlockWorklist worklist;
        worklist.pushAll(m_block->predecessors());

        WasmStructMatches matches;

        while (BasicBlock* block = worklist.pop()) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Looking at ", *block);

            ImpureBlockData& data = m_impureBlockData[block];

            auto* match = data.wasmStructValuesAtTail.find(structPtr, fieldHeapKey, filter);
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Considering match: ", pointerDump(match));
            if (match && match != m_value) {
                dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Found match: ", *match);
                matches.append(match);
                continue;
            }

            if (data.writes.overlaps(range)) {
                dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Giving up because of writes.");
                return { };
            }

            if (!block->numPredecessors()) {
                dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Giving up because it's live at root.");
                // This essentially proves that this is live at the prologue. That means that we
                // cannot reliably optimize this case.
                return { };
            }

            worklist.pushAll(block->predecessors());
        }

        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Got matches: ", pointerListDump(matches));
        return matches;
    }

    bool replaceWasmStructValue(const WasmStructMatches& matches, WasmStructGetValue* structGet)
    {
        if (matches.isEmpty())
            return false;

        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "Eliminating ", *m_value, " due to ", pointerListDump(matches));

        m_changed = true;

        auto replace = [&](Value* dominatingMatch, Vector<Value*, 16>& extraValues) -> Value* {
            if (auto* structSet = dominatingMatch->as<WasmStructSetValue>()) {
                Value* storedValue = structSet->child(1);
                SUPPRESS_UNCOUNTED_LOCAL const Wasm::StructType* structType = structGet->structType();
                auto fieldType = structType->field(structGet->fieldIndex()).type;

                Value* forwardedValue = storedValue;

                // Handle packed types: truncate the stored value to match the field size
                if (fieldType.is<Wasm::PackedType>()) {
                    uint32_t mask = 0;
                    switch (fieldType.as<Wasm::PackedType>()) {
                    case Wasm::PackedType::I8:
                        mask = 0xff;
                        break;
                    case Wasm::PackedType::I16:
                        mask = 0xffff;
                        break;
                    }

                    Value* maskValue = m_proc.add<Const32Value>(structGet->origin(), mask);
                    forwardedValue = m_proc.add<Value>(BitAnd, structGet->origin(), storedValue, maskValue);
                    extraValues.append(maskValue);
                    extraValues.append(forwardedValue);
                }

                dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Forwarding from WasmStructSet with value: ", *forwardedValue);
                return forwardedValue;
            }

            return dominatingMatch;
        };

        if (matches.size() == 1) {
            auto* dominatingMatch = matches[0];
            RELEASE_ASSERT(m_dominators.dominates(dominatingMatch->owner, m_block));

            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Eliminating using ", *dominatingMatch);

            // Handle store-to-load forwarding from WasmStructSet

            Vector<Value*, 16> extraValues;
            auto* value = replace(dominatingMatch, extraValues);
            ASSERT(value);
            for (auto* extraValue : extraValues)
                m_insertionSet.insertValue(m_index, extraValue);
            m_value->replaceWithIdentity(value);
            return true;
        }

        // Multiple matches from different paths - need SSA fixup with Variable
        Variable* variable = m_proc.addVariable(m_value->type());

        VariableValue* get = m_insertionSet.insert<VariableValue>(m_index, Get, m_value->origin(), variable);
        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Inserting get of value: ", *get);
        m_value->replaceWithIdentity(get);

        for (auto* match : matches) {
            Vector<Value*>& sets = m_sets.add(match, Vector<Value*>()).iterator->value;
            Vector<Value*, 16> extraValues;
            auto* value = replace(match, extraValues);
            ASSERT(value);
            sets.appendVector(extraValues);
            Value* set = m_proc.add<VariableValue>(Set, m_value->origin(), variable, value);
            sets.append(set);
        }

        return true;
    }

    bool processWasmStructSetBeforeClobber(WasmStructSetValue* structSet)
    {
        Value* structPtr = structSet->child(0);
        Value* value = structSet->child(1);
        uint64_t fieldHeapKey = structSet->fieldHeapKey();

        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Processing WasmStructSet (before clobber): ", *structSet, " fieldHeapKey=", fieldHeapKey);

        WasmStructMatches matches = findWasmStructValue(structPtr, structSet->range(), fieldHeapKey, [&](WasmStructFieldValue* candidate) {
            // @a: Set(@x, field0, @z)
            // @b: Set(@x, field0, @z) -> setting same value again.
            if (auto* candidateSet = candidate->as<WasmStructSetValue>())
                return candidateSet->child(1) == value;

            // @a: Get(@x, field0)
            // @b: Set(@x, field0, @a) -> setting a value loaded from the same struct's field.
            if (auto* candidateGet = candidate->as<WasmStructGetValue>())
                return candidateGet == value;

            return false;
        });
        if (matches.isEmpty())
            return false;

        m_value->replaceWithNop();
        m_changed = true;
        return true;
    }

    void processWasmStructSetAfterClobber(WasmStructSetValue* structSet)
    {
        Value* structPtr = structSet->child(0);
        HeapRange range = structSet->range();
        uint64_t fieldHeapKey = structSet->fieldHeapKey();

        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Processing WasmStructSet (after clobber): ", *structSet, " fieldHeapKey=", fieldHeapKey);

        if (!structSet->traps() && findWasmStructSetAfterClobber(structPtr, range, fieldHeapKey)) {
            dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, "    Forward elimination - replacing with nop");
            m_value->replaceWithNop();
            m_changed = true;
            return;
        }

        m_data.wasmStructValuesAtTail.add(structSet);
    }

    bool findWasmStructSetAfterClobber(Value* structPtr, HeapRange range, uint64_t fieldHeapKey)
    {
        dataLogLnIf(B3EliminateCommonSubexpressionsInternal::verbose, *m_value, ": looking forward for WasmStructSet to structPtr=", *structPtr, " fieldHeapKey=", fieldHeapKey, "...");

        // Search forward in this basic block first
        for (unsigned index = m_index + 1; index < m_block->size(); ++index) {
            Value* value = m_block->at(index);

            if (auto* candidateSet = value->as<WasmStructSetValue>()) {
                if (candidateSet->child(0) == structPtr
                    && candidateSet->fieldHeapKey() == fieldHeapKey)
                    return true;
            }

            Effects effects = value->effects();
            if (effects.reads.overlaps(range) || effects.writes.overlaps(range))
                return false;
        }

        if (!m_block->numSuccessors())
            return false;

        BlockWorklist worklist;
        worklist.pushAll(m_block->successorBlocks());

        while (BasicBlock* block = worklist.pop()) {
            ImpureBlockData& data = m_impureBlockData[block];

            Value* match = data.wasmStructStoresAtHead.find(structPtr, fieldHeapKey, [&](Value*) { return true; });
            if (match && match != m_value)
                continue;

            if (data.writes.overlaps(range) || data.reads.overlaps(range))
                return false;

            if (!block->numSuccessors())
                return false;

            worklist.pushAll(block->successorBlocks());
        }

        return true;
    }

    Procedure& m_proc;

    Dominators& m_dominators;
    PureCSE m_pureCSE;
    
    IndexMap<BasicBlock*, ImpureBlockData> m_impureBlockData;

    ImpureBlockData m_data;

    BasicBlock* m_block;
    unsigned m_index;
    Value* m_value;

    UncheckedKeyHashMap<Value*, Vector<Value*>> m_sets;

    InsertionSet m_insertionSet;

    bool m_changed { false };
};

} // anonymous namespace

bool eliminateCommonSubexpressions(Procedure& proc)
{
    PhaseScope phaseScope(proc, "eliminateCommonSubexpressions"_s);

    CSE cse(proc);
    return cse.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

