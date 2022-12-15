/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "B3Dominators.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
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
        hoistConstants(
            [&] (const Value* value) -> bool {
                return value->opcode() == ConstFloat || value->opcode() == ConstDouble || value->opcode() == Const128;
            });

        lowerMaterializationCostHeavyConstants();
        
        hoistConstants(
            [&] (const Value* value) -> bool {
                return value->opcode() == Const32 || value->opcode() == Const64 || value->opcode() == ArgumentReg;
            });
    }

private:
    template<typename Filter>
    void hoistConstants(const Filter& filter)
    {
        Dominators& dominators = m_proc.dominators();
        HashMap<ValueKey, Value*> valueForConstant;
        IndexMap<BasicBlock*, Vector<Value*>> materializations(m_proc.size());

        // We determine where things get materialized based on where they are used.
        for (BasicBlock* block : m_proc) {
            for (Value* value : *block) {
                for (Value*& child : value->children()) {
                    if (!filter(child))
                        continue;

                    ValueKey key = child->key();
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
            materializations[value->owner].append(value);
        }

        // Get rid of Value's that are fast constants but aren't canonical. Also remove the canonical
        // ones from the CFG, since we're going to reinsert them elsewhere.
        for (BasicBlock* block : m_proc) {
            for (Value*& value : *block) {
                if (!filter(value))
                    continue;

                ValueKey key = value->key();
                if (valueForConstant.get(key) == value)
                    value = m_proc.add<Value>(Nop, value->origin());
                else
                    value->replaceWithNopIgnoringType();
            }
        }

        // Now make sure that we move constants to where they are supposed to go. Again, we do this
        // based on uses.
        for (BasicBlock* block : m_proc) {
            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                Value* value = block->at(valueIndex);
                
                // This finds the outermost (best) block last. So, the functor overrides the result
                // each time it finds something acceptable.
                auto findBestConstant = [&] (const auto& predicate) -> Value* {
                    Value* result = nullptr;
                    dominators.forAllDominatorsOf(
                        block,
                        [&] (BasicBlock* dominator) {
                            for (Value* value : materializations[dominator]) {
                                if (predicate(value)) {
                                    result = value;
                                    break;
                                }
                            }
                        });
                    return result;
                };
                
                // We call this when we have found a constant that we'd like to use. It's possible that
                // we have computed that the constant should be materialized in this block, but we
                // haven't inserted it yet. This inserts the constant if necessary.
                auto materialize = [&] (Value* child) {
                    if (!filter(child))
                        return;

                    // If we encounter a fast constant, then it must be canonical, since we already
                    // got rid of the non-canonical ones.
                    ASSERT(valueForConstant.get(child->key()) == child);

                    if (child->owner != block) {
                        // This constant isn't our problem. It's going to be materialized in another
                        // block.
                        return;
                    }
                    
                    // We're supposed to materialize this constant in this block, and we haven't
                    // done it yet.
                    m_insertionSet.insertValue(valueIndex, child);
                    child->owner = nullptr;
                };
                
                if (MemoryValue* memoryValue = value->as<MemoryValue>()) {
                    Value* pointer = memoryValue->lastChild();
                    if (pointer->hasIntPtr() && filter(pointer)) {
                        auto desiredOffset = [&] (Value* otherPointer) -> intptr_t {
                            // We would turn this:
                            //
                            //     Load(@p, offset = c)
                            //
                            // into this:
                            //
                            //     Load(@q, offset = ?)
                            //
                            // The offset should be c + @p - @q, because then we're loading from:
                            //
                            //     @q + c + @p - @q
                            uintptr_t c = static_cast<uintptr_t>(static_cast<intptr_t>(memoryValue->offset()));
                            uintptr_t p = pointer->asIntPtr();
                            uintptr_t q = otherPointer->asIntPtr();
                            return c + p - q;
                        };
                        
                        Value* bestPointer = findBestConstant(
                            [&] (Value* candidatePointer) -> bool {
                                if (!candidatePointer->hasIntPtr())
                                    return false;
                                
                                int64_t offset = desiredOffset(candidatePointer);
                                return memoryValue->isLegalOffset(offset);
                            });
                        
                        if (bestPointer) {
                            memoryValue->lastChild() = bestPointer;
                            memoryValue->setOffset(static_cast<int32_t>(desiredOffset(bestPointer)));
                        }
                    }
                } else {
                    switch (value->opcode()) {
                    case Add:
                    case Sub: {
                        Value* addend = value->child(1);
                        if (!addend->hasInt() || !filter(addend))
                            break;
                        int64_t addendConst = addend->asInt();
                        Value* bestAddend = findBestConstant(
                            [&] (Value* candidateAddend) -> bool {
                                if (candidateAddend->type() != addend->type())
                                    return false;
                                if (!candidateAddend->hasInt())
                                    return false;
                                return candidateAddend == addend
                                    || candidateAddend->asInt() == -addendConst;
                            });
                        if (!bestAddend || bestAddend == addend)
                            break;
                        materialize(value->child(0));
                        materialize(bestAddend);
                        value->replaceWithIdentity(
                            m_insertionSet.insert<Value>(
                                valueIndex, value->opcode() == Add ? Sub : Add, value->origin(),
                                value->child(0), bestAddend));
                        break;
                    }
                    default:
                        break;
                    }
                }
                
                for (Value* child : value->children())
                    materialize(child);
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

    void lowerMaterializationCostHeavyConstants()
    {
        unsigned floatSize = 0;
        unsigned doubleSize = 0;
        unsigned v128Size = 0;
        HashMap<ValueKey, unsigned> constTable;
        for (Value* value : m_proc.values()) {
            if (!goesInTable(value))
                continue;

            ValueKey key = value->key();
            switch (value->opcode()) {
            case Const128:
                constTable.add(key, v128Size++);
                break;
            case ConstDouble:
                constTable.add(key, doubleSize++);
                break;
            case ConstFloat:
                constTable.add(key, floatSize++);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }

        auto getOffset = [&](Opcode opcode, unsigned indexInKind) -> size_t {
            switch (opcode) {
            case Const128:
                return sizeof(v128_t) * indexInKind;
            case ConstDouble:
                return sizeof(v128_t) * v128Size + sizeof(double) * indexInKind;
            case ConstFloat:
                return sizeof(v128_t) * v128Size + sizeof(double) * doubleSize + sizeof(float) * indexInKind;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        };

        uint8_t* dataSection = static_cast<uint8_t*>(m_proc.addDataSection(sizeof(v128_t) * v128Size + sizeof(double) * doubleSize + sizeof(float) * floatSize));
        for (auto& entry : constTable) {
            auto* pointer = dataSection + getOffset(entry.key.opcode(), entry.value);
            switch (entry.key.opcode()) {
            case Const128:
                *bitwise_cast<v128_t*>(pointer) = entry.key.vectorValue();
                break;
            case ConstDouble:
                *bitwise_cast<double*>(pointer) = entry.key.doubleValue();
                break;
            case ConstFloat:
                *bitwise_cast<float*>(pointer) = entry.key.floatValue();
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }

        IndexSet<Value*> offLimits;
        for (BasicBlock* block : m_proc) {
            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                StackmapValue* value = block->at(valueIndex)->as<StackmapValue>();
                if (!value)
                    continue;

                for (unsigned childIndex = 0; childIndex < value->numChildren(); ++childIndex) {
                    if (!value->constrainedChild(childIndex).rep().isAny())
                        continue;
                    
                    Value*& child = value->child(childIndex);
                    if (!goesInTable(child))
                        continue;

                    ValueKey key = child->key();
                    child = m_insertionSet.insertValue(
                        valueIndex, key.materialize(m_proc, value->origin()));
                    offLimits.add(child);
                }
            }

            m_insertionSet.execute(block);
        }

        for (BasicBlock* block : m_proc) {
            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                Value* value = block->at(valueIndex);
                if (!goesInTable(value))
                    continue;
                if (offLimits.contains(value))
                    continue;

                ValueKey key = value->key();
                auto offset = getOffset(key.opcode(), constTable.get(key));
                if (!isRepresentableAs<Value::OffsetType>(offset))
                    continue;

                Value* tableBase = m_insertionSet.insertIntConstant(
                    valueIndex, value->origin(), pointerType(),
                    bitwise_cast<intptr_t>(dataSection));
                Value* result = m_insertionSet.insert<MemoryValue>(
                    valueIndex, Load, value->type(), value->origin(), tableBase,
                    static_cast<Value::OffsetType>(offset));
                value->replaceWithIdentity(result);
            }

            m_insertionSet.execute(block);
        }
    }

    static bool goesInTable(const Value* value)
    {
        switch (value->opcode()) {
        case ConstDouble: {
            double doubleZero = 0.0;
            return bitwise_cast<uint64_t>(value->asDouble()) != bitwise_cast<uint64_t>(doubleZero);
        }
        case ConstFloat: {
            float floatZero = 0.0;
            return bitwise_cast<uint32_t>(value->asFloat()) != bitwise_cast<uint32_t>(floatZero);
        }
        case Const128: {
            return !bitEquals(value->asV128(), v128_t { });
        }
        default:
            break;
        }
        return false;
    }

    Procedure& m_proc;
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

