/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "B3CanonicalizePrePostIncrements.h"

#include "B3BackwardsDominators.h"
#include "B3BasicBlockInlines.h"
#include "B3BlockInsertionSet.h"
#include "B3Dominators.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
#include <wtf/HashMap.h>
#include <wtf/IndexSet.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(B3_JIT)

namespace JSC { namespace B3 {

bool canonicalizePrePostIncrements(Procedure& proc)
{
    if (!isARM64())
        return false;
    PhaseScope phaseScope(proc, "canonicalizePrePostIncrements");
    using Arg = Air::Arg;

    unsigned index { 0 };
    InsertionSet insertionSet { proc };
    BlockInsertionSet blockInsertionSet { proc };

    Dominators& dominators = proc.dominators();
    BackwardsDominators& backwardsDominators = proc.backwardsDominators();

    IndexSet<Value*> ignoredValues;
    HashMap<Value*, Vector<MemoryValue*>> baseToLoads;
    HashMap<MemoryValue*, Value*> preIndexLoadCandidates;
    HashMap<MemoryValue*, Value*> postIndexLoadCandidates;
    HashMap<std::tuple<Value*, int64_t>, Vector<Value*>> baseOffsetToAddresses;

    HashMap<Value*, Vector<MemoryValue*>> baseToStores;
    HashMap<MemoryValue*, Value*> postIndexStoreCandidates;

    auto tryAddPrePostIndexCandidate = [&] (Value* value) {
        switch (value->opcode()) {
        case Load: {
            // Pre-Index Pattern:
            //     address = Add(base, offset)
            //     ...
            //     memory = Load(base, offset)
            // Post-Index Pattern:
            //     memory = Load(base, 0)
            //     ...
            //     address = Add(base, offset)
            auto tryAddpreIndexLoadCandidates = [&] () {
                MemoryValue* memory = value->as<MemoryValue>();
                if (memory->type() != Int32 && memory->type() != Int64)
                    return;
                if (memory->offset()) {
                    if (!Arg::isValidIncrementIndexForm(memory->offset()))
                        return;
                    auto iterator = baseOffsetToAddresses.find(std::tuple { memory->child(0), static_cast<int64_t>(memory->offset()) });
                    if (iterator == baseOffsetToAddresses.end())
                        return;
                    for (Value* address : iterator->value)
                        preIndexLoadCandidates.add(memory, address);
                } else
                    baseToLoads.add(memory->child(0), Vector<MemoryValue*>()).iterator->value.append(memory);
            };

            tryAddpreIndexLoadCandidates();
            break;
        }

        case Store: {
            // Pre-Index Pattern:
            //     address = Add(base, offset)
            //     memory = Store(value, base, offset)
            // Post-Index Pattern:
            //     memory = Store(value, base, 0)
            //     ...
            //     address = Add(base, offset)
            auto tryUpdateBaseToStores = [&] () {
                MemoryValue* memory = value->as<MemoryValue>();
                if (memory->child(0)->type() != Int32 && memory->child(0)->type() != Int64)
                    return;
                if (memory->child(0)->hasInt() || memory->offset())
                    return;
                baseToStores.add(memory->child(1), Vector<MemoryValue*>()).iterator->value.append(memory);
            };

            tryUpdateBaseToStores();
            break;
        }

        case Add: {
            Value* left = value->child(0);
            Value* right = value->child(1);

            auto tryAddpostIndexCandidates = [&] () {
                if (!right->hasIntPtr() || value->type() != Int64)
                    return;
                intptr_t offset = right->asIntPtr();
                Value::OffsetType smallOffset = static_cast<Value::OffsetType>(offset);
                if (smallOffset != offset || !Arg::isValidIncrementIndexForm(smallOffset))
                    return;
                // so far this Add value is a valid address candidate for both prefix and postfix pattern
                baseOffsetToAddresses.add(std::tuple { left, static_cast<int64_t>(smallOffset) }, Vector<Value*>()).iterator->value.append(value);
                {
                    auto iterator = baseToLoads.find(left);
                    if (iterator != baseToLoads.end()) {
                        for (MemoryValue* memory : iterator->value)
                            postIndexLoadCandidates.add(memory, value);
                    }
                }
                {
                    auto iterator = baseToStores.find(left);
                    if (iterator != baseToStores.end()) {
                        for (MemoryValue* memory : iterator->value)
                            postIndexStoreCandidates.add(memory, value);
                    }
                }
            };

            tryAddpostIndexCandidates();
            break;
        }

        default:
            break;
        }
    };

    for (BasicBlock* basicBlock : proc.blocksInPreOrder()) {
        for (index = 0; index < basicBlock->size(); ++index)
            tryAddPrePostIndexCandidate(basicBlock->at(index));
    }

    auto controlEquivalent = [&] (Value* v1, Value* v2) -> bool {
        return (dominators.dominates(v1->owner, v2->owner) && backwardsDominators.dominates(v2->owner, v1->owner))
            || (dominators.dominates(v2->owner, v1->owner) && backwardsDominators.dominates(v1->owner, v2->owner));
    };

    // This search is expensive. However, due to the greedy pattern
    // matching, no better method can be proposed at present.
    auto valueIndexInBasicBlock = [&] (Value* value) -> unsigned {
        unsigned index = 0;
        BasicBlock* block = value->owner;
        for (index = 0; index < block->size(); ++index) {
            if (block->at(index) == value)
                return index;
        }
        return index;
    };

    for (auto pair : preIndexLoadCandidates) {
        MemoryValue* memory = pair.key;
        Value* address = pair.value;
        if (ignoredValues.contains(memory) || ignoredValues.contains(address) || !controlEquivalent(memory, address))
            continue;
        // address = Add(base, offset)       address = Add(base, offset)
        // ...                          -->  newMemory = Load(base, offset)
        // ...                               ...
        // memory = Load(base, offset)       memory = Identity(newMemory)
        unsigned insertionIndex = valueIndexInBasicBlock(address) + 1;
        MemoryValue* newMemory = insertionSet.insert<MemoryValue>(insertionIndex, Load, memory->type(), address->origin(), memory->lastChild());
        newMemory->setOffset(memory->offset());
        memory->replaceWithIdentity(newMemory);
        insertionSet.execute(address->owner);

        ignoredValues.add(memory);
        ignoredValues.add(address);
    }

    auto detectPostIndex = [&] (const HashMap<MemoryValue*, Value*>& candidates) {
        for (auto pair : candidates) {
            MemoryValue* memory = pair.key;
            Value* address = pair.value;
            if (ignoredValues.contains(memory) || ignoredValues.contains(address) || !controlEquivalent(memory, address))
                continue;

            unsigned insertionIndex = valueIndexInBasicBlock(memory);
            Value* newOffset = insertionSet.insert<Const64Value>(insertionIndex, memory->origin(), address->child(1)->asInt());
            Value* newAddress = insertionSet.insert<Value>(insertionIndex, Add, memory->origin(), address->child(0), newOffset);
            address->replaceWithIdentity(newAddress);
            insertionSet.execute(memory->owner);

            ignoredValues.add(memory);
            ignoredValues.add(address);
        }
    };

    // ...                                  newOffset = Constant
    // ...                                  newAddress = Add(base, newOffset)
    // memory = Load(base, 0)               memory = Load(base, 0)
    // ...                            -->   ...
    // address = Add(base, offset)          address = Identity(newAddress)
    detectPostIndex(postIndexLoadCandidates);

    // ...                                  newOffset = Constant
    // ...                                  newAddress = Add(base, newOffset)
    // memory = Store(value, base, 0)       memory = Store(value, base, 0)
    // ...                            -->   ...
    // address = Add(base, offset)          address = Identity(newAddress)
    detectPostIndex(postIndexStoreCandidates);
    return true;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
