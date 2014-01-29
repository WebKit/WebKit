/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLRegisterAtOffset_h
#define FTLRegisterAtOffset_h

#if ENABLE(FTL_JIT)

#include "GPRInfo.h"
#include <wtf/PrintStream.h>

namespace JSC { namespace FTL {

class RegisterAtOffset {
public:
    RegisterAtOffset()
        : m_gpr(InvalidGPRReg)
        , m_offset(0)
    {
    }
    
    RegisterAtOffset(GPRReg gpr, ptrdiff_t offset)
        : m_gpr(gpr)
        , m_offset(offset)
    {
    }
    
    bool operator!() const { return m_gpr == InvalidGPRReg; }
    
    GPRReg gpr() const { return m_gpr; }
    ptrdiff_t offset() const { return m_offset; }
    
    bool operator==(const RegisterAtOffset& other) const
    {
        return gpr() == other.gpr() && offset() == other.offset();
    }
    
    bool operator<(const RegisterAtOffset& other) const
    {
        if (gpr() != other.gpr())
            return gpr() < other.gpr();
        return offset() < other.offset();
    }
    
    static GPRReg getGPR(RegisterAtOffset* value) { return value->gpr(); }
    
    void dump(PrintStream& out) const;

private:
    GPRReg m_gpr;
    ptrdiff_t m_offset;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLRegisterAtOffset_h

