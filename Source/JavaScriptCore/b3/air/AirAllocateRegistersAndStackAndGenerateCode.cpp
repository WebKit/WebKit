/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "AirAllocateRegistersAndStackAndGenerateCode.h"

#if ENABLE(B3_JIT)

#include "AirArgInlines.h"
#include "AirBlockInsertionSet.h"
#include "AirCode.h"
#include "AirHandleCalleeSaves.h"
#include "AirLowerStackArgs.h"
#include "AirStackAllocation.h"
#include "AirTmpMap.h"
#include "CCallHelpers.h"
#include "DisallowMacroScratchRegisterUsage.h"

namespace JSC { namespace B3 { namespace Air {

GenerateAndAllocateRegisters::GenerateAndAllocateRegisters(Code& code)
    : m_code(code)
    , m_map(code)
{ }

ALWAYS_INLINE void GenerateAndAllocateRegisters::checkConsistency()
{
    // This isn't exactly the right option for this but adding a new one for just this seems silly.
    if (Options::validateGraph() || Options::validateGraphAtEachPhase()) {
        m_code.forEachTmp([&] (Tmp tmp) {
            Reg reg = m_map[tmp].reg;
            if (!reg)
                return;

            ASSERT(!m_availableRegs[tmp.bank()].contains(reg));
            ASSERT(m_currentAllocation->at(reg) == tmp);
        });

        for (Reg reg : RegisterSet::allRegisters()) {
            if (isDisallowedRegister(reg))
                continue;

            Tmp tmp = m_currentAllocation->at(reg);
            if (!tmp) {
                ASSERT(m_availableRegs[bankForReg(reg)].contains(reg));
                continue;
            }

            ASSERT(!m_availableRegs[tmp.bank()].contains(reg));
            ASSERT(m_map[tmp].reg == reg);
        }
    }
}

void GenerateAndAllocateRegisters::buildLiveRanges(UnifiedTmpLiveness& liveness)
{
    m_liveRangeEnd = TmpMap<size_t>(m_code, 0);

    m_globalInstIndex = 0;
    for (BasicBlock* block : m_code) {
        for (Tmp tmp : liveness.liveAtHead(block)) {
            if (!tmp.isReg())
                m_liveRangeEnd[tmp] = m_globalInstIndex;
        }
        ++m_globalInstIndex;
        for (Inst& inst : *block) {
            inst.forEachTmpFast([&] (Tmp tmp) {
                if (!tmp.isReg())
                    m_liveRangeEnd[tmp] = m_globalInstIndex;
            });
            ++m_globalInstIndex;
        }
        for (Tmp tmp : liveness.liveAtTail(block)) {
            if (!tmp.isReg())
                m_liveRangeEnd[tmp] = m_globalInstIndex;
        }
        ++m_globalInstIndex;
    }
}

void GenerateAndAllocateRegisters::insertBlocksForFlushAfterTerminalPatchpoints()
{
    BlockInsertionSet blockInsertionSet(m_code);
    for (BasicBlock* block : m_code) {
        Inst& inst = block->last();
        if (inst.kind.opcode != Patch)
            continue;

        HashMap<Tmp, Arg*> needToDef;

        inst.forEachArg([&] (Arg& arg, Arg::Role role, Bank, Width) {
            if (!arg.isTmp())
                return;
            Tmp tmp = arg.tmp();
            if (Arg::isAnyDef(role) && !tmp.isReg())
                needToDef.add(tmp, &arg);
        });

        if (needToDef.isEmpty())
            continue;

        for (FrequentedBlock& frequentedSuccessor : block->successors()) {
            BasicBlock* successor = frequentedSuccessor.block();
            BasicBlock* newBlock = blockInsertionSet.insertBefore(successor, successor->frequency());
            newBlock->appendInst(Inst(Jump, inst.origin));
            newBlock->setSuccessors(successor);
            newBlock->addPredecessor(block);
            frequentedSuccessor.block() = newBlock;
            successor->replacePredecessor(block, newBlock);

            m_blocksAfterTerminalPatchForSpilling.add(newBlock, PatchSpillData { CCallHelpers::Jump(), CCallHelpers::Label(), needToDef });
        }
    }

    blockInsertionSet.execute();
}

static ALWAYS_INLINE CCallHelpers::Address callFrameAddr(CCallHelpers& jit, intptr_t offsetFromFP)
{
    if (isX86()) {
        ASSERT(Arg::addr(Air::Tmp(GPRInfo::callFrameRegister), offsetFromFP).isValidForm(Width64));
        return CCallHelpers::Address(GPRInfo::callFrameRegister, offsetFromFP);
    }

    ASSERT(pinnedExtendedOffsetAddrRegister());
    auto addr = Arg::addr(Air::Tmp(GPRInfo::callFrameRegister), offsetFromFP);
    if (addr.isValidForm(Width64))
        return CCallHelpers::Address(GPRInfo::callFrameRegister, offsetFromFP);
    GPRReg reg = *pinnedExtendedOffsetAddrRegister();
    jit.move(CCallHelpers::TrustedImmPtr(offsetFromFP), reg);
    jit.add64(GPRInfo::callFrameRegister, reg);
    return CCallHelpers::Address(reg);
}

ALWAYS_INLINE void GenerateAndAllocateRegisters::release(Tmp tmp, Reg reg)
{
    ASSERT(reg);
    ASSERT(m_currentAllocation->at(reg) == tmp);
    m_currentAllocation->at(reg) = Tmp();
    ASSERT(!m_availableRegs[tmp.bank()].contains(reg));
    m_availableRegs[tmp.bank()].set(reg);
    ASSERT(m_map[tmp].reg == reg);
    m_map[tmp].reg = Reg();
}


ALWAYS_INLINE void GenerateAndAllocateRegisters::flush(Tmp tmp, Reg reg)
{
    ASSERT(tmp);
    intptr_t offset = m_map[tmp].spillSlot->offsetFromFP();
    if (tmp.isGP())
        m_jit->store64(reg.gpr(), callFrameAddr(*m_jit, offset));
    else
        m_jit->storeDouble(reg.fpr(), callFrameAddr(*m_jit, offset));
}

ALWAYS_INLINE void GenerateAndAllocateRegisters::spill(Tmp tmp, Reg reg)
{
    ASSERT(reg);
    ASSERT(m_map[tmp].reg == reg);
    flush(tmp, reg);
    release(tmp, reg);
}

ALWAYS_INLINE void GenerateAndAllocateRegisters::alloc(Tmp tmp, Reg reg, bool isDef)
{
    if (Tmp occupyingTmp = m_currentAllocation->at(reg))
        spill(occupyingTmp, reg);
    else {
        ASSERT(!m_currentAllocation->at(reg));
        ASSERT(m_availableRegs[tmp.bank()].get(reg));
    }

    m_map[tmp].reg = reg;
    m_availableRegs[tmp.bank()].clear(reg);
    m_currentAllocation->at(reg) = tmp;

    if (!isDef) {
        intptr_t offset = m_map[tmp].spillSlot->offsetFromFP();
        if (tmp.bank() == GP)
            m_jit->load64(callFrameAddr(*m_jit, offset), reg.gpr());
        else
            m_jit->loadDouble(callFrameAddr(*m_jit, offset), reg.fpr());
    }
}

ALWAYS_INLINE void GenerateAndAllocateRegisters::freeDeadTmpsIfNeeded()
{
    if (m_didAlreadyFreeDeadSlots)
        return;

    m_didAlreadyFreeDeadSlots = true;
    for (size_t i = 0; i < m_currentAllocation->size(); ++i) {
        Tmp tmp = m_currentAllocation->at(i);
        if (!tmp)
            continue;
        if (tmp.isReg())
            continue;
        if (m_liveRangeEnd[tmp] >= m_globalInstIndex)
            continue;

        release(tmp, Reg::fromIndex(i));
    }
}

ALWAYS_INLINE bool GenerateAndAllocateRegisters::assignTmp(Tmp& tmp, Bank bank, bool isDef)
{
    ASSERT(!tmp.isReg());
    if (Reg reg = m_map[tmp].reg) {
        ASSERT(!m_namedDefdRegs.contains(reg));
        tmp = Tmp(reg);
        m_namedUsedRegs.set(reg);
        ASSERT(!m_availableRegs[bank].get(reg));
        return true;
    }

    if (!m_availableRegs[bank].numberOfSetRegisters())
        freeDeadTmpsIfNeeded();

    if (m_availableRegs[bank].numberOfSetRegisters()) {
        // We first take an available register.
        for (Reg reg : m_registers[bank]) {
            if (m_namedUsedRegs.contains(reg) || m_namedDefdRegs.contains(reg))
                continue;
            if (!m_availableRegs[bank].contains(reg))
                continue;
            m_namedUsedRegs.set(reg); // At this point, it doesn't matter if we add it to the m_namedUsedRegs or m_namedDefdRegs. We just need to mark that we can't use it again.
            alloc(tmp, reg, isDef);
            tmp = Tmp(reg);
            return true;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    // Nothing was available, let's make some room.
    for (Reg reg : m_registers[bank]) {
        if (m_namedUsedRegs.contains(reg) || m_namedDefdRegs.contains(reg))
            continue;

        m_namedUsedRegs.set(reg);

        alloc(tmp, reg, isDef);
        tmp = Tmp(reg);
        return true;
    }

    // This can happen if we have a #WarmAnys > #Available registers
    return false;
}

ALWAYS_INLINE bool GenerateAndAllocateRegisters::isDisallowedRegister(Reg reg)
{
    return !m_allowedRegisters.get(reg);
}

void GenerateAndAllocateRegisters::prepareForGeneration()
{
    // We pessimistically assume we use all callee saves.
    handleCalleeSaves(m_code, RegisterSet::calleeSaveRegisters());
    allocateEscapedStackSlots(m_code);

    insertBlocksForFlushAfterTerminalPatchpoints();

#if ASSERT_ENABLED
    m_code.forEachTmp([&] (Tmp tmp) {
        ASSERT(!tmp.isReg());
        m_allTmps[tmp.bank()].append(tmp);
    });
#endif

    m_liveness = makeUnique<UnifiedTmpLiveness>(m_code);

    {
        buildLiveRanges(*m_liveness);

        Vector<StackSlot*, 16> freeSlots;
        Vector<StackSlot*, 4> toFree;
        m_globalInstIndex = 0;
        for (BasicBlock* block : m_code) {
            auto assignStackSlotToTmp = [&] (Tmp tmp) {
                if (tmp.isReg())
                    return;

                TmpData& data = m_map[tmp];
                if (data.spillSlot) {
                    if (m_liveRangeEnd[tmp] == m_globalInstIndex)
                        toFree.append(data.spillSlot);
                    return;
                }

                if (freeSlots.size())
                    data.spillSlot = freeSlots.takeLast();
                else
                    data.spillSlot = m_code.addStackSlot(8, StackSlotKind::Spill);
                data.reg = Reg();
            };

            auto flushToFreeList = [&] {
                for (auto* stackSlot : toFree)
                    freeSlots.append(stackSlot);
                toFree.clear();
            };

            for (Tmp tmp : m_liveness->liveAtHead(block))
                assignStackSlotToTmp(tmp);
            flushToFreeList();

            ++m_globalInstIndex;

            for (Inst& inst : *block) {
                Vector<Tmp, 4> seenTmps;
                inst.forEachTmpFast([&] (Tmp tmp) {
                    if (seenTmps.contains(tmp))
                        return;
                    seenTmps.append(tmp);
                    assignStackSlotToTmp(tmp);
                });

                flushToFreeList();
                ++m_globalInstIndex;
            }

            for (Tmp tmp : m_liveness->liveAtTail(block))
                assignStackSlotToTmp(tmp);
            flushToFreeList();

            ++m_globalInstIndex;
        }
    } 

    m_allowedRegisters = RegisterSet();

    forEachBank([&] (Bank bank) {
        m_registers[bank] = m_code.regsInPriorityOrder(bank);

        for (Reg reg : m_registers[bank]) {
            m_allowedRegisters.set(reg);
            TmpData& data = m_map[Tmp(reg)];
            data.spillSlot = m_code.addStackSlot(8, StackSlotKind::Spill);
            data.reg = Reg();
        }
    });

    {
        unsigned nextIndex = 0;
        for (StackSlot* slot : m_code.stackSlots()) {
            if (slot->isLocked())
                continue;
            intptr_t offset = -static_cast<intptr_t>(m_code.frameSize()) - static_cast<intptr_t>(nextIndex) * 8 - 8;
            ++nextIndex;
            slot->setOffsetFromFP(offset);
        }
    }

    updateFrameSizeBasedOnStackSlots(m_code);
    m_code.setStackIsAllocated(true);

    lowerStackArgs(m_code);

#if ASSERT_ENABLED
    // Verify none of these passes add any tmps.
    forEachBank([&] (Bank bank) {
        ASSERT(m_allTmps[bank].size() == m_code.numTmps(bank));
    });

    {
        // Verify that lowerStackArgs didn't change Tmp liveness at the boundaries for the Tmps and Registers we model.
        UnifiedTmpLiveness liveness(m_code);
        for (BasicBlock* block : m_code) {
            auto assertLivenessAreEqual = [&] (auto a, auto b) {
                HashSet<Tmp> livenessA;
                HashSet<Tmp> livenessB;
                for (Tmp tmp : a) {
                    if (tmp.isReg() && isDisallowedRegister(tmp.reg()))
                        continue;
                    livenessA.add(tmp);
                }
                for (Tmp tmp : b) {
                    if (tmp.isReg() && isDisallowedRegister(tmp.reg()))
                        continue;
                    livenessB.add(tmp);
                }

                ASSERT(livenessA == livenessB);
            };

            assertLivenessAreEqual(m_liveness->liveAtHead(block), liveness.liveAtHead(block));
            assertLivenessAreEqual(m_liveness->liveAtTail(block), liveness.liveAtTail(block));
        }
    }
#endif
}

void GenerateAndAllocateRegisters::generate(CCallHelpers& jit)
{
    m_jit = &jit;

    TimingScope timingScope("Air::generateAndAllocateRegisters");

    DisallowMacroScratchRegisterUsage disallowScratch(*m_jit);

    buildLiveRanges(*m_liveness);

    IndexMap<BasicBlock*, IndexMap<Reg, Tmp>> currentAllocationMap(m_code.size());
    {
        IndexMap<Reg, Tmp> defaultCurrentAllocation(Reg::maxIndex() + 1);
        for (BasicBlock* block : m_code)
            currentAllocationMap[block] = defaultCurrentAllocation;

        // The only things live that are in registers at the root blocks are
        // the explicitly named registers that are live.

        for (unsigned i = m_code.numEntrypoints(); i--;) {
            BasicBlock* entrypoint = m_code.entrypoint(i).block();
            for (Tmp tmp : m_liveness->liveAtHead(entrypoint)) {
                if (tmp.isReg())
                    currentAllocationMap[entrypoint][tmp.reg()] = tmp;
            }
        }
    }

    // And now, we generate code.
    GenerationContext context;
    context.code = &m_code;
    context.blockLabels.resize(m_code.size());
    for (BasicBlock* block : m_code)
        context.blockLabels[block] = Box<CCallHelpers::Label>::create();
    IndexMap<BasicBlock*, CCallHelpers::JumpList> blockJumps(m_code.size());

    auto link = [&] (CCallHelpers::Jump jump, BasicBlock* target) {
        if (context.blockLabels[target]->isSet()) {
            jump.linkTo(*context.blockLabels[target], m_jit);
            return;
        }

        blockJumps[target].append(jump);
    };

    Disassembler* disassembler = m_code.disassembler();

    m_globalInstIndex = 0;

    for (BasicBlock* block : m_code) {
        context.currentBlock = block;
        context.indexInBlock = UINT_MAX;
        blockJumps[block].link(m_jit);
        CCallHelpers::Label label = m_jit->label();
        *context.blockLabels[block] = label;

        if (disassembler)
            disassembler->startBlock(block, *m_jit);

        if (Optional<unsigned> entrypointIndex = m_code.entrypointIndex(block)) {
            ASSERT(m_code.isEntrypoint(block));
            if (disassembler)
                disassembler->startEntrypoint(*m_jit); 

            m_code.prologueGeneratorForEntrypoint(*entrypointIndex)->run(*m_jit, m_code);

            if (disassembler)
                disassembler->endEntrypoint(*m_jit); 
        } else
            ASSERT(!m_code.isEntrypoint(block));

        auto startLabel = m_jit->labelIgnoringWatchpoints();

        {
            auto iter = m_blocksAfterTerminalPatchForSpilling.find(block);
            if (iter != m_blocksAfterTerminalPatchForSpilling.end()) {
                auto& data = iter->value;
                data.jump = m_jit->jump();
                data.continueLabel = m_jit->label();
            }
        }

        forEachBank([&] (Bank bank) {
#if ASSERT_ENABLED
            // By default, everything is spilled at block boundaries. We do this after we process each block
            // so we don't have to walk all Tmps, since #Tmps >> #Available regs. Instead, we walk the register file at
            // each block boundary and clear entries in this map.
            for (Tmp tmp : m_allTmps[bank])
                ASSERT(m_map[tmp].reg == Reg()); 
#endif

            RegisterSet availableRegisters;
            for (Reg reg : m_registers[bank])
                availableRegisters.set(reg);
            m_availableRegs[bank] = WTFMove(availableRegisters);
        });

        IndexMap<Reg, Tmp>& currentAllocation = currentAllocationMap[block];
        m_currentAllocation = &currentAllocation;

        for (unsigned i = 0; i < currentAllocation.size(); ++i) {
            Tmp tmp = currentAllocation[i];
            if (!tmp)
                continue;
            Reg reg = Reg::fromIndex(i);
            m_map[tmp].reg = reg;
            m_availableRegs[tmp.bank()].clear(reg);
        }

        ++m_globalInstIndex;

        bool isReplayingSameInst = false;
        for (size_t instIndex = 0; instIndex < block->size(); ++instIndex) {
            checkConsistency();

            if (instIndex && !isReplayingSameInst)
                startLabel = m_jit->labelIgnoringWatchpoints();

            context.indexInBlock = instIndex;

            Inst& inst = block->at(instIndex);

            m_didAlreadyFreeDeadSlots = false;

            m_namedUsedRegs = RegisterSet();
            m_namedDefdRegs = RegisterSet();

            bool needsToGenerate = ([&] () -> bool {
                // FIXME: We should consider trying to figure out if we can also elide Mov32s
                if (!(inst.kind.opcode == Move || inst.kind.opcode == MoveDouble))
                    return true;

                ASSERT(inst.args.size() >= 2);
                Arg source = inst.args[0];
                Arg dest = inst.args[1];
                if (!source.isTmp() || !dest.isTmp())
                    return true;

                // FIXME: We don't track where the last use of a reg is globally so we don't know where we can elide them.
                ASSERT(source.isReg() || m_liveRangeEnd[source.tmp()] >= m_globalInstIndex);
                if (source.isReg() || m_liveRangeEnd[source.tmp()] != m_globalInstIndex)
                    return true;

                // If we are doing a self move at the end of the temps liveness we can trivially elide the move.
                if (source == dest)
                    return false;

                Reg sourceReg = m_map[source.tmp()].reg;
                // If the value is not already materialized into a register we may still move it into one so let the normal generation code run.
                if (!sourceReg)
                    return true;

                ASSERT(m_currentAllocation->at(sourceReg) == source.tmp());

                if (dest.isReg() && dest.reg() != sourceReg)
                    return true;

                if (Reg oldReg = m_map[dest.tmp()].reg)
                    release(dest.tmp(), oldReg);

                m_map[dest.tmp()].reg = sourceReg;
                m_currentAllocation->at(sourceReg) = dest.tmp();
                m_map[source.tmp()].reg = Reg();
                return false;
            })();
            checkConsistency();

            inst.forEachArg([&] (Arg& arg, Arg::Role role, Bank, Width) {
                if (!arg.isTmp())
                    return;

                Tmp tmp = arg.tmp();
                if (tmp.isReg() && isDisallowedRegister(tmp.reg()))
                    return;

                if (tmp.isReg()) {
                    if (Arg::isAnyUse(role))
                        m_namedUsedRegs.set(tmp.reg());
                    if (Arg::isAnyDef(role))
                        m_namedDefdRegs.set(tmp.reg());
                }

                // We convert any cold uses that are already in the stack to just point to
                // the canonical stack location.
                if (!Arg::isColdUse(role))
                    return;

                if (!inst.admitsStack(arg))
                    return;

                auto& entry = m_map[tmp];
                if (!entry.reg) {
                    // We're a cold use, and our current location is already on the stack. Just use that.
                    arg = Arg::addr(Tmp(GPRInfo::callFrameRegister), entry.spillSlot->offsetFromFP());
                }
            });

            RegisterSet clobberedRegisters;
            {
                Inst* nextInst = block->get(instIndex + 1);
                if (inst.kind.opcode == Patch || (nextInst && nextInst->kind.opcode == Patch)) {
                    if (inst.kind.opcode == Patch)
                        clobberedRegisters.merge(inst.extraClobberedRegs());
                    if (nextInst && nextInst->kind.opcode == Patch)
                        clobberedRegisters.merge(nextInst->extraEarlyClobberedRegs());

                    clobberedRegisters.filter(m_allowedRegisters);
                    clobberedRegisters.exclude(m_namedDefdRegs);

                    m_namedDefdRegs.merge(clobberedRegisters);
                }
            }

            auto allocNamed = [&] (const RegisterSet& named, bool isDef) {
                for (Reg reg : named) {
                    if (Tmp occupyingTmp = currentAllocation[reg]) {
                        if (occupyingTmp == Tmp(reg))
                            continue;
                    }

                    freeDeadTmpsIfNeeded(); // We don't want to spill a dead tmp.
                    alloc(Tmp(reg), reg, isDef);
                }
            };

            allocNamed(m_namedUsedRegs, false); // Must come before the defd registers since we may use and def the same register.
            allocNamed(m_namedDefdRegs, true);

            if (needsToGenerate) {
                auto tryAllocate = [&] {
                    Vector<Tmp*, 8> usesToAlloc;
                    Vector<Tmp*, 8> defsToAlloc;

                    inst.forEachTmp([&] (Tmp& tmp, Arg::Role role, Bank, Width) {
                        if (tmp.isReg())
                            return;

                        // We treat Use+Def as a use.
                        if (Arg::isAnyUse(role))
                            usesToAlloc.append(&tmp);
                        else if (Arg::isAnyDef(role))
                            defsToAlloc.append(&tmp);
                    });

                    auto tryAllocateTmps = [&] (auto& vector, bool isDef) {
                        bool success = true;
                        for (Tmp* tmp : vector)
                            success &= assignTmp(*tmp, tmp->bank(), isDef);
                        return success;
                    };

                    // We first handle uses, then defs. We want to be able to tell the register allocator
                    // which tmps need to be loaded from memory into their assigned register. Those such
                    // tmps are uses. Defs don't need to be reloaded since we're defining them. However,
                    // some tmps may both be used and defd. So we handle uses first since forEachTmp could
                    // walk uses/defs in any order.
                    bool success = true;
                    success &= tryAllocateTmps(usesToAlloc, false);
                    success &= tryAllocateTmps(defsToAlloc, true);
                    return success;
                };

                // We first allocate trying to give any Tmp a register. If that makes us exhaust the
                // available registers, we convert anything that accepts stack to be a stack addr
                // instead. This can happen for programs Insts that take in many args, but most
                // args can just be stack values.
                bool success = tryAllocate();
                if (!success) {
                    RELEASE_ASSERT(!isReplayingSameInst); // We should only need to do the below at most once per inst.

                    // We need to capture the register state before we start spilling things
                    // since we may have multiple arguments that are the same register.
                    IndexMap<Reg, Tmp> allocationSnapshot = currentAllocation;

                    // We rewind this Inst to be in its previous state, however, if any arg admits stack,
                    // we move to providing that arg in stack form. This will allow us to fully allocate
                    // this inst when we rewind.
                    inst.forEachArg([&] (Arg& arg, Arg::Role, Bank, Width) {
                        if (!arg.isTmp())
                            return;

                        Tmp tmp = arg.tmp();
                        if (tmp.isReg() && isDisallowedRegister(tmp.reg()))
                            return;

                        if (tmp.isReg()) {
                            Tmp originalTmp = allocationSnapshot[tmp.reg()];
                            if (originalTmp.isReg()) {
                                ASSERT(tmp.reg() == originalTmp.reg());
                                // This means this Inst referred to this reg directly. We leave these as is.
                                return;
                            }
                            tmp = originalTmp;
                        }

                        if (!inst.admitsStack(arg)) {
                            arg = tmp;
                            return;
                        }

                        auto& entry = m_map[tmp];
                        if (Reg reg = entry.reg)
                            spill(tmp, reg);

                        arg = Arg::addr(Tmp(GPRInfo::callFrameRegister), entry.spillSlot->offsetFromFP());
                    });

                    --instIndex;
                    isReplayingSameInst = true;
                    continue;
                }

                isReplayingSameInst = false;
            }

            if (m_code.needsUsedRegisters() && inst.kind.opcode == Patch) {
                freeDeadTmpsIfNeeded();
                RegisterSet registerSet;
                for (size_t i = 0; i < currentAllocation.size(); ++i) {
                    if (currentAllocation[i])
                        registerSet.set(Reg::fromIndex(i));
                }
                inst.reportUsedRegisters(registerSet);
            }

            if (inst.isTerminal() && block->numSuccessors()) {
                // By default, we spill everything between block boundaries. However, we have a small
                // heuristic to pass along register state. We should eventually make this better.
                // What we do now is if we have a successor with a single predecessor (us), and we
                // haven't yet generated code for it, we give it our register state. If all our successors
                // can take on our register state, we don't flush at the end of this block.

                bool everySuccessorGetsOurRegisterState = true;
                for (unsigned i = 0; i < block->numSuccessors(); ++i) {
                    BasicBlock* successor = block->successorBlock(i);
                    if (successor->numPredecessors() == 1 && !context.blockLabels[successor]->isSet())
                        currentAllocationMap[successor] = currentAllocation;
                    else
                        everySuccessorGetsOurRegisterState = false;
                }
                if (!everySuccessorGetsOurRegisterState) {
                    for (Tmp tmp : m_liveness->liveAtTail(block)) {
                        if (tmp.isReg() && isDisallowedRegister(tmp.reg()))
                            continue;
                        if (Reg reg = m_map[tmp].reg)
                            flush(tmp, reg);
                    }
                }
            }

            if (!inst.isTerminal()) {
                CCallHelpers::Jump jump;
                if (needsToGenerate)
                    jump = inst.generate(*m_jit, context);
                ASSERT_UNUSED(jump, !jump.isSet());

                for (Reg reg : clobberedRegisters) {
                    Tmp tmp(reg);
                    ASSERT(currentAllocation[reg] == tmp);
                    m_availableRegs[tmp.bank()].set(reg);
                    m_currentAllocation->at(reg) = Tmp();
                    m_map[tmp].reg = Reg();
                }
            } else {
                ASSERT(needsToGenerate);
                if (inst.kind.opcode == Jump && block->successorBlock(0) == m_code.findNextBlock(block))
                    needsToGenerate = false;

                if (isReturn(inst.kind.opcode)) {
                    needsToGenerate = false;

                    // We currently don't represent the full epilogue in Air, so we need to
                    // have this override.
                    if (m_code.frameSize()) {
                        m_jit->emitRestore(m_code.calleeSaveRegisterAtOffsetList());
                        m_jit->emitFunctionEpilogue();
                    } else
                        m_jit->emitFunctionEpilogueWithEmptyFrame();
                    m_jit->ret();
                }
                
                if (needsToGenerate) {
                    CCallHelpers::Jump jump = inst.generate(*m_jit, context);

                    // The jump won't be set for patchpoints. It won't be set for Oops because then it won't have
                    // any successors.
                    if (jump.isSet()) {
                        switch (block->numSuccessors()) {
                        case 1:
                            link(jump, block->successorBlock(0));
                            break;
                        case 2:
                            link(jump, block->successorBlock(0));
                            if (block->successorBlock(1) != m_code.findNextBlock(block))
                                link(m_jit->jump(), block->successorBlock(1));
                            break;
                        default:
                            RELEASE_ASSERT_NOT_REACHED();
                            break;
                        }
                    }
                }
            }

            auto endLabel = m_jit->labelIgnoringWatchpoints();
            if (disassembler)
                disassembler->addInst(&inst, startLabel, endLabel);

            ++m_globalInstIndex;
        }

        // Registers usually get spilled at block boundaries. We do it this way since we don't
        // want to iterate the entire TmpMap, since usually #Tmps >> #Regs. We may not actually spill
        // all registers, but at the top of this loop we handle that case by pre-populating register
        // state. Here, we just clear this map. After this loop, this map should contain only
        // null entries.
        for (size_t i = 0; i < currentAllocation.size(); ++i) {
            if (Tmp tmp = currentAllocation[i])
                m_map[tmp].reg = Reg();
        }

        ++m_globalInstIndex;
    }

    for (auto& entry : m_blocksAfterTerminalPatchForSpilling) {
        entry.value.jump.linkTo(m_jit->label(), m_jit);
        const HashMap<Tmp, Arg*>& spills = entry.value.defdTmps;
        for (auto& entry : spills) {
            Arg* arg = entry.value;
            if (!arg->isTmp())
                continue;
            Tmp originalTmp = entry.key;
            Tmp currentTmp = arg->tmp();
            ASSERT_WITH_MESSAGE(currentTmp.isReg(), "We already did register allocation so we should have assigned this Tmp to a register.");
            flush(originalTmp, currentTmp.reg());
        }
        m_jit->jump().linkTo(entry.value.continueLabel, m_jit);
    }

    context.currentBlock = nullptr;
    context.indexInBlock = UINT_MAX;
    
    Vector<CCallHelpers::Label> entrypointLabels(m_code.numEntrypoints());
    for (unsigned i = m_code.numEntrypoints(); i--;)
        entrypointLabels[i] = *context.blockLabels[m_code.entrypoint(i).block()];
    m_code.setEntrypointLabels(WTFMove(entrypointLabels));

    if (disassembler)
        disassembler->startLatePath(*m_jit);

    // FIXME: Make late paths have Origins: https://bugs.webkit.org/show_bug.cgi?id=153689
    for (auto& latePath : context.latePaths)
        latePath->run(*m_jit, context);

    if (disassembler)
        disassembler->endLatePath(*m_jit);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
