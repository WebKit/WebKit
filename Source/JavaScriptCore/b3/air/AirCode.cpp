/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "AirCCallSpecial.h"
#include "B3BasicBlockUtils.h"
#include "B3StackSlot.h"

namespace JSC { namespace B3 { namespace Air {

Code::Code(Procedure& proc)
    : m_proc(proc)
    , m_lastPhaseName("initial")
{
}

Code::~Code()
{
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
    return m_stackSlots.addNew(byteSize, kind, b3Slot);
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
            addSpecial(std::make_unique<CCallSpecial>()));
    }

    return m_cCallSpecial;
}

void Code::resetReachability()
{
    B3::resetReachability(
        m_blocks,
        [&] (BasicBlock*) {
            // We don't have to do anything special for deleted blocks.
        });
}

void Code::dump(PrintStream& out) const
{
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
    if (m_frameSize)
        out.print("Frame size: ", m_frameSize, "\n");
    if (m_callArgAreaSize)
        out.print("Call arg area size: ", m_callArgAreaSize, "\n");
    if (m_calleeSaveRegisters.size())
        out.print("Callee saves: ", m_calleeSaveRegisters, "\n");
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

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
