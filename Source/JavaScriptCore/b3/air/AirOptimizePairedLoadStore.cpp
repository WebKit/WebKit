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

static inline Width accessWidth(Opcode opcode)
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

static bool tryStorePair(Code& code, BasicBlock* block, unsigned current, Inst& inst)
{
    Width instWidth = accessWidth(inst.kind.opcode);
    int64_t instOffset = static_cast<int64_t>(inst.args[1].offset());
    unsigned limit = std::min(current, AirOptimizePairedLoadStoreInternal::scanInstructions);
    RegisterSet clobbered;
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
        if (clobbered.contains(inst.args[1].base().reg(), IgnoreVectors) || (inst.args[0].isTmp() && clobbered.contains(inst.args[0].reg(), IgnoreVectors))) {
            logFailed();
            return false;
        }

        {
            // If some instructions between the current and target have memory-load or memory-store,
            // then we cannot merge them since reordering can change the results.
            // But this is really pessimistic: if the base is the same to the current instruction, and if the offset
            // is different from the current instruction, it is OK actually.

            bool interfere = false;

            auto clobberMemory = [&](const Tmp& argBase, int64_t argOffset, Width argWidth) {
                if (argBase == inst.args[1].base()) {
                    Range<int64_t> argRange(argOffset, argOffset + bytesForWidth(argWidth));
                    Range<int64_t> instRange(instOffset, instOffset + bytesForWidth(instWidth));
                    return argRange.overlaps(instRange);
                }

                if ((argBase == Tmp(CCallHelpers::stackPointerRegister) || argBase == Tmp(GPRInfo::callFrameRegister)) && (inst.args[1].base() == Tmp(CCallHelpers::stackPointerRegister) || inst.args[1].base() == Tmp(GPRInfo::callFrameRegister))) {
                    int64_t instOffsetFromFP = instOffset;
                    if (inst.args[1].base() == Tmp(CCallHelpers::stackPointerRegister))
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

            Inst::forEachUse<Arg>(&block->at(index), &block->at(index + 1), checkInterfere);
            if (interfere) {
                logFailed();
                return false;
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

        if ((!target.args[0].isTmp() && !target.args[0].isZeroReg()) || !target.args[1].isAddr())
            continue;

        if (target.args[1].base() != inst.args[1].base())
            continue;

        Opcode pairOpcode = StorePair32;
        switch (inst.kind.opcode) {
        case Move32:
            pairOpcode = StorePair32;
            break;
        case Move:
            pairOpcode = StorePair64;
            break;
        case MoveDouble:
            pairOpcode = StorePairDouble;
            break;
        case MoveFloat:
            pairOpcode = StorePairFloat;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        auto isValidOffset = [&](auto offset) {
            switch (inst.kind.opcode) {
            case Move32:
                return ARM64Assembler::isValidSTPImm<32>(offset);
            case Move:
                return ARM64Assembler::isValidSTPImm<64>(offset);
            case MoveDouble:
                return ARM64Assembler::isValidSTPFPImm<64>(offset);
            case MoveFloat:
                return ARM64Assembler::isValidSTPFPImm<32>(offset);
            default:
                RELEASE_ASSERT_NOT_REACHED();
                return false;
            }
        };

        int64_t targetOffset = static_cast<int64_t>(target.args[1].offset());

        if (isValidOffset(instOffset) && targetOffset == (instOffset + bytesForWidth(instWidth))) {
            Inst newInst(pairOpcode, target.origin, inst.args[0], target.args[0], inst.args[1]);
            logFound(newInst);
            target = newInst;
            return true;
        }

        if (isValidOffset(targetOffset) && (targetOffset + bytesForWidth(instWidth)) == instOffset) {
            Inst newInst(pairOpcode, target.origin, target.args[0], inst.args[0], target.args[1]);
            logFound(newInst);
            target = newInst;
            return true;
        }

        // Because str pimm only takes unsigned offset, we tend to pick stackPointerRegister based offsetting.
        // But it is possible that framePointerRegister based offsetting can offer a benefit here.
        if (target.args[1].base() == Tmp(CCallHelpers::stackPointerRegister)) {
            int64_t instOffsetFromFP = instOffset - code.frameSize();
            int64_t targetOffsetFromFP = targetOffset - code.frameSize();

            if (isValidOffset(instOffsetFromFP) && targetOffsetFromFP == (instOffsetFromFP + bytesForWidth(instWidth))) {
                Inst newInst(pairOpcode, target.origin, inst.args[0], target.args[0], Arg::addr(Air::Tmp(GPRInfo::callFrameRegister), instOffsetFromFP));
                logFound(newInst);
                target = newInst;
                return true;
            }

            if (isValidOffset(targetOffsetFromFP) && (targetOffsetFromFP + bytesForWidth(instWidth)) == instOffsetFromFP) {
                Inst newInst(pairOpcode, target.origin, target.args[0], inst.args[0], Arg::addr(Air::Tmp(GPRInfo::callFrameRegister), targetOffsetFromFP));
                logFound(newInst);
                target = newInst;
                return true;
            }
        }
    }

    return false;
}

bool optimizePairedLoadStore(Code& code)
{
    constexpr bool verbose = false;

    PhaseScope phaseScope(code, "optimizePairedLoadStore");

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

            switch (inst.kind.opcode) {
            case Move:
            case Move32: {
                if (inst.args.size() != 2)
                    continue;

                if ((inst.args[0].isGPTmp() || inst.args[0].isZeroReg()) && inst.args[1].isAddr()) {
                    // sp & fp slot usage is, in particular, different for call args and spills.
                    // We would like to do stp merging only for spills.
                    if ((inst.args[1].base() == Tmp(CCallHelpers::stackPointerRegister) || inst.args[1].base() == Tmp(CCallHelpers::framePointerRegister)) && !inst.kind.spill)
                        continue;
                    if (tryStorePair(code, block, index, inst)) {
                        block->insts().remove(index);
                        changed = true;
                    }
                    continue;
                }
                break;
            }

            default:
                continue;
            }
        }
    }

    return changed;
}

} } } // namespace JSC::B3::Air

#endif // CPU(ARM64)
#endif // ENABLE(B3_JIT)
