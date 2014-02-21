/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "FTLUnwindInfo.h"

#if ENABLE(FTL_JIT)

#include <mach-o/compact_unwind_encoding.h>
#include <wtf/ListDump.h>

namespace JSC { namespace FTL {

UnwindInfo::UnwindInfo() { }
UnwindInfo::~UnwindInfo() { }

namespace {

struct CompactUnwind {
    void* function;
    uint32_t size;
    compact_unwind_encoding_t encoding;
    void* personality;
    void* lsda;
};

} // anonymous namespace

bool UnwindInfo::parse(void* section, size_t size, GeneratedFunction generatedFunction)
{
    m_registers.clear();
    
    RELEASE_ASSERT(!!section == !!size);
    if (!section)
        return false;
    
    RELEASE_ASSERT(size >= sizeof(CompactUnwind));
    
    CompactUnwind* data = bitwise_cast<CompactUnwind*>(section);
    
    RELEASE_ASSERT(!data->personality); // We don't know how to handle this.
    RELEASE_ASSERT(!data->lsda); // We don't know how to handle this.
    RELEASE_ASSERT(data->function == generatedFunction); // The unwind data better be for our function.
    
    compact_unwind_encoding_t encoding = data->encoding;
    RELEASE_ASSERT(!(encoding & UNWIND_IS_NOT_FUNCTION_START));
    RELEASE_ASSERT(!(encoding & UNWIND_HAS_LSDA));
    RELEASE_ASSERT(!(encoding & UNWIND_PERSONALITY_MASK));

#if CPU(X86_64)
    RELEASE_ASSERT((encoding & UNWIND_X86_64_MODE_MASK) == UNWIND_X86_64_MODE_RBP_FRAME);
    
    int32_t offset = -((encoding & UNWIND_X86_64_RBP_FRAME_OFFSET) >> 16) * 8;
    uint32_t nextRegisters = encoding;
    for (unsigned i = 5; i--;) {
        uint32_t currentRegister = nextRegisters & 7;
        nextRegisters >>= 3;
        
        switch (currentRegister) {
        case UNWIND_X86_64_REG_NONE:
            break;
            
        case UNWIND_X86_64_REG_RBX:
            m_registers.append(RegisterAtOffset(X86Registers::ebx, offset));
            break;
            
        case UNWIND_X86_64_REG_R12:
            m_registers.append(RegisterAtOffset(X86Registers::r12, offset));
            break;
            
        case UNWIND_X86_64_REG_R13:
            m_registers.append(RegisterAtOffset(X86Registers::r13, offset));
            break;
            
        case UNWIND_X86_64_REG_R14:
            m_registers.append(RegisterAtOffset(X86Registers::r14, offset));
            break;
            
        case UNWIND_X86_64_REG_R15:
            m_registers.append(RegisterAtOffset(X86Registers::r15, offset));
            break;
            
        case UNWIND_X86_64_REG_RBP:
            m_registers.append(RegisterAtOffset(X86Registers::ebp, offset));
            break;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        offset += 8;
    }
#elif CPU(ARM64)
    RELEASE_ASSERT((encoding & UNWIND_ARM64_MODE_MASK) == UNWIND_ARM64_MODE_FRAME);
    
    m_registers.append(RegisterAtOffset(ARM64Registers::fp, 0));
    
    int32_t offset = 0;
    if (encoding & UNWIND_ARM64_FRAME_X19_X20_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x19, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x20, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_X21_X22_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x21, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x22, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_X23_X24_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x23, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x24, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_X25_X26_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x25, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x26, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_X27_X28_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x27, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x28, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_D8_D9_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::q8, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::q9, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_D10_D11_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::q10, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::q11, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_D12_D13_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::q12, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::q13, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_D14_D15_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::q14, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::q15, offset -= 8));
    }
#else
#error "Unrecognized architecture"
#endif
    
    std::sort(m_registers.begin(), m_registers.end());
    return true;
}

void UnwindInfo::dump(PrintStream& out) const
{
    out.print(listDump(m_registers));
}

RegisterAtOffset* UnwindInfo::find(Reg reg) const
{
    return tryBinarySearch<RegisterAtOffset, Reg>(m_registers, m_registers.size(), reg, RegisterAtOffset::getReg);
}

unsigned UnwindInfo::indexOf(Reg reg) const
{
    if (RegisterAtOffset* pointer = find(reg))
        return pointer - m_registers.begin();
    return UINT_MAX;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

