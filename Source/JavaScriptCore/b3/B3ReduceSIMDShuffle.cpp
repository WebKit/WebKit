/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 the V8 project authors. All rights reserved.
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
#include "B3ReduceSIMDShuffle.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlock.h"
#include "B3Const128Value.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3SIMDValue.h"
#include "B3UseCounts.h"
#include "B3ValueInlines.h"
#include "SIMDShuffle.h"
#include <wtf/HashMap.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::B3 {

namespace {

namespace B3ReduceSIMDShuffleInternal {
static constexpr bool verbose = false;
}

class SIMDShuffleReduction {
public:
    SIMDShuffleReduction(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
    {
    }

    void run()
    {
        composeShuffles();
    }

private:
    // Compose shuffle-of-shuffle patterns into a single shuffle.
    //
    // We look for a 3-child (binary) VectorSwizzle whose child(0) or child(1) is a
    // 2-child (unary) VectorSwizzle with a single use. The outer and inner shuffle
    // patterns are composed into a single pattern, eliminating one shuffle operation.
    //
    // Before:
    //   inner = VectorSwizzle(innerSrc, innerPattern)          ; unary shuffle
    //   outer = VectorSwizzle(inner, other, outerPattern)      ; binary shuffle using inner's result
    //   -- or --
    //   outer = VectorSwizzle(other, inner, outerPattern)      ; binary shuffle with inner on other side
    //
    // After composition:
    //   composed = VectorSwizzle(innerSrc, other, composedPattern)   ; single binary shuffle
    //   -- or, if all composed indices come from one side --
    //   composed = VectorSwizzle(src, composedPattern)               ; single unary shuffle
    //
    // The composedPattern is computed by chasing each output byte through the outer pattern,
    // and if it references the inner's output, further chasing through the inner pattern to
    // find the original source byte.
    //
    // Example (from argon2/BLAKE2b):
    //   inner = VectorSwizzle(b, dupLowPattern)                ; dup low 64-bit element of b
    //   outer = VectorSwizzle(a, inner, {8..15, 24..31})       ; UZP2.2D: {a[hi64], inner[hi64]}
    //   Composed: {a[hi64], b[lo64]} — inner[hi64] = dupLow(b)[hi64] = b[lo64]
    //   Result: VectorSwizzle(a, b, {8..15, 16..23})           ; EXT #8
    //
    // This runs iteratively (up to maxIterations) because composing one level may
    // expose another composition opportunity.
    void composeShuffles()
    {
        UseCountsWithoutUsingInstructions useCounts(m_proc);
        bool changed = true;
        unsigned iterations = 0;
        constexpr unsigned maxIterations = 4;

        while (changed && iterations < maxIterations) {
            changed = false;
            ++iterations;

            for (BasicBlock* block : m_proc) {
                for (unsigned index = 0; index < block->size(); ++index) {
                    Value* value = block->at(index);
                    if (value->opcode() != VectorSwizzle)
                        continue;
                    if (value->numChildren() != 3)
                        continue;

                    Value* patternValue = value->child(2);
                    if (!patternValue->isConstant())
                        continue;

                    v128_t outerPattern = patternValue->as<Const128Value>()->value();

                    for (unsigned childIdx = 0; childIdx < 2; ++childIdx) {
                        Value* inner = value->child(childIdx);
                        // The inner must be a single-use unary VectorSwizzle with a constant pattern.
                        if (inner->opcode() != VectorSwizzle)
                            continue;
                        if (inner->numChildren() != 2)
                            continue;
                        if (useCounts.numUses(inner) != 1)
                            continue;
                        if (!inner->child(1)->isConstant())
                            continue;

                        v128_t innerPattern = inner->child(1)->as<Const128Value>()->value();
                        // Compose: for each output byte, chase through outer → inner patterns
                        // to find the original source byte index.
                        v128_t newPattern = SIMDShuffle::composeShuffle(outerPattern, innerPattern, childIdx == 0);

                        // After composition, the new shuffle reads from innerSrc (the inner's
                        // original input) and other (the outer's other child).
                        Value* innerSrc = inner->child(0);
                        Value* other = value->child(1 - childIdx);

                        // Maintain the convention: child(0) = newChild0, child(1) = newChild1.
                        // If inner was child(0), innerSrc becomes newChild0 and other stays newChild1.
                        // If inner was child(1), other stays newChild0 and innerSrc becomes newChild1.
                        Value* newChild0;
                        Value* newChild1;
                        if (childIdx == 0) {
                            newChild0 = innerSrc;
                            newChild1 = other;
                        } else {
                            newChild0 = other;
                            newChild1 = innerSrc;
                        }

                        // If all composed indices reference only one side (0..15 or 16..31),
                        // emit a 2-child unary shuffle instead of a 3-child binary shuffle.
                        // This enables further optimizations like DUP detection in ReduceStrength.
                        if (auto side = SIMDShuffle::isOnlyOneSideMask(newPattern)) {
                            Value* src;
                            v128_t unaryPattern = newPattern;
                            if (*side == 0)
                                src = newChild0;
                            else {
                                src = newChild1;
                                for (unsigned i = 0; i < 16; ++i)
                                    unaryPattern.u8x16[i] -= 16;
                            }
                            Value* newPat = m_proc.addConstant(value->origin(), B3::V128, unaryPattern);
                            m_insertionSet.insertValue(index, newPat);
                            Value* newShuffle = m_insertionSet.insert<SIMDValue>(
                                index, value->origin(), VectorSwizzle, B3::V128,
                                SIMDLane::i8x16, SIMDSignMode::None, src, newPat);
                            value->replaceWithIdentity(newShuffle);
                        } else {
                            Value* newPat = m_proc.addConstant(value->origin(), B3::V128, newPattern);
                            m_insertionSet.insertValue(index, newPat);
                            Value* newShuffle = m_insertionSet.insert<SIMDValue>(
                                index, value->origin(), VectorSwizzle, B3::V128,
                                SIMDLane::i8x16, SIMDSignMode::None, newChild0, newChild1, newPat);
                            value->replaceWithIdentity(newShuffle);
                        }

                        changed = true;
                        dataLogLnIf(B3ReduceSIMDShuffleInternal::verbose, "Composed shuffle: ", *value);
                        break;
                    }
                }

                m_insertionSet.execute(block);
            }

            if (changed)
                useCounts = UseCountsWithoutUsingInstructions(m_proc);
        }
    }

    Procedure& m_proc;
    InsertionSet m_insertionSet;
};

} // anonymous namespace

void reduceSIMDShuffle(Procedure& procedure)
{
    PhaseScope phaseScope(procedure, "reduceSIMDShuffle"_s);

    SIMDShuffleReduction reduction(procedure);
    reduction.run();
}

} // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
