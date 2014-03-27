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

#include "config_llvm.h"

#if HAVE(LLVM)

#include "LLVMAPI.h"
#include "LLVMTrapCallback.h"

// Include some extra LLVM C++ headers. This is the only place where including LLVM C++
// headers is OK, since this module lives inside of the LLVM dylib we make and so can see
// otherwise private things. But to include those headers, we need to do some weird things
// first. Anyway, we should keep LLVM C++ includes to a minimum if possible.

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif // COMPILER(CLANG)

#include <llvm/Support/CommandLine.h>

#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif // COMPILER(CLANG)

#undef __STDC_LIMIT_MACROS
#undef __STDC_CONSTANT_MACROS

extern "C" WTF_EXPORT_PRIVATE JSC::LLVMAPI* initializeAndGetJSCLLVMAPI(void (*)(const char*, ...));

static void llvmCrash(const char* reason)
{
    g_llvmTrapCallback("LLVM fatal error: %s", reason);
}

extern "C" JSC::LLVMAPI* initializeAndGetJSCLLVMAPI(void (*callback)(const char*, ...))
{
    g_llvmTrapCallback = callback;
    
    LLVMInstallFatalErrorHandler(llvmCrash);

    if (!LLVMStartMultithreaded())
        callback("Could not start LLVM multithreading");
    
    LLVMLinkInMCJIT();
    
    // You think you want to call LLVMInitializeNativeTarget()? Think again. This presumes that
    // LLVM was ./configured correctly, which won't be the case in cross-compilation situations.
    
#if CPU(X86_64)
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86Disassembler();
#elif CPU(ARM64)
    LLVMInitializeARM64TargetInfo();
    LLVMInitializeARM64Target();
    LLVMInitializeARM64TargetMC();
    LLVMInitializeARM64AsmPrinter();
    LLVMInitializeARM64Disassembler();
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    
    const char* args[] = {
        "-enable-stackmap-liveness=true",
        "-enable-patchpoint-liveness=true"
    };
    llvm::cl::ParseCommandLineOptions(sizeof(args) / sizeof(const char*), args);
    
    JSC::LLVMAPI* result = new JSC::LLVMAPI;
    
#define LLVM_API_FUNCTION_ASSIGNMENT(returnType, name, signature) \
    result->name = LLVM##name;
    FOR_EACH_LLVM_API_FUNCTION(LLVM_API_FUNCTION_ASSIGNMENT);
#undef LLVM_API_FUNCTION_ASSIGNMENT
    
    return result;
}

#else

// Create a dummy initializeAndGetJSCLLVMAPI to placate the build system.
extern "C" WTF_EXPORT_PRIVATE void initializeAndGetJSCLLVMAPI();
extern "C" void initializeAndGetJSCLLVMAPI() { }

#endif // HAVE(LLVM)
