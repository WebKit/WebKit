/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "AirEmitShuffle.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirInstInlines.h"
#include "CCallHelpers.h"
#include <wtf/GraphNodeWorklist.h>
#include <wtf/ListDump.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

namespace AirEmitShuffleInternal {
static constexpr bool verbose = false;
}

} // anonymous namespace

Bank ShufflePair::bank() const
{
    if (src().isMemory() && dst().isMemory() && width() > pointerWidth()) {
        // 8-byte memory-to-memory moves on a 32-bit platform are best handled as float moves.
        return FP;
    }
    
    if (src().isGP() && dst().isGP()) {
        // This means that gpPairs gets memory-to-memory shuffles. The assumption is that we
        // can do that more efficiently using GPRs, except in the special case above.
        return GP;
    }
    
    return FP;
}

void emitMoveForPair(InsertionLocation insertionLocation, const ShufflePair& pair, const Arg& scratch, Value* origin)
{
    const auto& src = pair.src();
    const auto& dst = pair.dst();
    auto bank = pair.bank();
    auto width = pair.width();
    auto moveOp = moveFor(bank, width);
    if (src.isMemory() && dst.isMemory()) [[unlikely]] {
        insertionLocation.insert(Inst(moveOp, origin, src, dst, scratch));
        return;
    }

    if (isValidForm(moveOp, src.kind(), dst.kind())) {
        insertionLocation.insert(Inst(moveOp, origin, src, dst));
        return;
    }

    // We must be a store immediate or a move immediate if we reach here. The reason:
    // 1. We're not a mem->mem move, given the above check.
    // 2. It's always valid to do a load from Addr into a tmp using Move/Move32/MoveFloat/MoveDouble.
    ASSERT(isValidForm(moveOp, Arg::Addr, Arg::Tmp));
    // 3. It's also always valid to do a Tmp->Tmp move.
    ASSERT(isValidForm(moveOp, Arg::Tmp, Arg::Tmp));
    // 4. It's always valid to do a Tmp->Addr store.
    ASSERT(isValidForm(moveOp, Arg::Tmp, Arg::Addr));

    ASSERT(src.isSomeImm());
    ASSERT(isValidForm(Move, Arg::BigImm, Arg::Tmp));
    ASSERT(isValidForm(moveOp, Arg::Tmp, dst.kind()));

    if (dst.isMemory()) {
        insertionLocation.insert(Inst(Move, origin, Arg::bigImm(src.value()), scratch));
        insertionLocation.insert(Inst(moveOp, origin, scratch, dst));
        return;
    }

    // If we aren't storing the immediate, then we don't need a second move
    // into the destination - the immediate should already be the correct
    // representation for the destination type.
    insertionLocation.insert(Inst(Move, origin, Arg::bigImm(src.value()), dst));
}

void ShufflePair::dump(PrintStream& out) const
{
    out.print(width(), ":", src(), "=>", dst());
}

Inst createShuffle(Value* origin, const Vector<ShufflePair>& pairs)
{
    Inst result(Shuffle, origin);
    for (const ShufflePair& pair : pairs)
        result.append(pair.src(), pair.dst(), Arg::widthArg(pair.width()));
    return result;
}

using ShuffleStatus = CCallHelpers::ShuffleStatus;
using StatusVector = Vector<ShuffleStatus, 16>;

static void emitShufflePair(InsertionLocation insertionLocation, Vector<ShufflePair, 8>& pairs, StatusVector& status, unsigned index, const std::array<Arg, 2>& scratches, Value* origin)
{
    ShufflePair& pair = pairs[index];
    ASSERT(pair.src() != pair.dst());

    status[index] = ShuffleStatus::BeingMoved;
    for (unsigned i = 0; i < pairs.size(); i ++) {
        if (pairs[i].src() == pair.dst()) {
            switch (status[i]) {
            case ShuffleStatus::ToMove:
                emitShufflePair(insertionLocation, pairs, status, i, scratches, origin);
                break;
            case ShuffleStatus::BeingMoved: {
                emitMoveForPair(insertionLocation, ShufflePair(pairs[i].src(), scratches[0], pairs[i].width()), scratches[1], origin);
                pairs[i].setSrc(scratches[0]);
                break;
            }
            case ShuffleStatus::Moved:
                break;
            }
        }
    }
    emitMoveForPair(insertionLocation, pair, scratches[1], origin);
    status[index] = ShuffleStatus::Moved;
}

void emitShuffle(
    InsertionLocation insertionLocation, Vector<ShufflePair, 8>& pairs, std::array<Arg, 2> scratches, Bank bank,
    Value* origin)
{
    if (AirEmitShuffleInternal::verbose) {
        dataLog(
            "Dealing with pairs: ", listDump(pairs), " and scratches ", scratches[0], ", ", scratches[1], "\n");
    }
    
    pairs.removeAllMatching(
        [&] (const ShufflePair& pair) -> bool {
            return pair.src() == pair.dst();
        });
    
    if (pairs.isEmpty())
        return;

    if (pairs.size() == 1) {
        emitMoveForPair(insertionLocation, pairs[0], scratches[0], origin);
        return;
    }

    // First validate that this is the kind of shuffle that we know how to deal with.
#if ASSERT_ENABLED
    for (const ShufflePair& pair : pairs) {
        ASSERT(pair.src().isBank(bank));
        ASSERT(pair.dst().isBank(bank));
        ASSERT(pair.dst().isTmp() || pair.dst().isMemory());
    }
#else
    UNUSED_VARIABLE(bank);
#endif // ASSERT_ENABLED

    StatusVector statusVector(pairs.size(), ShuffleStatus::ToMove);
    for (unsigned i = 0; i < pairs.size(); i ++) {
        if (statusVector[i] == ShuffleStatus::ToMove)
            emitShufflePair(insertionLocation, pairs, statusVector, i, scratches, origin);
    }
}

void emitShuffle(
    InsertionLocation insertionLocation, const Vector<ShufflePair>& pairs,
    const std::array<Arg, 2>& gpScratch, const std::array<Arg, 2>& fpScratch,
    Value* origin)
{
    Vector<ShufflePair, 8> gpPairs;
    Vector<ShufflePair, 8> fpPairs;
    for (const ShufflePair& pair : pairs) {
        switch (pair.bank()) {
        case GP:
            gpPairs.append(pair);
            break;
        case FP:
            fpPairs.append(pair);
            break;
        }
    }

    emitShuffle(insertionLocation, gpPairs, gpScratch, GP, origin);
    emitShuffle(insertionLocation, fpPairs, fpScratch, FP, origin);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

