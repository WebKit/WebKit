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

#pragma once

#include <optional>
#include <wtf/Span.h>
#include <wtf/SystemFree.h>

#if HAVE(BACKTRACE_SYMBOLS) || HAVE(BACKTRACE)
#include <execinfo.h>
#endif

#if HAVE(DLADDR)
#include <cxxabi.h>
#include <dlfcn.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#include <wtf/win/DbgHelperWin.h>
#endif

namespace WTF {

class PrintStream;

class StackTrace {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE NEVER_INLINE static std::unique_ptr<StackTrace> captureStackTrace(size_t maxFrames, size_t framesToSkip = 0);

    Span<void* const> stack() const
    {
        return Span<void* const> { m_stack + m_initialFrame, m_size };
    }

    void dump(PrintStream&) const;
private:

    StackTrace(size_t size, size_t initialFrame)
        : m_size(size)
        , m_initialFrame(initialFrame)
    {
    }

    size_t m_size;
    size_t m_initialFrame;
    void* m_stack[1];
};

class StackTraceSymbolResolver {
public:
    StackTraceSymbolResolver(Span<void* const> stack)
        : m_stack(stack)
    {
    }
    StackTraceSymbolResolver(const StackTrace& stack)
        : m_stack(stack.stack())
    {
    }

    class DemangleEntry {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        friend class StackTraceSymbolResolver;
        const char* mangledName() const { return m_mangledName; }
        const char* demangledName() const { return m_demangledName.get(); }

    private:
        DemangleEntry(const char* mangledName, const char* demangledName)
            : m_mangledName(mangledName)
            , m_demangledName(demangledName)
        { }

        const char* m_mangledName { nullptr };
        std::unique_ptr<const char[], SystemFree<const char[]>> m_demangledName;
    };

    WTF_EXPORT_PRIVATE static std::optional<DemangleEntry> demangle(void*);

    template<typename Functor>
    void forEach(Functor functor) const
    {
#if HAVE(BACKTRACE_SYMBOLS)
        char** symbols = backtrace_symbols(m_stack.data(), m_stack.size());
        if (!symbols)
            return;
#elif OS(WINDOWS)
        HANDLE hProc = GetCurrentProcess();
        uint8_t symbolData[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = { 0 };
        auto symbolInfo = reinterpret_cast<SYMBOL_INFO*>(symbolData);

        symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbolInfo->MaxNameLen = MAX_SYM_NAME;
#endif
        for (size_t i = 0; i < m_stack.size(); ++i) {
            const char* name = nullptr;
            auto demangled = demangle(m_stack[i]);
            if (demangled)
                name = demangled->demangledName() ? demangled->demangledName() : demangled->mangledName();
#if HAVE(BACKTRACE_SYMBOLS)
            if (!name)
                name = symbols[i];
#elif OS(WINDOWS)
            if (!name && DbgHelper::SymFromAddress(hProc, reinterpret_cast<DWORD64>(m_stack[i]), nullptr, symbolInfo))
                name = symbolInfo->Name;
#endif
            functor(i + 1, m_stack[i], name);
        }

#if HAVE(BACKTRACE_SYMBOLS)
        free(symbols);
#endif
    }
private:
    Span<void* const> m_stack;
};

class StackTracePrinter {
public:
    StackTracePrinter(Span<void* const> stack, const char* prefix = "")
        : m_stack(stack)
        , m_prefix(prefix)
    {
    }

    StackTracePrinter(const StackTrace& stack, const char* prefix = "")
        : m_stack(stack.stack())
        , m_prefix(prefix)
    {
    }

    WTF_EXPORT_PRIVATE void dump(PrintStream&) const;

private:
    const Span<void* const> m_stack;
    const char* const m_prefix;
};

inline void StackTrace::dump(PrintStream& out) const
{
    StackTracePrinter { *this }.dump(out);
}

} // namespace WTF

using WTF::StackTrace;
using WTF::StackTraceSymbolResolver;
using WTF::StackTracePrinter;
