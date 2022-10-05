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

#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/PrintStream.h>

void WTFGetBacktrace(void** stack, int* size)
{
#if HAVE(BACKTRACE)
    *size = backtrace(stack, *size);
#elif OS(WINDOWS)
    *size = RtlCaptureStackBackTrace(0, *size, stack, nullptr);
#else
    UNUSED_PARAM(stack);
    *size = 0;
#endif
}

namespace WTF {

std::unique_ptr<StackTrace> StackTrace::captureStackTrace(size_t maxFrames, size_t framesToSkip)
{
    static_assert(sizeof(StackTrace) == sizeof(void*) * 3);
    // We overwrite the memory of the two first skipped frames, m_stack[0] will hold the third one.
    static_assert(offsetof(StackTrace, m_stack) == sizeof(void*) * 2);

    maxFrames = std::max<size_t>(1, maxFrames);
    // Skip 2 additional frames i.e. StackTrace::captureStackTrace and WTFGetBacktrace.
    framesToSkip += 2;
    size_t capacity = maxFrames + framesToSkip;
    void** storage = static_cast<void**>(fastMalloc(capacity * sizeof(void*)));
    size_t size = 0;
    size_t initialFrame = 0;
    int capturedFrames = static_cast<int>(capacity);
    WTFGetBacktrace(storage, &capturedFrames);
    if (static_cast<size_t>(capturedFrames) > framesToSkip) {
        size = static_cast<size_t>(capturedFrames) - framesToSkip;
        initialFrame = framesToSkip - 2; 
    }
    return std::unique_ptr<StackTrace> { new (NotNull, storage) StackTrace(size, initialFrame) };
}

auto StackTraceSymbolResolver::demangle(void* pc) -> std::optional<DemangleEntry>
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
#else
    UNUSED_PARAM(pc);
#endif
    return std::nullopt;
}

void StackTracePrinter::dump(PrintStream& out) const
{
    StackTraceSymbolResolver { m_stack }.forEach([&](int frameNumber, void* stackFrame, const char* name) {
        out.printf("%s%-3d %p %s\n", m_prefix ? m_prefix : "", frameNumber, stackFrame, name);
    });
}

} // namespace WTF
