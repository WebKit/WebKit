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

#include <span>
#include <wtf/Forward.h>
#include <wtf/SystemFree.h>

#if HAVE(BACKTRACE) || HAVE(BACKTRACE_SYMBOLS)
#include <execinfo.h>
#endif

namespace WTF {

class PrintStream;

class StackTrace {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE NEVER_INLINE static std::unique_ptr<StackTrace> captureStackTrace(size_t maxFrames, size_t framesToSkip = 0);

    std::span<void* const> stack() const
    {
        return std::span<void* const> { m_stack + m_initialFrame, m_size };
    }

    void dump(PrintStream&) const;
    WTF_EXPORT_PRIVATE String toString() const;

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
    StackTraceSymbolResolver(std::span<void* const> stack)
        : m_stack(stack)
    {
    }
    StackTraceSymbolResolver(const StackTrace& stack)
        : m_stack(stack.stack())
    {
    }

    WTF_EXPORT_PRIVATE static auto resolve(void*) -> const char*;

    WTF_EXPORT_PRIVATE static auto demangle(const char*) -> std::unique_ptr<char, SystemFree<char>>;

    template<typename Functor>
    void forEach(Functor functor) const
    {
#if !USE(LIBBACKTRACE) && !HAVE(DLADDR) && !OS(WINDOWS)
        std::unique_ptr<char*, SystemFree<char*>> symbols(backtrace_symbols(m_stack.data(), m_stack.size()));
        if (!symbols)
            return;
#endif

        for (size_t i = 0; i < m_stack.size(); ++i) {
#if USE(LIBBACKTRACE) || HAVE(DLADDR) || OS(WINDOWS)
            auto mangledName = resolve(m_stack[i]);
#else
            auto mangledName = symbols.get()[i];
#endif
            auto demangledName = demangle(mangledName);
            functor(i + 1, m_stack[i], demangledName ? demangledName.get() : mangledName);
        }
    }
private:
    std::span<void* const> m_stack;
};

class StackTracePrinter {
public:
    StackTracePrinter(std::span<void* const> stack, const char* prefix = "")
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
    const std::span<void* const> m_stack;
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
