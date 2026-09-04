/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "AirLowerStackArgs.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirInsertionSet.h"
#include "AirInstInlines.h"
#include "AirPhaseScope.h"
#include <optional>

namespace JSC { namespace B3 { namespace Air {

void lowerStackArgs(Code& code)
{
    PhaseScope phaseScope(code, "lowerStackArgs"_s);
    
    // Now we need to deduce how much argument area we need. We always reserve the conservative
    // register bytes for Bank::FP because CallArgs do not record which bank they are.
    unsigned conservativeCallArgBytes = code.usesSIMD() ? conservativeRegisterBytes(Bank::FP) : conservativeRegisterBytesWithoutVectors(Bank::FP);
    for (BasicBlock* block : code) {
        for (Inst& inst : *block) {
            for (Arg& arg : inst.args()) {
                if (arg.isCallArg()) {
                    ASSERT(arg.offset() >= 0);
                    code.requestCallArgAreaSizeInBytes(arg.offset() + conservativeCallArgBytes);
                }
            }
        }
    }

    code.setFrameSize(code.frameSize() + code.callArgAreaSizeInBytes());

    // Finally, transform the code to use Addr's instead of StackSlot's. This is a lossless
    // transformation since we can search the StackSlots array to figure out which StackSlot any
    // offset-from-FP refers to.

    InsertionSet insertionSet(code);
    for (BasicBlock* block : code) {
#if CPU(ARM64) || CPU(RISCV64)
        std::optional<int32_t> materializedOffsetFromSP;
        Air::Tmp extendedOffsetTmp = Air::Tmp(extendedOffsetAddrRegister());
        auto invalidateIfClobbered = [&] (Inst& inst) {
            if (inst.hasNonArgNonControlEffects()) {
                materializedOffsetFromSP.reset();
                return;
            }
            bool clobbers = false;
            inst.forEachTmp([&] (Tmp& tmp, Arg::Role role, Bank, Width) {
                if (tmp == extendedOffsetTmp && Arg::isAnyDef(role))
                    clobbers = true;
            });
            if (clobbers)
                materializedOffsetFromSP.reset();
        };
#endif

        for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
            Inst& inst = block->at(instIndex);
#if CPU(ARM64) || CPU(RISCV64)
            inst.forEachTmp([&] (Tmp& tmp, Arg::Role role, Bank, Width) {
                if (tmp == extendedOffsetTmp && Arg::isEarlyDef(role))
                    materializedOffsetFromSP.reset();
            });
#endif
            bool extendedOffsetAddrRegInUse = false;

            auto stackAddr = [&] (unsigned insertionIndex, Arg arg, Width width, Value::OffsetType offsetFromFP) -> Arg {
                int32_t offsetFromSP = offsetFromFP + code.frameSize();

                if (inst.admitsExtendedOffsetAddr(arg)) {
                    // Stackmaps and patchpoints expect addr inputs relative to SP or FP only. We might as well
                    // not even bother generating an addr with valid form for these opcodes since extended offset
                    // addr is always valid.
                    return Arg::extendedOffsetAddr(offsetFromFP);
                }

                Arg result = Arg::addr(Air::Tmp(GPRInfo::callFrameRegister), offsetFromFP);
                if (result.isValidForm(Move, width))
                    return result;

                result = Arg::addr(Air::Tmp(MacroAssembler::stackPointerRegister), offsetFromSP);
                if (result.isValidForm(Move, width))
                    return result;

                if (inst.kind.opcode == Patch)
                    return Arg::extendedOffsetAddr(offsetFromFP);

#if CPU(ARM64) || CPU(RISCV64)
                if (materializedOffsetFromSP) {
                    int64_t delta = static_cast<int64_t>(offsetFromSP) - static_cast<int64_t>(*materializedOffsetFromSP);
                    if (isRepresentableAs<int32_t>(delta)) {
                        Arg reused = Arg::addr(extendedOffsetTmp, static_cast<int32_t>(delta));
                        if (reused.isValidForm(Move, width)) {
                            extendedOffsetAddrRegInUse = true;
                            return reused;
                        }
                    }
                }

                RELEASE_ASSERT(!extendedOffsetAddrRegInUse);

                Arg largeOffset = Arg::isValidImmForm(offsetFromSP) ? Arg::imm(offsetFromSP) : Arg::bigImm(offsetFromSP);
                insertionSet.insert(insertionIndex, Move, inst.origin, largeOffset, extendedOffsetTmp);
                insertionSet.insert(insertionIndex, Add64, inst.origin, Air::Tmp(MacroAssembler::stackPointerRegister), extendedOffsetTmp);
                materializedOffsetFromSP = offsetFromSP;
                extendedOffsetAddrRegInUse = true;
                result = Arg::addr(extendedOffsetTmp, 0);
                return result;
#elif CPU(X86_64)
                UNUSED_PARAM(insertionIndex);
                // Can't happen on x86: immediates are always big enough for frame size.
                RELEASE_ASSERT_NOT_REACHED();
#else
#error Unhandled architecture.
#endif
            };

            auto isMove = [] (Inst& inst) -> std::optional<Width> {
                switch (inst.kind.opcode) {
                case Move:
                    return pointerWidth();
                case Move32:
                case MoveFloat:
                    return Width32;
                case MoveDouble:
                    return Width64;
                case MoveVector:
                    return Width128;
                default:
                    return std::nullopt;
                }
            };

            if (isARM64() && (inst.kind.opcode == Lea32 || inst.kind.opcode == Lea64)) {
                // On ARM64, Lea is just an add. We can't handle this below because
                // taking into account the Width to see if we can compute the immediate
                // is wrong.
#if CPU(ARM64) || CPU(RISCV64)
                bool leaUsedExtendedOffsetReg = false;
#endif
                auto lowerArmLea = [&] (Value::OffsetType offset, Tmp base) {
                    ASSERT(inst.args()[1].isTmp());

                    if (Arg::isValidImmForm(offset))
                        inst = Inst(inst.kind.opcode == Lea32 ? Add32 : Add64, inst.origin, Arg::imm(offset), base, inst.args()[1]);
                    else {
#if CPU(ARM64) || CPU(RISCV64)
                        leaUsedExtendedOffsetReg = true;
#endif
                        Air::Tmp tmp = Air::Tmp(extendedOffsetAddrRegister());
                        Arg offsetArg = Arg::bigImm(offset);
                        insertionSet.insert(instIndex, Move, inst.origin, offsetArg, tmp);
                        inst = Inst(inst.kind.opcode == Lea32 ? Add32 : Add64, inst.origin, tmp, base, inst.args()[1]);
                    }
                };

                switch (inst.args()[0].kind()) {
                case Arg::Stack: {
                    StackSlot* slot = inst.args()[0].stackSlot();
                    lowerArmLea(inst.args()[0].offset() + slot->offsetFromFP(), Tmp(GPRInfo::callFrameRegister));
                    break;
                }
                case Arg::CallArg:
                    lowerArmLea(inst.args()[0].offset() - code.frameSize(), Tmp(GPRInfo::callFrameRegister));
                    break;
                case Arg::Addr:
                    lowerArmLea(inst.args()[0].offset(), inst.args()[0].base());
                    break;
                case Arg::ExtendedOffsetAddr:
                    ASSERT_NOT_REACHED();
                    break;
                default:
                    break;
                }

#if CPU(ARM64) || CPU(RISCV64)
                if (leaUsedExtendedOffsetReg)
                    materializedOffsetFromSP.reset();
#endif
                continue;
            }

            // Moves between stack spills may require using the extendedOffsetReg for both the src and dst addresses.
            // In that case, split the move into separate load and store instructions so that the extendedOffsetReg can
            // be used for each address, one at a time.
            std::optional<Width> moveWidth = isMove(inst);
            if (isARM64() && moveWidth && inst.args().size() == 3 && inst.args()[0].isStack()) {
                Arg& src = inst.args()[0];
                src = stackAddr(instIndex, src, *moveWidth, src.offset() + src.stackSlot()->offsetFromFP());
                if (extendedOffsetAddrRegInUse) {
                    Arg scratch = inst.args()[2];
                    ASSERT(scratch.isReg());
                    // Insert Mov src, scratchReg
                    insertionSet.insert(instIndex, static_cast<Opcode>(inst.kind.opcode), inst.origin, src, scratch);
                    extendedOffsetAddrRegInUse = false; // Used by the inserted instruction; no longer needed.
                    // Modify inst to be 'Move scratch, dest'.
                    inst.setArgs(scratch, inst.args()[1]);
                }
                // Fall through to handle remainder of the original or modified inst, including potential ZDef handling.
            }

            // The scan below only ever acts on Stack and CallArg operands, and after register
            // allocation most instructions have neither. Checking that does not need the Arg roles,
            // and iterating args() directly can only over-approximate what forEachArg reports.
            bool mayHaveStackArg = false;
            for (Arg& arg : inst.args()) {
                if (arg.isStack() || arg.isCallArg()) {
                    mayHaveStackArg = true;
                    break;
                }
            }
            if (!mayHaveStackArg) {
#if CPU(ARM64) || CPU(RISCV64)
                invalidateIfClobbered(inst);
#endif
                continue;
            }

            inst.forEachArg(
                [&] (Arg& arg, Arg::Role role, Bank, Width width) {
                    switch (arg.kind()) {
                    case Arg::Stack: {
                        StackSlot* slot = arg.stackSlot();
                        if (inst.kind.opcode == Move && slot->kind() == StackSlotKind::Spill)
                            inst.kind.spill = true;
                        Arg originalArg = arg;
                        Value::OffsetType currentOffsetFromFP = arg.offset() + slot->offsetFromFP();
                        bool needsZDefFill = Arg::isZDef(role)
                            && slot->kind() == StackSlotKind::Spill
                            && slot->byteSize() > bytesForWidth(width);
                        if (needsZDefFill) {
                            // Currently we only handle this simple case because it's the only one
                            // that arises: ZDef's are only 32-bit right now. So, when we hit these
                            // assertions it means that we need to implement those other kinds of
                            // zero fills.
                            RELEASE_ASSERT(slot->byteSize() == 8);
                            RELEASE_ASSERT(width == Width32);
                        }
                        arg = stackAddr(instIndex, originalArg, width, currentOffsetFromFP);
                        if (needsZDefFill) {
#if CPU(ARM64) || CPU(RISCV64)
                            Air::Opcode storeOpcode = Move32;
                            Air::Arg::Kind operandKind = Arg::ZeroReg;
                            Air::Arg operand = Arg::zeroReg();
#elif CPU(X86_64)
                            Air::Opcode storeOpcode = Move32;
                            Air::Arg::Kind operandKind = Arg::Imm;
                            Air::Arg operand = Arg::imm(0);
#else
#error Unhandled architecture.
#endif
                            RELEASE_ASSERT(isValidForm(storeOpcode, operandKind, Arg::Stack));
                            extendedOffsetAddrRegInUse = false;
                            insertionSet.insert(
                                instIndex + 1, storeOpcode, inst.origin, operand,
                                stackAddr(instIndex + 1, originalArg, width, originalArg.offset() + 4 + slot->offsetFromFP()));
                        }
                        break;
                    }
                    case Arg::CallArg:
                        arg = stackAddr(instIndex, arg, width, arg.offset() - code.frameSize());
                        break;
                    default:
                        break;
                    }
                }
            );
#if CPU(ARM64) || CPU(RISCV64)
            invalidateIfClobbered(inst);
#endif
        }
        insertionSet.execute(block);
    }
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
