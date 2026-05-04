/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AirOptimizePairedLoadStore.h"

#if ENABLE(B3_JIT)
#if CPU(ARM64)

#include "AirArgInlines.h"
#include "AirCode.h"
#include "AirInst.h"
#include "AirInstInlines.h"
#include "AirPhaseScope.h"
#include "CCallHelpers.h"
#include <wtf/Range.h>

namespace JSC { namespace B3 { namespace Air {
namespace AirOptimizePairedLoadStoreInternal {
static constexpr bool verbose = false;
static constexpr unsigned scanInstructions = 16;
}

static inline Width NODELETE accessWidth(Opcode opcode)
{
    switch (opcode) {
    case Move:
        return pointerWidth();
    case Move32:
        return Width32;
    case MoveFloat:
        return Width32;
    case MoveDouble:
        return Width64;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return Width8;
    }
}

enum class PairDirection : uint8_t { Store, Load };

static bool tryMergePair(Code& code, BasicBlock* block, unsigned current, Inst& inst, PairDirection direction)
{
    // For a store: inst = "Move Tmp/ZeroReg, Addr" — args[0] is source value, args[1] is destination memory.
    // For a load:  inst = "Move Addr, Tmp"         — args[0] is source memory,  args[1] is destination register.
    // In both cases the "other" instruction we try to fuse with must be of the same kind and shape,
    // and the Addr operands must share a base register with offsets differing by exactly the element width.
    //
    // We merge at the earlier (target) position and drop the later (inst) instruction. For stores this
    // effectively moves the later store earlier; the existing interference/clobber checks cover that.
    // For loads this effectively moves the later load earlier, so we additionally require that the later
    // load's destination register is neither used nor defined between the two loads — otherwise those
    // intervening uses would incorrectly observe the post-ldp value.
    const bool isLoad = direction == PairDirection::Load;
    const unsigned addrIndex = isLoad ? 0 : 1;
    const unsigned valueIndex = isLoad ? 1 : 0;

    Width instWidth = accessWidth(inst.kind.opcode);
    int64_t instOffset = static_cast<int64_t>(inst.args[addrIndex].offset());
    unsigned limit = std::min(current, AirOptimizePairedLoadStoreInternal::scanInstructions);
    RegisterSet clobbered;
    RegisterSet intermediateUses;
    for (unsigned count = 1; count <= limit; ++count) {
        unsigned index = current - count;
        Inst& target = block->at(index);

        auto logFound = [&](const Inst& newInst) {
            if (AirOptimizePairedLoadStoreInternal::verbose) {
                dataLogLn("FOUND  ", inst, " ", target, " => ", newInst);
                for (unsigned i = 1; i < count; ++i)
                    dataLogLn("    ", block->at(current - i));
            }
        };

        auto logFailed = [&]() {
            if (AirOptimizePairedLoadStoreInternal::verbose) {
                dataLogLn("FAILED ", inst, " ", target);
                for (unsigned i = 1; i < count; ++i)
                    dataLogLn("    ", block->at(current - i));
            }
        };

        // If the instruction has some special effect (including Patchpoint), we give up since we cannot model the effect of this.
        if (target.hasNonArgEffects()) {
            logFailed();
            return false;
        }

        // If some instructions between the current and target clobbers registers used for current / target,
        // then we cannot merge them since the current instruction's registers are changed.
        //
        //     stur %x1, [%fp]
        //     movz %x2, #0
        //     stur %x2, [%fp, #8]
        //
        // Then we cannot make it to
        //
        //     stp %x1, %x2, [%fp]
        //     movz %x2, #0
        Inst::forEachDefWithExtraClobberedRegs<Tmp>(
            &block->at(index), &block->at(index + 1),
            [&] (const Tmp& arg, Arg::Role, Bank, Width, PreservedWidth) {
                clobbered.add(arg.reg(), IgnoreVectors);
            });
        if (clobbered.contains(inst.args[addrIndex].base().reg(), IgnoreVectors)) {
            logFailed();
            return false;
        }
        if (!isLoad && inst.args[valueIndex].isTmp() && clobbered.contains(inst.args[valueIndex].reg(), IgnoreVectors)) {
            logFailed();
            return false;
        }
        if (isLoad) {
            // Moving the later load earlier is only safe if nothing between the target and inst positions
            // reads or writes the later load's destination register. We already tracked defs in `clobbered`;
            // now also accumulate uses so we can bail if the destination is observed between.
            Inst::forEachUse<Tmp>(
                &block->at(index), &block->at(index + 1),
                [&] (const Tmp& arg, Arg::Role, Bank, Width) {
                    intermediateUses.add(arg.reg(), IgnoreVectors);
                });
            if (clobbered.contains(inst.args[valueIndex].reg(), IgnoreVectors)
                || intermediateUses.contains(inst.args[valueIndex].reg(), IgnoreVectors)) {
                logFailed();
                return false;
            }
        }

        {
            // If some instructions between the current and target have memory-load or memory-store,
            // then we cannot merge them since reordering can change the results.
            // But this is really pessimistic: if the base is the same to the current instruction, and if the offset
            // is different from the current instruction, it is OK actually.

            bool interfere = false;

            auto clobberMemory = [&](const Tmp& argBase, int64_t argOffset, Width argWidth) {
                if (argBase == inst.args[addrIndex].base()) {
                    Range<int64_t> argRange(argOffset, argOffset + bytesForWidth(argWidth));
                    Range<int64_t> instRange(instOffset, instOffset + bytesForWidth(instWidth));
                    return argRange.overlaps(instRange);
                }

                if ((argBase == Tmp(CCallHelpers::stackPointerRegister) || argBase == Tmp(GPRInfo::callFrameRegister)) && (inst.args[addrIndex].base() == Tmp(CCallHelpers::stackPointerRegister) || inst.args[addrIndex].base() == Tmp(GPRInfo::callFrameRegister))) {
                    int64_t instOffsetFromFP = instOffset;
                    if (inst.args[addrIndex].base() == Tmp(CCallHelpers::stackPointerRegister))
                        instOffsetFromFP = instOffset - code.frameSize();

                    int64_t argOffsetFromFP = argOffset;
                    if (argBase == Tmp(CCallHelpers::stackPointerRegister))
                        argOffsetFromFP = argOffset - code.frameSize();

                    Range<int64_t> argRange(argOffsetFromFP, argOffsetFromFP + bytesForWidth(argWidth));
                    Range<int64_t> instRange(instOffsetFromFP, instOffsetFromFP + bytesForWidth(instWidth));
                    return argRange.overlaps(instRange);
                }

                return true;
            };

            auto checkInterfere = [&](const Arg& arg, Arg::Role, Bank, Width width) {
                if (!arg.isMemory())
                    return;
                if (arg.isAddr()) {
                    if (!clobberMemory(arg.base(), static_cast<int64_t>(arg.offset()), width))
                        return;
                }
                if (arg.isSimpleAddr()) {
                    if (!clobberMemory(arg.base(), 0, width))
                        return;
                }
                interfere = true;
            };

            // For load merging we only need to worry about intervening memory writes (loads do not
            // change memory), whereas for store merging any aliasing load or store between matters.
            if (!isLoad) {
                Inst::forEachUse<Arg>(&block->at(index), &block->at(index + 1), checkInterfere);
                if (interfere) {
                    logFailed();
                    return false;
                }
            }
            Inst::forEachDef<Arg>(&block->at(index), &block->at(index + 1), checkInterfere);
            if (interfere) {
                logFailed();
                return false;
            }
        }

        if (target.kind != inst.kind)
            continue;

        if (target.args.size() != 2)
            continue;

        if (isLoad) {
            if (!target.args[addrIndex].isAddr() || !target.args[valueIndex].isTmp())
                continue;
        } else {
            if ((!target.args[valueIndex].isTmp() && !target.args[valueIndex].isZeroReg()) || !target.args[addrIndex].isAddr())
                continue;
        }

        if (target.args[addrIndex].base() != inst.args[addrIndex].base())
            continue;

        // When we fuse two loads into an ldp we would produce `ldp Rn, Rn, [base]`, which is an
        // illegal instruction on ARM64. (Two stp sources to the same register are legal.)
        if (isLoad && target.args[valueIndex].isTmp() && inst.args[valueIndex].isTmp()
            && target.args[valueIndex].tmp() == inst.args[valueIndex].tmp())
            continue;

        Opcode pairOpcode = StorePair32;
        switch (inst.kind.opcode) {
        case Move32:
            pairOpcode = isLoad ? LoadPair32 : StorePair32;
            break;
        case Move:
            pairOpcode = isLoad ? LoadPair64 : StorePair64;
            break;
        case MoveDouble:
            pairOpcode = isLoad ? LoadPairDouble : StorePairDouble;
            break;
        case MoveFloat:
            pairOpcode = isLoad ? LoadPairFloat : StorePairFloat;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        auto isValidOffset = [&](auto offset) {
            switch (inst.kind.opcode) {
            case Move32:
                return isLoad ? ARM64Assembler::isValidLDPImm<32>(offset) : ARM64Assembler::isValidSTPImm<32>(offset);
            case Move:
                return isLoad ? ARM64Assembler::isValidLDPImm<64>(offset) : ARM64Assembler::isValidSTPImm<64>(offset);
            case MoveDouble:
                return isLoad ? ARM64Assembler::isValidLDPFPImm<64>(offset) : ARM64Assembler::isValidSTPFPImm<64>(offset);
            case MoveFloat:
                return isLoad ? ARM64Assembler::isValidLDPFPImm<32>(offset) : ARM64Assembler::isValidSTPFPImm<32>(offset);
            default:
                RELEASE_ASSERT_NOT_REACHED();
                return false;
            }
        };

        int64_t targetOffset = static_cast<int64_t>(target.args[addrIndex].offset());

        // Produce the merged instruction with the same operand ordering used by the pair opcodes:
        //   Store pair: (srcLow, srcHigh, Addr)
        //   Load  pair: (Addr, destLow, destHigh)
        auto makeInst = [&](Arg lowValue, Arg highValue, Arg addr) {
            if (isLoad)
                return Inst(pairOpcode, target.origin, addr, lowValue, highValue);
            return Inst(pairOpcode, target.origin, lowValue, highValue, addr);
        };

        if (isValidOffset(instOffset) && targetOffset == (instOffset + bytesForWidth(instWidth))) {
            Inst newInst = makeInst(inst.args[valueIndex], target.args[valueIndex], inst.args[addrIndex]);
            logFound(newInst);
            target = newInst;
            return true;
        }

        if (isValidOffset(targetOffset) && (targetOffset + bytesForWidth(instWidth)) == instOffset) {
            Inst newInst = makeInst(target.args[valueIndex], inst.args[valueIndex], target.args[addrIndex]);
            logFound(newInst);
            target = newInst;
            return true;
        }

        // Because str/ldr pimm only takes unsigned offset, we tend to pick stackPointerRegister based offsetting.
        // But it is possible that framePointerRegister based offsetting can offer a benefit here.
        if (target.args[addrIndex].base() == Tmp(CCallHelpers::stackPointerRegister)) {
            int64_t instOffsetFromFP = instOffset - code.frameSize();
            int64_t targetOffsetFromFP = targetOffset - code.frameSize();

            if (isValidOffset(instOffsetFromFP) && targetOffsetFromFP == (instOffsetFromFP + bytesForWidth(instWidth))) {
                Inst newInst = makeInst(inst.args[valueIndex], target.args[valueIndex], Arg::addr(Air::Tmp(GPRInfo::callFrameRegister), static_cast<int32_t>(instOffsetFromFP)));
                logFound(newInst);
                target = newInst;
                return true;
            }

            if (isValidOffset(targetOffsetFromFP) && (targetOffsetFromFP + bytesForWidth(instWidth)) == instOffsetFromFP) {
                Inst newInst = makeInst(target.args[valueIndex], inst.args[valueIndex], Arg::addr(Air::Tmp(GPRInfo::callFrameRegister), static_cast<int32_t>(targetOffsetFromFP)));
                logFound(newInst);
                target = newInst;
                return true;
            }
        }
    }

    return false;
}

static bool tryStorePair(Code& code, BasicBlock* block, unsigned current, Inst& inst)
{
    return tryMergePair(code, block, current, inst, PairDirection::Store);
}

static bool tryLoadPair(Code& code, BasicBlock* block, unsigned current, Inst& inst)
{
    return tryMergePair(code, block, current, inst, PairDirection::Load);
}

bool optimizePairedLoadStore(Code& code)
{
    constexpr bool verbose = false;

    PhaseScope phaseScope(code, "optimizePairedLoadStore"_s);

    if (verbose) {
        dataLog("Air before an iteration of optimizePairedLoadStore:\n");
        dataLog(code);
    }

    bool changed = false;
    for (BasicBlock* block : code) {
        unsigned index = block->size();
        while (index--) {
            Inst& inst = block->at(index);
            if (inst.hasNonArgEffects())
                continue;

            auto isPairableKind = [&] {
                switch (inst.kind.opcode) {
                case Move:
                case Move32:
                case MoveFloat:
                case MoveDouble:
                    return true;
                default:
                    return false;
                }
            };

            if (!isPairableKind())
                continue;

            if (inst.args.size() != 2)
                continue;

            const bool valueIsTmp = inst.args[0].isTmp();
            const bool valueIsZeroReg = inst.args[0].isZeroReg();
            const bool destIsAddr = inst.args[1].isAddr();
            const bool addrFirst = inst.args[0].isAddr();
            const bool destIsTmp = inst.args[1].isTmp();

            if ((valueIsTmp || valueIsZeroReg) && destIsAddr) {
                // Store shape: Tmp/ZeroReg -> Addr.
                // sp & fp slot usage is, in particular, different for call args and spills.
                // We would like to do stp merging only for spills.
                if ((inst.args[1].base() == Tmp(CCallHelpers::stackPointerRegister) || inst.args[1].base() == Tmp(CCallHelpers::framePointerRegister)) && !inst.kind.spill)
                    continue;
                if (tryStorePair(code, block, index, inst)) {
                    block->insts().removeAt(index);
                    changed = true;
                }
                continue;
            }

            if (addrFirst && destIsTmp) {
                // Load shape: Addr -> Tmp. Apply the same sp/fp-vs-spill heuristic as for stores.
                if ((inst.args[0].base() == Tmp(CCallHelpers::stackPointerRegister) || inst.args[0].base() == Tmp(CCallHelpers::framePointerRegister)) && !inst.kind.spill)
                    continue;
                if (tryLoadPair(code, block, index, inst)) {
                    block->insts().removeAt(index);
                    changed = true;
                }
                continue;
            }
        }
    }

    return changed;
}

} } } // namespace JSC::B3::Air

#endif // CPU(ARM64)
#endif // ENABLE(B3_JIT)
