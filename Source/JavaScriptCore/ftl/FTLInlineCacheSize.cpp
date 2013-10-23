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

#include "config.h"
#include "FTLInlineCacheSize.h"

#if ENABLE(FTL_JIT)

#include "JITInlineCacheGenerator.h"
#include "MacroAssembler.h"

namespace JSC { namespace FTL {

static size_t s_sizeOfGetById;
static size_t s_sizeOfPutById;

size_t sizeOfGetById()
{
    if (s_sizeOfGetById)
        return s_sizeOfGetById;
    
    MacroAssembler jit;
    
    JITGetByIdGenerator generator(
        0, CodeOrigin(), RegisterSet(), JSValueRegs(GPRInfo::regT6), JSValueRegs(GPRInfo::regT7),
        false);
    generator.generateFastPath(jit);
    
    return s_sizeOfGetById = jit.m_assembler.codeSize();
}

size_t sizeOfPutById()
{
    if (s_sizeOfPutById)
        return s_sizeOfPutById;
    
    MacroAssembler jit;
    
    JITPutByIdGenerator generator(
        0, CodeOrigin(), RegisterSet(), JSValueRegs(GPRInfo::regT6), JSValueRegs(GPRInfo::regT7),
        GPRInfo::regT8, false, NotStrictMode, NotDirect);
    generator.generateFastPath(jit);
    
    return s_sizeOfPutById = jit.m_assembler.codeSize();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

