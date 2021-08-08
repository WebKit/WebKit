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

    InsertionSet insertionSet { proc };
    BlockInsertionSet blockInsertionSet { proc };
    unsigned index { 0 };

    Dominators& dominators = proc.dominators();
    BackwardsDominators& backwardsDominators = proc.backwardsDominators();

    HashMap<ValueKey, Vector<Value*>> baseOffsetToAddresses;
    HashMap<MemoryValue*, Value*> preIndexCandidates;
    HashMap<Value*, Vector<MemoryValue*>> baseToMemories;
    HashMap<MemoryValue*, Value*> postIndexCandidates;
    HashMap<Value*, unsigned> valueToIndex;
    IndexSet<Value*> ignoredValues;

    // Pre-Index Pattern:
    //     address = Add(base, offset)
    //     ...
    //     memory = Load(base, offset)
    // Post-Index Pattern:
    //     memory = Load(base, 0)
    //     ...
    //     address = Add(base, offset)
    auto tryPrePostIndex = [&] (Value* value) {
        switch (value->opcode()) {
        case Load: {
            MemoryValue* memory = value->as<MemoryValue>();

            auto tryPrePostIndexMemory = [&] () {
                if (memory->type() != Int32 && memory->type() != Int64)
                    return;
                if (memory->offset()) {
                    if (!Arg::isValidPreIndexForm(memory->offset()))
                        return;
                    ValueKey baseOffsetkey = ValueKey(memory->child(0), static_cast<int64_t>(memory->offset()));
                    if (!baseOffsetToAddresses.contains(baseOffsetkey))
                        return;
                    for (Value* address : baseOffsetToAddresses.get(baseOffsetkey))
                        preIndexCandidates.add(memory, address);
                } else
                    baseToMemories.add(memory->child(0), Vector<MemoryValue*>()).iterator->value.append(memory);
                valueToIndex.add(value, index);
            };

            tryPrePostIndexMemory();
            break;
        }

        case Add: {
            Value* left = value->child(0);
            Value* right = value->child(1);

            auto tryPotentialPostIndexAddress = [&] () {
                if (!right->hasIntPtr() || value->type() != Int64)
                    return;
                intptr_t offset = right->asIntPtr();
                Value::OffsetType smallOffset = static_cast<Value::OffsetType>(offset);
                if (smallOffset != offset || !Arg::isValidPreIndexForm(smallOffset))
                    return;
                // so far this Add value is a valid address candidate for both prefix and postfix pattern
                ValueKey baseOffsetkey = ValueKey(left, static_cast<int64_t>(smallOffset));
                baseOffsetToAddresses.add(baseOffsetkey, Vector<Value*>()).iterator->value.append(value);
                valueToIndex.add(value, index);
                if (!baseToMemories.contains(left))
                    return;
                for (MemoryValue* memory : baseToMemories.get(left))
                    postIndexCandidates.add(memory, value);
            };

            tryPotentialPostIndexAddress();
            break;
        }

        default:
            break;
        }
    };

    for (BasicBlock* basicBlock : proc.blocksInPreOrder()) {
        for (index = 0; index < basicBlock->size(); ++index) {
            Value* value = basicBlock->at(index);
            tryPrePostIndex(value);
        }
    }

    auto controlEquivalent = [&] (Value* v1, Value* v2) -> bool {
        return (dominators.dominates(v1->owner, v2->owner) && backwardsDominators.dominates(v2->owner, v1->owner))
            || (dominators.dominates(v2->owner, v1->owner) && backwardsDominators.dominates(v1->owner, v2->owner));
    };

    auto detect = [&] (HashMap<MemoryValue*, Value*> candidates) {
        for (auto itr = candidates.begin(); itr != candidates.end(); ++itr) {
            MemoryValue* memory = itr->key;
            Value* address = itr->value;
            if (ignoredValues.contains(memory) || ignoredValues.contains(address) || !controlEquivalent(memory, address))
                continue;

            if (candidates == preIndexCandidates) {
                // address = Add(base, offset)       address = Add(base, offset)
                //                              -->  newMemory = Load(base, offset)
                // ...                               ...
                // memory = Load(base, offset)       memory = Identity(newMemory)
                MemoryValue* newMemory = insertionSet.insert<MemoryValue>(valueToIndex.get(address) + 1, Load, memory->type(), address->origin(), memory->lastChild());
                newMemory->setOffset(memory->offset());
                memory->replaceWithIdentity(newMemory);
                insertionSet.execute(address->owner);
            } else {
                //                                   newOffset = Constant
                //                                   newAddress = Add(base, newOffset)
                // memory = Load(base, 0)            memory = Load(base, 0)
                // ...                          -->  ...
                // offset = Constant                 offset = Identity(newOffset)
                // address = Add(base, offset)       address = Identity(newAddress)
                unsigned memoryIndex = valueToIndex.get(memory);
                Value* newOffset = insertionSet.insert<Const64Value>(memoryIndex, memory->origin(), address->child(1)->asInt());
                Value* newAddress = insertionSet.insert<Value>(memoryIndex, Add, memory->origin(), address->child(0), newOffset);
                address->child(1)->replaceWithIdentity(newOffset);
                address->replaceWithIdentity(newAddress);
                insertionSet.execute(memory->owner);
            }

            ignoredValues.add(address);
            ignoredValues.add(memory);
        }
    };

    detect(preIndexCandidates);
    detect(postIndexCandidates);
    return true;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
