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
#include "LLVMDisassembler.h"

#if USE(LLVM_DISASSEMBLER)

#include "InitializeLLVM.h"
#include "LLVMAPI.h"
#include "MacroAssemblerCodeRef.h"

namespace JSC {

static const unsigned symbolStringSize = 40;

static const char *symbolLookupCallback(
    void* opaque, uint64_t referenceValue, uint64_t* referenceType, uint64_t referencePC,
    const char** referenceName)
{
    // Set this if you want to debug an unexpected reference type. Currently we only encounter these
    // if we try to disassemble garbage, since our code generator never uses them. These include things
    // like PC-relative references.
    static const bool crashOnUnexpected = false;
    
    char* symbolString = static_cast<char*>(opaque);
    
    switch (*referenceType) {
    case LLVMDisassembler_ReferenceType_InOut_None:
        return 0;
    case LLVMDisassembler_ReferenceType_In_Branch:
        *referenceName = 0;
        *referenceType = LLVMDisassembler_ReferenceType_InOut_None;
        snprintf(
            symbolString, symbolStringSize, "0x%lx",
            static_cast<unsigned long>(referenceValue));
        return symbolString;
    default:
        if (crashOnUnexpected) {
            dataLog("referenceValue = ", referenceValue, "\n");
            dataLog("referenceType = ", RawPointer(referenceType), ", *referenceType = ", *referenceType, "\n");
            dataLog("referencePC = ", referencePC, "\n");
            dataLog("referenceName = ", RawPointer(referenceName), "\n");
            
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        *referenceName = "unimplemented reference type!";
        *referenceType = LLVMDisassembler_ReferenceType_InOut_None;
        snprintf(
            symbolString, symbolStringSize, "unimplemented:0x%lx",
            static_cast<unsigned long>(referenceValue));
        return symbolString;
    }
}

bool tryToDisassembleWithLLVM(
    const MacroAssemblerCodePtr& codePtr, size_t size, const char* prefix, PrintStream& out,
    InstructionSubsetHint)
{
    initializeLLVM();
    
    const char* triple;
#if CPU(X86_64)
    triple = "x86_64-apple-darwin";
#elif CPU(X86)
    triple = "x86-apple-darwin";
#elif CPU(ARM64)
    triple = "arm64-apple-darwin";
#else
#error "LLVM disassembler currently not supported on this CPU."
#endif

    char symbolString[symbolStringSize];
    
    LLVMDisasmContextRef disassemblyContext =
        llvm->CreateDisasm(triple, symbolString, 0, 0, symbolLookupCallback);
    RELEASE_ASSERT(disassemblyContext);
    
    char pcString[20];
    char instructionString[1000];
    
    uint8_t* pc = static_cast<uint8_t*>(codePtr.executableAddress());
    uint8_t* end = pc + size;
    
    while (pc < end) {
        snprintf(
            pcString, sizeof(pcString), "0x%lx",
            static_cast<unsigned long>(bitwise_cast<uintptr_t>(pc)));

        size_t instructionSize = llvm->DisasmInstruction(
            disassemblyContext, pc, end - pc, bitwise_cast<uintptr_t>(pc),
            instructionString, sizeof(instructionString));
        
        if (!instructionSize)
            snprintf(instructionString, sizeof(instructionString), ".byte 0x%02x", *pc++);
        else
            pc += instructionSize;
        
        out.printf("%s%16s: %s\n", prefix, pcString, instructionString);
    }
    
    llvm->DisasmDispose(disassemblyContext);
    
    return true;
}

} // namespace JSC

#endif // USE(LLVM_DISASSEMBLER)

