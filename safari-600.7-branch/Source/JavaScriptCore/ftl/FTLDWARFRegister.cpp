/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "FTLDWARFRegister.h"

#if ENABLE(FTL_JIT)

#include "FPRInfo.h"
#include "GPRInfo.h"

namespace JSC { namespace FTL {

Reg DWARFRegister::reg() const
{
#if CPU(X86_64)
    if (m_dwarfRegNum >= 0 && m_dwarfRegNum < 16) {
        switch (dwarfRegNum()) {
        case 0:
            return X86Registers::eax;
        case 1:
            return X86Registers::edx;
        case 2:
            return X86Registers::ecx;
        case 3:
            return X86Registers::ebx;
        case 4:
            return X86Registers::esi;
        case 5:
            return X86Registers::edi;
        case 6:
            return X86Registers::ebp;
        case 7:
            return X86Registers::esp;
        default:
            RELEASE_ASSERT(m_dwarfRegNum < 16);
            // Registers r8..r15 are numbered sensibly.
            return static_cast<GPRReg>(m_dwarfRegNum);
        }
    }
    if (m_dwarfRegNum >= 17 && m_dwarfRegNum <= 32)
        return static_cast<FPRReg>(m_dwarfRegNum - 17);
#elif CPU(ARM64)
    if (m_dwarfRegNum >= 0 && m_dwarfRegNum <= 31)
        return static_cast<GPRReg>(m_dwarfRegNum);
    if (m_dwarfRegNum >= 64 && m_dwarfRegNum <= 95)
        return static_cast<FPRReg>(m_dwarfRegNum - 64);
#endif
    return Reg();
}

void DWARFRegister::dump(PrintStream& out) const
{
    Reg reg = this->reg();
    if (!!reg)
        out.print(reg);
    else
        out.print("<unknown:", m_dwarfRegNum, ">");
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

