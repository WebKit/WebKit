/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "AirLowerMacros.h"

#if ENABLE(B3_JIT)

#include "AirCCallingConvention.h"
#include "AirCode.h"
#include "AirEmitShuffle.h"
#include "AirInsertionSet.h"
#include "AirPhaseScope.h"
#include "B3CCallValue.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 { namespace Air {

void lowerMacros(Code& code)
{
    PhaseScope phaseScope(code, "Air::lowerMacros");

    InsertionSet insertionSet(code);
    for (BasicBlock* block : code) {
        for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
            Inst& inst = block->at(instIndex);
            
            auto handleCall = [&] () {
                CCallValue* value = inst.origin->as<CCallValue>();
                Kind oldKind = inst.kind;

                Vector<Arg> destinations = computeCCallingConvention(code, value);
                
                unsigned resultCount = cCallResultCount(value);

                Vector<ShufflePair, 16> shufflePairs;
                bool hasRegisterSource = false;
                unsigned offset = 1;
                auto addNextPair = [&](Width width) {
                    ShufflePair pair(inst.args[offset + resultCount], destinations[offset], width);
                    shufflePairs.append(pair);
                    hasRegisterSource |= pair.src().isReg();
                    ++offset;
                };
                for (unsigned i = 1; i < value->numChildren(); ++i) {
                    Value* child = value->child(i);
#if USE(JSVALUE32_64)
                    if (child->type() == Int64) {
                        addNextPair(Width32);
                        addNextPair(Width32);
                        continue;
                    }
#endif
                    addNextPair(widthForType(child->type()));
                }
                ASSERT(offset = inst.args.size());
                
                if (UNLIKELY(hasRegisterSource))
                    insertionSet.insertInst(instIndex, createShuffle(inst.origin, Vector<ShufflePair>(shufflePairs)));
                else {
                    // If none of the inputs are registers, then we can efficiently lower this
                    // shuffle before register allocation. First we lower all of the moves to
                    // memory, in the hopes that this is the last use of the operands. This
                    // avoids creating interference between argument registers and arguments
                    // that don't go into argument registers.
                    for (ShufflePair& pair : shufflePairs) {
                        if (pair.dst().isMemory())
                            insertionSet.insertInsts(instIndex, pair.insts(code, inst.origin));
                    }
                    
                    // Fill the argument registers by starting with the first one. This avoids
                    // creating interference between things passed to low-numbered argument
                    // registers and high-numbered argument registers. The assumption here is
                    // that lower-numbered argument registers are more likely to be
                    // incidentally clobbered.
                    for (ShufflePair& pair : shufflePairs) {
                        if (!pair.dst().isMemory())
                            insertionSet.insertInsts(instIndex, pair.insts(code, inst.origin));
                    }
                }

                // Indicate that we're using our original callee argument.
                destinations[0] = inst.args[0];

                // Save where the original instruction put its result.
                Arg resultDst0 = resultCount >= 1 ? inst.args[1] : Arg();
#if USE(JSVALUE32_64)
                Arg resultDst1 = resultCount >= 2 ? inst.args[2] : Arg();
#endif

                inst = buildCCall(code, inst.origin, destinations);
                if (oldKind.effects)
                    inst.kind.effects = true;

                switch (value->type().kind()) {
                case Void:
                case Tuple:
                    break;
                case Float:
                    insertionSet.insert(instIndex + 1, MoveFloat, value, cCallResult(value, 0), resultDst0);
                    break;
                case Double:
                    insertionSet.insert(instIndex + 1, MoveDouble, value, cCallResult(value, 0), resultDst0);
                    break;
                case Int32:
                    insertionSet.insert(instIndex + 1, Move32, value, cCallResult(value, 0), resultDst0);
                    break;
                case Int64:
                    insertionSet.insert(instIndex + 1, Move, value, cCallResult(value, 0), resultDst0);
#if USE(JSVALUE32_64)
                    insertionSet.insert(instIndex + 1, Move, value, cCallResult(value, 1), resultDst1);
#endif
                    break;
                }
            };

            switch (inst.kind.opcode) {
            case ColdCCall:
                if (code.optLevel() < 2)
                    handleCall();
                break;
                
            case CCall:
                handleCall();
                break;

            default:
                break;
            }
        }
        insertionSet.execute(block);
    }
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

