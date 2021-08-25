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
#include "B3ValueKeyInlines.h"
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

    Dominators& dominators = proc.dominators();
    BackwardsDominators& backwardsDominators = proc.backwardsDominators();

    IndexSet<Value*> handledValues;
    HashMap<Value*, unsigned> memoryToIndex;
    HashMap<Value*, Vector<Value*>> addUses;
    HashMap<Value*, Vector<MemoryValue*>> baseToMemories;
    HashMap<ValueKey, Vector<Value*>> baseOffsetToAddresses;
    HashMap<MemoryValue*, Vector<Value*>> preIndexCandidates;
    HashMap<MemoryValue*, Vector<Value*>> postIndexCandidates;
    HashMap<BasicBlock*, HashSet<MemoryValue*>> blockToPrePostIndexCandidates;

    // Strength Reduction will leave the IR in the form we're matching now. 
    // It'll take an add(x, constant) that's an address and move the offset 
    // into the load itself, and that's why we can match this redundancy.
    auto tryAddCandidates = [&] (Value* value) {
        switch (value->opcode()) {
        case Load:
        case Store: {
            MemoryValue* memory = value->as<MemoryValue>();
            Value* type = value->opcode() == Load ? memory : memory->child(0);
            if (type->type() != Int32 && type->type() != Int64)
                break;
            if (memory->offset()) {
                // Pre-Index Load/Store Pattern:
                //     address = Add(base, offset)
                //     ...
                //     memory = MemoryValue(base, offset)
                ValueKey baseOffsetKey = ValueKey(memory->lastChild(), memory->offset());
                auto iter = baseOffsetToAddresses.find(baseOffsetKey);
                if (iter == baseOffsetToAddresses.end())
                    break;
                for (Value* address : iter->value) {
                    // The goal is to move the Add to this Load/Store. We only move the Add to this Memory value
                    // if we guarantee it dominates all uses. If there are already other uses of the Add, it guarantees
                    // this Memory value doesn't dominate those uses. This is because we're visiting the program in pre-order
                    // traversal, so we visit this Memory value before all the things it dominates. So if the Add has other
                    // users, we must not dominate those users. Therefore, this MemoryValue won't be a candidate.
                    auto addUsesIter = addUses.find(address);
                    if (addUsesIter != addUses.end() && addUsesIter->value.size() > 0)
                        continue;
                    // double check offsets to avoid ValueKey collisions
                    if (memory->offset() != static_cast<Value::OffsetType>(address->child(1)->asIntPtr()))
                        continue;
                    preIndexCandidates.add(memory, Vector<Value*>()).iterator->value.append(address);
                    blockToPrePostIndexCandidates.add(memory->owner, HashSet<MemoryValue*>()).iterator->value.add(memory);
                }
            } else
                baseToMemories.add(memory->lastChild(), Vector<MemoryValue*>()).iterator->value.append(memory);
            memoryToIndex.add(memory, index);
            break;
        }

        case Add: {
            Value* left = value->child(0);
            Value* right = value->child(1);

            if (!right->hasIntPtr() || value->type() != Int64)
                break;
            intptr_t offset = right->asIntPtr();
            Value::OffsetType smallOffset = static_cast<Value::OffsetType>(offset);
            if (smallOffset != offset || !Arg::isValidIncrementIndexForm(smallOffset))
                break;

            // so far this Add value is a valid address candidate for both prefix and postfix pattern
            addUses.add(value, Vector<Value*>());
            ValueKey baseOffsetKey = ValueKey(left, smallOffset);
            baseOffsetToAddresses.add(baseOffsetKey, Vector<Value*>()).iterator->value.append(value);

            // Post-Index Load/Store Pattern:
            //     memory = MemoryValue(base, 0)
            //     ...
            //     address = Add(base, offset)
            auto iter = baseToMemories.find(left);
            if (iter == baseToMemories.end())
                break;
            for (MemoryValue* memory : iter->value) {
                postIndexCandidates.add(memory, Vector<Value*>()).iterator->value.append(value);
                blockToPrePostIndexCandidates.add(memory->owner, HashSet<MemoryValue*>()).iterator->value.add(memory);
            }
            break;
        }

        default:
            break;
        }

        for (Value* child : value->children()) {
            auto iter = addUses.find(child);
            if (iter != addUses.end())
                iter->value.append(value);
        }
    };

    for (BasicBlock* basicBlock : proc.blocksInPreOrder()) {
        for (index = 0; index < basicBlock->size(); ++index)
            tryAddCandidates(basicBlock->at(index));
    }

    auto controlEquivalent = [&] (Value* v1, Value* v2) -> bool {
        return (dominators.dominates(v1->owner, v2->owner) && backwardsDominators.dominates(v2->owner, v1->owner))
            || (dominators.dominates(v2->owner, v1->owner) && backwardsDominators.dominates(v1->owner, v2->owner));
    };

    for (const auto& pair : blockToPrePostIndexCandidates) {
        BasicBlock* basicBlock = pair.key;
        for (MemoryValue* memory : pair.value) {
            if (handledValues.contains(memory))
                continue;

            // Convert Pre-Index Load/Store Pattern to the Canonical Form:
            //     address = Add(base, offset)                    address = Nop
            //     ...                                            ...
            //     ...                                            newAddress = Add(base, offset)
            //     memory = MemoryValue(base, offset)     -->     memory = MemoryValue(base, offset)
            //     ...                                            ...
            //     parent = B3Opcode(address, ...)                parent = B3Opcode(newAddress, ...)
            for (Value* address : preIndexCandidates.get(memory)) {
                if (handledValues.contains(address) || !controlEquivalent(memory, address))
                    continue;

                auto dominatesAllAddUses = [&] () -> bool {
                    auto iter = addUses.find(address);
                    ASSERT(iter != addUses.end() && iter->value.size());
                    for (Value* parent : iter->value) {
                        if (!dominators.dominates(memory->owner, parent->owner))
                            return false;
                    }
                    return true;
                };

                if (!dominatesAllAddUses())
                    continue;

                unsigned insertionIndex = memoryToIndex.get(memory);
                Value* newAddress = insertionSet.insert<Value>(insertionIndex, Add, memory->origin(), address->child(0), address->child(1));
                for (Value* parent : addUses.get(address)) {
                    for (unsigned i = 0; i < parent->numChildren(); ++i) {
                        Value*& child = parent->child(i);
                        if (child == address)
                            child = newAddress;
                    }
                }
                address->replaceWithNopIgnoringType();

                handledValues.add(memory);
                handledValues.add(address);
            }

            // Convert Post-Index Load/Store Pattern to the Canonical Form:
            //     ...                                  newOffset = Constant
            //     ...                                  newAddress = Add(base, newOffset)
            //     memory = MemoryValue(base, 0)        memory = MemoryValue(base, 0)
            //     ...                            -->   ...
            //     address = Add(base, offset)          address = Identity(newAddress)
            for (Value* address : postIndexCandidates.get(memory)) {
                if (handledValues.contains(address) || !controlEquivalent(memory, address))
                    continue;

                unsigned insertionIndex = memoryToIndex.get(memory);
                Value* newOffset = insertionSet.insert<Const64Value>(insertionIndex, memory->origin(), address->child(1)->asInt());
                Value* newAddress = insertionSet.insert<Value>(insertionIndex, Add, memory->origin(), address->child(0), newOffset);
                address->replaceWithIdentity(newAddress);

                handledValues.add(memory);
                handledValues.add(address);
            }
        }
        // After this executes, memoryToIndex no longer contains up to date information for this block.
        // But that's ok because we never touch this block again.
        insertionSet.execute(basicBlock);
    }

    return true;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
