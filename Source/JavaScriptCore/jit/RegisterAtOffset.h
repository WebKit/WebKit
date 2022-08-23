/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#pragma once

#include <cstddef>
#include <sys/_types/_intptr_t.h>
#if ENABLE(ASSEMBLER)

#include "B3Width.h"
#include "Reg.h"
#include <wtf/PrintStream.h>

namespace JSC {

class RegisterAtOffset {
public:
    RegisterAtOffset()
        : m_offset(0)
    {
    }
    
    RegisterAtOffset(Reg reg, ptrdiff_t offset, size_t size)
        : m_reg(reg)
        , m_offset(offset)
        , m_size(size)
    {
        constexpr ptrdiff_t maxOffset = (1l << ((sizeof(ptrdiff_t) - sizeof(Reg) - 1) * CHAR_BIT)) - 1;
        constexpr ptrdiff_t minOffset = -maxOffset + 1;
        RELEASE_ASSERT(offset < maxOffset && offset > minOffset);
        RELEASE_ASSERT(size <= B3::bytesForWidth(B3::conservativeWidth(B3::FP)));
    }
    
    bool operator!() const { return !m_reg; }
    
    Reg reg() const { return m_reg; }
    ptrdiff_t offset() const { return m_offset; }
    ptrdiff_t byteSize() const { return m_size; }
    int offsetAsIndex() const { ASSERT(!(offset() % sizeof(CPURegister))); return offset() / static_cast<int>(sizeof(CPURegister)); }
    
    bool operator==(const RegisterAtOffset& other) const
    {
        return reg() == other.reg() && offset() == other.offset();
    }
    
    bool operator<(const RegisterAtOffset& other) const
    {
        if (reg() != other.reg())
            return reg() < other.reg();
        return offset() < other.offset();
    }
    
    static Reg getReg(RegisterAtOffset* value) { return value->reg(); }
    
    void dump(PrintStream& out) const;

private:
    Reg m_reg;
    ptrdiff_t m_offset : (sizeof(ptrdiff_t) - sizeof(Reg) - 1) * CHAR_BIT;
    size_t m_size : CHAR_BIT;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
