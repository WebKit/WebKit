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
#include "Disassembler.h"

#if ENABLE(DISASSEMBLER)
#if USE(UDIS86) || (USE(LLVM_DISASSEMBLER) && (CPU(X86_64) || CPU(X86)))

#include "MacroAssemblerCodeRef.h"
#include "Options.h"
#include "UDis86Disassembler.h"
#include "LLVMDisassembler.h"

namespace JSC {

// This horrifying monster is needed because neither of our disassemblers supports
// all of x86, and using them together to disassemble the same instruction stream
// would result in a fairly jarring print-out since they print in different
// styles. Maybe we can do better in the future, but for now the caller hints
// whether he's using the subset of the architecture that our MacroAssembler
// supports (in which case we go with UDis86) or if he's using the LLVM subset.

bool tryToDisassemble(const MacroAssemblerCodePtr& codePtr, size_t size, const char* prefix, PrintStream& out, InstructionSubsetHint subsetHint)
{
    if (Options::forceUDis86Disassembler())
        return tryToDisassembleWithUDis86(codePtr, size, prefix, out, subsetHint);

    if (Options::forceLLVMDisassembler())
        return tryToDisassembleWithLLVM(codePtr, size, prefix, out, subsetHint);
    
    if (subsetHint == MacroAssemblerSubset
        && tryToDisassembleWithUDis86(codePtr, size, prefix, out, MacroAssemblerSubset))
        return true;

    if (subsetHint == LLVMSubset
        && tryToDisassembleWithLLVM(codePtr, size, prefix, out, LLVMSubset))
        return true;
    
    if (tryToDisassembleWithUDis86(codePtr, size, prefix, out, subsetHint))
        return true;
    if (tryToDisassembleWithLLVM(codePtr, size, prefix, out, subsetHint))
        return true;
    
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

} // namespace JSC

#endif // USE(UDIS86) || USE(LLVM_DISASSEMBLER)
#endif // ENABLE(DISASSEMBLER)
