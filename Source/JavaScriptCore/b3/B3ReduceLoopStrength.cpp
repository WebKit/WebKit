/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "B3ReduceLoopStrength.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3BlockInsertionSet.h"
#include "B3ConstPtrValue.h"
#include "B3InsertionSet.h"
#include "B3NaturalLoops.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
#include <wtf/SmallPtrSet.h>
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

#if CPU(X86_64)
/*
 * The semantics of B3 require that loads and stores are not reduced in granularity.
 * If we would use system memcpy, then it is possible that we would get a byte copy loop,
 * and this could cause GC tearing.
 */
void fastForwardCopy32(uint32_t* dst, const uint32_t* src, size_t size)
{
    uint64_t i;
    int64_t counter;
    uint64_t tmp, tmp2;

    asm volatile(
        "test %q[size], %q[size]\t\n"
        "jz 900f\t\n" // exit
        "xorq %q[i], %q[i]\t\n"
        // if size < 8
        "cmp $8, %q[size]\t\n"
        "jl 100f\t\n" // backporch
        // if (dst + size*4 <= src || src + size*4 <= dst)
        "leaq (%q[src], %q[size], 4), %q[tmp]\t\n"
        "cmp %q[tmp], %q[dst]\t\n"
        "jae 500f\t\n" // simd no overlap
        "leaq (%q[dst], %q[size], 4), %q[tmp]\t\n"
        "cmp %q[tmp], %q[src]\t\n"
        "jae 500f\t\n" // simd no overlap
        "jmp 100f\t\n" // Backporch

        // Backporch (assumes we can write at least one int)
        "100:\t\n"
        // counter = (size - i)%4
        "mov %q[size], %q[counter]\t\n"
        "subq %q[i], %q[counter]\t\n"

        // If we are a multiple of 4, unroll the loop
        // We already know we are nonzero
        "and $3, %q[counter]\t\n"
        "jz 200f\t\n" // unrolled loop

        "negq %q[counter]\t\n"
        ".balign 32\t\n"
        "101:\t\n"
        "mov (%q[src], %q[i], 4), %k[tmp]\t\n"
        "mov %k[tmp], (%q[dst], %q[i], 4)\t\n"
        "incq %q[i]\t\n"
        "incq %q[counter]\t\n"
        "jnz 101b\t\n"
        // Do we still have anything left?
        "cmpq %q[size], %q[i]\t\n"
        "je 900f\t\n" // exit
        // Then go to the unrolled loop
        "jmp 200f\t\n"

        // Unrolled loop (assumes multiple of 4, can do one iteration)
        "200:\t\n"
        "shr $2, %q[counter]\t\n"
        "negq %q[counter]\t\n"
        ".balign 32\t\n"
        "201:\t\n"
        "mov (%q[src], %q[i], 4), %k[tmp]\t\n"
        "mov %k[tmp], (%q[dst], %q[i], 4)\t\n"
        "mov 4(%q[src], %q[i], 4), %k[tmp2]\t\n"
        "mov %k[tmp2], 4(%q[dst], %q[i], 4)\t\n"
        "mov 8(%q[src], %q[i], 4), %k[tmp2]\t\n"
        "mov %k[tmp2], 8(%q[dst], %q[i], 4)\t\n"
        "mov 12(%q[src], %q[i], 4), %k[tmp]\t\n"
        "mov %k[tmp], 12(%q[dst], %q[i], 4)\t\n"
        "addq $4, %q[i]\t\n"
        "cmpq %q[size], %q[i]\t\n"
        "jb 201b\t\n"
        "jmp 900f\t\n" // exit

        // simd no overlap
        // counter = -(size/8);
        "500:\t\n"
        "mov %q[size], %q[counter]\t\n"
        "shrq $3, %q[counter]\t\n"
        "negq %q[counter]\t\n"
        ".balign 32\t\n"
        "501:\t\n"
        "movups (%q[src], %q[i], 4), %%xmm0\t\n"
        "movups 16(%q[src], %q[i], 4), %%xmm1\t\n"
        "movups %%xmm0, (%q[dst], %q[i], 4)\t\n"
        "movups %%xmm1, 16(%q[dst], %q[i], 4)\t\n"
        "addq $8, %q[i]\t\n"
        "incq %q[counter]\t\n"
        "jnz 501b\t\n"
        // if (i == size)
        "cmpq %q[size], %q[i]\t\n"
        "je 900f\t\n" // end
        "jmp 100b\t\n" // backporch
        "900:\t\n"
    : [i] "=&c" (i), [counter] "=&r" (counter), [tmp] "=&a" (tmp),  [tmp2] "=&r" (tmp2)
    : [dst] "D" (dst), [src] "S" (src), [size] "r" (size)
    : "xmm0", "xmm1");
}
#endif

class ReduceLoopStrength {
    static constexpr bool verbose = false;

    struct AddrInfo {
        Value* appendAddr(Procedure& proc, BasicBlock* block, Value* addr)
        {
            block->appendNew<Value>(proc, Identity, addr->origin(), addr);
            return addr;
        }

        Value* insideLoopAddr;

        Value* arrayBase;
        Value* index;
    };

public:
    ReduceLoopStrength(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
        , m_blockInsertionSet(proc)
        , m_root(proc.at(0))
        , m_phiChildren(proc)
    {
    }

#if CPU(X86_64)
    void reduceByteCopyLoopsToMemcpy()
    {
        HashSet<Value*> patternMatchedValues;

        Value* store = m_value;
        if (store->opcode() != Store)
            return;
        patternMatchedValues.add(store);

        Value* load = store->child(0);
        uint32_t elemSize = sizeofType(load->type());
        if (load->opcode() != Load || elemSize != 4)
            return;
        patternMatchedValues.add(load);

        if (verbose)
            dataLogLn("Found store/load:", *store);

        auto extractArrayInfo = [&] (Value* value) -> Optional<AddrInfo> {
            patternMatchedValues.add(value);

            Optional<AddrInfo> addr = { AddrInfo() };

            Value* sum = value;
            if (value->opcode() == ZExt32)
                sum = sum->child(0);

            if (sum->opcode() != Add || sum->numChildren() != 2)
                return { };

            addr->insideLoopAddr = sum;
            auto extractLoopIndexInSubtree = [&] (Value* value) -> Value* {
                // Match arrBase[i*elemSize]
                if (value->opcode() == Shl
                    && value->child(0)->opcode() == Phi
                    && value->child(1)->isInt(WTF::fastLog2(elemSize)))
                    return value->child(0);
                if (value->opcode() == Shl
                    && value->child(0)->opcode() == ZExt32
                    && value->child(0)->child(0)->opcode() == Phi
                    && value->child(1)->isInt(WTF::fastLog2(elemSize)))
                    return value->child(0)->child(0);
                return nullptr;
            };

            // Try to find a known pattern applied to a single phi, which we can assume is our loop index.
            Value* left = extractLoopIndexInSubtree(sum->child(0));
            Value* right = extractLoopIndexInSubtree(sum->child(1));
            if (left && !right) {
                addr->index = left;
                addr->arrayBase = sum->child(1);
            } else if (right && !left) {
                addr->index = right;
                addr->arrayBase = sum->child(0);
            } else
                return { };

            patternMatchedValues.add(addr->index);
            patternMatchedValues.add(addr->arrayBase);

            return addr;
        };

        auto destination = extractArrayInfo(store->child(1));
        auto source = extractArrayInfo(load->child(0));

        if (!source || !destination || source->index != destination->index)
            return;

        if (destination->arrayBase->type() != Int64 || source->arrayBase->type() != Int64)
            return;

        if (verbose)
            dataLogLn("Found array info: ", *source->arrayBase, "[", *source->index, "] = ", *destination->arrayBase, "[", *destination->index, "]");

        // Find the loop header, footer and inner blocks.
        // Verify that there is one entrance to and one exit from the loop.
        auto* loop = m_proc.naturalLoops().innerMostLoopOf(m_block);
        if (!loop)
            return;
        BasicBlock* loopHead = loop->header();
        BasicBlock* loopFoot = nullptr;
        BasicBlock* loopPreheader = nullptr;
        BasicBlock* loopPostfooter = nullptr;
        SmallPtrSet<BasicBlock*> loopInnerBlocks;

        {
            for (unsigned i = 0; i < loop->size(); ++i)
                loopInnerBlocks.add(loop->at(i));

            if (!loopInnerBlocks.contains(load->owner))
                return;

            if (!loopInnerBlocks.contains(store->owner))
                return;

            SmallPtrSet<BasicBlock*> loopEntrances;
            SmallPtrSet<BasicBlock*> loopExits;
            for (auto* block : loopInnerBlocks) {
                for (auto successor : block->successors()) {
                    if (!loopInnerBlocks.contains(successor.block()))
                        loopExits.add(successor.block());
                }
                for (auto* predecessor : block->predecessors()) {
                    if (!loopInnerBlocks.contains(predecessor))
                        loopEntrances.add(predecessor);
                }
            }

            if (loopExits.size() != 1) {
                if (verbose) {
                    dataLog("Loop has incorrect number of exits: ");
                    for (BasicBlock* block : loopExits)
                        dataLog(*block, " ");
                    dataLogLn();
                }
                return;
            }
            loopPostfooter = *loopExits.begin();

            if (loopEntrances.size() != 1) {
                if (verbose) {
                    dataLog("Loop has incorrect number of entrances: ");
                    for (BasicBlock* block : loopEntrances)
                        dataLog(*block, " ");
                    dataLogLn();
                }
                return;
            }
            loopPreheader = *loopEntrances.begin();

            if (!loopHead->predecessors().contains(loopPreheader)) {
                if (verbose)
                    dataLogLn("Loop head does not follow preheader");
                return;
            }

            for (BasicBlock* predecessor : loopPostfooter->predecessors()) {
                if (loopInnerBlocks.contains(predecessor)) {
                    if (loopFoot) {
                        if (verbose)
                            dataLogLn("Loop has more than one exit point");
                        return;
                    }
                    loopFoot = predecessor;
                }
            }
        }

        if (verbose) {
            dataLog("Found loop head:", *loopHead, " foot:", *loopFoot, " prehead ", *loopPreheader, " postfoot ", *loopPostfooter, " inner blocks: ");
            for (BasicBlock* block : loopInnerBlocks)
                dataLog(*block, " ");
            dataLogLn();
        }

        // Verify that the index is a monotonic inductive variable.
        Value* startingIndex = nullptr;
        {
            if (m_phiChildren.at(source->index).size() != 2)
                return;

            Value* updateIndex = nullptr;
            for (Value* up : m_phiChildren.at(source->index)) {
                if (m_proc.dominators().dominates(up->owner, loopPreheader))
                    startingIndex = up;
                else
                    updateIndex = up;
            }

            if (!updateIndex || !startingIndex || !loopInnerBlocks.contains(updateIndex->owner))
                return;
            patternMatchedValues.add(updateIndex);
            patternMatchedValues.add(startingIndex);

            updateIndex = updateIndex->child(0);
            startingIndex = startingIndex->child(0);
            if (updateIndex->opcode() != Add || !updateIndex->child(1)->isInt(1) || updateIndex->child(0) != source->index)
                return;

            if (!startingIndex->isInt(0))
                return;
        }

        if (verbose)
            dataLogLn("Found starting index:", *startingIndex);

        // Identify the loop exit condition.
        Value* exitCondition = nullptr;
        // Is this an exit condition or a continuation condition?
        bool conditionInverted = false;
        // Do we check the index before or after using it?
        bool checkBeforeWrite = false;
        {
            Value* branch = loopFoot->last();
            if (branch->opcode() != Branch)
                return;
            patternMatchedValues.add(branch);
            exitCondition = branch->child(0);

            conditionInverted = loopPostfooter != loopFoot->successor(0).block();
            checkBeforeWrite = m_proc.dominators().strictlyDominates(loopFoot, m_block);
        }

        if (verbose)
            dataLogLn("Found exit condition: ", *exitCondition, " inverted: ", conditionInverted, " checks before the first write: ", checkBeforeWrite);

        // Idenfity the loop bound by matching loop bound expressions.
        Value* loopBound = nullptr;
        {
            auto extractLoopBound = [&] (Value* index, Value* bound) -> Value* {
                // Match i+1 when we do a write first followed by the check for the next iteration
                if (!checkBeforeWrite && index->opcode() == Add && index->child(0) == source->index && index->child(1)->isInt(1))
                    return bound;
                return nullptr;
            };

            if (exitCondition->opcode() == GreaterThan && conditionInverted) {
                // enter loop again if size > i
                loopBound = extractLoopBound(exitCondition->child(1), exitCondition->child(0));
            } else if (exitCondition->opcode() == LessThan && !conditionInverted) {
                // enter loop again if size < i
                loopBound = extractLoopBound(exitCondition->child(1), exitCondition->child(0));
            }

            if (!loopBound) {
                if (verbose)
                    dataLogLn("Unable to extract bound from exit condition ", *exitCondition);
                return;
            }
        }

        // Make sure there are no other effectful things happening.
        // This also guarantees that we found the only branch in the loop. This means that our
        // load/store must happen iff our index update and branch do.
        for (BasicBlock* block : loopInnerBlocks) {
            for (unsigned index = 0; index < block->size(); ++index) {
                Value* value = block->at(index);
                if (patternMatchedValues.contains(value) || value->opcode() == Jump)
                    continue;

                Effects effects = value->effects();
                effects.readsLocalState = false;
                if (effects != Effects::none()) {
                    if (verbose)
                        dataLogLn("Not byte copy loop, node ", *value, " has effects");
                    return;
                }
            }
        }

        if (!hoistValue(loopPreheader, source->arrayBase, false)
            || !hoistValue(loopPreheader, destination->arrayBase, false)
            || !hoistValue(loopPreheader, loopBound, false))
            return;

        if (verbose)
            dataLogLn("Found byte copy loop: ", *source->arrayBase, "[", *source->index, "] = ", *destination->arrayBase, "[", *destination->index, "] from index ", *startingIndex, " to max index ", *loopBound, " stride: ", elemSize);

        bool hoistResult = hoistValue(loopPreheader, source->arrayBase);
        hoistResult &= hoistValue(loopPreheader, destination->arrayBase);
        hoistResult &= hoistValue(loopPreheader, loopBound);
        ASSERT_UNUSED(hoistResult, hoistResult);

        auto origin = loopPostfooter->at(0)->origin();

        BasicBlock* memcpy = m_blockInsertionSet.insertBefore(loopPostfooter, loopPostfooter->frequency());
        memcpy->setSuccessors(FrequentedBlock(loopPostfooter));
        memcpy->addPredecessor(loopPreheader);
        while (loopPostfooter->predecessors().size()) 
            loopPostfooter->removePredecessor(loopPostfooter->predecessors()[0]);
        loopPostfooter->addPredecessor(memcpy);
        loopPreheader->setSuccessors(memcpy);

        Effects effects = Effects::forCall();
        memcpy->appendNew<CCallValue>(m_proc, B3::Void, origin, effects,
            memcpy->appendNew<ConstPtrValue>(m_proc, origin, tagCFunction<B3CCallPtrTag>(fastForwardCopy32)),
            destination->appendAddr(m_proc, memcpy, destination->arrayBase),
            source->appendAddr(m_proc, memcpy, source->arrayBase),
            loopBound);

        memcpy->appendNew<Value>(m_proc, Jump, origin);
    }

    bool hoistValue(BasicBlock* into, Value* value, bool canMutate = true)
    {
        if (m_proc.dominators().dominates(value->owner, into))
            return true;

        Effects effects = value->effects();
        if (effects != Effects::none()) {
            if (verbose)
                dataLogLn("Cannot hoist ", *value, " into ", *into, " since it has effects");
            return false;
        }

        for (Value* child : value->children()) {
            if (!hoistValue(into, child, false))
                return false;
        }

        if (canMutate) {
            for (Value* child : value->children())
                hoistValue(into, child);

            value->owner->at(value->index()) = m_proc.add<Value>(Nop, Void, value->origin());
            into->appendNonTerminal(value);
        }

        return true;
    }

    bool run()
    {
        if (verbose)
            dataLogLn("ReduceLoopStrength examining proc: ", m_proc);
        bool result = false;

        do {
            m_changed = false;

            for (BasicBlock* block : m_proc.blocksInPreOrder()) {
                m_block = block;

                for (m_index = 0; m_index < block->size(); ++m_index) {
                    m_value = m_block->at(m_index);
                    m_value->performSubstitution();
                    reduceByteCopyLoopsToMemcpy();
                    m_insertionSet.execute(m_block);
                    m_changed |= m_blockInsertionSet.execute();

                    if (m_changed) {
                        m_proc.resetReachability();
                        m_proc.invalidateCFG();
                        m_proc.resetValueOwners();
                        result = true;
                        break;
                    }
                }

                if (m_changed)
                    break;
            }
        } while (m_changed && m_proc.optLevel() >= 2);

        if (result && verbose)
            dataLogLn("ReduceLoopStrength produced proc: ", m_proc);

        return result;
    }
#else
    bool run()
    {
        return false;
    }
#endif

    Procedure& m_proc;
    InsertionSet m_insertionSet;
    BlockInsertionSet m_blockInsertionSet;
    BasicBlock* m_root { nullptr };
    BasicBlock* m_block { nullptr };
    unsigned m_index { 0 };
    Value* m_value { nullptr };
    bool m_changed { false };
    PhiChildren m_phiChildren;
};

bool reduceLoopStrength(Procedure& proc)
{
    PhaseScope phaseScope(proc, "reduceLoopStrength");
    return ReduceLoopStrength(proc).run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
