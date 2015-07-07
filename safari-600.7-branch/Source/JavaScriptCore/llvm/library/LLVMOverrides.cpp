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

#include "config_llvm.h"

#include "LLVMTrapCallback.h"

#if HAVE(LLVM)

extern "C" int __cxa_atexit();
extern "C" int __cxa_atexit() { return 0; }

void (*g_llvmTrapCallback)(const char* message, ...);

// If LLVM tries to raise signals, abort, or fail an assertion, then let
// WebKit handle it instead of trapping.
extern "C" int raise(int signal);
extern "C" void __assert_rtn(const char* function, const char* filename, int lineNumber, const char* expression);
extern "C" void abort(void);

extern "C" int raise(int signal)
{
    g_llvmTrapCallback("raise(%d) called.", signal);
    return 0;
}
extern "C" void __assert_rtn(const char* function, const char* filename, int lineNumber, const char* expression)
{
    if (function)
        g_llvmTrapCallback("Assertion failed: (%s), function %s, file %s, line %d.", expression, function, filename, lineNumber);
    g_llvmTrapCallback("Assertion failed: (%s), file %s, line %d.", expression, filename, lineNumber);
}
extern "C" void abort(void)
{
    g_llvmTrapCallback("abort() called.");
}

#endif // HAVE(LLVM)
