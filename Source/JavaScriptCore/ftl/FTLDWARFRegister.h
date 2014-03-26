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

#ifndef FTLDWARFRegister_h
#define FTLDWARFRegister_h

#if ENABLE(FTL_JIT)

#include "Reg.h"
#include <wtf/PrintStream.h>

namespace JSC { namespace FTL {

class DWARFRegister {
public:
    DWARFRegister()
        : m_dwarfRegNum(-1)
    {
    }
    
    explicit DWARFRegister(int16_t dwarfRegNum)
        : m_dwarfRegNum(dwarfRegNum)
    {
    }
    
    int16_t dwarfRegNum() const { return m_dwarfRegNum; }
    
    Reg reg() const; // This may return Reg() if it can't parse the Dwarf register number.
    
    void dump(PrintStream&) const;
    
private:
    int16_t m_dwarfRegNum;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLDWARFRegister_h

