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

static void llvmCrash(const char*) NO_RETURN;
extern "C" WTF_EXPORT_PRIVATE JSC::LLVMAPI* initializeAndGetJSCLLVMAPI(
    void (*)(const char*, ...) NO_RETURN, bool* enableFastISel);

static void llvmCrash(const char* reason)
{
    g_llvmTrapCallback("LLVM fatal error: %s", reason);
}

template<typename... Args>
void initCommandLine(Args... args)
{
    const char* theArgs[] = { args... };
    llvm::cl::ParseCommandLineOptions(sizeof(theArgs) / sizeof(const char*), theArgs);
}

extern "C" JSC::LLVMAPI* initializeAndGetJSCLLVMAPI(
    void (*callback)(const char*, ...) NO_RETURN,
    bool* enableFastISel)
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
#if (__IPHONE_OS_VERSION_MIN_REQUIRED > 80200) || OS(LINUX)
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeAArch64Disassembler();
#else
    LLVMInitializeARM64TargetInfo();
    LLVMInitializeARM64Target();
    LLVMInitializeARM64TargetMC();
    LLVMInitializeARM64AsmPrinter();
    LLVMInitializeARM64Disassembler();
#endif
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    
#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 6)
    // It's OK to have fast ISel, if it was requested.
#else
    // We don't have enough support for fast ISel. Disable it.
    *enableFastISel = false;
#endif

    if (*enableFastISel)
        initCommandLine("llvmForJSC.dylib", "-enable-misched=false", "-regalloc=basic");
    else
        initCommandLine("llvmForJSC.dylib", "-enable-patchpoint-liveness=true");
    
    JSC::LLVMAPI* result = new JSC::LLVMAPI;
    
    // Initialize the whole thing to null.
    memset(result, 0, sizeof(*result));
    
#define LLVM_API_FUNCTION_ASSIGNMENT(returnType, name, signature) \
    result->name = LLVM##name;
    FOR_EACH_LLVM_API_FUNCTION(LLVM_API_FUNCTION_ASSIGNMENT);
#undef LLVM_API_FUNCTION_ASSIGNMENT
    
    // Handle conditionally available functions.
#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 6)
    result->AddLowerSwitchPass = LLVMAddLowerSwitchPass;
#endif
    
    return result;
}

#else

// Create a dummy initializeAndGetJSCLLVMAPI to placate the build system.
extern "C" WTF_EXPORT_PRIVATE void initializeAndGetJSCLLVMAPI();
extern "C" void initializeAndGetJSCLLVMAPI() { }

#endif // HAVE(LLVM)
