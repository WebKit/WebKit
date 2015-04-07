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

#include "config.h"
#include "InitializeLLVMPOSIX.h"

#if HAVE(LLVM)

#include "LLVMAPI.h"
#include "Options.h"
#include <dlfcn.h>
#include <wtf/DataLog.h>

namespace JSC {

LLVMInitializerFunction getLLVMInitializerFunctionPOSIX(const char* libraryName, bool verbose)
{
    int flags = RTLD_NOW;
    
#if OS(LINUX)
    // We need this to cause our overrides (like __cxa_atexit) to take precedent over the __cxa_atexit that is already
    // globally exported. Those overrides are necessary to prevent crashes (our __cxa_atexit turns off LLVM's exit-time
    // destruction, which causes exit-time crashes if the concurrent JIT is still running) and to make LLVM assertion
    // failures funnel through WebKit's mechanisms. This flag induces behavior that is the default on Darwin. Other OSes
    // may need their own flags in place of this.
    flags |= RTLD_DEEPBIND;
#endif
    
    void* library = dlopen(libraryName, flags);
    if (!library) {
        if (verbose)
            dataLog("Failed to load LLVM library at ", libraryName, ": ", dlerror(), "\n");
        return nullptr;
    }
    
    const char* symbolName = "initializeAndGetJSCLLVMAPI";
    LLVMInitializerFunction initializer = bitwise_cast<LLVMInitializerFunction>(
        dlsym(library, symbolName));
    if (!initializer) {
        if (verbose)
            dataLog("Failed to find ", symbolName, " in ", libraryName, ": ", dlerror());
        return nullptr;
    }
    
    return initializer;
}

} // namespace JSC

#endif // HAVE(LLVM)

