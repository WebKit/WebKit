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
    WTF_EXPORT_PRIVATE static std::unique_ptr<StackTrace> captureStackTrace(int maxFrames, int framesToSkip = 0);

    // Borrowed stack trace.
    StackTrace(void** stack, int size, const char* prefix = "")
        : m_prefix(prefix)
        , m_size(size)
        , m_borrowedStack(stack)
    { }

    int size() const { return m_size; }
    void* const * stack() const
    {
        if (!m_capacity)
            return m_borrowedStack;
        return m_stack;
    }

    class DemangleEntry {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        friend class StackTrace;
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

    WTF_EXPORT_PRIVATE void dump(PrintStream&, const char* indentString = nullptr) const;

    template<typename Functor>
    void forEach(Functor functor) const
    {
        const auto* stack = this->stack();
#if HAVE(BACKTRACE_SYMBOLS)
        char** symbols = backtrace_symbols(stack, m_size);
        if (!symbols)
            return;
#elif OS(WINDOWS)
        HANDLE hProc = GetCurrentProcess();
        uint8_t symbolData[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = { 0 };
        auto symbolInfo = reinterpret_cast<SYMBOL_INFO*>(symbolData);

        symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbolInfo->MaxNameLen = MAX_SYM_NAME;
#endif

        for (int i = 0; i < m_size; ++i) {
            const char* mangledName = nullptr;
            const char* cxaDemangled = nullptr;
#if HAVE(BACKTRACE_SYMBOLS)
            mangledName = symbols[i];
#elif OS(WINDOWS)
            if (DbgHelper::SymFromAddress(hProc, reinterpret_cast<DWORD64>(stack[i]), nullptr, symbolInfo))
                mangledName = symbolInfo->Name;
#endif
            auto demangled = demangle(stack[i]);
            if (demangled) {
                mangledName = demangled->mangledName();
                cxaDemangled = demangled->demangledName();
            }
            const int frameNumber = i + 1;
            functor(frameNumber, stack[i], cxaDemangled ? cxaDemangled : mangledName);
        }

#if HAVE(BACKTRACE_SYMBOLS)
        free(symbols);
#endif
    }

private:
    inline static size_t instanceSize(int capacity);

    // We can't default initialize the member since it's not supported by windows
    StackTrace() : m_size(0) // NOLINT
    {
    }

    const char* m_prefix { nullptr };

    // We structure the top fields this way because the underlying stack capture
    // facility will capture from the top of the stack, and we'll need to skip the
    // top 2 frame which is of no interest. Setting up the fields layout this way
    // allows us to capture the stack in place and minimize space wastage.
    union {
        struct {
            int m_size;
            int m_capacity;
        };
        struct {
            void* m_skippedFrame0;
            void* m_skippedFrame1;
        };
    };
    union {
        void** m_borrowedStack;
        void* m_stack[1];
    };
};

} // namespace WTF

using WTF::StackTrace;
