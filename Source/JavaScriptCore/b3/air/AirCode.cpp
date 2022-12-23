/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "CCallHelpers.h"
#include <wtf/ListDump.h>
#include <wtf/MathExtras.h>

namespace JSC { namespace B3 { namespace Air {

const char* const tierName = "Air ";

static void defaultPrologueGenerator(CCallHelpers& jit, Code& code)
{
    jit.emitFunctionPrologue();

    // NOTE: on ARM64, if the callee saves have bigger offsets due to a potential tail call,
    // the macro assembler might assert scratch register usage on store operations emitted by emitSave.
    AllowMacroScratchRegisterUsageIf allowScratch(jit, isARM64() || isARM64E() || isARM());

    if (code.frameSize()) {
        jit.subPtr(MacroAssembler::TrustedImm32(code.frameSize()), MacroAssembler::stackPointerRegister);
    }
    
    jit.emitSave(code.calleeSaveRegisterAtOffsetList());
}

Code::Code(Procedure& proc)
    : m_proc(proc)
    , m_cfg(new CFG(*this))
    , m_preserveB3Origins(Options::dumpAirGraphAtEachPhase() || Options::dumpFTLDisassembly())
    , m_lastPhaseName("initial")
    , m_defaultPrologueGenerator(createSharedTask<PrologueGeneratorFunction>(&defaultPrologueGenerator))
{
    // Come up with initial orderings of registers. The user may replace this with something else.
    std::optional<WeakRandom> weakRandom;
    if (Options::airRandomizeRegs())
        weakRandom.emplace();
    forEachBank(
        [&](Bank bank) {
            Vector<Reg> volatileRegs;
            Vector<Reg> fullCalleeSaveRegs;
            Vector<Reg> calleeSaveRegs;
            RegisterSetBuilder all = bank == GP ? RegisterSetBuilder::allGPRs() : RegisterSetBuilder::allFPRs();
            all.exclude(RegisterSetBuilder::stackRegisters());
            all.exclude(RegisterSetBuilder::reservedHardwareRegisters());
#if CPU(ARM)
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=243888
            // Unfortunately, the extra registers provided by the neon/vfpv3
            // extensions can't be used by Air right now, because they are
            // invalid as f32 operands, and Air doesn't know about anything
            // other than a single class of FP registers.
#if CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)
            if (bank == FP) {
                for (auto reg = ARMRegisters::d16; reg <= ARMRegisters::d31; reg = MacroAssembler::nextFPRegister(reg))
                    all.remove(reg);
            }
#endif
            all.remove(MacroAssembler::fpTempRegister);
#endif // CPU(ARM)
            auto calleeSave = RegisterSetBuilder::calleeSaveRegisters();
            all.buildAndValidate().forEach(
                [&] (Reg reg) {
                    if (!calleeSave.contains(reg, IgnoreVectors))
                        volatileRegs.append(reg);
                    if (calleeSave.contains(reg, conservativeWidth(reg)))
                        fullCalleeSaveRegs.append(reg);
                    else if (calleeSave.contains(reg, conservativeWidthWithoutVectors(reg)))
                        calleeSaveRegs.append(reg);
                });
            if (Options::airRandomizeRegs()) {
                WeakRandom random(Options::airRandomizeRegsSeed() ? Options::airRandomizeRegsSeed() : weakRandom->getUint32());
                shuffleVector(volatileRegs, [&] (unsigned limit) { return random.getUint32(limit); });
                shuffleVector(calleeSaveRegs, [&] (unsigned limit) { return random.getUint32(limit); });
                shuffleVector(fullCalleeSaveRegs, [&] (unsigned limit) { return random.getUint32(limit); });
            }
            Vector<Reg> result;
            result.appendVector(volatileRegs);
            result.appendVector(fullCalleeSaveRegs);
            if (!usesSIMD())
                result.appendVector(calleeSaveRegs);
            setRegsInPriorityOrder(bank, result);
        });

#if CPU(ARM_THUMB2)
    if (auto reg = extendedOffsetAddrRegister())
        pinRegister(reg);
#endif
    m_pinnedRegs.add(MacroAssembler::framePointerRegister, IgnoreVectors);
}

Code::~Code()
{
}

void Code::emitDefaultPrologue(CCallHelpers& jit)
{
    defaultPrologueGenerator(jit, *this);
}

void Code::emitEpilogue(CCallHelpers& jit)
{
    if (frameSize()) {
        // NOTE: on ARM64, if the callee saves have bigger offsets due to a potential tail call,
        // the macro assembler might assert scratch register usage on load operations emitted by emitRestore.
        AllowMacroScratchRegisterUsageIf allowScratch(jit, isARM64());
        jit.emitRestore(calleeSaveRegisterAtOffsetList());
        jit.emitFunctionEpilogue();
    } else
        jit.emitFunctionEpilogueWithEmptyFrame();
    jit.ret();
}

void Code::setRegsInPriorityOrder(Bank bank, const Vector<Reg>& regs)
{
    regsInPriorityOrderImpl(bank) = regs;
    m_mutableRegs = { };
    forEachBank(
        [&] (Bank bank) {
            for (Reg reg : regsInPriorityOrder(bank))
                m_mutableRegs.add(reg, IgnoreVectors);
        });
}

void Code::pinRegister(Reg reg)
{
    Vector<Reg>& regs = regsInPriorityOrderImpl(Arg(Tmp(reg)).bank());
    ASSERT(regs.contains(reg));
    regs.removeFirst(reg);
    m_mutableRegs.remove(reg);
    ASSERT(!regs.contains(reg));
    m_pinnedRegs.add(reg, IgnoreVectors);
}

RegisterSet Code::mutableGPRs()
{
    RegisterSetBuilder result = m_mutableRegs.toRegisterSet();
    result.filter(RegisterSetBuilder::allGPRs());
    return result.buildAndValidate();
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

StackSlot* Code::addStackSlot(uint64_t byteSize, StackSlotKind kind)
{
    StackSlot* result = m_stackSlots.addNew(byteSize, kind);
    if (m_stackIsAllocated) {
        // FIXME: This is unnecessarily awful. Fortunately, it doesn't run often.
        unsigned extent = WTF::roundUpToMultipleOf(result->alignment(), frameSize() - stackAdjustmentForAlignment() + byteSize);
        result->setOffsetFromFP(-static_cast<ptrdiff_t>(extent));
        setFrameSize(WTF::roundUpToMultipleOf(stackAlignmentBytes(), extent) + stackAdjustmentForAlignment());
    }
    return result;
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
            addSpecial(makeUnique<CCallSpecial>(usesSIMD())));
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

std::optional<unsigned> Code::entrypointIndex(BasicBlock* block) const
{
    RELEASE_ASSERT(m_entrypoints.size());
    for (unsigned i = 0; i < m_entrypoints.size(); ++i) {
        if (m_entrypoints[i].block() == block)
            return i;
    }
    return std::nullopt;
}

void Code::setCalleeSaveRegisterAtOffsetList(RegisterAtOffsetList&& registerAtOffsetList, StackSlot* slot)
{
    m_uncorrectedCalleeSaveRegisterAtOffsetList = WTFMove(registerAtOffsetList);
    for (const RegisterAtOffset& registerAtOffset : m_uncorrectedCalleeSaveRegisterAtOffsetList) {
        ASSERT(registerAtOffset.width() <= Width64);
        m_calleeSaveRegisters.add(registerAtOffset.reg(), registerAtOffset.width());
    }
    m_calleeSaveStackSlot = slot;
}

RegisterAtOffsetList Code::calleeSaveRegisterAtOffsetList() const
{
    RegisterAtOffsetList result = m_uncorrectedCalleeSaveRegisterAtOffsetList;
    if (StackSlot* slot = m_calleeSaveStackSlot)
        result.adjustOffsets(slot->byteSize() + slot->offsetFromFP());
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
        out.print(tierName, "Entrypoints: ", listDump(m_entrypoints), "\n");
    for (BasicBlock* block : *this)
        out.print(deepDump(block));
    if (stackSlots().size()) {
        out.print(tierName, "Stack slots:\n");
        for (StackSlot* slot : stackSlots())
            out.print(tierName, "    ", pointerDump(slot), ": ", deepDump(slot), "\n");
    }
    if (specials().size()) {
        out.print(tierName, "Specials:\n");
        for (Special* special : specials())
            out.print(tierName, "    ", deepDump(special), "\n");
    }
    if (m_frameSize || m_stackIsAllocated)
        out.print(tierName, "Frame size: ", m_frameSize, m_stackIsAllocated ? " (Allocated)" : "", "\n");
    if (m_callArgAreaSize)
        out.print(tierName, "Call arg area size: ", m_callArgAreaSize, "\n");
    RegisterAtOffsetList calleeSaveRegisters = this->calleeSaveRegisterAtOffsetList();
    if (calleeSaveRegisters.registerCount())
        out.print(tierName, "Callee saves: ", calleeSaveRegisters, "\n");
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

void Code::setNumEntrypoints(unsigned numEntryPoints)
{
    m_prologueGenerators = { numEntryPoints, m_defaultPrologueGenerator.copyRef() };
}

bool Code::usesSIMD() const
{
    return m_proc.usesSIMD();
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
