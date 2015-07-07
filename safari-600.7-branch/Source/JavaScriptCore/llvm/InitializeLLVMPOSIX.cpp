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
#include "InitializeLLVMPOSIX.h"

#if HAVE(LLVM)

#include "LLVMAPI.h"
#include "Options.h"
#include <dlfcn.h>
#include <wtf/DataLog.h>

namespace JSC {

typedef void (*LoggerFunction)(const char*, ...) WTF_ATTRIBUTE_PRINTF(1, 2);
typedef LLVMAPI* (*InitializerFunction)(LoggerFunction);

void initializeLLVMPOSIX(const char* libraryName)
{
    const bool verbose =
        Options::verboseFTLCompilation()
        || Options::showFTLDisassembly()
        || Options::verboseFTLFailure()
        || Options::verboseCompilation()
        || Options::showDFGDisassembly()
        || Options::showDisassembly();
    
    void* library = dlopen(libraryName, RTLD_NOW);
    if (!library) {
        if (verbose)
            dataLog("Failed to load LLVM library at ", libraryName, ": ", dlerror(), "\n");
        return;
    }
    
    const char* symbolName = "initializeAndGetJSCLLVMAPI";
    InitializerFunction initializer = bitwise_cast<InitializerFunction>(
        dlsym(library, symbolName));
    if (!initializer) {
        if (verbose)
            dataLog("Failed to find ", symbolName, " in ", libraryName, ": ", dlerror());
        return;
    }
    
    llvm = initializer(WTFLogAlwaysAndCrash);
    if (!llvm) {
        if (verbose)
            dataLog("LLVM initilization failed.\n");
    }
}

} // namespace JSC

#endif // HAVE(LLVM)

