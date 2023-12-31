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

#include <optional>
#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/PrintStream.h>
#include <wtf/StringPrintStream.h>

#if USE(LIBBACKTRACE)
#include <backtrace.h>
#include <wtf/NeverDestroyed.h>
#endif

#if HAVE(DLADDR)
#include <dlfcn.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#include <wtf/win/DbgHelperWin.h>
#else
#include <cxxabi.h>
#endif

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

#if USE(LIBBACKTRACE)
static struct backtrace_state* backtraceState()
{
    static NeverDestroyed<struct backtrace_state*> backtraceState = backtrace_create_state(nullptr, 1, nullptr, nullptr);
    return backtraceState;
}

static void backtraceSyminfoCallback(void* data, uintptr_t, const char* symname, uintptr_t, uintptr_t)
{
    const char** symbol = static_cast<const char**>(data);
    *symbol = symname;
}

static int backtraceFullCallback(void* data, uintptr_t, const char*, int, const char* function)
{
    const char** symbol = static_cast<const char**>(data);
    *symbol = function;
    return 0;
}
#endif

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

String StackTrace::toString() const
{
    StringPrintStream stream;
    dump(stream);
    return stream.toString();
}

auto StackTraceSymbolResolver::resolve(void* pc) -> const char *
{
#if USE(LIBBACKTRACE)
    struct backtrace_state* state = backtraceState();
    if (!state)
        return nullptr;

    char* symbol;
    backtrace_pcinfo(state, reinterpret_cast<uintptr_t>(pc), backtraceFullCallback, nullptr, &symbol);
    if (symbol)
        return symbol;

    backtrace_syminfo(backtraceState(), reinterpret_cast<uintptr_t>(pc), backtraceSyminfoCallback, nullptr, &symbol);
    if (symbol)
        return symbol;
#elif HAVE(DLADDR)
    Dl_info info;
    if (dladdr(pc, &info) && info.dli_sname)
        return info.dli_sname;
#elif OS(WINDOWS)
    HANDLE hProc = GetCurrentProcess();
    uint8_t symbolData[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = { 0 };
    auto symbolInfo = reinterpret_cast<SYMBOL_INFO*>(symbolData);

    symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbolInfo->MaxNameLen = MAX_SYM_NAME;

    if (DbgHelper::SymFromAddress(hProc, reinterpret_cast<DWORD64>(pc), nullptr, symbolInfo))
        return symbolInfo->Name;
#else
    UNUSED_PARAM(pc);
#endif
    return nullptr;
}

auto StackTraceSymbolResolver::demangle(const char* mangledName) -> std::unique_ptr<char, SystemFree<char>>
{
#if OS(WINDOWS)
    UNUSED_PARAM(mangledName);
    return nullptr;
#else
    return std::unique_ptr<char, SystemFree<char>>(abi::__cxa_demangle(mangledName, nullptr, nullptr, nullptr));
#endif
}

void StackTracePrinter::dump(PrintStream& out) const
{
    StackTraceSymbolResolver { m_stack }.forEach([&](int frameNumber, void* stackFrame, const char* name) {
        out.printf("%s%-3d %p %s\n", m_prefix ? m_prefix : "", frameNumber, stackFrame, name ? name : "?");
    });
}

} // namespace WTF
