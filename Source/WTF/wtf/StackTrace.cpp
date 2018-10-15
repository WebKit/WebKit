/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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
#include <wtf/StackTrace.h>

#include <wtf/Assertions.h>
#include <wtf/PrintStream.h>

#if HAVE(BACKTRACE_SYMBOLS) || HAVE(BACKTRACE)
#include <execinfo.h>
#endif

#if HAVE(DLADDR)
#include <cxxabi.h>
#include <dlfcn.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

void WTFGetBacktrace(void** stack, int* size)
{
#if HAVE(BACKTRACE)
    *size = backtrace(stack, *size);
#elif OS(WINDOWS)
    *size = RtlCaptureStackBackTrace(0, *size, stack, 0);
#else
    *size = 0;
#endif
}

namespace WTF {

ALWAYS_INLINE size_t StackTrace::instanceSize(int capacity)
{
    ASSERT(capacity >= 1);
    return sizeof(StackTrace) + (capacity - 1) * sizeof(void*);
}

std::unique_ptr<StackTrace> StackTrace::captureStackTrace(int maxFrames, int framesToSkip)
{
    maxFrames = std::max(1, maxFrames);
    size_t sizeToAllocate = instanceSize(maxFrames);
    std::unique_ptr<StackTrace> trace(new (NotNull, fastMalloc(sizeToAllocate)) StackTrace());

    // Skip 2 additional frames i.e. StackTrace::captureStackTrace and WTFGetBacktrace.
    framesToSkip += 2;
    int numberOfFrames = maxFrames + framesToSkip;

    WTFGetBacktrace(&trace->m_skippedFrame0, &numberOfFrames);
    if (numberOfFrames) {
        RELEASE_ASSERT(numberOfFrames >= framesToSkip);
        trace->m_size = numberOfFrames - framesToSkip;
    } else
        trace->m_size = 0;

    trace->m_capacity = maxFrames;

    return trace;
}

auto StackTrace::demangle(void* pc) -> std::optional<DemangleEntry>
{
#if HAVE(DLADDR)
    const char* mangledName = nullptr;
    const char* cxaDemangled = nullptr;
    Dl_info info;
    if (dladdr(pc, &info) && info.dli_sname)
        mangledName = info.dli_sname;
    if (mangledName) {
        int status = 0;
        cxaDemangled = abi::__cxa_demangle(mangledName, nullptr, nullptr, &status);
        UNUSED_PARAM(status);
    }
    if (mangledName || cxaDemangled)
        return DemangleEntry { mangledName, cxaDemangled };
#endif
    return std::nullopt;
}

void StackTrace::dump(PrintStream& out, const char* indentString) const
{
    const auto* stack = this->stack();
#if HAVE(BACKTRACE_SYMBOLS)
    char** symbols = backtrace_symbols(stack, m_size);
    if (!symbols)
        return;
#endif

    if (!indentString)
        indentString = "";
    for (int i = 0; i < m_size; ++i) {
        const char* mangledName = nullptr;
        const char* cxaDemangled = nullptr;
#if HAVE(BACKTRACE_SYMBOLS)
        mangledName = symbols[i];
#elif HAVE(DLADDR)
        auto demangled = demangle(stack[i]);
        if (demangled) {
            mangledName = demangled->mangledName();
            cxaDemangled = demangled->demangledName();
        }
#endif
        const int frameNumber = i + 1;
        if (mangledName || cxaDemangled)
            out.printf("%s%-3d %p %s\n", indentString, frameNumber, stack[i], cxaDemangled ? cxaDemangled : mangledName);
        else
            out.printf("%s%-3d %p\n", indentString, frameNumber, stack[i]);
    }

#if HAVE(BACKTRACE_SYMBOLS)
    free(symbols);
#endif
}

} // namespace WTF
