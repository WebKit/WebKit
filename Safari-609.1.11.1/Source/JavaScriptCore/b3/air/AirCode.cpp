/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#include "AirCode.h"

#if ENABLE(B3_JIT)

#include "AirAllocateRegistersAndStackAndGenerateCode.h"
#include "AirCCallSpecial.h"
#include "AirCFG.h"
#include "AllowMacroScratchRegisterUsageIf.h"
#include "B3BasicBlockUtils.h"
#include "B3Procedure.h"
#include "B3StackSlot.h"
#include <wtf/ListDump.h>
#include <wtf/MathExtras.h>

namespace JSC { namespace B3 { namespace Air {

static void defaultPrologueGenerator(CCallHelpers& jit, Code& code)
{
    jit.emitFunctionPrologue();
    if (code.frameSize()) {
        AllowMacroScratchRegisterUsageIf allowScratch(jit, isARM64());
        jit.addPtr(MacroAssembler::TrustedImm32(-code.frameSize()), MacroAssembler::framePointerRegister,  MacroAssembler::stackPointerRegister);
        if (Options::zeroStackFrame())
            jit.clearStackFrame(MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister, GPRInfo::nonArgGPR0, code.frameSize());
    }
    
    jit.emitSave(code.calleeSaveRegisterAtOffsetList());
}

Code::Code(Procedure& proc)
    : m_proc(proc)
    , m_cfg(new CFG(*this))
    , m_lastPhaseName("initial")
    , m_defaultPrologueGenerator(createSharedTask<PrologueGeneratorFunction>(&defaultPrologueGenerator))
{
    // Come up with initial orderings of registers. The user may replace this with something else.
    forEachBank(
        [&] (Bank bank) {
            Vector<Reg> volatileRegs;
            Vector<Reg> calleeSaveRegs;
            RegisterSet all = bank == GP ? RegisterSet::allGPRs() : RegisterSet::allFPRs();
            all.exclude(RegisterSet::stackRegisters());
            all.exclude(RegisterSet::reservedHardwareRegisters());
            RegisterSet calleeSave = RegisterSet::calleeSaveRegisters();
            all.forEach(
                [&] (Reg reg) {
                    if (!calleeSave.get(reg))
                        volatileRegs.append(reg);
                });
            all.forEach(
                [&] (Reg reg) {
                    if (calleeSave.get(reg))
                        calleeSaveRegs.append(reg);
                });
            if (Options::airRandomizeRegs()) {
                WeakRandom random(Options::airRandomizeRegsSeed() ? Options::airRandomizeRegsSeed() : m_weakRandom.getUint32());
                shuffleVector(volatileRegs, [&] (unsigned limit) { return random.getUint32(limit); });
                shuffleVector(calleeSaveRegs, [&] (unsigned limit) { return random.getUint32(limit); });
            }
            Vector<Reg> result;
            result.appendVector(volatileRegs);
            result.appendVector(calleeSaveRegs);
            setRegsInPriorityOrder(bank, result);
        });

    if (auto reg = pinnedExtendedOffsetAddrRegister())
        pinRegister(*reg);

    m_pinnedRegs.set(MacroAssembler::framePointerRegister);
}

Code::~Code()
{
}

void Code::emitDefaultPrologue(CCallHelpers& jit)
{
    defaultPrologueGenerator(jit, *this);
}

void Code::setRegsInPriorityOrder(Bank bank, const Vector<Reg>& regs)
{
    regsInPriorityOrderImpl(bank) = regs;
    m_mutableRegs = { };
    forEachBank(
        [&] (Bank bank) {
            for (Reg reg : regsInPriorityOrder(bank))
                m_mutableRegs.set(reg);
        });
}

void Code::pinRegister(Reg reg)
{
    Vector<Reg>& regs = regsInPriorityOrderImpl(Arg(Tmp(reg)).bank());
    ASSERT(regs.contains(reg));
    regs.removeFirst(reg);
    m_mutableRegs.clear(reg);
    ASSERT(!regs.contains(reg));
    m_pinnedRegs.set(reg);
}

RegisterSet Code::mutableGPRs()
{
    RegisterSet result = m_mutableRegs;
    result.filter(RegisterSet::allGPRs());
    return result;
}

RegisterSet Code::mutableFPRs()
{
    RegisterSet result = m_mutableRegs;
    result.filter(RegisterSet::allFPRs());
    return result;
}

bool Code::needsUsedRegisters() const
{
    return m_proc.needsUsedRegisters();
}

BasicBlock* Code::addBlock(double frequency)
{
    std::unique_ptr<BasicBlock> block(new BasicBlock(m_blocks.size(), frequency));
    BasicBlock* result = block.get();
    m_blocks.append(WTFMove(block));
    return result;
}

StackSlot* Code::addStackSlot(unsigned byteSize, StackSlotKind kind, B3::StackSlot* b3Slot)
{
    StackSlot* result = m_stackSlots.addNew(byteSize, kind, b3Slot);
    if (m_stackIsAllocated) {
        // FIXME: This is unnecessarily awful. Fortunately, it doesn't run often.
        unsigned extent = WTF::roundUpToMultipleOf(result->alignment(), frameSize() + byteSize);
        result->setOffsetFromFP(-static_cast<ptrdiff_t>(extent));
        setFrameSize(WTF::roundUpToMultipleOf(stackAlignmentBytes(), extent));
    }
    return result;
}

StackSlot* Code::addStackSlot(B3::StackSlot* b3Slot)
{
    return addStackSlot(b3Slot->byteSize(), StackSlotKind::Locked, b3Slot);
}

Special* Code::addSpecial(std::unique_ptr<Special> special)
{
    special->m_code = this;
    return m_specials.add(WTFMove(special));
}

CCallSpecial* Code::cCallSpecial()
{
    if (!m_cCallSpecial) {
        m_cCallSpecial = static_cast<CCallSpecial*>(
            addSpecial(makeUnique<CCallSpecial>()));
    }

    return m_cCallSpecial;
}

bool Code::isEntrypoint(BasicBlock* block) const
{
    // Note: This function must work both before and after LowerEntrySwitch.

    if (m_entrypoints.isEmpty())
        return !block->index();
    
    for (const FrequentedBlock& entrypoint : m_entrypoints) {
        if (entrypoint.block() == block)
            return true;
    }
    return false;
}

Optional<unsigned> Code::entrypointIndex(BasicBlock* block) const
{
    RELEASE_ASSERT(m_entrypoints.size());
    for (unsigned i = 0; i < m_entrypoints.size(); ++i) {
        if (m_entrypoints[i].block() == block)
            return i;
    }
    return WTF::nullopt;
}

void Code::setCalleeSaveRegisterAtOffsetList(RegisterAtOffsetList&& registerAtOffsetList, StackSlot* slot)
{
    m_uncorrectedCalleeSaveRegisterAtOffsetList = WTFMove(registerAtOffsetList);
    for (const RegisterAtOffset& registerAtOffset : m_uncorrectedCalleeSaveRegisterAtOffsetList)
        m_calleeSaveRegisters.set(registerAtOffset.reg());
    m_calleeSaveStackSlot = slot;
}

RegisterAtOffsetList Code::calleeSaveRegisterAtOffsetList() const
{
    RegisterAtOffsetList result = m_uncorrectedCalleeSaveRegisterAtOffsetList;
    if (StackSlot* slot = m_calleeSaveStackSlot) {
        ptrdiff_t offset = slot->byteSize() + slot->offsetFromFP();
        for (size_t i = result.size(); i--;) {
            result.at(i) = RegisterAtOffset(
                result.at(i).reg(),
                result.at(i).offset() + offset);
        }
    }
    return result;
}

void Code::resetReachability()
{
    clearPredecessors(m_blocks);
    if (m_entrypoints.isEmpty())
        updatePredecessorsAfter(m_blocks[0].get());
    else {
        for (const FrequentedBlock& entrypoint : m_entrypoints)
            updatePredecessorsAfter(entrypoint.block());
    }
    
    for (auto& block : m_blocks) {
        if (isBlockDead(block.get()) && !isEntrypoint(block.get()))
            block = nullptr;
    }
}

void Code::dump(PrintStream& out) const
{
    if (!m_entrypoints.isEmpty())
        out.print("Entrypoints: ", listDump(m_entrypoints), "\n");
    for (BasicBlock* block : *this)
        out.print(deepDump(block));
    if (stackSlots().size()) {
        out.print("Stack slots:\n");
        for (StackSlot* slot : stackSlots())
            out.print("    ", pointerDump(slot), ": ", deepDump(slot), "\n");
    }
    if (specials().size()) {
        out.print("Specials:\n");
        for (Special* special : specials())
            out.print("    ", deepDump(special), "\n");
    }
    if (m_frameSize || m_stackIsAllocated)
        out.print("Frame size: ", m_frameSize, m_stackIsAllocated ? " (Allocated)" : "", "\n");
    if (m_callArgAreaSize)
        out.print("Call arg area size: ", m_callArgAreaSize, "\n");
    RegisterAtOffsetList calleeSaveRegisters = this->calleeSaveRegisterAtOffsetList();
    if (calleeSaveRegisters.size())
        out.print("Callee saves: ", calleeSaveRegisters, "\n");
}

unsigned Code::findFirstBlockIndex(unsigned index) const
{
    while (index < size() && !at(index))
        index++;
    return index;
}

unsigned Code::findNextBlockIndex(unsigned index) const
{
    return findFirstBlockIndex(index + 1);
}

BasicBlock* Code::findNextBlock(BasicBlock* block) const
{
    unsigned index = findNextBlockIndex(block->index());
    if (index < size())
        return at(index);
    return nullptr;
}

void Code::addFastTmp(Tmp tmp)
{
    m_fastTmps.add(tmp);
}

void* Code::addDataSection(size_t size)
{
    return m_proc.addDataSection(size);
}

unsigned Code::jsHash() const
{
    unsigned result = 0;
    
    for (BasicBlock* block : *this) {
        result *= 1000001;
        for (Inst& inst : *block) {
            result *= 97;
            result += inst.jsHash();
        }
        for (BasicBlock* successor : block->successorBlocks()) {
            result *= 7;
            result += successor->index();
        }
    }
    for (StackSlot* slot : stackSlots()) {
        result *= 101;
        result += slot->jsHash();
    }
    
    return result;
}

void Code::setNumEntrypoints(unsigned numEntrypoints)
{
    m_prologueGenerators.clear();
    m_prologueGenerators.reserveCapacity(numEntrypoints);
    for (unsigned i = 0; i < numEntrypoints; ++i)
        m_prologueGenerators.uncheckedAppend(m_defaultPrologueGenerator.copyRef());
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
